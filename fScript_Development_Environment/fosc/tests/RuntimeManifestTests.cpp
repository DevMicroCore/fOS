#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include "../compiler/CodeGenerator.h"
#include "../compiler/UiSymbols.h"
#include "../format/FAppFile.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include "src/fapp/FAppLoader.h"
#include "src/fapp/FAppManifest.h"
#include "src/fapp/FAppRuntime.h"

namespace {

int failures = 0;

void require(bool condition, const std::string& message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

class FakeUi : public FAppUiApi {
 public:
  bool getProperty(uint16_t objectId, uint8_t property, FAppValue * result) override
  {
    if (property != 1 || result == nullptr) return false;
    *result = values_[objectId];
    return true;
  }
  bool setProperty(uint16_t objectId, uint8_t property, const FAppValue& value) override
  {
    if (property != 1) return false;
    values_[objectId] = value;
    return true;
  }
  bool callMethod(uint16_t, uint8_t, const FAppValue *, uint8_t, FAppValue * result) override
  {
    if (result == nullptr) return false;
    *result = FAppValue::nil();
    return true;
  }
  std::string text(uint16_t objectId) const
  {
    const auto value = values_.find(objectId);
    return value == values_.end() ? "" : value->second.asString();
  }

 private:
  std::map<uint16_t, FAppValue> values_;
};

class FakeNative : public FAppNativeApi {
 public:
  bool call(FAppNativeFunction function, const FAppValue * arguments, uint8_t argumentCount, FAppValue * result) override
  {
    ++calls;
    if (result == nullptr) return false;
    if (function == FAppNativeFunction::HttpGet && argumentCount == 1) *result = FAppValue::boolean(true);
    else if (function == FAppNativeFunction::HttpStatus && argumentCount == 0) *result = FAppValue::integer(200);
    else if (function == FAppNativeFunction::HttpJson && argumentCount == 1) *result = FAppValue::string("18.4");
    else if (function == FAppNativeFunction::FileRead && argumentCount == 1) *result = FAppValue::string("data");
    else if (function == FAppNativeFunction::FileWrite && argumentCount == 2) *result = FAppValue::boolean(true);
    else if (function == FAppNativeFunction::AudioPlay && argumentCount == 1) *result = FAppValue::boolean(true);
    else if (function == FAppNativeFunction::WifiStatus && argumentCount == 0) *result = FAppValue::boolean(true);
    else if (function == FAppNativeFunction::DateWeekday && argumentCount == 1) *result = FAppValue::string("Tue");
    else if (function == FAppNativeFunction::SystemRestart && argumentCount == 0) *result = FAppValue::nil();
    else if (function == FAppNativeFunction::TimerStart && argumentCount == 1) *result = FAppValue::boolean(true);
    else if (function == FAppNativeFunction::SerialPrintf && argumentCount == 1) *result = FAppValue::nil();
    else if (function == FAppNativeFunction::Text && argumentCount == 1) {
      char buffer[FAppValue::kStringCapacity];
      arguments[0].toText(buffer, sizeof(buffer));
      *result = FAppValue::string(buffer);
    }
    else if (function == FAppNativeFunction::NumberParse && argumentCount == 1) *result = FAppValue::floating(12.5);
    else if (function == FAppNativeFunction::Round && argumentCount == 1) *result = FAppValue::integer(13);
    else if (function == FAppNativeFunction::StringLength && argumentCount == 1) *result = FAppValue::integer(4);
    else if (function == FAppNativeFunction::StringSlice && argumentCount == 3) *result = FAppValue::string("12");
    else if (function == FAppNativeFunction::MathEval && argumentCount == 1) *result = FAppValue::string("14");
    else if (function == FAppNativeFunction::StringLastIndex && argumentCount == 2) *result = FAppValue::integer(1);
    else if (function == FAppNativeFunction::EventX && argumentCount == 0) {
      *result = FAppValue::integer(eventContext.pointer ? eventContext.x : 0);
    }
    else if (function == FAppNativeFunction::EventY && argumentCount == 0) {
      *result = FAppValue::integer(eventContext.pointer ? eventContext.y : 0);
    }
    else if (function == FAppNativeFunction::EventScreenX && argumentCount == 0) {
      *result = FAppValue::integer(eventContext.pointer ? eventContext.screenX : 0);
    }
    else if (function == FAppNativeFunction::EventScreenY && argumentCount == 0) {
      *result = FAppValue::integer(eventContext.pointer ? eventContext.screenY : 0);
    }
    else if (function == FAppNativeFunction::EventPressed && argumentCount == 0) {
      *result = FAppValue::boolean(eventContext.pointer && eventContext.pressed);
    }
    else return false;
    return true;
  }
  const char * errorMessage() const override { return "fake native error"; }
  void reset() override { resets++; }
  void setEventContext(const FAppEventContext& context) override { eventContext = context; }
  void clearEventContext() override { eventContext = FAppEventContext{}; }
  int calls = 0;
  int resets = 0;
  FAppEventContext eventContext;
};

std::vector<std::uint8_t> compileImage()
{
  const std::string source =
    "var counter = 0\n"
    "on app.start\n"
    "lbl_status.text = \"Bereit\"\n"
    "end\n"
    "on btn_add.click\n"
    "counter = counter + 1\n"
    "lbl_status.text = \"Klicks: \" + counter\n"
    "end\n"
    "on app.close\n"
    "lbl_status.text = \"Geschlossen\"\n"
    "end\n";
  const std::string layout =
    "type=button;id=btn_add\n"
    "type=label;id=lbl_status\n";
  fosc::Lexer lexer(source, "main.fscript");
  fosc::LexerResult lexed = lexer.scan();
  fosc::Parser parser(std::move(lexed.tokens), "main.fscript");
  fosc::ParserResult parsed = parser.parse();
  fosc::UiSymbolsResult ui = fosc::UiSymbols::parse(layout, "layout.ui");
  fosc::CodeGenerator generator(parsed.program, ui.symbols, "main.fscript", source);
  fosc::CodeGeneratorResult generated = generator.generate();
  require(lexed.success() && parsed.success() && ui.success() && generated.success(),
          "end-to-end fixture must compile");
  fosc::FAppWriteResult written = fosc::FAppFile::write(generated.module);
  require(written.success(), "end-to-end fixture must serialize");
  return written.image;
}

std::vector<std::uint8_t> compileNativeImage()
{
  const std::string source =
    "var ok = http_get(\"https://example.com/weather.json\")\n"
    "var status = http_status()\n"
    "var temperature = http_json(\"current.temperature_2m\")\n";
  fosc::Lexer lexer(source, "native.fscript");
  fosc::LexerResult lexed = lexer.scan();
  fosc::Parser parser(std::move(lexed.tokens), "native.fscript");
  fosc::ParserResult parsed = parser.parse();
  fosc::CodeGenerator generator(parsed.program, {}, "native.fscript", source);
  fosc::CodeGeneratorResult generated = generator.generate();
  require(lexed.success() && parsed.success() && generated.success(), "native fixture must compile");
  return fosc::FAppFile::write(generated.module).image;
}

std::vector<std::uint8_t> compileNativeImage(const std::string& source)
{
  fosc::Lexer lexer(source, "native.fscript");
  fosc::LexerResult lexed = lexer.scan();
  fosc::Parser parser(std::move(lexed.tokens), "native.fscript");
  fosc::ParserResult parsed = parser.parse();
  fosc::CodeGenerator generator(parsed.program, {}, "native.fscript", source);
  fosc::CodeGeneratorResult generated = generator.generate();
  require(lexed.success() && parsed.success() && generated.success(), "native permission fixture must compile");
  return fosc::FAppFile::write(generated.module).image;
}

std::vector<std::uint8_t> compilePointerImage()
{
  const std::string source =
    "on drawing.pointer_move\n"
    "lbl_status.text = text(event_x())\n"
    "end\n"
    "on drawing.pointer_up\n"
    "lbl_status.text = \"up\"\n"
    "end\n";
  const std::string layout =
    "type=canvas;id=drawing\n"
    "type=label;id=lbl_status\n";
  fosc::Lexer lexer(source, "pointer.fscript");
  fosc::LexerResult lexed = lexer.scan();
  fosc::Parser parser(std::move(lexed.tokens), "pointer.fscript");
  fosc::ParserResult parsed = parser.parse();
  fosc::UiSymbolsResult ui = fosc::UiSymbols::parse(layout, "layout.ui");
  fosc::CodeGenerator generator(parsed.program, ui.symbols, "pointer.fscript", source);
  fosc::CodeGeneratorResult generated = generator.generate();
  require(lexed.success() && parsed.success() && ui.success() && generated.success(),
          "pointer fixture must compile");
  return fosc::FAppFile::write(generated.module).image;
}

void runUntilIdle(FAppRuntime * runtime)
{
  for (int iteration = 0; iteration < 64 && runtime->state() == FAppRuntimeState::Running; ++iteration) {
    runtime->update(4, 0);
  }
  runtime->update(64, 0);
}

void testRuntimeLifecycleAndClick()
{
  fs::FS filesystem;
  filesystem.addFile("/apps/counter/layout.ui", {'x'});
  filesystem.addFile("/apps/counter/main.fapp", compileImage());
  FAppLoader loader;
  loader.begin(filesystem);
  require(loader.prepare("/apps/counter", "layout.ui", "main.fapp") == FAppLoaderStatus::ExecutableReady,
          "embedded loader should accept runtime fixture");
  FakeUi ui;
  FAppRuntime runtime;
  require(runtime.begin(filesystem, loader, ui, static_cast<FAppPermissions>(FAppPermission::Ui)),
          "runtime should load fixed metadata");
  require(runtime.start(), "runtime should start init function");
  runUntilIdle(&runtime);
  require(runtime.state() == FAppRuntimeState::Ready, "runtime should yield in ready state");
  require(ui.text(2) == "Bereit", "app.start should update the label");
  filesystem.resetOpenCount();
  require(runtime.queueEvent(1, 1), "click handler should enter bounded event queue");
  runtime.update(2, 0);
  require(runtime.isActive(), "small instruction budget must yield without terminating the app");
  runUntilIdle(&runtime);
  require(ui.text(2) == "Klicks: 1", "click bytecode should update global state and UI");
  require(filesystem.openCount() == 0, "cached runtime should not reopen the fAPP during an event");
  runtime.shutdown(128);
  require(runtime.state() == FAppRuntimeState::Stopped, "shutdown should stop runtime");
  require(ui.text(2) == "Geschlossen", "app.close should run before shutdown");
}

void testPermissionFaultIsContained()
{
  fs::FS filesystem;
  filesystem.addFile("/apps/counter/layout.ui", {'x'});
  filesystem.addFile("/apps/counter/main.fapp", compileImage());
  FAppLoader loader;
  loader.begin(filesystem);
  loader.prepare("/apps/counter", "layout.ui", "main.fapp");
  FakeUi ui;
  FAppRuntime runtime;
  require(runtime.begin(filesystem, loader, ui, 0), "runtime metadata should load without UI permission");
  require(runtime.start(), "permission test should start");
  for (int iteration = 0; iteration < 16 && runtime.isActive(); ++iteration) runtime.update(16, 0);
  require(runtime.state() == FAppRuntimeState::Faulted &&
          runtime.error() == FAppRuntimeError::PermissionDenied,
          "missing UI permission must fault only the app runtime");
}

void testNativeNetworkPermissionAndDispatch()
{
  fs::FS filesystem;
  filesystem.addFile("/apps/native/layout.ui", {'x'});
  filesystem.addFile("/apps/native/main.fapp", compileNativeImage());
  FAppLoader loader;
  loader.begin(filesystem);
  loader.prepare("/apps/native", "layout.ui", "main.fapp");
  FakeUi ui;
  FakeNative native;
  FAppRuntime runtime;
  const FAppPermissions permissions = static_cast<FAppPermissions>(FAppPermission::Ui) |
    static_cast<FAppPermissions>(FAppPermission::Network);
  require(runtime.begin(filesystem, loader, ui, permissions, &native), "native runtime should load");
  require(runtime.start(), "native runtime should start");
  runUntilIdle(&runtime);
  require(runtime.state() == FAppRuntimeState::Ready && native.calls == 3,
          "native HTTP calls should dispatch through the guarded API");

  FAppRuntime denied;
  require(denied.begin(filesystem, loader, ui, static_cast<FAppPermissions>(FAppPermission::Ui), &native),
          "denied runtime should still load metadata");
  denied.start();
  runUntilIdle(&denied);
  require(denied.state() == FAppRuntimeState::Faulted &&
          denied.error() == FAppRuntimeError::PermissionDenied,
          "native HTTP bytecode must require network permission");
}

void testNativePermissionMapping()
{
  struct Case {
    const char * source;
    FAppPermission permission;
  };
  const Case cases[] = {
    {"var value = file_read(\"state.txt\")\n", FAppPermission::StorageRead},
    {"var value = file_write(\"state.txt\", \"x\")\n", FAppPermission::StorageWrite},
    {"var value = audio_play(\"tone.mp3\")\n", FAppPermission::Audio},
    {"var value = wifi_status()\n", FAppPermission::Network},
    {"system.restart()\n", FAppPermission::SystemRestart}
  };
  for (const Case& test : cases) {
    fs::FS filesystem;
    filesystem.addFile("/apps/native/layout.ui", {'x'});
    filesystem.addFile("/apps/native/main.fapp", compileNativeImage(test.source));
    FAppLoader loader;
    loader.begin(filesystem);
    loader.prepare("/apps/native", "layout.ui", "main.fapp");
    FakeUi ui;
    FakeNative native;
    FAppRuntime allowed;
    require(allowed.begin(filesystem, loader, ui, static_cast<FAppPermissions>(test.permission), &native),
            "permission-specific runtime should load");
    require(allowed.start(), "permission-specific runtime should start");
    runUntilIdle(&allowed);
    require(allowed.state() == FAppRuntimeState::Ready && native.calls == 1,
            "matching native permission should allow exactly one call");

    FAppRuntime denied;
    require(denied.begin(filesystem, loader, ui, 0, &native), "denied permission runtime should load");
    denied.start();
    runUntilIdle(&denied);
    require(denied.state() == FAppRuntimeState::Faulted &&
            denied.error() == FAppRuntimeError::PermissionDenied,
            "missing function-specific permission should be contained");
  }

  fs::FS filesystem;
  filesystem.addFile("/apps/safe/layout.ui", {'x'});
  filesystem.addFile("/apps/safe/main.fapp", compileNativeImage(
    "var day = date_weekday(\"2026-08-04\")\n"
    "var timer_ok = timer.start(500)\n"
    "Serial.printf(day)\n"));
  FAppLoader loader;
  loader.begin(filesystem);
  loader.prepare("/apps/safe", "layout.ui", "main.fapp");
  FakeUi ui;
  FakeNative native;
  FAppRuntime safe;
  require(safe.begin(filesystem, loader, ui, 0, &native), "safe built-ins should load without permissions");
  safe.start();
  runUntilIdle(&safe);
  require(safe.state() == FAppRuntimeState::Ready && native.calls == 3,
          "date, timer and serial should not require unrelated network permission");
}

void testPointerMoveCoalescingAndContext()
{
  fs::FS filesystem;
  filesystem.addFile("/apps/pointer/layout.ui", {'x'});
  filesystem.addFile("/apps/pointer/main.fapp", compilePointerImage());
  FAppLoader loader;
  loader.begin(filesystem);
  require(loader.prepare("/apps/pointer", "layout.ui", "main.fapp") ==
          FAppLoaderStatus::ExecutableReady, "pointer fixture should load");
  FakeUi ui;
  FakeNative native;
  FAppRuntime runtime;
  require(runtime.begin(filesystem, loader, ui,
          static_cast<FAppPermissions>(FAppPermission::Ui), &native),
          "pointer runtime should load");
  require(runtime.start(), "pointer runtime should start");
  runUntilIdle(&runtime);
  for (int x = 0; x < 20; ++x) {
    require(runtime.queuePointerEvent(1, 9, x, 7, x + 10, 17, true),
            "rapid pointer moves should be coalesced instead of rejected");
  }
  runUntilIdle(&runtime);
  require(ui.text(2) == "19", "the newest coalesced pointer position should reach fScript");
  require(runtime.queuePointerEvent(1, 10, 20, 7, 30, 17, false),
          "pointer_up should enter the queue");
  runUntilIdle(&runtime);
  require(ui.text(2) == "up", "pointer_up should execute after movement");
}

void testManifest()
{
  const std::string json =
    "{\"id\":\"devmicro.counter\",\"name\":\"Counter\",\"version\":\"1.0.0\","
    "\"min_fos\":\"4.0.0\",\"layout\":\"layout.ui\",\"executable\":\"main.fapp\","
    "\"permissions\":[\"ui\",\"storage.read\",\"system.restart\"],\"scrollable\":false}";
  fs::FS filesystem;
  filesystem.addFile("/apps/counter/app.json", std::vector<std::uint8_t>(json.begin(), json.end()));
  FAppManifest manifest;
  require(manifest.load(filesystem, "/apps/counter/app.json") == FAppManifestStatus::Ready,
          "valid app.json should load");
  require(std::string(manifest.id()) == "devmicro.counter", "manifest id differs");
  require(fappHasPermission(manifest.permissions(), FAppPermission::Ui), "UI permission missing");
  require(fappHasPermission(manifest.permissions(), FAppPermission::StorageRead), "read permission missing");
  require(fappHasPermission(manifest.permissions(), FAppPermission::SystemRestart), "restart permission missing");

  const std::string invalid = "{\"id\":\"x\",\"name\":\"X\",\"version\":\"1.0.0\",\"layout\":\"../bad.ui\"}";
  filesystem.addFile("/apps/bad/app.json", std::vector<std::uint8_t>(invalid.begin(), invalid.end()));
  require(manifest.load(filesystem, "/apps/bad/app.json") == FAppManifestStatus::InvalidValue,
          "unsafe manifest path must be rejected");

  const std::string future =
    "{\"id\":\"future\",\"name\":\"Future\",\"version\":\"1.0.0\",\"min_fos\":\"5.0.0\"}";
  filesystem.addFile("/apps/future/app.json", std::vector<std::uint8_t>(future.begin(), future.end()));
  require(manifest.load(filesystem, "/apps/future/app.json") == FAppManifestStatus::IncompatibleFos,
          "manifest requiring newer fOS must be rejected");
}

}  // namespace

int main()
{
  require(sizeof(FAppRuntime) < 8192,
          "runtime object must not reserve the value stack permanently in internal RAM");
  testRuntimeLifecycleAndClick();
  testPermissionFaultIsContained();
  testNativeNetworkPermissionAndDispatch();
  testNativePermissionMapping();
  testPointerMoveCoalescingAndContext();
  testManifest();
  if (failures != 0) {
    std::cerr << failures << " runtime/manifest test(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All runtime, lifecycle and manifest tests passed.\n";
  return EXIT_SUCCESS;
}
