#include <lvgl.h>
#include <cassert>
#include <cmath>
#include "ui.h"
#include "src/fapp/LvglFAppUiApi.h"
#include "Support.h"
#include "src/fapp/FAppLoader.h"
#include "src/fapp/FAppRuntime.h"
#include "src/fapp/FAppEventBridge.h"
#include "src/fapp/FAppManifest.h"
#include "src/fapp/FAppFilePairs.h"
lv_obj_t *ui_ScreenHome=nullptr;
void ui_ScreenHome_screen_init(){}
void _ui_screen_change(lv_obj_t**,lv_scr_load_anim_t,int,int,void(*)()){}
void UnloadApp(lv_event_t*){}
const ui_theme_variable_t _ui_theme_color_MainTheme[3]={0x2095F6,0xDEBE28,0x22AD32};
const ui_theme_variable_t _ui_theme_alpha_MainTheme[3]={255,255,255};
ui_style_variable_t ui_get_theme_value(const ui_theme_variable_t*v){return v[0];}
void ui_object_set_themeable_style_property(lv_obj_t*o,lv_style_selector_t s,lv_style_prop_t p,const ui_theme_variable_t*v){
  const auto value=ui_get_theme_value(v);
  if(p==LV_STYLE_BG_COLOR)lv_obj_set_style_bg_color(o,lv_color_hex(value),s);
  if(p==LV_STYLE_BG_OPA)lv_obj_set_style_bg_opa(o,value,s);
  if(p==LV_STYLE_BORDER_COLOR)lv_obj_set_style_border_color(o,lv_color_hex(value),s);
  if(p==LV_STYLE_BORDER_OPA)lv_obj_set_style_border_opa(o,value,s);
  if(p==LV_STYLE_TEXT_COLOR)lv_obj_set_style_text_color(o,lv_color_hex(value),s);
}
static FAppUiRegistry gFAppUiRegistry;
static FAppKeyboardController gFAppKeyboards;
#include "renderer.inc"
static lv_color_t framebuffer[800*480];
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*c){for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x)framebuffer[y*800+x]=*c++;lv_disp_flush_ready(d);}
static void snapshot(const char* name){
  lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);
  std::ofstream f(std::string(FOS_KEYBOARD_TEST_OUTPUT_DIR)+"/"+name+".ppm",std::ios::binary); f<<"P6\n800 480\n255\n";
  for(auto c:framebuffer){uint32_t rgb=lv_color_to32(c); char p[3]={(char)(rgb>>16),(char)(rgb>>8),(char)rgb};f.write(p,3);}
}
static void dump(){lv_obj_update_layout(lv_scr_act());for(int id=1;id<=64;++id){auto*o=gFAppUiRegistry.resolve(id);if(!o)continue;lv_area_t a;lv_obj_get_coords(o,&a);printf("ID %d type %d hidden %d bounds %d,%d %d,%d\n",id,(int)gFAppUiRegistry.objectType(id),lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN),a.x1,a.y1,a.x2,a.y2);}}

static fs::FS filesystem;
static FAppLoader loader;
static FAppRuntime runtime;
static FAppEventBridge bridge;
class Native : public FAppNativeApi {
 public:
  bool playing = false, local = false;
  int volume = 15, position = 0, duration = 180;
  std::string source;
  bool call(FAppNativeFunction function,const FAppValue * args,uint8_t,FAppValue * result) override {
    switch(function) {
      case FAppNativeFunction::AudioPlay:
        source=args[0].asString();playing=filesystem.exists(source.c_str());local=playing;position=0;*result=FAppValue::boolean(playing);break;
      case FAppNativeFunction::AudioStream:
        source=args[0].asString();playing=source.find("http")==0;local=false;*result=FAppValue::boolean(playing);break;
      case FAppNativeFunction::AudioStop:playing=false;local=false;*result=FAppValue::boolean(true);break;
      case FAppNativeFunction::AudioSetVolume:volume=args[0].asInteger();*result=FAppValue::boolean(true);break;
      case FAppNativeFunction::AudioVolume:*result=FAppValue::integer(volume);break;
      case FAppNativeFunction::AudioPlaying:*result=FAppValue::boolean(playing);break;
      case FAppNativeFunction::AudioPosition:*result=FAppValue::integer(position);break;
      case FAppNativeFunction::AudioDuration:*result=FAppValue::integer(duration);break;
      case FAppNativeFunction::AudioSeek:position=args[0].asInteger();*result=FAppValue::boolean(true);break;
      case FAppNativeFunction::Round:*result=FAppValue::integer(static_cast<int32_t>(std::lround(args[0].asFloat())));break;
      case FAppNativeFunction::FilePairValue:{std::string value;FAppFilePairs::read(filesystem,args[0].asString(),args[1].asString()[0],nullptr,args[2].asInteger(),&value);*result=FAppValue::string(value.c_str());break;}
      case FAppNativeFunction::TimerStart:*result=FAppValue::boolean(true);break;
      case FAppNativeFunction::FileList: *result=FAppValue::string("Welcome.txt");break;
      case FAppNativeFunction::FileRead: *result=FAppValue::string("");break;
      case FAppNativeFunction::StringLength: *result=FAppValue::integer(strlen(args[0].asString()));break;
      case FAppNativeFunction::StringSlice: {std::string s=args[0].asString();int start=args[1].asInteger(),count=args[2].asInteger();*result=FAppValue::string(start<0||start>=(int)s.size()||count<0?"":s.substr(start,count).c_str());break;}
      case FAppNativeFunction::StringLastIndex: {std::string s=args[0].asString();auto p=s.rfind(args[1].asString());*result=FAppValue::integer(p==s.npos?-1:p);break;}
      default: *result=FAppValue::boolean(false);break;
    }
    return true;
  }
  const char* errorMessage()const override{return "test native";}
  void reset()override{}
} native;
static void drain(){for(int i=0;i<40;++i)runtime.update(256,0);gFAppKeyboards.update();if(runtime.state()==FAppRuntimeState::Faulted)fprintf(stderr,"VM FAULT: %s\n",runtime.errorMessage());assert(runtime.state()!=FAppRuntimeState::Faulted);}
static void beginVm(const char* folder,LvglFAppUiApi& api){
  std::string dir=std::string(APPS_DIR)+"/"+folder;
  std::ifstream f(dir+"/main.fapp",std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
  filesystem.addFile(dir+"/main.fapp",bytes);
  filesystem.addFile(dir+"/layout.ui",{1});
  loader.begin(filesystem);
  assert(loader.prepare(dir.c_str(),"layout.ui","main.fapp")==FAppLoaderStatus::ExecutableReady);
  std::ifstream m(dir+"/app.json");std::string json((std::istreambuf_iterator<char>(m)),{});
  filesystem.addFile(dir+"/app.json",std::vector<uint8_t>(json.begin(),json.end()));
  FAppManifest manifest;assert(manifest.load(filesystem,(dir+"/app.json").c_str())==FAppManifestStatus::Ready);
  api.configureFileContext(filesystem,dir.c_str(),&manifest.storagePolicy());
  assert(runtime.begin(filesystem,loader,api,manifest.permissions(),&native));
  assert(runtime.start());bridge.attach(runtime,gFAppUiRegistry);drain();
}
class DiagnosticUi : public LvglFAppUiApi {
public:
  DiagnosticUi(FAppUiRegistry& r,FAppKeyboardController*k):LvglFAppUiApi(r,k){}
  bool getProperty(uint16_t id,uint8_t p,FAppValue*r)override{bool ok=LvglFAppUiApi::getProperty(id,p,r);if(!ok)fprintf(stderr,"getProperty %u %u failed\n",id,p);return ok;}
  bool setProperty(uint16_t id,uint8_t p,const FAppValue&v)override{bool ok=LvglFAppUiApi::setProperty(id,p,v);if(!ok)fprintf(stderr,"setProperty %u %u failed value=%d type=%d\n",id,p,v.asInteger(),(int)v.type());return ok;}
  bool callMethod(uint16_t id,uint8_t m,const FAppValue*a,uint8_t n,FAppValue*r)override{bool ok=LvglFAppUiApi::callMethod(id,m,a,n,r);if(!ok)fprintf(stderr,"callMethod %u %u argc %u failed\n",id,m,n);return ok;}
};
static void clearScene(){runtime.shutdown();bridge.clear();gFAppKeyboards.reset();lv_obj_clean(uic_AppContentArea);gFAppUiRegistry.clear();injectedLayout="";}
static void press(lv_obj_t* kb,const char* text){
  const char **map=lv_btnmatrix_get_map(kb);int button=0;
  for(int i=0;map[i][0];++i){if(std::string(map[i])=="\n")continue;if(std::string(map[i])==text){lv_btnmatrix_set_selected_btn(kb,button);lv_event_send(kb,LV_EVENT_VALUE_CHANGED,nullptr);return;}++button;}
  assert(false);
}
static void inside(lv_obj_t* kb){lv_obj_update_layout(ui_AppContent);lv_area_t a;lv_obj_get_coords(kb,&a);assert(a.x1>=0&&a.y1>=0&&a.x2<800&&a.y2<480);}
int main(){
  lv_init();static lv_color_t pixels[800*10];static lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,pixels,nullptr,800*10);
  lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=800;d.ver_res=480;d.draw_buf=&db;d.flush_cb=flush;auto* disp=lv_disp_drv_register(&d);
  lv_disp_set_theme(disp,lv_theme_default_init(disp,lv_palette_main(LV_PALETTE_BLUE),lv_palette_main(LV_PALETTE_RED),true,LV_FONT_DEFAULT));
  ui_AppContent_screen_init();lv_scr_load(ui_AppContent);
  lv_obj_set_style_pad_all(uic_AppContentArea,0,0);
  LauncherAppEntry app;app.folderName="text";app.layoutFile="layout.ui";
  assert(renderAppLayout(app));dump();snapshot("text");
  auto*kb=gFAppUiRegistry.resolve(5);assert(gFAppUiRegistry.objectType(5)==FAppUiObjectType::Keyboard);assert(!lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  DiagnosticUi api(gFAppUiRegistry, &gFAppKeyboards);FAppValue result;
  filesystem.addFile("/text/start.txt",{'s','t','a','r','t'});
  filesystem.addFile("/text/folder/nested.txt",{'n','e','s','t','e','d'});
  for(int i=0;i<80;++i)filesystem.addFile("/text/file_"+std::to_string(i)+".txt",{'x'});
  beginVm("text",api);
  assert(lv_roller_get_option_cnt(gFAppUiRegistry.resolve(12))==82);
  auto bounds=[&](int id,int x,int y,int width,int height){lv_obj_update_layout(ui_AppContent);lv_area_t a;lv_obj_get_coords(gFAppUiRegistry.resolve(id),&a);assert(a.x1==x&&a.y1==y&&a.x2==x+width-1&&a.y2==y+height-1);};
  bounds(1,166,5,200,50);bounds(2,376,5,200,50);bounds(3,586,5,200,50);
  bounds(4,13,69,774,203);bounds(5,3,276,793,197);
  snapshot("text");
  lv_event_send(gFAppUiRegistry.resolve(3),LV_EVENT_CLICKED,nullptr);drain();
  assert(!lv_obj_has_flag(gFAppUiRegistry.resolve(11),LV_OBJ_FLAG_HIDDEN));
  bounds(11,100,23,600,350);bounds(12,112,35,576,266);
  bounds(13,114,313,160,50);bounds(14,286,313,160,50);
  assert(lv_obj_get_index(kb)<lv_obj_get_index(gFAppUiRegistry.resolve(11)));
  snapshot("text-open");
  lv_roller_set_selected(gFAppUiRegistry.resolve(12),0,LV_ANIM_OFF);
  lv_event_send(gFAppUiRegistry.resolve(13),LV_EVENT_CLICKED,nullptr);drain();
  assert(!lv_obj_has_flag(gFAppUiRegistry.resolve(11),LV_OBJ_FLAG_HIDDEN));
  char selected[128];lv_roller_get_selected_str(gFAppUiRegistry.resolve(12),selected,sizeof(selected));assert(std::string(selected)=="..");
  lv_roller_set_selected(gFAppUiRegistry.resolve(12),1,LV_ANIM_OFF);
  lv_event_send(gFAppUiRegistry.resolve(13),LV_EVENT_CLICKED,nullptr);drain();
  assert(lv_obj_has_flag(gFAppUiRegistry.resolve(11),LV_OBJ_FLAG_HIDDEN));
  assert(std::string(lv_textarea_get_text(gFAppUiRegistry.resolve(4)))=="nested");
  lv_event_send(gFAppUiRegistry.resolve(3),LV_EVENT_CLICKED,nullptr);drain();
  lv_event_send(gFAppUiRegistry.resolve(13),LV_EVENT_CLICKED,nullptr);drain();
  assert(lv_roller_get_option_cnt(gFAppUiRegistry.resolve(12))==82);
  assert(!lv_obj_has_flag(gFAppUiRegistry.resolve(11),LV_OBJ_FLAG_HIDDEN));
  lv_event_send(gFAppUiRegistry.resolve(14),LV_EVENT_CLICKED,nullptr);drain();
  lv_event_send(gFAppUiRegistry.resolve(2),LV_EVENT_CLICKED,nullptr);drain();
  auto*filenameKb=gFAppUiRegistry.resolve(10);
  assert(!lv_obj_has_flag(filenameKb,LV_OBJ_FLAG_HIDDEN));inside(filenameKb);
  assert(lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  bounds(6,100,88,600,140);bounds(7,111,100,577,52);
  bounds(8,114,166,160,50);bounds(9,286,166,160,50);
  snapshot("text-save");
  press(filenameKb,"q");press(filenameKb,LV_SYMBOL_OK);drain();
  assert(filesystem.exists("/text/q.txt"));
  assert(lv_obj_has_flag(gFAppUiRegistry.resolve(6),LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_has_flag(filenameKb,LV_OBJ_FLAG_HIDDEN));assert(!lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  assert(std::string(lv_textarea_get_text(gFAppUiRegistry.resolve(7)))=="q.txt");
  // New file clears only editor content, preserving the native save-name behavior.
  lv_event_send(gFAppUiRegistry.resolve(1),LV_EVENT_CLICKED,nullptr);drain();
  assert(std::string(lv_textarea_get_text(gFAppUiRegistry.resolve(4))).empty());
  assert(std::string(lv_textarea_get_text(gFAppUiRegistry.resolve(7)))=="q.txt");
  auto saveAs=[&](const char*name){api.setProperty(4,1,FAppValue::string("updated"));api.setProperty(7,1,FAppValue::string(name));lv_event_send(gFAppUiRegistry.resolve(2),LV_EVENT_CLICKED,nullptr);drain();lv_event_send(gFAppUiRegistry.resolve(8),LV_EVENT_CLICKED,nullptr);drain();};
  saveAs("folder/sub/new");assert(filesystem.exists("/text/folder/sub/new.txt"));
  saveAs("  /folder//UPPER.TXT/  ");assert(filesystem.exists("/text/folder/UPPER.TXT"));assert(!filesystem.exists("/text/folder/UPPER.TXT.txt"));
  saveAs("folder\\windows");assert(filesystem.exists("/text/folder/windows.txt"));
  saveAs("../escape");assert(!filesystem.exists("/escape.txt"));
  saveAs("folder/../escape");assert(!filesystem.exists("/text/escape.txt"));
  saveAs("");assert(!filesystem.exists("/text/.txt"));
  saveAs("q.txt");auto saved=filesystem.open("/text/q.txt",FILE_READ);char savedText[8]={};saved.read(reinterpret_cast<uint8_t*>(savedText),7);saved.close();assert(std::string(savedText)=="updated");
  FAppValue protectedPath=FAppValue::string("/other/failure.txt");assert(api.callMethod(4,7,&protectedPath,1,&result)&&!result.truthy());
  // Loading large text uses the direct textarea path, not VM string truncation.
  filesystem.addFile("/text/long.txt",std::vector<uint8_t>(4500,'a'));
  FAppValue longPath=FAppValue::string("/text/long.txt");assert(api.callMethod(4,6,&longPath,1,&result)&&result.truthy());
  assert(std::string(lv_textarea_get_text(gFAppUiRegistry.resolve(4))).find("[Datei gekuerzt]")==4002);
  clearScene();app.folderName="fscript_weather";assert(renderAppLayout(app));
  beginVm("fscript_weather",api);
  assert(lv_obj_has_flag(gFAppUiRegistry.resolve(8),LV_OBJ_FLAG_HIDDEN));
  lv_event_send(gFAppUiRegistry.resolve(4),LV_EVENT_CLICKED,nullptr);drain();
  assert(!lv_obj_has_flag(gFAppUiRegistry.resolve(8),LV_OBJ_FLAG_HIDDEN));
  dump();snapshot("weather");inside(gFAppUiRegistry.resolve(8));
  press(gFAppUiRegistry.resolve(8),"q");assert(std::string(lv_textarea_get_text(gFAppUiRegistry.resolve(6)))=="q");
  press(gFAppUiRegistry.resolve(8),LV_SYMBOL_KEYBOARD);drain();
  assert(lv_obj_has_flag(gFAppUiRegistry.resolve(8),LV_OBJ_FLAG_HIDDEN));
  clearScene();app.folderName="fscript_keyboard_test";assert(renderAppLayout(app));
  beginVm("fscript_keyboard_test",api);
  kb=gFAppUiRegistry.resolve(1);assert(lv_keyboard_get_textarea(kb)==gFAppUiRegistry.resolve(3));
  assert(!lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));inside(kb);
  press(kb,"q");press(kb,LV_SYMBOL_OK);drain();
  assert(std::string(lv_label_get_text(gFAppUiRegistry.resolve(2)))=="Confirmed: q");
  press(kb,LV_SYMBOL_KEYBOARD);drain();assert(lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  assert(api.callMethod(3,2,nullptr,0,&result));assert(!lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  snapshot("diagnostic");
  // A keyboard larger than its logical dialog must not be clipped by it.
  clearScene();app.folderName="__regression__";
  injectedLayout="type=panel;id=panel;x=20;y=20;w=760;h=120;styleless=true;hidden=true\n"
    "type=keyboard;id=kb;parent=panel;target=ta;x=0;y=250;w=760;h=197\n"
    "type=textarea;id=ta;parent=panel;x=0;y=0;w=740;h=70;text=\n";
  assert(renderAppLayout(app));kb=gFAppUiRegistry.resolve(2);
  assert(lv_keyboard_get_textarea(kb)==gFAppUiRegistry.resolve(3));
  assert(lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  assert(api.setProperty(1,4,FAppValue::boolean(false)));gFAppKeyboards.update();
  assert(!lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));inside(kb);
  assert(lv_obj_get_parent(kb)==uic_AppContentArea);
  press(kb,"q");assert(std::string(lv_textarea_get_text(gFAppUiRegistry.resolve(3)))=="q");
  snapshot("small-dialog");
  assert(api.setProperty(1,4,FAppValue::boolean(true)));gFAppKeyboards.update();
  assert(lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  assert(api.getProperty(2,4,&result)&&!result.asBoolean());
  assert(api.setProperty(1,4,FAppValue::boolean(false)));gFAppKeyboards.update();
  assert(!lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  lv_obj_del(gFAppUiRegistry.resolve(1));gFAppKeyboards.update();
  assert(lv_keyboard_get_textarea(kb)==nullptr&&lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  // Offscreen legacy geometry is clamped; deleting/unloading detaches all hooks.
  clearScene();injectedLayout="type=textarea;id=ta;x=0;y=0;w=760;h=70\n"
    "type=keyboard;id=kb;target=ta;x=25;y=561;w=900;h=197\n";
  assert(renderAppLayout(app));kb=gFAppUiRegistry.resolve(2);inside(kb);
  lv_group_t*group=lv_group_create();auto*other=lv_textarea_create(uic_AppContentArea);
  lv_group_add_obj(group,other);lv_group_add_obj(group,gFAppUiRegistry.resolve(1));
  assert(api.setProperty(2,4,FAppValue::boolean(true)));assert(api.callMethod(1,2,nullptr,0,&result));
  assert(!lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));
  assert(api.setProperty(2,4,FAppValue::boolean(true)));assert(api.callMethod(1,2,nullptr,0,&result));
  assert(!lv_obj_has_flag(kb,LV_OBJ_FLAG_HIDDEN));lv_group_remove_all_objs(group);lv_group_del(group);
  lv_obj_del(kb);gFAppKeyboards.update();assert(!gFAppKeyboards.owns(kb));
  clearScene();
  std::string many;
  for(int i=0;i<9;++i)many+="type=keyboard;id=kb"+std::to_string(i)+";target=ta;x=0;y=278;w=800;h=197\n";
  many+="type=textarea;id=ta;x=0;y=0;w=760;h=70\n";
  injectedLayout=many;assert(renderAppLayout(app));
  assert(gFAppUiRegistry.resolve(9)==nullptr);
  assert(gFAppUiRegistry.objectType(10)==FAppUiObjectType::TextArea);
  assert(lv_keyboard_get_textarea(gFAppUiRegistry.resolve(1))==gFAppUiRegistry.resolve(10));
  clearScene();
  auto*sdText=lv_textarea_create(uic_AppContentArea);
  assert(gFAppUiRegistry.registerObject(1,sdText,FAppUiObjectType::TextArea));
  FAppStoragePolicy rootPolicy;rootPolicy.clear();assert(rootPolicy.add("/",false)&&rootPolicy.add("/",true));
  api.configureFileContext(filesystem,"/apps/test",&rootPolicy);
  assert(api.setProperty(1,1,FAppValue::string("SD root data")));
  FAppValue sdPath=FAppValue::string("/text/deep/test.txt");
  assert(api.callMethod(1,7,&sdPath,1,&result)&&result.truthy());
  assert(api.setProperty(1,1,FAppValue::string("")));
  assert(api.callMethod(1,6,&sdPath,1,&result)&&result.truthy());
  assert(std::string(lv_textarea_get_text(sdText))=="SD root data");
  sdPath=FAppValue::string("/SYSTEM/private.txt");int before=filesystem.openCount();
  assert(api.callMethod(1,6,&sdPath,1,&result)&&!result.truthy());
  assert(api.callMethod(1,7,&sdPath,1,&result)&&!result.truthy());
  assert(filesystem.openCount()==before);
  clearScene();app.folderName="text";assert(renderAppLayout(app));
  std::string deniedDir=std::string(APPS_DIR)+"/text";
  loader.begin(filesystem);assert(loader.prepare(deniedDir.c_str(),"layout.ui","main.fapp")==FAppLoaderStatus::ExecutableReady);
  api.configureFileContext(filesystem,deniedDir.c_str(),&rootPolicy);
  assert(runtime.begin(filesystem,loader,api,static_cast<FAppPermissions>(FAppPermission::Ui),&native));
  assert(runtime.start());
  for(int i=0;i<20;++i)runtime.update(256,0);
  assert(runtime.state()==FAppRuntimeState::Faulted&&runtime.error()==FAppRuntimeError::PermissionDenied);
  clearScene();app.folderName="radio";assert(renderAppLayout(app));
  filesystem.addFile("/music/files/Album/track.MP3",{'x'});
  filesystem.addFile("/music/files/ignored.txt",{'x'});
  filesystem.addFile("/music/files/._hidden.mp3",{'x'});
  filesystem.addFile("/music/files/top.wav",{'x'});
  std::string config="invalid\n|url\nName|\n";
  for(int i=0;i<45;++i)config+="Station "+std::to_string(i)+" | https://example.test/"+std::to_string(i)+"\r\n";
  filesystem.addFile("/music/webradio/webradio.txt",std::vector<uint8_t>(config.begin(),config.end()));
  beginVm("radio",api);
  auto*play=gFAppUiRegistry.resolve(1);auto*vol=gFAppUiRegistry.resolve(2);auto*progress=gFAppUiRegistry.resolve(3);auto*tabs=gFAppUiRegistry.resolve(4);auto*files=gFAppUiRegistry.resolve(6);auto*web=gFAppUiRegistry.resolve(8);
  assert(gFAppUiRegistry.count()==8);
  assert(lv_obj_check_type(vol,&lv_slider_class));assert(lv_obj_check_type(tabs,&lv_tabview_class));
  assert(lv_tabview_get_tab_act(tabs)==0);assert(lv_slider_get_min_value(vol)==0&&lv_slider_get_max_value(vol)==21);
  assert(lv_slider_get_value(vol)==15);
  assert(lv_roller_get_option_cnt(files)==2);assert(lv_roller_get_option_cnt(web)==40);
  bounds(1,339,8,122,45);bounds(2,30,77,740,18);bounds(3,30,131,740,18);bounds(4,0,179,800,295);
  snapshot("radio-files");
  lv_event_send(play,LV_EVENT_CLICKED,nullptr);drain();assert(lv_roller_get_option_cnt(files)==2); // Album/ -> ../ + track.MP3
  lv_roller_set_selected(files,1,LV_ANIM_OFF);lv_event_send(play,LV_EVENT_CLICKED,nullptr);drain();
  assert(native.playing&&native.local&&native.source=="/music/files/Album/track.MP3");
  assert(std::string(lv_label_get_text(lv_obj_get_child(play,0)))=="||");
  native.position=90;runtime.queueEvent(0,19);drain();assert(lv_slider_get_value(progress)==500);
  lv_obj_add_state(progress,LV_STATE_PRESSED);lv_slider_set_value(progress,750,LV_ANIM_OFF);runtime.queueEvent(0,19);drain();assert(lv_slider_get_value(progress)==750);
  lv_obj_clear_state(progress,LV_STATE_PRESSED);lv_event_send(progress,LV_EVENT_RELEASED,nullptr);drain();assert(native.position==135);
  lv_slider_set_value(vol,7,LV_ANIM_OFF);lv_event_send(vol,LV_EVENT_VALUE_CHANGED,nullptr);drain();assert(native.volume==7);
  native.duration=40000;lv_slider_set_value(progress,500,LV_ANIM_OFF);lv_event_send(progress,LV_EVENT_RELEASED,nullptr);drain();assert(native.position==20000);native.duration=180;
  lv_tabview_set_act(tabs,1,LV_ANIM_OFF);assert(native.playing);lv_event_send(play,LV_EVENT_CLICKED,nullptr);drain();assert(!native.playing);
  lv_roller_set_selected(web,39,LV_ANIM_OFF);lv_event_send(play,LV_EVENT_CLICKED,nullptr);drain();assert(native.playing&&!native.local&&native.source=="https://example.test/39");
  assert(lv_obj_has_state(progress,LV_STATE_DISABLED));snapshot("radio-web");
  native.playing=false;runtime.queueEvent(0,19);drain();assert(std::string(lv_label_get_text(lv_obj_get_child(play,0)))==">");
  lv_tabview_set_act(tabs,0,LV_ANIM_OFF);lv_roller_set_selected(files,0,LV_ANIM_OFF);lv_event_send(play,LV_EVENT_CLICKED,nullptr);drain();assert(lv_roller_get_option_cnt(files)==2);
  lv_roller_set_selected(files,1,LV_ANIM_OFF);lv_event_send(play,LV_EVENT_CLICKED,nullptr);drain();assert(native.source=="/music/files/top.wav"&&native.playing);
  runtime.shutdown();assert(!native.playing);
  puts("PASS: Radio real LVGL/VM: reference geometry, tabs, scoped music browser/filter, 40 stations beyond string limit, play/stop, volume, EOF, progress/drag/seek, long files and app.close");
  clearScene();puts("PASS: full firmware layout + real LVGL + existing fAPP + events; text reference geometry, scoped /text IO, long listings, navigation, save normalization, overwrite/new/cancel, weather Search/cancel, forward binding, nested clipping, hidden parents, offscreen bounds, focus and deletion");
}
