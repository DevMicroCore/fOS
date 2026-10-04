#include "CodeGenerator.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "../optimizer/ConstantFolder.h"

namespace fosc {
namespace {

class Emitter {
 public:
  void opcode(fapp::Opcode instruction, int stackDelta = 0)
  {
    code.push_back(static_cast<std::uint8_t>(instruction));
    lastOpcode = instruction;
    hasOpcode = true;
    adjustStack(stackDelta);
  }

  void u8(std::uint8_t value) { code.push_back(value); }
  void u16(std::uint16_t value) { fapp::appendU16(&code, value); }
  void i32(std::int32_t value) { fapp::appendI32(&code, value); }
  void f64(double value) { fapp::appendF64(&code, value); }

  std::size_t jump(fapp::Opcode instruction, int stackDelta)
  {
    opcode(instruction, stackDelta);
    const std::size_t operand = code.size();
    fapp::appendU32(&code, 0);
    return operand;
  }

  void patchJump(std::size_t operand, std::size_t target)
  {
    const std::int64_t relative = static_cast<std::int64_t>(target) -
                                  static_cast<std::int64_t>(operand + 4);
    fapp::patchU32(&code, operand, static_cast<std::uint32_t>(static_cast<std::int32_t>(relative)));
  }

  std::vector<std::uint8_t> code;
  std::uint16_t maxStack = 0;
  fapp::Opcode lastOpcode = fapp::Opcode::Nop;
  bool hasOpcode = false;

 private:
  void adjustStack(int delta)
  {
    stack_ += delta;
    if (stack_ < 0) stack_ = 0;
    if (stack_ > static_cast<int>(std::numeric_limits<std::uint16_t>::max())) {
      stack_ = std::numeric_limits<std::uint16_t>::max();
    }
    if (stack_ > maxStack) maxStack = static_cast<std::uint16_t>(stack_);
  }

  int stack_ = 0;
};

struct FunctionContext {
  Emitter emitter;
  std::unordered_map<std::string, std::uint16_t> locals;
  bool isInit = false;

  struct LoopContext {
    std::vector<std::size_t> breakJumps;
    std::vector<std::size_t> continueJumps;
  };
  std::vector<LoopContext> loops;
};

class GeneratorImpl {
 public:
  GeneratorImpl(
    const Program& program,
    const std::unordered_map<std::string, UiSymbol>& uiSymbols,
    std::string filename,
    const std::string& source,
    bool optimizeConstants)
    : program_(program),
      uiSymbols_(uiSymbols),
      filename_(std::move(filename)),
      source_(source),
      optimizeConstants_(optimizeConstants)
  {
    module_.sourceHash = fapp::fnv1a32(source_);
  }

  CodeGeneratorResult run()
  {
    declareTopLevelSymbols();
    if (!diagnostics_.empty()) return {std::move(module_), std::move(diagnostics_)};

    module_.functions.resize(1 + functions_.size() + events_.size());
    compileInit();

    std::size_t functionIndex = 1;
    for (const FunctionDeclaration * declaration : functions_) {
      compileFunction(*declaration, static_cast<std::uint16_t>(functionIndex++));
    }
    for (const EventDeclaration * declaration : events_) {
      compileEvent(*declaration, static_cast<std::uint16_t>(functionIndex++));
    }
    return {std::move(module_), std::move(diagnostics_)};
  }

 private:
  void addError(const SourceLocation& location, const char * code, const std::string& message)
  {
    diagnostics_.push_back({code, message, filename_, location});
  }

  void declareTopLevelSymbols()
  {
    std::uint16_t globalIndex = 0;
    std::uint16_t functionIndex = 1;
    for (const StatementPtr& statement : program_.statements) {
      if (statement->kind == StatementKind::VariableDeclaration) {
        const auto& declaration = static_cast<const VariableDeclaration&>(*statement);
        if (globals_.find(declaration.name) != globals_.end()) {
          addError(declaration.location, "FS304", "Duplicate global variable '" + declaration.name + "'.");
        } else if (globalIndex == std::numeric_limits<std::uint16_t>::max()) {
          addError(declaration.location, "FS305", "Too many global variables.");
        } else {
          globals_.emplace(declaration.name, globalIndex++);
        }
      } else if (statement->kind == StatementKind::FunctionDeclaration) {
        const auto& declaration = static_cast<const FunctionDeclaration&>(*statement);
        if (functionIndices_.find(declaration.name) != functionIndices_.end()) {
          addError(declaration.location, "FS306", "Duplicate function '" + declaration.name + "'.");
        } else {
          functionIndices_.emplace(declaration.name, functionIndex++);
          functionArities_.emplace(declaration.name, declaration.parameters.size());
          functions_.push_back(&declaration);
        }
      } else if (statement->kind == StatementKind::EventDeclaration) {
        events_.push_back(&static_cast<const EventDeclaration&>(*statement));
      }
    }
    module_.globalCount = globalIndex;
    module_.initFunction = 0;

    if (1u + functions_.size() + events_.size() > std::numeric_limits<std::uint16_t>::max()) {
      addError({1, 1, 0}, "FS307", "Too many functions and event handlers.");
    }
  }

  void compileInit()
  {
    FunctionContext context;
    context.isInit = true;
    for (const StatementPtr& statement : program_.statements) {
      if (statement->kind == StatementKind::If ||
          statement->kind == StatementKind::While ||
          statement->kind == StatementKind::For) collectLocals(*statement, &context);
    }
    for (const StatementPtr& statement : program_.statements) {
      if (statement->kind == StatementKind::FunctionDeclaration ||
          statement->kind == StatementKind::EventDeclaration) {
        continue;
      }
      compileStatement(*statement, &context, true);
    }
    emitNilReturn(&context);
    finishFunction(&context, 0, 0);
  }

  void compileFunction(const FunctionDeclaration& declaration, std::uint16_t index)
  {
    FunctionContext context;
    for (const std::string& parameter : declaration.parameters) {
      if (context.locals.find(parameter) != context.locals.end()) {
        addError(declaration.location, "FS308", "Duplicate parameter '" + parameter + "'.");
      } else {
        context.locals.emplace(parameter, static_cast<std::uint16_t>(context.locals.size()));
      }
    }
    for (const StatementPtr& statement : declaration.body) collectLocals(*statement, &context);
    for (const StatementPtr& statement : declaration.body) compileStatement(*statement, &context, false);
    emitNilReturn(&context);
    finishFunction(&context, index, static_cast<std::uint16_t>(declaration.parameters.size()));
  }

  void compileEvent(const EventDeclaration& declaration, std::uint16_t index)
  {
    const auto object = uiSymbols_.find(declaration.objectName);
    fapp::UiEvent event;
    uint16_t objectId = 0;
    bool eventValid = false;
    if (fapp::parseSystemEvent(declaration.objectName, declaration.eventName, &event)) {
      objectId = 0;
      eventValid = true;
    } else if (object == uiSymbols_.end()) {
      addError(
        declaration.location,
        "FS309",
        "Unknown UI object '" + declaration.objectName + "'. Supply the matching layout with --ui.");
    } else if (!fapp::parseUiEvent(declaration.eventName, &event)) {
      addError(declaration.location, "FS310", "Unsupported UI event '" + declaration.eventName + "'.");
    } else {
      objectId = object->second.id;
      eventValid = true;
    }
    if (eventValid && (objectId != 0 || fapp::isSystemEvent(event)) &&
        (objectId == 0 || object != uiSymbols_.end())) {
      const std::uint32_t key = (static_cast<std::uint32_t>(objectId) << 8u) |
                                static_cast<std::uint8_t>(event);
      if (!eventKeys_.insert(key).second) {
        addError(declaration.location, "FS311", "Duplicate handler for UI event '" +
          declaration.objectName + "." + declaration.eventName + "'.");
      } else {
        module_.events.push_back({objectId, event, index});
      }
    }

    FunctionContext context;
    for (const StatementPtr& statement : declaration.body) collectLocals(*statement, &context);
    for (const StatementPtr& statement : declaration.body) compileStatement(*statement, &context, false);
    emitNilReturn(&context);
    finishFunction(&context, index, 0);
  }

  void finishFunction(FunctionContext * context, std::uint16_t index, std::uint16_t arity)
  {
    if (index >= module_.functions.size()) return;
    fapp::BytecodeFunction& function = module_.functions[index];
    function.code = std::move(context->emitter.code);
    function.arity = arity;
    function.localCount = static_cast<std::uint16_t>(context->locals.size());
    function.maxStack = context->emitter.maxStack;
  }

  void collectLocals(const Statement& statement, FunctionContext * context)
  {
    if (statement.kind == StatementKind::VariableDeclaration) {
      const auto& declaration = static_cast<const VariableDeclaration&>(statement);
      if (context->locals.find(declaration.name) != context->locals.end()) {
        addError(declaration.location, "FS312", "Duplicate local variable '" + declaration.name + "'.");
      } else if (context->locals.size() >= std::numeric_limits<std::uint16_t>::max()) {
        addError(declaration.location, "FS313", "Too many local variables.");
      } else {
        context->locals.emplace(declaration.name, static_cast<std::uint16_t>(context->locals.size()));
      }
      return;
    }
    if (statement.kind == StatementKind::If) {
      const auto& conditional = static_cast<const IfStatement&>(statement);
      for (const StatementPtr& child : conditional.thenBranch) collectLocals(*child, context);
      for (const auto& branch : conditional.elseIfBranches) {
        for (const StatementPtr& child : branch.body) collectLocals(*child, context);
      }
      for (const StatementPtr& child : conditional.elseBranch) collectLocals(*child, context);
      return;
    }
    if (statement.kind == StatementKind::While) {
      const auto& loop = static_cast<const WhileStatement&>(statement);
      for (const StatementPtr& child : loop.body) collectLocals(*child, context);
      return;
    }
    if (statement.kind == StatementKind::For) {
      const auto& loop = static_cast<const ForStatement&>(statement);
      if (loop.declaresVariable &&
          context->locals.find(loop.variableName) == context->locals.end() &&
          globals_.find(loop.variableName) == globals_.end()) {
        context->locals.emplace(loop.variableName, static_cast<std::uint16_t>(context->locals.size()));
      } else if (loop.declaresVariable &&
                 context->locals.find(loop.variableName) != context->locals.end()) {
        addError(loop.location, "FS312", "Duplicate local variable '" + loop.variableName + "'.");
      }
      for (const StatementPtr& child : loop.body) collectLocals(*child, context);
    }
  }

  void compileStatement(const Statement& statement, FunctionContext * context, bool directTopLevel)
  {
    switch (statement.kind) {
      case StatementKind::VariableDeclaration: {
        const auto& declaration = static_cast<const VariableDeclaration&>(statement);
        if (declaration.initializer) compileExpression(*declaration.initializer, context);
        else context->emitter.opcode(fapp::Opcode::PushNil, 1);

        if (directTopLevel) {
          const auto slot = globals_.find(declaration.name);
          if (slot != globals_.end()) {
            context->emitter.opcode(fapp::Opcode::StoreGlobal, -1);
            context->emitter.u16(slot->second);
          }
        } else {
          const auto slot = context->locals.find(declaration.name);
          if (slot != context->locals.end()) {
            context->emitter.opcode(fapp::Opcode::StoreLocal, -1);
            context->emitter.u16(slot->second);
          }
        }
        return;
      }
      case StatementKind::FunctionDeclaration:
      case StatementKind::EventDeclaration:
        addError(statement.location, "FS314", "Functions and event handlers must be declared at top level.");
        return;
      case StatementKind::If: {
        const auto& conditional = static_cast<const IfStatement&>(statement);
        compileExpression(*conditional.condition, context);
        const std::size_t falseJump = context->emitter.jump(fapp::Opcode::JumpIfFalse, -1);
        for (const StatementPtr& child : conditional.thenBranch) {
          compileStatement(*child, context, false);
        }
        std::vector<std::size_t> endJumps;
        endJumps.push_back(context->emitter.jump(fapp::Opcode::Jump, 0));
        context->emitter.patchJump(falseJump, context->emitter.code.size());
        for (const auto& branch : conditional.elseIfBranches) {
          compileExpression(*branch.condition, context);
          const std::size_t branchFalseJump = context->emitter.jump(fapp::Opcode::JumpIfFalse, -1);
          for (const StatementPtr& child : branch.body) {
            compileStatement(*child, context, false);
          }
          endJumps.push_back(context->emitter.jump(fapp::Opcode::Jump, 0));
          context->emitter.patchJump(branchFalseJump, context->emitter.code.size());
        }
        for (const StatementPtr& child : conditional.elseBranch) {
          compileStatement(*child, context, false);
        }
        for (std::size_t jump : endJumps) {
          context->emitter.patchJump(jump, context->emitter.code.size());
        }
        return;
      }
      case StatementKind::While: {
        const auto& loop = static_cast<const WhileStatement&>(statement);
        const std::size_t loopStart = context->emitter.code.size();
        compileExpression(*loop.condition, context);
        const std::size_t exitJump = context->emitter.jump(fapp::Opcode::JumpIfFalse, -1);
        context->loops.push_back({});
        for (const StatementPtr& child : loop.body) {
          compileStatement(*child, context, false);
        }
        const std::size_t continueTarget = loopStart;
        FunctionContext::LoopContext loopContext = std::move(context->loops.back());
        context->loops.pop_back();
        const std::size_t backJump = context->emitter.jump(fapp::Opcode::Jump, 0);
        context->emitter.patchJump(backJump, continueTarget);
        const std::size_t exitTarget = context->emitter.code.size();
        context->emitter.patchJump(exitJump, exitTarget);
        for (std::size_t jump : loopContext.continueJumps) {
          context->emitter.patchJump(jump, continueTarget);
        }
        for (std::size_t jump : loopContext.breakJumps) {
          context->emitter.patchJump(jump, exitTarget);
        }
        return;
      }
      case StatementKind::For: {
        const auto& loop = static_cast<const ForStatement&>(statement);
        compileExpression(*loop.start, context);
        emitStoreVariable(loop.variableName, loop.location, context);

        const std::size_t loopStart = context->emitter.code.size();
        compileExpression(*loop.step, context);
        context->emitter.opcode(fapp::Opcode::PushInt, 1);
        context->emitter.i32(0);
        context->emitter.opcode(fapp::Opcode::GreaterEqual, -1);
        const std::size_t negativeStepJump = context->emitter.jump(fapp::Opcode::JumpIfFalse, -1);

        emitLoadVariable(loop.variableName, loop.location, context);
        compileExpression(*loop.end, context);
        context->emitter.opcode(fapp::Opcode::LessEqual, -1);
        const std::size_t exitPositiveJump = context->emitter.jump(fapp::Opcode::JumpIfFalse, -1);
        const std::size_t bodyJump = context->emitter.jump(fapp::Opcode::Jump, 0);

        context->emitter.patchJump(negativeStepJump, context->emitter.code.size());
        emitLoadVariable(loop.variableName, loop.location, context);
        compileExpression(*loop.end, context);
        context->emitter.opcode(fapp::Opcode::GreaterEqual, -1);
        const std::size_t exitNegativeJump = context->emitter.jump(fapp::Opcode::JumpIfFalse, -1);

        context->emitter.patchJump(bodyJump, context->emitter.code.size());
        context->loops.push_back({});
        for (const StatementPtr& child : loop.body) {
          compileStatement(*child, context, false);
        }
        FunctionContext::LoopContext loopContext = std::move(context->loops.back());
        context->loops.pop_back();

        const std::size_t continueTarget = context->emitter.code.size();
        emitLoadVariable(loop.variableName, loop.location, context);
        compileExpression(*loop.step, context);
        context->emitter.opcode(fapp::Opcode::Add, -1);
        emitStoreVariable(loop.variableName, loop.location, context);
        const std::size_t backJump = context->emitter.jump(fapp::Opcode::Jump, 0);
        context->emitter.patchJump(backJump, loopStart);

        const std::size_t exitTarget = context->emitter.code.size();
        context->emitter.patchJump(exitPositiveJump, exitTarget);
        context->emitter.patchJump(exitNegativeJump, exitTarget);
        for (std::size_t jump : loopContext.continueJumps) {
          context->emitter.patchJump(jump, continueTarget);
        }
        for (std::size_t jump : loopContext.breakJumps) {
          context->emitter.patchJump(jump, exitTarget);
        }
        return;
      }
      case StatementKind::Break: {
        if (context->loops.empty()) {
          addError(statement.location, "FS330", "Break is only allowed inside a loop.");
          return;
        }
        const std::size_t jump = context->emitter.jump(fapp::Opcode::Jump, 0);
        context->loops.back().breakJumps.push_back(jump);
        return;
      }
      case StatementKind::Continue: {
        if (context->loops.empty()) {
          addError(statement.location, "FS330", "Continue is only allowed inside a loop.");
          return;
        }
        const std::size_t jump = context->emitter.jump(fapp::Opcode::Jump, 0);
        context->loops.back().continueJumps.push_back(jump);
        return;
      }
      case StatementKind::Return: {
        const auto& returnStatement = static_cast<const ReturnStatement&>(statement);
        if (context->isInit) {
          addError(statement.location, "FS315", "Return is not allowed at top level.");
          return;
        }
        if (returnStatement.value) compileExpression(*returnStatement.value, context);
        else context->emitter.opcode(fapp::Opcode::PushNil, 1);
        context->emitter.opcode(fapp::Opcode::Return, -1);
        return;
      }
      case StatementKind::Expression: {
        const auto& expressionStatement = static_cast<const ExpressionStatement&>(statement);
        compileExpression(*expressionStatement.expression, context);
        context->emitter.opcode(fapp::Opcode::Pop, -1);
        return;
      }
    }
  }

  void emitNilReturn(FunctionContext * context)
  {
    // Keep a concrete epilogue even when the preceding statement returned.
    // Conditional branches may contain an unreachable jump that targets this
    // boundary, and every encoded jump target must still be an instruction.
    context->emitter.opcode(fapp::Opcode::PushNil, 1);
    context->emitter.opcode(fapp::Opcode::Return, -1);
  }

  bool compileExpression(const Expression& expression, FunctionContext * context)
  {
    if (optimizeConstants_) {
      ConstantValue folded;
      if (ConstantFolder::evaluate(expression, &folded)) {
        emitConstant(folded, expression.location, context);
        return true;
      }
    }
    switch (expression.kind) {
      case ExpressionKind::Literal:
        return compileLiteral(static_cast<const LiteralExpression&>(expression), context);
      case ExpressionKind::Variable:
        return compileVariable(static_cast<const VariableExpression&>(expression), context);
      case ExpressionKind::Grouping:
        return compileExpression(*static_cast<const GroupingExpression&>(expression).expression, context);
      case ExpressionKind::Unary:
        return compileUnary(static_cast<const UnaryExpression&>(expression), context);
      case ExpressionKind::Binary:
        return compileBinary(static_cast<const BinaryExpression&>(expression), context);
      case ExpressionKind::Assignment:
        return compileAssignment(static_cast<const AssignmentExpression&>(expression), context);
      case ExpressionKind::Member:
        return compileMember(static_cast<const MemberExpression&>(expression), context);
      case ExpressionKind::Call:
        return compileCall(static_cast<const CallExpression&>(expression), context);
    }
    return false;
  }

  void emitConstant(
    const ConstantValue& value,
    const SourceLocation& location,
    FunctionContext * context)
  {
    switch (value.kind) {
      case ConstantValueKind::Nil:
        context->emitter.opcode(fapp::Opcode::PushNil, 1);
        return;
      case ConstantValueKind::Boolean:
        context->emitter.opcode(
          value.booleanValue ? fapp::Opcode::PushTrue : fapp::Opcode::PushFalse,
          1);
        return;
      case ConstantValueKind::Integer:
        context->emitter.opcode(fapp::Opcode::PushInt, 1);
        context->emitter.i32(value.integerValue);
        return;
      case ConstantValueKind::Float:
        context->emitter.opcode(fapp::Opcode::PushFloat, 1);
        context->emitter.f64(value.floatValue);
        return;
      case ConstantValueKind::String:
        context->emitter.opcode(fapp::Opcode::PushString, 1);
        context->emitter.u16(stringConstant(value.stringValue, location));
        return;
    }
  }

  bool compileLiteral(const LiteralExpression& literal, FunctionContext * context)
  {
    try {
      switch (literal.literalKind) {
        case LiteralKind::Nil:
          context->emitter.opcode(fapp::Opcode::PushNil, 1);
          return true;
        case LiteralKind::Boolean:
          context->emitter.opcode(
            literal.value == "true" ? fapp::Opcode::PushTrue : fapp::Opcode::PushFalse,
            1);
          return true;
        case LiteralKind::Integer: {
          const long long value = std::stoll(literal.value);
          if (value < std::numeric_limits<std::int32_t>::min() ||
              value > std::numeric_limits<std::int32_t>::max()) {
            throw std::out_of_range("integer");
          }
          context->emitter.opcode(fapp::Opcode::PushInt, 1);
          context->emitter.i32(static_cast<std::int32_t>(value));
          return true;
        }
        case LiteralKind::Float: {
          const double value = std::stod(literal.value);
          if (!std::isfinite(value)) throw std::out_of_range("float");
          context->emitter.opcode(fapp::Opcode::PushFloat, 1);
          context->emitter.f64(value);
          return true;
        }
        case LiteralKind::String: {
          const std::uint16_t index = stringConstant(literal.value, literal.location);
          context->emitter.opcode(fapp::Opcode::PushString, 1);
          context->emitter.u16(index);
          return true;
        }
      }
    } catch (const std::exception&) {
      addError(literal.location, "FS316", "Numeric literal is outside the supported range.");
    }
    context->emitter.opcode(fapp::Opcode::PushNil, 1);
    return false;
  }

  bool compileVariable(const VariableExpression& variable, FunctionContext * context)
  {
    return emitLoadVariable(variable.name, variable.location, context);
  }

  bool emitLoadVariable(
    const std::string& name,
    const SourceLocation& location,
    FunctionContext * context)
  {
    const auto local = context->locals.find(name);
    if (local != context->locals.end()) {
      context->emitter.opcode(fapp::Opcode::LoadLocal, 1);
      context->emitter.u16(local->second);
      return true;
    }
    const auto global = globals_.find(name);
    if (global != globals_.end()) {
      context->emitter.opcode(fapp::Opcode::LoadGlobal, 1);
      context->emitter.u16(global->second);
      return true;
    }
    addError(location, "FS317", "Unknown variable '" + name + "'.");
    context->emitter.opcode(fapp::Opcode::PushNil, 1);
    return false;
  }

  bool emitStoreVariable(
    const std::string& name,
    const SourceLocation& location,
    FunctionContext * context)
  {
    const auto local = context->locals.find(name);
    if (local != context->locals.end()) {
      context->emitter.opcode(fapp::Opcode::StoreLocal, -1);
      context->emitter.u16(local->second);
      return true;
    }
    const auto global = globals_.find(name);
    if (global != globals_.end()) {
      context->emitter.opcode(fapp::Opcode::StoreGlobal, -1);
      context->emitter.u16(global->second);
      return true;
    }
    addError(location, "FS317", "Unknown variable '" + name + "'.");
    context->emitter.opcode(fapp::Opcode::Pop, -1);
    return false;
  }

  bool compileUnary(const UnaryExpression& unary, FunctionContext * context)
  {
    const bool success = compileExpression(*unary.operand, context);
    if (unary.operation == TokenType::Minus) context->emitter.opcode(fapp::Opcode::Negate);
    else if (unary.operation == TokenType::Bang || unary.operation == TokenType::Not) {
      context->emitter.opcode(fapp::Opcode::LogicalNot);
    } else if (unary.operation != TokenType::Plus) {
      addError(unary.location, "FS318", "Unsupported unary operator.");
      return false;
    }
    return success;
  }

  bool compileBinary(const BinaryExpression& binary, FunctionContext * context)
  {
    const bool left = compileExpression(*binary.left, context);
    const bool right = compileExpression(*binary.right, context);
    fapp::Opcode opcode = fapp::Opcode::Nop;
    switch (binary.operation) {
      case TokenType::Plus: opcode = fapp::Opcode::Add; break;
      case TokenType::Minus: opcode = fapp::Opcode::Subtract; break;
      case TokenType::Star: opcode = fapp::Opcode::Multiply; break;
      case TokenType::Slash: opcode = fapp::Opcode::Divide; break;
      case TokenType::Percent: opcode = fapp::Opcode::Modulo; break;
      case TokenType::EqualEqual: opcode = fapp::Opcode::Equal; break;
      case TokenType::BangEqual: opcode = fapp::Opcode::NotEqual; break;
      case TokenType::Less: opcode = fapp::Opcode::Less; break;
      case TokenType::LessEqual: opcode = fapp::Opcode::LessEqual; break;
      case TokenType::Greater: opcode = fapp::Opcode::Greater; break;
      case TokenType::GreaterEqual: opcode = fapp::Opcode::GreaterEqual; break;
      case TokenType::And:
      case TokenType::AndAnd: opcode = fapp::Opcode::LogicalAnd; break;
      case TokenType::Or:
      case TokenType::OrOr: opcode = fapp::Opcode::LogicalOr; break;
      default:
        addError(binary.location, "FS319", "Unsupported binary operator.");
        break;
    }
    context->emitter.opcode(opcode, -1);
    return left && right && opcode != fapp::Opcode::Nop;
  }

  bool compileAssignment(const AssignmentExpression& assignment, FunctionContext * context)
  {
    const bool success = compileExpression(*assignment.value, context);
    context->emitter.opcode(fapp::Opcode::Duplicate, 1);

    if (assignment.target->kind == ExpressionKind::Variable) {
      const auto& variable = static_cast<const VariableExpression&>(*assignment.target);
      return emitStoreVariable(variable.name, variable.location, context) && success;
    }

    const auto& member = static_cast<const MemberExpression&>(*assignment.target);
    std::uint16_t objectId = 0;
    if (!resolveUiObject(*member.object, &objectId)) {
      addError(member.location, "FS320", "UI property target must start with a named UI object.");
      context->emitter.opcode(fapp::Opcode::Pop, -1);
      return false;
    }
    fapp::UiProperty property;
    if (!fapp::parseUiProperty(member.member, &property)) {
      addError(member.location, "FS321", "Unsupported UI property '" + member.member + "'.");
      context->emitter.opcode(fapp::Opcode::Pop, -1);
      return false;
    }
    context->emitter.opcode(fapp::Opcode::SetUiProperty, -1);
    context->emitter.u16(objectId);
    context->emitter.u8(static_cast<std::uint8_t>(property));
    return success;
  }

  bool compileMember(const MemberExpression& member, FunctionContext * context)
  {
    std::uint16_t objectId = 0;
    if (!resolveUiObject(*member.object, &objectId)) {
      addError(member.location, "FS320", "UI property reference must start with a named UI object.");
      context->emitter.opcode(fapp::Opcode::PushNil, 1);
      return false;
    }
    fapp::UiProperty property;
    if (!fapp::parseUiProperty(member.member, &property)) {
      addError(member.location, "FS321", "Unsupported UI property '" + member.member + "'.");
      context->emitter.opcode(fapp::Opcode::PushNil, 1);
      return false;
    }
    context->emitter.opcode(fapp::Opcode::GetUiProperty, 1);
    context->emitter.u16(objectId);
    context->emitter.u8(static_cast<std::uint8_t>(property));
    return true;
  }

  bool compileCall(const CallExpression& call, FunctionContext * context)
  {
    if (call.arguments.size() > 255) {
      addError(call.location, "FS322", "A call may not have more than 255 arguments.");
      context->emitter.opcode(fapp::Opcode::PushNil, 1);
      return false;
    }

    std::string nativeName;
    fapp::NativeFunction nativeFunction;
    std::uint8_t nativeArity = 0;
    if (resolveNativeCall(*call.callee, &nativeName, &nativeFunction, &nativeArity)) {
      bool success = true;
      if (call.arguments.size() != nativeArity) {
        addError(call.location, "FS329", "Native function '" + nativeName + "' expects " +
          std::to_string(nativeArity) + " argument(s), but received " +
          std::to_string(call.arguments.size()) + ".");
      }
      for (const ExpressionPtr& argument : call.arguments) {
        success = compileExpression(*argument, context) && success;
      }
      context->emitter.opcode(
        fapp::Opcode::CallNative,
        1 - static_cast<int>(call.arguments.size()));
      context->emitter.u8(static_cast<std::uint8_t>(nativeFunction));
      context->emitter.u8(static_cast<std::uint8_t>(call.arguments.size()));
      return success && call.arguments.size() == nativeArity;
    }

    if (call.callee->kind == ExpressionKind::Variable) {
      const auto& callee = static_cast<const VariableExpression&>(*call.callee);
      const auto function = functionIndices_.find(callee.name);
      if (function == functionIndices_.end()) {
        addError(callee.location, "FS323", "Unknown function '" + callee.name + "'.");
        context->emitter.opcode(fapp::Opcode::PushNil, 1);
        return false;
      }
      const auto arity = functionArities_.find(callee.name);
      if (arity != functionArities_.end() && arity->second != call.arguments.size()) {
        addError(call.location, "FS324", "Function '" + callee.name + "' expects " +
          std::to_string(arity->second) + " argument(s), but received " +
          std::to_string(call.arguments.size()) + ".");
      }
      bool success = true;
      for (const ExpressionPtr& argument : call.arguments) {
        success = compileExpression(*argument, context) && success;
      }
      context->emitter.opcode(
        fapp::Opcode::CallFunction,
        1 - static_cast<int>(call.arguments.size()));
      context->emitter.u16(function->second);
      context->emitter.u8(static_cast<std::uint8_t>(call.arguments.size()));
      return success;
    }

    if (call.callee->kind == ExpressionKind::Member) {
      const auto& member = static_cast<const MemberExpression&>(*call.callee);
      std::uint16_t objectId = 0;
      if (!resolveUiObject(*member.object, &objectId)) {
        addError(member.location, "FS325", "Method target must be a named UI object.");
        context->emitter.opcode(fapp::Opcode::PushNil, 1);
        return false;
      }
      fapp::UiMethod method;
      if (!fapp::parseUiMethod(member.member, &method)) {
        addError(member.location, "FS326", "Unsupported UI method '" + member.member + "'.");
        context->emitter.opcode(fapp::Opcode::PushNil, 1);
        return false;
      }
      bool success = true;
      for (const ExpressionPtr& argument : call.arguments) {
        success = compileExpression(*argument, context) && success;
      }
      context->emitter.opcode(
        fapp::Opcode::CallUiMethod,
        1 - static_cast<int>(call.arguments.size()));
      context->emitter.u16(objectId);
      context->emitter.u8(static_cast<std::uint8_t>(method));
      context->emitter.u8(static_cast<std::uint8_t>(call.arguments.size()));
      return success;
    }

    addError(call.location, "FS327", "Unsupported call target.");
    context->emitter.opcode(fapp::Opcode::PushNil, 1);
    return false;
  }

  static bool resolveNativeCall(
    const Expression& callee,
    std::string * name,
    fapp::NativeFunction * function,
    std::uint8_t * arity)
  {
    if (name == nullptr || function == nullptr || arity == nullptr) return false;
    if (callee.kind == ExpressionKind::Variable) {
      *name = static_cast<const VariableExpression&>(callee).name;
      return fapp::parseNativeFunction(*name, function, arity);
    }
    if (callee.kind != ExpressionKind::Member) return false;
    const auto& member = static_cast<const MemberExpression&>(callee);
    if (member.object->kind != ExpressionKind::Variable) return false;
    *name = static_cast<const VariableExpression&>(*member.object).name + "." + member.member;
    return fapp::parseNativeFunction(*name, function, arity);
  }

  bool resolveUiObject(const Expression& expression, std::uint16_t * objectId) const
  {
    if (expression.kind != ExpressionKind::Variable) return false;
    const auto& variable = static_cast<const VariableExpression&>(expression);
    const auto symbol = uiSymbols_.find(variable.name);
    if (symbol == uiSymbols_.end()) return false;
    *objectId = symbol->second.id;
    return true;
  }

  std::uint16_t stringConstant(const std::string& value, const SourceLocation& location)
  {
    if (value.size() > fapp::kMaximumRuntimeStringBytes) {
      addError(location, "FS330", "String constant exceeds the embedded runtime limit of 255 bytes.");
      return 0;
    }
    const auto existing = stringIndices_.find(value);
    if (existing != stringIndices_.end()) return existing->second;
    if (module_.stringConstants.size() >= fapp::kMaximumRuntimeConstants) {
      addError(location, "FS328", "Too many string constants.");
      return 0;
    }
    const std::uint16_t index = static_cast<std::uint16_t>(module_.stringConstants.size());
    module_.stringConstants.push_back(value);
    stringIndices_.emplace(value, index);
    return index;
  }

  const Program& program_;
  const std::unordered_map<std::string, UiSymbol>& uiSymbols_;
  std::string filename_;
  const std::string& source_;
  fapp::BytecodeModule module_;
  std::vector<Diagnostic> diagnostics_;
  std::unordered_map<std::string, std::uint16_t> globals_;
  std::unordered_map<std::string, std::uint16_t> functionIndices_;
  std::unordered_map<std::string, std::size_t> functionArities_;
  std::unordered_map<std::string, std::uint16_t> stringIndices_;
  std::vector<const FunctionDeclaration *> functions_;
  std::vector<const EventDeclaration *> events_;
  std::unordered_set<std::uint32_t> eventKeys_;
  bool optimizeConstants_;
};

}  // namespace

CodeGenerator::CodeGenerator(
  const Program& program,
  const std::unordered_map<std::string, UiSymbol>& uiSymbols,
  std::string filename,
  std::string source,
  bool optimizeConstants)
  : program_(program),
    uiSymbols_(uiSymbols),
    filename_(std::move(filename)),
    source_(std::move(source)),
    optimizeConstants_(optimizeConstants)
{
}

CodeGeneratorResult CodeGenerator::generate()
{
  GeneratorImpl generator(program_, uiSymbols_, std::move(filename_), source_, optimizeConstants_);
  return generator.run();
}

}  // namespace fosc
