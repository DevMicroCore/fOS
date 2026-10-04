#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../bytecode/Bytecode.h"
#include "../format/FAppFile.h"
#include "src/fapp/FAppLoader.h"

namespace {
int failures = 0;

void require(bool condition, const std::string& message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

std::vector<std::uint8_t> makeImage()
{
  fosc::fapp::BytecodeModule module;
  fosc::fapp::BytecodeFunction init;
  init.code = {
    static_cast<std::uint8_t>(fosc::fapp::Opcode::PushNil),
    static_cast<std::uint8_t>(fosc::fapp::Opcode::Return)
  };
  init.maxStack = 1;
  module.functions.push_back(init);
  const fosc::FAppWriteResult result = fosc::FAppFile::write(module);
  require(result.success(), "test fAPP image should serialize");
  return result.image;
}

fs::FS filesystemWith(std::vector<std::uint8_t> image)
{
  fs::FS filesystem;
  filesystem.addFile("/apps/test/layout.ui", {'o', 'k'});
  filesystem.addFile("/apps/test/main.fapp", std::move(image));
  return filesystem;
}

void testValidExecutable()
{
  fs::FS filesystem = filesystemWith(makeImage());
  FAppLoader loader;
  loader.begin(filesystem);
  require(loader.prepare("/apps/test", "layout.ui", "main.fapp") ==
            FAppLoaderStatus::ExecutableReady,
          "valid fAPP should be ready");
  require(loader.hasExecutable(), "valid fAPP should be reported as executable");
  require(loader.formatMajor() == 1 && loader.bytecodeMajor() == 1,
          "loader format versions differ");
  require(loader.functionCount() == 1 && loader.initFunction() == 0,
          "loader function metadata differs");
  require(loader.codeSize() == 2, "loader code size differs");
}

void testChecksumFailure()
{
  std::vector<std::uint8_t> image = makeImage();
  image.back() ^= 1u;
  fs::FS filesystem = filesystemWith(std::move(image));
  FAppLoader loader;
  loader.begin(filesystem);
  require(loader.prepare("/apps/test", "layout.ui", "main.fapp") ==
            FAppLoaderStatus::ExecutableChecksumFailed,
          "modified fAPP should fail checksum validation");
}

void testIncompatibleVersion()
{
  std::vector<std::uint8_t> image = makeImage();
  image[6] = 2;
  fs::FS filesystem = filesystemWith(std::move(image));
  FAppLoader loader;
  loader.begin(filesystem);
  require(loader.prepare("/apps/test", "layout.ui", "main.fapp") ==
            FAppLoaderStatus::IncompatibleExecutable,
          "newer bytecode major version should be rejected");
}

void testLegacyUiOnly()
{
  fs::FS filesystem;
  filesystem.addFile("/apps/test/layout.ui", {'o', 'k'});
  FAppLoader loader;
  loader.begin(filesystem);
  require(loader.prepare("/apps/test", "layout.ui", "main.fapp") ==
            FAppLoaderStatus::LegacyUiOnly,
          "layout without main.fapp should remain a legacy app");
}

void testUnsafeDirectory()
{
  fs::FS filesystem = filesystemWith(makeImage());
  FAppLoader loader;
  loader.begin(filesystem);
  require(loader.prepare("/apps/../system", "layout.ui", "main.fapp") ==
            FAppLoaderStatus::InvalidPath,
          "directory traversal must be rejected before filesystem access");
}

}  // namespace

int main()
{
  testValidExecutable();
  testChecksumFailure();
  testIncompatibleVersion();
  testLegacyUiOnly();
  testUnsafeDirectory();
  if (failures != 0) {
    std::cerr << failures << " embedded loader test(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All embedded fAPP loader tests passed.\n";
  return EXIT_SUCCESS;
}
