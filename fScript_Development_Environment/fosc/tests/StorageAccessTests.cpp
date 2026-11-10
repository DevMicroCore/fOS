#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "src/fapp/FAppHttpApi.h"
#include "src/fapp/FAppManifest.h"
#include "src/fapp/LvglFAppUiApi.h"

extern "C" bool OTARecovery_IsNetworkBusy(void) { return false; }
static std::string played;
static bool play(const char * path) { played = path; return true; }
static int failures = 0;
static void require(bool condition, const char * message) {
  if (!condition) { ++failures; std::cerr << "FAILED: " << message << '\n'; }
}
static void add(fs::FS& fs, const char * path, const std::string& data) {
  fs.addFile(path, std::vector<uint8_t>(data.begin(), data.end()));
}
static FAppManifest manifest(fs::FS& fs, const std::string& permissions) {
  const std::string json = "{\"id\":\"test.sd\",\"name\":\"SD\",\"version\":\"1.0.0\",\"permissions\":" + permissions + "}";
  add(fs, "/apps/test/app.json", json);
  FAppManifest result;
  require(result.load(fs, "/apps/test/app.json") == FAppManifestStatus::Ready, "folder manifest must load");
  return result;
}
static bool native(FAppHttpApi& api, FAppNativeFunction fn, const char * path, FAppValue * result, const char * data = "new") {
  FAppValue args[] = {FAppValue::string(path), FAppValue::string(data)};
  return api.call(fn, args, fn == FAppNativeFunction::FileWrite ? 2 : 1, result);
}
int main() {
  fs::FS fs;
  add(fs, "/apps/test/local.txt", "local");
  add(fs, "/public/info.txt", "public");
  add(fs, "/system/secret.txt", "protected");
  add(fs, "/system_backup/info.txt", "backup");
  add(fs, "/root.txt", "root");
  FAppValue result;
  FAppHttpApi api;
  auto root = manifest(fs, "[\"ui\",\"storage.read:/\",\"storage.write:/\",\"audio\"]");
  require(fappHasPermission(root.permissions(), FAppPermission::StorageRead) && fappHasPermission(root.permissions(), FAppPermission::StorageWrite), "scoped permissions must enable VM storage bits");
  api.configure(fs, "/apps/test", play, nullptr, nullptr, &root.storagePolicy());
  require(native(api, FAppNativeFunction::FileRead, "/root.txt", &result) && std::string(result.asString()) == "root", "root read");
  require(native(api, FAppNativeFunction::FileWrite, "/new.txt", &result) && result.truthy(), "root write");
  require(native(api, FAppNativeFunction::FileRead, "/new.txt", &result) && std::string(result.asString()) == "new", "read root write");
  require(native(api, FAppNativeFunction::FileRead, "local.txt", &result) && std::string(result.asString()) == "local", "relative paths stay app-relative");
  require(native(api, FAppNativeFunction::FileRead, "../../public/./info.txt", &result) && std::string(result.asString()) == "public", "normalize parent traversal into granted folder");
  require(native(api, FAppNativeFunction::FileList, "/", &result), "root listing");
  const std::string listing = result.asString();
  require(listing.find("system/") == std::string::npos && listing.find("system_backup/") != std::string::npos && listing.find("root.txt") != std::string::npos, "root listing hides system, not system_backup");
  require(native(api, FAppNativeFunction::AudioPlay, "/public/info.txt", &result) && played == "/public/info.txt", "audio honors granted read paths");
  const char * blocked[] = {"/system", "/system/secret.txt", "/SYSTEM/secret.txt", "/sYsTeM/secret.txt", "//system//secret.txt", "/public/../system/secret.txt", "../../system/secret.txt", "/system./secret.txt", "/system /secret.txt", "/ system/secret.txt", "/system\\secret.txt"};
  for (const char * path : blocked) {
    for (auto fn : {FAppNativeFunction::FileRead,FAppNativeFunction::FileWrite,FAppNativeFunction::FileList,FAppNativeFunction::AudioPlay}) {
      const int before = fs.openCount();
      require(!native(api, fn, path, &result), "protected path must be rejected");
      require(fs.openCount() == before, "reject protected path before any filesystem open");
    }
  }
  // Two independent scopes: read the whole card, write only /public.
  auto scoped = manifest(fs, "[\"ui\",\"storage.read:/\",\"storage.write:/public\"]");
  api.configure(fs, "/apps/test", play, nullptr, nullptr, &scoped.storagePolicy());
  require(native(api, FAppNativeFunction::FileWrite, "/public/new.txt", &result) && result.truthy(), "write in permitted folder");
  require(!native(api, FAppNativeFunction::FileWrite, "/public_backup/new.txt", &result), "prefix lookalike is not a subtree");
  require(!native(api, FAppNativeFunction::FileWrite, "/root.txt", &result), "read grant does not imply write");
  require(!native(api, FAppNativeFunction::FileWrite, "local.txt", &result), "explicit folder scope does not imply app-local write");
  require(native(api, FAppNativeFunction::FileRead, "/root.txt", &result), "separate read scope remains available");
  require(scoped.storagePolicy().allows("/PUBLIC/new.txt", "/apps/test", true), "folder matching is case insensitive");
  require(!scoped.storagePolicy().allows("/public/a/../../outside.txt", "/apps/test", true), "normalize before scope boundary check");
  auto old = manifest(fs, "[\"ui\",\"storage.read\",\"storage.write\",\"audio\"]");
  api.configure(fs, "/apps/test", play, nullptr, nullptr, &old.storagePolicy());
  require(native(api, FAppNativeFunction::FileRead, "local.txt", &result), "legacy app-local read preserved");
  require(!native(api, FAppNativeFunction::FileRead, "/root.txt", &result), "legacy permission does not grant root");
  require(!native(api, FAppNativeFunction::FileRead, "../other.txt", &result), "legacy permission does not escape app");
  // UI file methods use the same paths and policy, including parent mkdir.
  FAppUiRegistry registry;
  lv_obj_t textarea;
  registry.registerObject(1, &textarea, FAppUiObjectType::TextArea);
  LvglFAppUiApi ui(registry);
  ui.configureFileContext(fs, "/apps/test", &root.storagePolicy());
  FAppValue path = FAppValue::string("/root.txt");
  require(ui.callMethod(1, 6, &path, 1, &result) && result.truthy() && std::string(lv_textarea_get_text(&textarea)) == "root", "textarea root load");
  ui.setProperty(1, 1, FAppValue::string("textarea data"));
  path = FAppValue::string("/public/nested/deeper/document.txt");
  require(ui.callMethod(1, 7, &path, 1, &result) && result.truthy(), "textarea absolute save creates permitted parents");
  api.configure(fs, "/apps/test", play, nullptr, nullptr, &root.storagePolicy());
  require(native(api, FAppNativeFunction::FileRead, path.asString(), &result) && std::string(result.asString()) == "textarea data", "native and textarea share absolute paths");
  for (const char * blockedPath : blocked) {
    path = FAppValue::string(blockedPath);
    const int before = fs.openCount();
    require(ui.callMethod(1, 6, &path, 1, &result) && !result.truthy(), "textarea protected load returns false");
    require(ui.callMethod(1, 7, &path, 1, &result) && !result.truthy(), "textarea protected save returns false");
    require(fs.openCount() == before, "textarea rejects system before IO");
  }
  ui.configureFileContext(fs, "/apps/test", &scoped.storagePolicy());
  path = FAppValue::string("/outside/new.txt");
  require(ui.callMethod(1, 7, &path, 1, &result) && !result.truthy() && !fs.exists("/outside"), "denied write does not create directories");
  FAppStoragePolicy narrow;
  narrow.clear();require(narrow.add("/not-created/allowed", true), "narrow grant");
  ui.configureFileContext(fs, "/apps/test", &narrow);
  path = FAppValue::string("/not-created/allowed/file.txt");
  require(ui.callMethod(1, 7, &path, 1, &result) && !result.truthy() && !fs.exists("/not-created"), "cannot mkdir outside granted folder");
  api.configure(fs, "/apps/test", play, nullptr, nullptr, &root.storagePolicy());
  require(!native(api, FAppNativeFunction::FileRead, "/system/secret.txt", &result), "system still protected after UI writes");
  // Invalid scope declarations fail closed while valid lookalikes work.
  for (const char * permission : {"storage.read:relative", "storage.write:/SYSTEM", "storage.read:/system/private", "storage.read:/system."}) {
    const std::string json = "{\"id\":\"bad\",\"name\":\"Bad\",\"version\":\"1.0.0\",\"permissions\":[\"" + std::string(permission) + "\"]}";
    add(fs, "/apps/bad/app.json", json);
    FAppManifest bad;require(bad.load(fs, "/apps/bad/app.json") == FAppManifestStatus::InvalidValue, "invalid/protected scope must reject manifest");
  }
  FAppStoragePolicy policy;policy.clear();require(policy.add("/system_backup", true), "system_backup is not protected");
  for (int i=1;i<FAppStoragePolicy::kMaximumGrants;++i)require(policy.add(("/folder"+std::to_string(i)).c_str(),false), "bounded grants fit");
  require(!policy.add("/overflow",false), "grant count limit enforced");
  if (failures) return EXIT_FAILURE;
  std::cout << "All SD folder permission, system protection, native/UI path and manifest tests passed.\n";
}
