#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <cstdio>
#include <cstdlib>
class String {
  std::string s;
public:
  String() = default;
  String(const char* x):s(x?x:""){}
  String(std::string x):s(std::move(x)){}
  size_t length() const {return s.size();}
  const char* c_str() const {return s.c_str();}
  char operator[](size_t x) const {return s[x];}
  void reserve(size_t x) {s.reserve(x);}
  void trim(){auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n"); s=a==s.npos?"":s.substr(a,b-a+1);}
  void toLowerCase(){for(auto& c:s)c=std::tolower((unsigned char)c);}
  int indexOf(char c,int a=0) const {auto n=s.find(c,a);return n==s.npos?-1:n;}
  int indexOf(const String& x,int a=0) const {auto n=s.find(x.s,a);return n==s.npos?-1:n;}
  String substring(int a,int b)const{return s.substr(a,b-a);}
  String substring(int a)const{return s.substr(a);}
  bool startsWith(const char*x)const{return s.rfind(x,0)==0;}
  int toInt()const{return std::strtol(s.c_str(),nullptr,10);}
  void toCharArray(char*p,size_t n)const{snprintf(p,n,"%s",s.c_str());}
  String& operator+=(char c){s+=c;return *this;}
  String& operator+=(const String&c){s+=c.s;return *this;}
  friend String operator+(const String&a,const String&b){return a.s+b.s;}
  friend bool operator==(const String&a,const String&b){return a.s==b.s;}
};
struct SerialStub {template<typename...T> void printf(const char*f,T...t){std::printf(f,t...);} void println(const char*s){puts(s);} } Serial;
struct LauncherAppEntry {String folderName, layoutFile; bool scrollable=false;};
#define APPS_DIR FOS_KEYBOARD_TEST_APPS_DIR
#define FAPP_KEYBOARD_BINDING_CAPACITY 8U
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
static void* heap_caps_malloc(size_t n,int){return malloc(n);}
static void heap_caps_free(void*p){free(p);}
static String injectedLayout;
static bool readTextFileLimited(const String& path,String*out,size_t limit){if(injectedLayout.length()>0){*out=injectedLayout;return true;}std::ifstream f(path.c_str()); if(!f)return false; std::string s((std::istreambuf_iterator<char>(f)),{}); *out=s.substr(0,limit); return true;}
