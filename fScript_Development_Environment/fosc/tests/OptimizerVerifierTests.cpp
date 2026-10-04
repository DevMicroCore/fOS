#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../bytecode/Bytecode.h"
#include "../compiler/CodeGenerator.h"
#include "../format/FAppFile.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include "../verifier/BytecodeVerifier.h"

namespace {
int failures = 0;

void require(bool condition, const std::string& message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

fosc::CodeGeneratorResult generate(const std::string& source, bool optimize)
{
  fosc::Lexer lexer(source, "test.fscript");
  fosc::LexerResult lexerResult = lexer.scan();
  require(lexerResult.success(), "optimizer test source must pass lexer");
  fosc::Parser parser(std::move(lexerResult.tokens), "test.fscript");
  fosc::ParserResult parserResult = parser.parse();
  require(parserResult.success(), "optimizer test source must pass parser");
  const std::unordered_map<std::string, fosc::UiSymbol> ui;
  fosc::CodeGenerator generator(parserResult.program, ui, "test.fscript", source, optimize);
  return generator.generate();
}

fosc::BytecodeVerifierResult verify(const fosc::fapp::BytecodeModule& module)
{
  fosc::FAppWriteResult written = fosc::FAppFile::write(module);
  require(written.success(), "verifier test module should serialize");
  fosc::FAppReadResult read = fosc::FAppFile::read(std::move(written.image));
  require(read.success(), "verifier test image should pass structural validation");
  return fosc::BytecodeVerifier::verify(read.file);
}

bool hasCode(const fosc::BytecodeVerifierResult& result, const std::string& code)
{
  for (const fosc::VerificationIssue& issue : result.issues) {
    if (issue.code == code) return true;
  }
  return false;
}

fosc::fapp::BytecodeModule moduleWith(std::vector<std::uint8_t> code, std::uint16_t maxStack)
{
  fosc::fapp::BytecodeModule module;
  fosc::fapp::BytecodeFunction init;
  init.code = std::move(code);
  init.maxStack = maxStack;
  module.functions.push_back(std::move(init));
  return module;
}

void testConstantFolding()
{
  fosc::CodeGeneratorResult optimized = generate("var result = 1 + 2 * 3\n", true);
  fosc::CodeGeneratorResult unoptimized = generate("var result = 1 + 2 * 3\n", false);
  require(optimized.success() && unoptimized.success(), "both generator modes should succeed");
  require(optimized.module.functions[0].code.size() < unoptimized.module.functions[0].code.size(),
          "constant folding should reduce bytecode size");
  require(optimized.module.functions[0].code[0] ==
            static_cast<std::uint8_t>(fosc::fapp::Opcode::PushInt),
          "folded expression should start with PUSH_INT");
  std::int32_t value = 0;
  require(fosc::fapp::readI32(optimized.module.functions[0].code, 1, &value) && value == 7,
          "1 + 2 * 3 should fold to 7");

  fosc::CodeGeneratorResult strings = generate("var text = \"a\" + \"b\"\n", true);
  require(strings.success() && strings.module.stringConstants.size() == 1 &&
            strings.module.stringConstants[0] == "ab",
          "constant string concatenation should be folded and deduplicated");

  fosc::CodeGeneratorResult divideByZero = generate("var value = 1 / 0\n", true);
  require(divideByZero.success(), "unsafe constant expression should remain runtime bytecode");
  bool containsDivide = false;
  for (std::uint8_t byte : divideByZero.module.functions[0].code) {
    if (byte == static_cast<std::uint8_t>(fosc::fapp::Opcode::Divide)) containsDivide = true;
  }
  require(containsDivide, "division by zero must not be folded by the compiler");
}

void testValidBytecode()
{
  const fosc::fapp::BytecodeModule module = moduleWith({
    static_cast<std::uint8_t>(fosc::fapp::Opcode::PushNil),
    static_cast<std::uint8_t>(fosc::fapp::Opcode::Return)
  }, 1);
  require(verify(module).success(), "minimal balanced function should verify");
}

void testVerifierRejectsInvalidIndices()
{
  const fosc::fapp::BytecodeModule module = moduleWith({
    static_cast<std::uint8_t>(fosc::fapp::Opcode::LoadGlobal), 0, 0,
    static_cast<std::uint8_t>(fosc::fapp::Opcode::Return)
  }, 1);
  require(hasCode(verify(module), "FS603"), "out-of-range global index should fail verification");
}

void testVerifierRejectsStackAndJumps()
{
  const fosc::fapp::BytecodeModule underflow = moduleWith({
    static_cast<std::uint8_t>(fosc::fapp::Opcode::Pop),
    static_cast<std::uint8_t>(fosc::fapp::Opcode::PushNil),
    static_cast<std::uint8_t>(fosc::fapp::Opcode::Return)
  }, 1);
  require(hasCode(verify(underflow), "FS606"), "operand-stack underflow should fail verification");

  std::vector<std::uint8_t> jumpCode = {
    static_cast<std::uint8_t>(fosc::fapp::Opcode::Jump)
  };
  fosc::fapp::appendI32(&jumpCode, -3);
  jumpCode.push_back(static_cast<std::uint8_t>(fosc::fapp::Opcode::PushNil));
  jumpCode.push_back(static_cast<std::uint8_t>(fosc::fapp::Opcode::Return));
  require(hasCode(verify(moduleWith(std::move(jumpCode), 1)), "FS605"),
          "jump into an operand should fail verification");

  const fosc::fapp::BytecodeModule smallStack = moduleWith({
    static_cast<std::uint8_t>(fosc::fapp::Opcode::PushNil),
    static_cast<std::uint8_t>(fosc::fapp::Opcode::Return)
  }, 0);
  require(hasCode(verify(smallStack), "FS608"), "too-small max stack should fail verification");
}

}  // namespace

int main()
{
  testConstantFolding();
  testValidBytecode();
  testVerifierRejectsInvalidIndices();
  testVerifierRejectsStackAndJumps();
  if (failures != 0) {
    std::cerr << failures << " optimizer/verifier test(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All optimizer and bytecode verifier tests passed.\n";
  return EXIT_SUCCESS;
}
