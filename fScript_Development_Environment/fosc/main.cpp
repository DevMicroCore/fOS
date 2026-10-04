#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Version.h"
#include "ast/AstPrinter.h"
#include "compiler/CodeGenerator.h"
#include "compiler/UiSymbols.h"
#include "diagnostics/Diagnostic.h"
#include "format/Disassembler.h"
#include "format/FAppFile.h"
#include "lexer/Lexer.h"
#include "lexer/Token.h"
#include "parser/Parser.h"
#include "semantic/SemanticAnalyzer.h"
#include "verifier/BytecodeVerifier.h"

namespace {

void printUsage()
{
  std::cout << "fosc " << fosc::version::kCompiler << " (fScript "
            << fosc::version::kFScript << ")\n\n"
            << "Usage:\n  fosc lex <file.fscript>\n  fosc parse <file.fscript>\n"
            << "  fosc check <file.fscript> [--ui layout.ui]\n"
            << "  fosc build <file.fscript> [-o main.fapp] [--ui layout.ui] [--no-optimize]\n"
            << "  fosc verify <file.fapp>\n"
            << "  fosc disasm <file.fapp>\n"
            << "  fosc --version\n  fosc --help\n\n"
            << "Without --ui, fosc automatically uses main.ui or layout.ui beside the source.\n"
            << "Native API: HTTP/JSON, file_read/write, audio_play, wifi_status,\n"
            << "            system.restart, timer.start, Serial.printf, text,\n"
            << "            number_parse, round, string helpers and math_eval.\n";
}

bool readFile(const std::string& path, std::string * output)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  std::ostringstream buffer;
  buffer << file.rdbuf();
  *output = buffer.str();
  return true;
}

bool readBinaryFile(const std::string& path, std::vector<std::uint8_t> * output)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (size < 0) return false;
  file.seekg(0, std::ios::beg);
  output->resize(static_cast<std::size_t>(size));
  if (size > 0) file.read(reinterpret_cast<char *>(output->data()), size);
  return static_cast<bool>(file) || size == 0;
}

bool writeBinaryFile(const std::string& path, const std::vector<std::uint8_t>& data)
{
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) return false;
  if (!data.empty()) {
    file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
  }
  return static_cast<bool>(file);
}

bool fileExists(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  return static_cast<bool>(file);
}

std::string parentDirectory(const std::string& path)
{
  const std::size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

std::string defaultOutputPath(const std::string& path)
{
  const std::size_t slash = path.find_last_of("/\\");
  const std::size_t dot = path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return path + ".fapp";
  return path.substr(0, dot) + ".fapp";
}

std::string escapeForDisplay(const std::string& input)
{
  std::string output;
  for (char value : input) {
    switch (value) {
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      case '\\': output += "\\\\"; break;
      case '"': output += "\\\""; break;
      default: output += value; break;
    }
  }
  return output;
}

int runLexer(const std::string& path)
{
  std::string source;
  if (!readFile(path, &source)) {
    std::cerr << "Error FS000: Cannot open source file.\nFile: " << path << '\n';
    return 1;
  }

  fosc::Lexer lexer(std::move(source), path);
  fosc::LexerResult result = lexer.scan();
  for (const fosc::Token& token : result.tokens) {
    std::cout << std::setw(4) << token.location.line << ':'
              << std::left << std::setw(4) << token.location.column << std::right
              << std::setw(16) << fosc::tokenTypeName(token.type);
    if (!token.lexeme.empty()) std::cout << "  \"" << escapeForDisplay(token.lexeme) << '"';
    if (!token.literal.empty()) std::cout << "  value=\"" << escapeForDisplay(token.literal) << '"';
    std::cout << '\n';
  }

  for (const fosc::Diagnostic& diagnostic : result.diagnostics) {
    std::cerr << fosc::formatDiagnostic(diagnostic) << "\n\n";
  }
  return result.success() ? 0 : 1;
}

int runParser(const std::string& path)
{
  std::string source;
  if (!readFile(path, &source)) {
    std::cerr << "Error FS000: Cannot open source file.\nFile: " << path << '\n';
    return 1;
  }

  fosc::Lexer lexer(std::move(source), path);
  fosc::LexerResult lexerResult = lexer.scan();
  if (!lexerResult.success()) {
    for (const fosc::Diagnostic& diagnostic : lexerResult.diagnostics) {
      std::cerr << fosc::formatDiagnostic(diagnostic) << "\n\n";
    }
    return 1;
  }

  fosc::Parser parser(std::move(lexerResult.tokens), path);
  fosc::ParserResult parserResult = parser.parse();
  for (const fosc::Diagnostic& diagnostic : parserResult.diagnostics) {
    std::cerr << fosc::formatDiagnostic(diagnostic) << "\n\n";
  }

  if (!parserResult.success()) return 1;
  fosc::AstPrinter printer;
  std::cout << printer.print(parserResult.program);
  return 0;
}

void printDiagnostics(const std::vector<fosc::Diagnostic>& diagnostics)
{
  for (const fosc::Diagnostic& diagnostic : diagnostics) {
    std::cerr << fosc::formatDiagnostic(diagnostic) << "\n\n";
  }
}

int runCheck(int argc, char ** argv)
{
  if (argc < 3) {
    std::cerr << "Error FS420: Missing fScript source file.\n";
    return 2;
  }
  const std::string sourcePath = argv[2];
  std::string uiPath;
  bool uiExplicit = false;
  for (int index = 3; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--ui" && index + 1 < argc) {
      uiPath = argv[++index];
      uiExplicit = true;
    } else {
      std::cerr << "Error FS420: Unknown or incomplete check option '" << argument << "'.\n";
      return 2;
    }
  }

  std::string source;
  if (!readFile(sourcePath, &source)) {
    std::cerr << "Error FS000: Cannot open source file.\nFile: " << sourcePath << '\n';
    return 1;
  }
  fosc::Lexer lexer(source, sourcePath);
  fosc::LexerResult lexerResult = lexer.scan();
  if (!lexerResult.success()) {
    printDiagnostics(lexerResult.diagnostics);
    return 1;
  }
  fosc::Parser parser(std::move(lexerResult.tokens), sourcePath);
  fosc::ParserResult parserResult = parser.parse();
  if (!parserResult.success()) {
    printDiagnostics(parserResult.diagnostics);
    return 1;
  }

  if (!uiExplicit) {
    const std::string directory = parentDirectory(sourcePath);
    if (fileExists(directory + "main.ui")) uiPath = directory + "main.ui";
    else if (fileExists(directory + "layout.ui")) uiPath = directory + "layout.ui";
  }
  std::unordered_map<std::string, fosc::UiSymbol> uiSymbols;
  if (!uiPath.empty()) {
    std::string uiSource;
    if (!readFile(uiPath, &uiSource)) {
      std::cerr << "Error FS000: Cannot open UI layout file.\nFile: " << uiPath << '\n';
      return 1;
    }
    fosc::UiSymbolsResult uiResult = fosc::UiSymbols::parse(uiSource, uiPath);
    if (!uiResult.success()) {
      printDiagnostics(uiResult.diagnostics);
      return 1;
    }
    uiSymbols = std::move(uiResult.symbols);
  }

  fosc::SemanticAnalyzer analyzer(parserResult.program, uiSymbols, sourcePath);
  fosc::SemanticAnalyzerResult semanticResult = analyzer.analyze();
  if (!semanticResult.success()) {
    printDiagnostics(semanticResult.diagnostics);
    return 1;
  }
  std::cout << "Semantic check passed for " << sourcePath << "\n"
            << "Globals: " << semanticResult.globalCount
            << ", functions: " << semanticResult.functionCount
            << ", events: " << semanticResult.eventCount
            << ", UI objects: " << uiSymbols.size() << '\n';
  return 0;
}

int runBuild(int argc, char ** argv)
{
  if (argc < 3) {
    std::cerr << "Error FS420: Missing fScript source file.\n";
    return 2;
  }

  const std::string sourcePath = argv[2];
  std::string outputPath = defaultOutputPath(sourcePath);
  std::string uiPath;
  bool uiExplicit = false;
  bool optimizeConstants = true;
  for (int index = 3; index < argc; ++index) {
    const std::string argument = argv[index];
    if ((argument == "-o" || argument == "--output") && index + 1 < argc) {
      outputPath = argv[++index];
    } else if (argument == "--ui" && index + 1 < argc) {
      uiPath = argv[++index];
      uiExplicit = true;
    } else if (argument == "--no-optimize") {
      optimizeConstants = false;
    } else {
      std::cerr << "Error FS420: Unknown or incomplete build option '" << argument << "'.\n";
      return 2;
    }
  }

  std::string source;
  if (!readFile(sourcePath, &source)) {
    std::cerr << "Error FS000: Cannot open source file.\nFile: " << sourcePath << '\n';
    return 1;
  }

  fosc::Lexer lexer(source, sourcePath);
  fosc::LexerResult lexerResult = lexer.scan();
  if (!lexerResult.success()) {
    printDiagnostics(lexerResult.diagnostics);
    return 1;
  }
  fosc::Parser parser(std::move(lexerResult.tokens), sourcePath);
  fosc::ParserResult parserResult = parser.parse();
  if (!parserResult.success()) {
    printDiagnostics(parserResult.diagnostics);
    return 1;
  }

  if (!uiExplicit) {
    const std::string directory = parentDirectory(sourcePath);
    if (fileExists(directory + "main.ui")) uiPath = directory + "main.ui";
    else if (fileExists(directory + "layout.ui")) uiPath = directory + "layout.ui";
  }

  std::unordered_map<std::string, fosc::UiSymbol> uiSymbols;
  if (!uiPath.empty()) {
    std::string uiSource;
    if (!readFile(uiPath, &uiSource)) {
      std::cerr << "Error FS000: Cannot open UI layout file.\nFile: " << uiPath << '\n';
      return 1;
    }
    fosc::UiSymbolsResult uiResult = fosc::UiSymbols::parse(uiSource, uiPath);
    if (!uiResult.success()) {
      printDiagnostics(uiResult.diagnostics);
      return 1;
    }
    uiSymbols = std::move(uiResult.symbols);
  }

  fosc::CodeGenerator generator(
    parserResult.program,
    uiSymbols,
    sourcePath,
    source,
    optimizeConstants);
  fosc::SemanticAnalyzer analyzer(parserResult.program, uiSymbols, sourcePath);
  fosc::SemanticAnalyzerResult semanticResult = analyzer.analyze();
  if (!semanticResult.success()) {
    printDiagnostics(semanticResult.diagnostics);
    return 1;
  }

  fosc::CodeGeneratorResult generatorResult = generator.generate();
  if (!generatorResult.success()) {
    printDiagnostics(generatorResult.diagnostics);
    return 1;
  }

  fosc::FAppWriteResult writeResult = fosc::FAppFile::write(generatorResult.module);
  if (!writeResult.success()) {
    std::cerr << "Error " << writeResult.error << '\n';
    return 1;
  }
  const fosc::FAppReadResult verification = fosc::FAppFile::read(writeResult.image);
  if (!verification.success()) {
    std::cerr << "Error FS422: Internal output verification failed: "
              << verification.error << '\n';
    return 1;
  }
  const fosc::BytecodeVerifierResult bytecodeVerification =
    fosc::BytecodeVerifier::verify(verification.file);
  if (!bytecodeVerification.success()) {
    std::cerr << "Error FS422: Internal bytecode verification failed.\n";
    for (const fosc::VerificationIssue& issue : bytecodeVerification.issues) {
      std::cerr << fosc::BytecodeVerifier::formatIssue(issue) << "\n\n";
    }
    return 1;
  }
  if (!writeBinaryFile(outputPath, writeResult.image)) {
    std::cerr << "Error FS421: Cannot write output file.\nFile: " << outputPath << '\n';
    return 1;
  }

  std::cout << "Built " << outputPath << " (" << writeResult.image.size() << " bytes)\n"
            << "Functions: " << generatorResult.module.functions.size()
            << ", events: " << generatorResult.module.events.size()
            << ", globals: " << generatorResult.module.globalCount
            << ", UI objects: " << uiSymbols.size()
            << ", optimization: " << (optimizeConstants ? "on" : "off") << '\n';
  return 0;
}

int runVerifier(const std::string& path)
{
  std::vector<std::uint8_t> image;
  if (!readBinaryFile(path, &image)) {
    std::cerr << "Error FS000: Cannot open fAPP file.\nFile: " << path << '\n';
    return 1;
  }
  fosc::FAppReadResult file = fosc::FAppFile::read(std::move(image));
  if (!file.success()) {
    std::cerr << "Error " << file.error << "\nFile: " << path << '\n';
    return 1;
  }
  const fosc::BytecodeVerifierResult result = fosc::BytecodeVerifier::verify(file.file);
  if (!result.success()) {
    for (const fosc::VerificationIssue& issue : result.issues) {
      std::cerr << fosc::BytecodeVerifier::formatIssue(issue) << "\n\n";
    }
    return 1;
  }
  std::cout << "Verified " << path << "\nFunctions: " << file.file.header.functionCount
            << ", code: " << file.file.header.codeSize << " bytes, max UI objects: "
            << fosc::fapp::kMaximumUiObjects << '\n';
  return 0;
}

int runDisassembler(const std::string& path)
{
  std::vector<std::uint8_t> image;
  if (!readBinaryFile(path, &image)) {
    std::cerr << "Error FS000: Cannot open fAPP file.\nFile: " << path << '\n';
    return 1;
  }
  fosc::FAppReadResult result = fosc::FAppFile::read(std::move(image));
  if (!result.success()) {
    std::cerr << "Error " << result.error << "\nFile: " << path << '\n';
    return 1;
  }
  const fosc::BytecodeVerifierResult verification = fosc::BytecodeVerifier::verify(result.file);
  if (!verification.success()) {
    for (const fosc::VerificationIssue& issue : verification.issues) {
      std::cerr << fosc::BytecodeVerifier::formatIssue(issue) << "\n\n";
    }
    return 1;
  }
  std::cout << fosc::Disassembler::disassemble(result.file);
  return 0;
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "fosc " << fosc::version::kCompiler
              << "\nfScript " << fosc::version::kFScript << '\n';
    return 0;
  }
  if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
    printUsage();
    return argc == 1 ? 1 : 0;
  }
  if (argc == 3 && std::string(argv[1]) == "lex") return runLexer(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "parse") return runParser(argv[2]);
  if (argc >= 2 && std::string(argv[1]) == "check") return runCheck(argc, argv);
  if (argc >= 2 && std::string(argv[1]) == "build") return runBuild(argc, argv);
  if (argc == 3 && std::string(argv[1]) == "verify") return runVerifier(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "disasm") return runDisassembler(argv[2]);
  printUsage();
  return 1;
}
