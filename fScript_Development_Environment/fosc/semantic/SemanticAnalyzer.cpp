#include "SemanticAnalyzer.h"

#include <cstdint>
#include <unordered_set>
#include <utility>

#include "../bytecode/Bytecode.h"

namespace fosc {
namespace {

struct SymbolInfo {
  ValueType type = ValueType::Unknown;
  SourceLocation location;
};

struct FunctionInfo {
  std::size_t arity = 0;
  ValueType returnType = ValueType::Unknown;
  SourceLocation location;
};

enum class AnalysisContext {
  Init,
  Function,
  Event
};

class AnalyzerImpl {
 public:
  AnalyzerImpl(
    const Program& program,
    const std::unordered_map<std::string, UiSymbol>& uiSymbols,
    std::string filename)
    : program_(program), uiSymbols_(uiSymbols), filename_(std::move(filename)) {}

  SemanticAnalyzerResult run()
  {
    declareTopLevelSymbols();
    analyzeInit();
    analyzeFunctions();
    analyzeEvents();
    return {std::move(diagnostics_), globals_.size(), functions_.size(), eventCount_};
  }

 private:
  void error(const SourceLocation& location, const char * code, const std::string& message)
  {
    diagnostics_.push_back({code, message, filename_, location});
  }

  void declareTopLevelSymbols()
  {
    for (const StatementPtr& statement : program_.statements) {
      if (statement->kind == StatementKind::VariableDeclaration) {
        const auto& declaration = static_cast<const VariableDeclaration&>(*statement);
        if (globals_.find(declaration.name) != globals_.end()) {
          error(declaration.location, "FS501", "Duplicate global variable '" + declaration.name + "'.");
        } else if (functions_.find(declaration.name) != functions_.end()) {
          error(declaration.location, "FS501", "Name '" + declaration.name + "' is already used by a function.");
        } else {
          globals_.emplace(declaration.name, SymbolInfo{ValueType::Unknown, declaration.location});
        }
      } else if (statement->kind == StatementKind::FunctionDeclaration) {
        const auto& declaration = static_cast<const FunctionDeclaration&>(*statement);
        if (functions_.find(declaration.name) != functions_.end()) {
          error(declaration.location, "FS501", "Duplicate function '" + declaration.name + "'.");
        } else if (globals_.find(declaration.name) != globals_.end()) {
          error(declaration.location, "FS501", "Name '" + declaration.name + "' is already used by a global variable.");
        } else {
          functions_.emplace(
            declaration.name,
            FunctionInfo{declaration.parameters.size(), ValueType::Unknown, declaration.location});
        }
      } else if (statement->kind == StatementKind::EventDeclaration) {
        ++eventCount_;
      }
    }
  }

  void analyzeInit()
  {
    AnalysisContext previousContext = context_;
    context_ = AnalysisContext::Init;
    locals_.clear();
    for (const StatementPtr& statement : program_.statements) {
      if (statement->kind == StatementKind::If ||
          statement->kind == StatementKind::While ||
          statement->kind == StatementKind::For) collectLocals(*statement);
    }
    for (const StatementPtr& statement : program_.statements) {
      if (statement->kind == StatementKind::FunctionDeclaration ||
          statement->kind == StatementKind::EventDeclaration) continue;
      analyzeStatement(*statement, true);
    }
    context_ = previousContext;
  }

  void analyzeFunctions()
  {
    for (const StatementPtr& statement : program_.statements) {
      if (statement->kind != StatementKind::FunctionDeclaration) continue;
      const auto& declaration = static_cast<const FunctionDeclaration&>(*statement);
      auto function = functions_.find(declaration.name);
      if (function == functions_.end() || function->second.location.offset != declaration.location.offset) {
        continue;
      }

      context_ = AnalysisContext::Function;
      locals_.clear();
      currentReturnType_ = ValueType::Unknown;
      hasReturn_ = false;
      for (const std::string& parameter : declaration.parameters) {
        if (locals_.find(parameter) != locals_.end()) {
          error(declaration.location, "FS502", "Duplicate parameter '" + parameter + "'.");
        } else {
          locals_.emplace(parameter, SymbolInfo{ValueType::Any, declaration.location});
        }
      }
      for (const StatementPtr& child : declaration.body) collectLocals(*child);
      for (const StatementPtr& child : declaration.body) analyzeStatement(*child, false);
      function->second.returnType = hasReturn_ ? currentReturnType_ : ValueType::Nil;
    }
  }

  void analyzeEvents()
  {
    std::unordered_set<std::uint32_t> eventKeys;
    for (const StatementPtr& statement : program_.statements) {
      if (statement->kind != StatementKind::EventDeclaration) continue;
      const auto& declaration = static_cast<const EventDeclaration&>(*statement);
      const auto object = uiSymbols_.find(declaration.objectName);
      fapp::UiEvent event;
      uint16_t objectId = 0;
      const bool systemEvent = fapp::parseSystemEvent(declaration.objectName, declaration.eventName, &event);
      if (systemEvent) {
        objectId = 0;
      } else if (object == uiSymbols_.end()) {
        error(
          declaration.location,
          "FS512",
          "Unknown UI object '" + declaration.objectName + "'. Supply the matching layout with --ui.");
      } else if (!fapp::parseUiEvent(declaration.eventName, &event)) {
        error(declaration.location, "FS517", "Unsupported UI event '" + declaration.eventName + "'.");
      } else {
        objectId = object->second.id;
        if (!supportsEvent(object->second.type, event)) {
          error(
            declaration.location,
            "FS517",
            "UI event '" + declaration.eventName + "' is not supported by " +
              object->second.type + " '" + declaration.objectName + "'.");
        }
      }
      if (systemEvent || objectId != 0) {
        const std::uint32_t key = (static_cast<std::uint32_t>(objectId) << 8u) |
                                  static_cast<std::uint8_t>(event);
        if (!eventKeys.insert(key).second) {
          error(
            declaration.location,
            "FS517",
            "Duplicate handler for UI event '" + declaration.objectName + "." +
              declaration.eventName + "'.");
        }
      }

      context_ = AnalysisContext::Event;
      locals_.clear();
      for (const StatementPtr& child : declaration.body) collectLocals(*child);
      for (const StatementPtr& child : declaration.body) analyzeStatement(*child, false);
    }
  }

  void collectLocals(const Statement& statement)
  {
    if (statement.kind == StatementKind::VariableDeclaration) {
      const auto& declaration = static_cast<const VariableDeclaration&>(statement);
      if (locals_.find(declaration.name) != locals_.end()) {
        error(declaration.location, "FS502", "Duplicate local variable '" + declaration.name + "'.");
      } else {
        locals_.emplace(declaration.name, SymbolInfo{ValueType::Unknown, declaration.location});
      }
      return;
    }
    if (statement.kind == StatementKind::If) {
      const auto& conditional = static_cast<const IfStatement&>(statement);
      for (const StatementPtr& child : conditional.thenBranch) collectLocals(*child);
      for (const auto& branch : conditional.elseIfBranches) {
        for (const StatementPtr& child : branch.body) collectLocals(*child);
      }
      for (const StatementPtr& child : conditional.elseBranch) collectLocals(*child);
      return;
    }
    if (statement.kind == StatementKind::While) {
      const auto& loop = static_cast<const WhileStatement&>(statement);
      for (const StatementPtr& child : loop.body) collectLocals(*child);
      return;
    }
    if (statement.kind == StatementKind::For) {
      const auto& loop = static_cast<const ForStatement&>(statement);
      if (loop.declaresVariable && locals_.find(loop.variableName) == locals_.end() &&
          globals_.find(loop.variableName) == globals_.end()) {
        locals_.emplace(loop.variableName, SymbolInfo{ValueType::Unknown, loop.location});
      } else if (loop.declaresVariable && locals_.find(loop.variableName) != locals_.end()) {
        error(loop.location, "FS502", "Duplicate local variable '" + loop.variableName + "'.");
      }
      for (const StatementPtr& child : loop.body) collectLocals(*child);
    }
  }

  void analyzeStatement(const Statement& statement, bool directTopLevel)
  {
    switch (statement.kind) {
      case StatementKind::VariableDeclaration: {
        const auto& declaration = static_cast<const VariableDeclaration&>(statement);
        SymbolInfo * symbol = directTopLevel
          ? findGlobal(declaration.name)
          : findLocal(declaration.name);
        if (symbol != nullptr && declaration.initializer) {
          assignType(symbol, infer(*declaration.initializer), declaration.location, declaration.name);
        }
        return;
      }
      case StatementKind::FunctionDeclaration:
      case StatementKind::EventDeclaration:
        error(statement.location, "FS503", "Functions and event handlers must be declared at top level.");
        return;
      case StatementKind::If: {
        const auto& conditional = static_cast<const IfStatement&>(statement);
        const ValueType condition = infer(*conditional.condition);
        if (!isDynamic(condition) && condition != ValueType::Boolean) {
          error(
            conditional.condition->location,
            "FS509",
            "If condition must be boolean, but is " + std::string(valueTypeName(condition)) + ".");
        }
        for (const StatementPtr& child : conditional.thenBranch) analyzeStatement(*child, false);
        for (const auto& branch : conditional.elseIfBranches) {
          const ValueType elseIfCondition = infer(*branch.condition);
          if (!isDynamic(elseIfCondition) && elseIfCondition != ValueType::Boolean) {
            error(
              branch.condition->location,
              "FS509",
              "Elseif condition must be boolean, but is " +
                std::string(valueTypeName(elseIfCondition)) + ".");
          }
          for (const StatementPtr& child : branch.body) analyzeStatement(*child, false);
        }
        for (const StatementPtr& child : conditional.elseBranch) analyzeStatement(*child, false);
        return;
      }
      case StatementKind::While: {
        const auto& loop = static_cast<const WhileStatement&>(statement);
        const ValueType condition = infer(*loop.condition);
        if (!isDynamic(condition) && condition != ValueType::Boolean) {
          error(
            loop.condition->location,
            "FS509",
            "While condition must be boolean, but is " + std::string(valueTypeName(condition)) + ".");
        }
        ++loopDepth_;
        for (const StatementPtr& child : loop.body) analyzeStatement(*child, false);
        --loopDepth_;
        return;
      }
      case StatementKind::For: {
        const auto& loop = static_cast<const ForStatement&>(statement);
        SymbolInfo * symbol = resolveVariable(loop.variableName);
        if (symbol == nullptr) {
          error(loop.location, "FS504", "Unknown loop variable '" + loop.variableName + "'. Use 'for var " +
            loop.variableName + " = ...' to declare it.");
        }
        const ValueType start = infer(*loop.start);
        const ValueType end = infer(*loop.end);
        const ValueType step = infer(*loop.step);
        checkNumeric(start, loop.start->location, "For loop start");
        checkNumeric(end, loop.end->location, "For loop end");
        checkNumeric(step, loop.step->location, "For loop step");
        if (isLiteralZero(*loop.step)) {
          error(loop.step->location, "FS522", "For loop step may not be zero.");
        }
        if (symbol != nullptr) {
          assignType(symbol, start, loop.location, loop.variableName);
          if (!isDynamic(symbol->type) && !isNumericType(symbol->type)) {
            error(loop.location, "FS522", "For loop variable '" + loop.variableName + "' must be numeric.");
          }
        }
        ++loopDepth_;
        for (const StatementPtr& child : loop.body) analyzeStatement(*child, false);
        --loopDepth_;
        return;
      }
      case StatementKind::Break:
        if (loopDepth_ == 0) error(statement.location, "FS521", "Break is only allowed inside a loop.");
        return;
      case StatementKind::Continue:
        if (loopDepth_ == 0) error(statement.location, "FS521", "Continue is only allowed inside a loop.");
        return;
      case StatementKind::Return: {
        const auto& returnStatement = static_cast<const ReturnStatement&>(statement);
        if (context_ == AnalysisContext::Init) {
          error(statement.location, "FS511", "Return is not allowed at top level.");
          if (returnStatement.value) infer(*returnStatement.value);
          return;
        }
        const ValueType returnType = returnStatement.value
          ? infer(*returnStatement.value)
          : ValueType::Nil;
        if (context_ == AnalysisContext::Event) {
          if (returnStatement.value && returnType != ValueType::Nil) {
            error(statement.location, "FS518", "Event handlers may only use return without a value.");
          }
          return;
        }
        mergeReturnType(returnType, statement.location);
        return;
      }
      case StatementKind::Expression: {
        const auto& expression = static_cast<const ExpressionStatement&>(statement);
        infer(*expression.expression);
        return;
      }
    }
  }

  ValueType infer(const Expression& expression)
  {
    switch (expression.kind) {
      case ExpressionKind::Literal: {
        const auto& literal = static_cast<const LiteralExpression&>(expression);
        switch (literal.literalKind) {
          case LiteralKind::Integer: return ValueType::Integer;
          case LiteralKind::Float: return ValueType::Float;
          case LiteralKind::String: return ValueType::String;
          case LiteralKind::Boolean: return ValueType::Boolean;
          case LiteralKind::Nil: return ValueType::Nil;
        }
        return ValueType::Unknown;
      }
      case ExpressionKind::Variable: {
        const auto& variable = static_cast<const VariableExpression&>(expression);
        SymbolInfo * symbol = resolveVariable(variable.name);
        if (symbol == nullptr) {
          error(variable.location, "FS504", "Unknown variable '" + variable.name + "'.");
          return ValueType::Any;
        }
        return symbol->type == ValueType::Unknown ? ValueType::Any : symbol->type;
      }
      case ExpressionKind::Grouping:
        return infer(*static_cast<const GroupingExpression&>(expression).expression);
      case ExpressionKind::Unary:
        return inferUnary(static_cast<const UnaryExpression&>(expression));
      case ExpressionKind::Binary:
        return inferBinary(static_cast<const BinaryExpression&>(expression));
      case ExpressionKind::Assignment:
        return inferAssignment(static_cast<const AssignmentExpression&>(expression));
      case ExpressionKind::Member:
        return inferMember(static_cast<const MemberExpression&>(expression));
      case ExpressionKind::Call:
        return inferCall(static_cast<const CallExpression&>(expression));
    }
    return ValueType::Any;
  }

  ValueType inferUnary(const UnaryExpression& unary)
  {
    const ValueType operand = infer(*unary.operand);
    if (unary.operation == TokenType::Bang || unary.operation == TokenType::Not) {
      if (!isDynamic(operand) && operand != ValueType::Boolean) {
        operatorError(unary.location, "not", operand, ValueType::Unknown);
      }
      return ValueType::Boolean;
    }
    if (unary.operation == TokenType::Plus || unary.operation == TokenType::Minus) {
      if (!isDynamic(operand) && !isNumericType(operand)) {
        operatorError(unary.location, "numeric unary", operand, ValueType::Unknown);
      }
      return isDynamic(operand) ? ValueType::Any : operand;
    }
    return ValueType::Any;
  }

  ValueType inferBinary(const BinaryExpression& binary)
  {
    const ValueType left = infer(*binary.left);
    const ValueType right = infer(*binary.right);
    if (isDynamic(left) || isDynamic(right)) return ValueType::Any;

    switch (binary.operation) {
      case TokenType::Plus:
        // String concatenation converts scalar values using the runtime's
        // deterministic text representation.
        if (left == ValueType::String || right == ValueType::String) return ValueType::String;
        if (isNumericType(left) && isNumericType(right)) return commonNumericType(left, right);
        operatorError(binary.location, "+", left, right);
        return ValueType::Any;
      case TokenType::Minus:
      case TokenType::Star:
        if (isNumericType(left) && isNumericType(right)) return commonNumericType(left, right);
        operatorError(binary.location, binary.operation == TokenType::Minus ? "-" : "*", left, right);
        return ValueType::Any;
      case TokenType::Slash:
        if (isNumericType(left) && isNumericType(right)) return ValueType::Float;
        operatorError(binary.location, "/", left, right);
        return ValueType::Any;
      case TokenType::Percent:
        if (left == ValueType::Integer && right == ValueType::Integer) return ValueType::Integer;
        operatorError(binary.location, "%", left, right);
        return ValueType::Any;
      case TokenType::EqualEqual:
      case TokenType::BangEqual:
        if (left == right || (isNumericType(left) && isNumericType(right)) ||
            left == ValueType::Nil || right == ValueType::Nil) return ValueType::Boolean;
        operatorError(binary.location, "equality", left, right);
        return ValueType::Boolean;
      case TokenType::Less:
      case TokenType::LessEqual:
      case TokenType::Greater:
      case TokenType::GreaterEqual:
        if ((isNumericType(left) && isNumericType(right)) ||
            (left == ValueType::String && right == ValueType::String)) return ValueType::Boolean;
        operatorError(binary.location, "comparison", left, right);
        return ValueType::Boolean;
      case TokenType::And:
      case TokenType::AndAnd:
      case TokenType::Or:
      case TokenType::OrOr:
        if (left == ValueType::Boolean && right == ValueType::Boolean) return ValueType::Boolean;
        operatorError(binary.location, "boolean", left, right);
        return ValueType::Boolean;
      default:
        return ValueType::Any;
    }
  }

  ValueType inferAssignment(const AssignmentExpression& assignment)
  {
    const ValueType value = infer(*assignment.value);
    if (assignment.target->kind == ExpressionKind::Variable) {
      const auto& variable = static_cast<const VariableExpression&>(*assignment.target);
      SymbolInfo * symbol = resolveVariable(variable.name);
      if (symbol == nullptr) {
        error(variable.location, "FS504", "Unknown variable '" + variable.name + "'.");
      } else {
        assignType(symbol, value, assignment.location, variable.name);
      }
      return value;
    }

    const auto& member = static_cast<const MemberExpression&>(*assignment.target);
    ValueType propertyType = ValueType::Any;
    if (resolveProperty(member, &propertyType)) {
      if (value == ValueType::Nil || !isAssignableType(propertyType, value)) {
        error(
          assignment.location,
          "FS514",
          "Cannot assign " + std::string(valueTypeName(value)) + " to UI property '" +
            member.member + "' of type " + valueTypeName(propertyType) + ".");
      }
    }
    return value;
  }

  ValueType inferMember(const MemberExpression& member)
  {
    ValueType propertyType = ValueType::Any;
    resolveProperty(member, &propertyType);
    return propertyType;
  }

  ValueType inferCall(const CallExpression& call)
  {
    std::vector<ValueType> argumentTypes;
    argumentTypes.reserve(call.arguments.size());
    for (const ExpressionPtr& argument : call.arguments) argumentTypes.push_back(infer(*argument));

    std::string nativeName;
    fapp::NativeFunction nativeFunction;
    std::uint8_t nativeArity = 0;
    if (resolveNativeCall(*call.callee, &nativeName, &nativeFunction, &nativeArity)) {
      if (call.arguments.size() != nativeArity) {
        error(call.location, "FS519", "Native function '" + nativeName + "' expects " +
          std::to_string(nativeArity) + " argument(s), but received " +
          std::to_string(call.arguments.size()) + ".");
      }
      for (std::size_t index = 0; index < argumentTypes.size() && index < nativeArity; ++index) {
        const ValueType expected = nativeArgumentType(nativeFunction, index);
        if (expected != ValueType::Any && !isDynamic(argumentTypes[index]) &&
            argumentTypes[index] != expected) {
          error(call.arguments[index]->location, "FS520", "Argument " +
            std::to_string(index + 1) + " of '" + nativeName + "' must be " +
            valueTypeName(expected) + ".");
        }
      }
      return nativeReturnType(nativeFunction);
    }

    if (call.callee->kind == ExpressionKind::Variable) {
      const auto& callee = static_cast<const VariableExpression&>(*call.callee);
      const auto function = functions_.find(callee.name);
      if (function == functions_.end()) {
        error(callee.location, "FS505", "Unknown function '" + callee.name + "'.");
        return ValueType::Any;
      }
      if (function->second.arity != call.arguments.size()) {
        error(
          call.location,
          "FS506",
          "Function '" + callee.name + "' expects " +
            std::to_string(function->second.arity) + " argument(s), but received " +
            std::to_string(call.arguments.size()) + ".");
      }
      return function->second.returnType == ValueType::Unknown
        ? ValueType::Any
        : function->second.returnType;
    }

    if (call.callee->kind == ExpressionKind::Member) {
      const auto& member = static_cast<const MemberExpression&>(*call.callee);
      const UiSymbol * object = resolveUiObject(*member.object, member.location);
      if (object == nullptr) return ValueType::Any;
      fapp::UiMethod method;
      if (!fapp::parseUiMethod(member.member, &method) || !supportsMethod(object->type, method)) {
        error(
          member.location,
          "FS515",
          "UI method '" + member.member + "' is not supported by " + object->type + ".");
      }
      const bool fileMethod =
        method == fapp::UiMethod::LoadFile || method == fapp::UiMethod::SaveFile;
      std::size_t expectedArguments = fileMethod ? 1 : 0;
      if (object->type == "canvas") {
        if (method == fapp::UiMethod::Clear) expectedArguments = 1;
        else if (method == fapp::UiMethod::Pixel) expectedArguments = 3;
        else if (method == fapp::UiMethod::Line || method == fapp::UiMethod::Rect) expectedArguments = 6;
        else if (method == fapp::UiMethod::Circle || method == fapp::UiMethod::DrawText) expectedArguments = 5;
      }
      if (call.arguments.size() != expectedArguments) {
        error(call.location, "FS516", "UI method '" + member.member + "' expects " +
          std::to_string(expectedArguments) + " argument(s).");
      }
      if (fileMethod && !argumentTypes.empty() && !isDynamic(argumentTypes[0]) &&
          argumentTypes[0] != ValueType::String) {
        error(call.arguments[0]->location, "FS520", "The file path must be a string.");
      }
      if (object->type == "canvas") {
        for (std::size_t index = 0; index < argumentTypes.size(); ++index) {
          const bool textArgument = method == fapp::UiMethod::DrawText && index == 2;
          const ValueType expected = textArgument ? ValueType::String :
            ((method == fapp::UiMethod::Rect || method == fapp::UiMethod::Circle) &&
             index + 1 == argumentTypes.size() ? ValueType::Boolean : ValueType::Integer);
          if (!isDynamic(argumentTypes[index]) && argumentTypes[index] != expected) {
            error(call.arguments[index]->location, "FS520", "Canvas argument " +
              std::to_string(index + 1) + " must be " + valueTypeName(expected) + ".");
          }
        }
        return ValueType::Boolean;
      }
      return fileMethod ? ValueType::Boolean : ValueType::Nil;
    }

    error(call.location, "FS505", "Unsupported call target.");
    return ValueType::Any;
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

  static ValueType nativeArgumentType(fapp::NativeFunction function, std::size_t index)
  {
    if (function == fapp::NativeFunction::HttpText ||
        function == fapp::NativeFunction::TimerStart) return ValueType::Integer;
    if (function == fapp::NativeFunction::Text) return ValueType::Any;
    if (function == fapp::NativeFunction::Round) return ValueType::Float;
    if (function == fapp::NativeFunction::StringSlice) {
      return index == 0 ? ValueType::String : ValueType::Integer;
    }
    if (function == fapp::NativeFunction::StringLastIndex) return ValueType::String;
    return ValueType::String;
  }

  static ValueType nativeReturnType(fapp::NativeFunction function)
  {
    switch (function) {
      case fapp::NativeFunction::HttpGet:
      case fapp::NativeFunction::FileWrite:
      case fapp::NativeFunction::AudioPlay:
      case fapp::NativeFunction::WifiStatus:
      case fapp::NativeFunction::TimerStart:
        return ValueType::Boolean;
      case fapp::NativeFunction::HttpStatus:
      case fapp::NativeFunction::StringLength:
      case fapp::NativeFunction::Round:
      case fapp::NativeFunction::StringLastIndex:
      case fapp::NativeFunction::EventX:
      case fapp::NativeFunction::EventY:
      case fapp::NativeFunction::EventScreenX:
      case fapp::NativeFunction::EventScreenY:
        return ValueType::Integer;
      case fapp::NativeFunction::EventPressed:
        return ValueType::Boolean;
      case fapp::NativeFunction::NumberParse:
        return ValueType::Float;
      case fapp::NativeFunction::SystemRestart:
      case fapp::NativeFunction::SerialPrintf:
        return ValueType::Nil;
      default:
        return ValueType::String;
    }
  }

  bool resolveProperty(const MemberExpression& member, ValueType * propertyType)
  {
    const UiSymbol * object = resolveUiObject(*member.object, member.location);
    if (object == nullptr) return false;
    fapp::UiProperty property;
    if (!fapp::parseUiProperty(member.member, &property) ||
        !propertyTypeFor(object->type, property, propertyType)) {
      error(
        member.location,
        "FS513",
        "UI property '" + member.member + "' is not supported by " + object->type + ".");
      *propertyType = ValueType::Any;
      return false;
    }
    return true;
  }

  const UiSymbol * resolveUiObject(const Expression& expression, const SourceLocation& location)
  {
    if (expression.kind != ExpressionKind::Variable) {
      error(location, "FS512", "UI access must start with a named UI object.");
      return nullptr;
    }
    const auto& variable = static_cast<const VariableExpression&>(expression);
    const auto object = uiSymbols_.find(variable.name);
    if (object == uiSymbols_.end()) {
      error(location, "FS512", "Unknown UI object '" + variable.name + "'.");
      return nullptr;
    }
    return &object->second;
  }

  static bool propertyTypeFor(
    const std::string& objectType,
    fapp::UiProperty property,
    ValueType * type)
  {
    if (property == fapp::UiProperty::Hidden || property == fapp::UiProperty::Enabled) {
      *type = ValueType::Boolean;
      return true;
    }
    if (property == fapp::UiProperty::Text) {
      if (objectType == "label" || objectType == "button" || objectType == "textarea" ||
          objectType == "checkbox" || objectType == "roller" || objectType == "dropdown") {
        *type = ValueType::String;
        return true;
      }
      return false;
    }
    if (property == fapp::UiProperty::Value) {
      if (objectType == "textarea") {
        *type = ValueType::String;
        return true;
      }
      if (objectType == "switch" || objectType == "checkbox") {
        *type = ValueType::Boolean;
        return true;
      }
      if (objectType == "roller" || objectType == "dropdown") {
        *type = ValueType::Integer;
        return true;
      }
      return false;
    }
    if (property == fapp::UiProperty::Checked &&
        (objectType == "switch" || objectType == "checkbox")) {
      *type = ValueType::Boolean;
      return true;
    }
    return false;
  }

  static bool supportsMethod(const std::string& objectType, fapp::UiMethod method)
  {
    if (method == fapp::UiMethod::Clear) return objectType == "textarea" || objectType == "canvas";
    if (method == fapp::UiMethod::Pixel || method == fapp::UiMethod::Line ||
        method == fapp::UiMethod::Rect || method == fapp::UiMethod::Circle ||
        method == fapp::UiMethod::DrawText) return objectType == "canvas";
    if (method == fapp::UiMethod::Focus || method == fapp::UiMethod::Blur) {
      return objectType == "button" || objectType == "textarea" ||
             objectType == "switch" || objectType == "checkbox" ||
             objectType == "roller" || objectType == "dropdown";
    }
    if (method == fapp::UiMethod::ScrollToTop || method == fapp::UiMethod::ScrollToBottom) {
      return objectType == "textarea" || objectType == "panel" || objectType == "roller";
    }
    if (method == fapp::UiMethod::LoadFile || method == fapp::UiMethod::SaveFile) {
      return objectType == "textarea";
    }
    return false;
  }

  static bool supportsEvent(const std::string& objectType, fapp::UiEvent event)
  {
    if (event == fapp::UiEvent::Click) return true;
    if (event == fapp::UiEvent::Changed || event == fapp::UiEvent::ValueChanged) {
      return objectType == "textarea" || objectType == "switch" || objectType == "checkbox" ||
             objectType == "roller" || objectType == "dropdown";
    }
    if (event == fapp::UiEvent::Pressed || event == fapp::UiEvent::Released) {
      return objectType == "button" || objectType == "switch" || objectType == "checkbox";
    }
    if (event == fapp::UiEvent::Ready || event == fapp::UiEvent::Cancel) {
      return objectType == "keyboard";
    }
    if (event == fapp::UiEvent::PointerDown || event == fapp::UiEvent::PointerMove ||
        event == fapp::UiEvent::PointerUp) return objectType == "canvas" || objectType == "panel";
    return false;
  }

  void assignType(
    SymbolInfo * symbol,
    ValueType value,
    const SourceLocation& location,
    const std::string& name)
  {
    if (symbol == nullptr) return;
    if (symbol->type == ValueType::Unknown || symbol->type == ValueType::Nil) {
      if (value != ValueType::Nil && value != ValueType::Unknown) symbol->type = value;
      return;
    }
    if (!isAssignableType(symbol->type, value)) {
      error(
        location,
        "FS507",
        "Cannot assign " + std::string(valueTypeName(value)) + " to '" + name +
          "' of type " + valueTypeName(symbol->type) + ".");
    }
  }

  void mergeReturnType(ValueType value, const SourceLocation& location)
  {
    if (!hasReturn_) {
      currentReturnType_ = value;
      hasReturn_ = true;
      return;
    }
    if (currentReturnType_ == value) return;
    if (isDynamic(currentReturnType_) || isDynamic(value) ||
        currentReturnType_ == ValueType::Nil || value == ValueType::Nil) {
      currentReturnType_ = ValueType::Any;
      return;
    }
    if (isNumericType(currentReturnType_) && isNumericType(value)) {
      currentReturnType_ = ValueType::Float;
      return;
    }
    error(
      location,
      "FS510",
      "Function returns incompatible types " + std::string(valueTypeName(currentReturnType_)) +
        " and " + valueTypeName(value) + ".");
    currentReturnType_ = ValueType::Any;
  }

  void operatorError(
    const SourceLocation& location,
    const std::string& operation,
    ValueType left,
    ValueType right)
  {
    std::string message = "Operator '" + operation + "' cannot be applied to " + valueTypeName(left);
    if (right != ValueType::Unknown) message += " and " + std::string(valueTypeName(right));
    message += ".";
    error(location, "FS508", message);
  }

  static bool isDynamic(ValueType type)
  {
    return type == ValueType::Any || type == ValueType::Unknown;
  }

  void checkNumeric(ValueType type, const SourceLocation& location, const std::string& label)
  {
    if (!isDynamic(type) && !isNumericType(type)) {
      error(location, "FS522", label + " must be numeric, but is " + valueTypeName(type) + ".");
    }
  }

  static bool isLiteralZero(const Expression& expression)
  {
    if (expression.kind != ExpressionKind::Literal) return false;
    const auto& literal = static_cast<const LiteralExpression&>(expression);
    if (literal.literalKind != LiteralKind::Integer && literal.literalKind != LiteralKind::Float) return false;
    try {
      return std::stod(literal.value) == 0.0;
    } catch (...) {
      return false;
    }
  }

  SymbolInfo * findLocal(const std::string& name)
  {
    const auto local = locals_.find(name);
    return local == locals_.end() ? nullptr : &local->second;
  }

  SymbolInfo * findGlobal(const std::string& name)
  {
    const auto global = globals_.find(name);
    return global == globals_.end() ? nullptr : &global->second;
  }

  SymbolInfo * resolveVariable(const std::string& name)
  {
    SymbolInfo * local = findLocal(name);
    return local != nullptr ? local : findGlobal(name);
  }

  const Program& program_;
  const std::unordered_map<std::string, UiSymbol>& uiSymbols_;
  std::string filename_;
  std::vector<Diagnostic> diagnostics_;
  std::unordered_map<std::string, SymbolInfo> globals_;
  std::unordered_map<std::string, SymbolInfo> locals_;
  std::unordered_map<std::string, FunctionInfo> functions_;
  std::size_t eventCount_ = 0;
  AnalysisContext context_ = AnalysisContext::Init;
  ValueType currentReturnType_ = ValueType::Unknown;
  bool hasReturn_ = false;
  std::size_t loopDepth_ = 0;
};

}  // namespace

SemanticAnalyzer::SemanticAnalyzer(
  const Program& program,
  const std::unordered_map<std::string, UiSymbol>& uiSymbols,
  std::string filename)
  : program_(program), uiSymbols_(uiSymbols), filename_(std::move(filename)) {}

SemanticAnalyzerResult SemanticAnalyzer::analyze()
{
  AnalyzerImpl analyzer(program_, uiSymbols_, std::move(filename_));
  return analyzer.run();
}

}  // namespace fosc
