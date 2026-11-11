#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <wincred.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <mmsystem.h>
#include <imm.h>
#include <deque>
#include "chat_protocol.hpp"
#include "transport.hpp"
#include "chat_data.hpp"
#include <filesystem>
#include <fstream>
#include "model.hpp"
#include "discovery.hpp"
#include "backend_runtime.hpp"
#include "visual.hpp"
#include "reset_tracker.hpp"
#include "quota_refresh.hpp"
#include "update_windows.hpp"
using namespace Gdiplus;
constexpr UINT WM_DATA=WM_APP+1,WM_TRAY=WM_APP+2,WM_CHAT=WM_APP+3,WM_CREDITS=WM_APP+4,WM_APP_UPDATE=WM_APP+5;
enum : UINT { ID_REFRESH=100,ID_LOGIN=101,ID_THEME=103,ID_TOP=104,ID_TASKBAR=105,ID_SHOW=106,ID_QUIT=107,ID_SETTINGS=108,ID_RESET=109,ID_DASHBOARD=110,ID_AUTO=111,ID_DETECT=113,ID_CAPSULE=115,ID_LIGHT=116,ID_DARK=117,ID_STARTUP=118,ID_CHAT=119,ID_CHAT_DIAGNOSTICS=120,ID_HTTPS=121,ID_DEFAULT_TRANSPORT=122 };
HINSTANCE instance;HFONT settingsFont=nullptr;HWND mainWindow,taskWindow,settingsWindow,opacityControl;ULONG_PTR graphicsToken;
std::wstring configPath,cliPath;bool dark=false,alwaysTop=true,taskEnabled=true,autoTheme=true;bool capsule=false;int expandedWidth=420,expandedHeight=480;int taskFontSize=14;std::atomic<bool> httpsMode{true};bool chatView=false;int usageWidth=0,usageHeight=0;int opacity=95,taskOffset=0;RECT restored={100,100,500,530};uw::Quota quota;bool fresh=false;time_t lastUpdate=0;std::wstring status=L"Starting...";NOTIFYICONDATAW tray{};std::atomic<bool> stopWorker{false},wantRefresh{true},wantRestart{false},wantDetect{true};std::thread worker;std::mutex pathMutex;
struct ChatAction {int kind;std::string text,model,effort,context,thread;std::vector<std::string>images;};
std::mutex chatMutex;std::deque<ChatAction> chatActions;std::wstring chatDirectory;
void queueChat(int kind,const std::string&text="",const std::string&model="",const std::string&effort=""){std::lock_guard<std::mutex>lock(chatMutex);chatActions.push_back({kind,text,model,effort,"","",{}});}
void queueSend(const std::string&text,const std::string&model,const std::string&effort,const std::string&context,const std::string&thread,const std::vector<std::string>&images){std::lock_guard<std::mutex>lock(chatMutex);chatActions.push_back({0,text,model,effort,context,thread,images});}
void queueBind(const std::string&context,const std::string&thread){std::lock_guard<std::mutex>lock(chatMutex);chatActions.push_back({5,"","","",context,thread,{}});}
std::string createOutput(const uw::Json&,const std::string&);
uw::data::Credits resetCredits;time_t creditsUpdated=0;bool resetBusy=false;std::wstring resetMessage;std::string pendingResetKey;
unsigned refreshIntervalSeconds = uw::refresh::defaultSeconds;
std::atomic<unsigned> automaticQuotaSeconds{uw::refresh::defaultSeconds};
std::atomic<unsigned> requestedQuotaRead{0};
unsigned quotaReadSequence = 0, activeQuotaRead = 0;
ULONGLONG quotaReadDeadline = 0;
bool confirmResetAfterRead = false, resetConfirmationOpen = false;
std::wstring quotaReadMessage;
bool updateInstalling();

void loadQuotaPrefs() {
    wchar_t value[16]{};
    GetPrivateProfileStringW(L"Usage", L"RefreshSeconds", L"", value, 16, configPath.c_str());
    refreshIntervalSeconds = uw::refresh::parseInterval(value).value_or(uw::refresh::defaultSeconds);
    automaticQuotaSeconds = GetPrivateProfileIntW(L"Usage", L"AutoRefresh", 1, configPath.c_str())
        ? refreshIntervalSeconds : 0;
}

void requestQuotaRead(bool forReset = false) {
    confirmResetAfterRead = confirmResetAfterRead || forReset;
    if (activeQuotaRead) return;
    if (++quotaReadSequence == 0) ++quotaReadSequence;
    activeQuotaRead = quotaReadSequence;
    quotaReadDeadline = GetTickCount64() + 65000;
    quotaReadMessage = L"Refreshing data...";
    requestedQuotaRead = activeQuotaRead;
}

void publishCredits(const uw::data::Credits&c){auto*p=new uw::data::Credits(c);if(!PostMessageW(mainWindow,WM_CREDITS,0,(LPARAM)p))delete p;}
void publishChat(const uw::chat::Event&e){auto*p=new uw::chat::Event(e);if(!PostMessageW(mainWindow,WM_CHAT,0,(LPARAM)p))delete p;}UINT taskbarCreated;float uiScale=1;
std::wstring wide(const std::string&s){if(s.empty())return L"";int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,L' ');MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),w.data(),n);return w;}
std::string narrow(const std::wstring&s){int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string o(n,' ');WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),o.data(),n,nullptr,nullptr);return o;}
bool systemLight(){DWORD x=1,n=4;RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&x,&n);return x!=0;}
bool taskLight(){DWORD x=1,n=4;RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&x,&n);return x!=0;}
void save(){WritePrivateProfileStringW(L"Appearance",L"TaskFontSize",std::to_wstring(taskFontSize).c_str(),configPath.c_str());WritePrivateProfileStringW(L"Appearance",L"Dark",dark?L"1":L"0",configPath.c_str());WritePrivateProfileStringW(L"Appearance",L"Auto",autoTheme?L"1":L"0",configPath.c_str());WritePrivateProfileStringW(L"Appearance",L"Opacity",std::to_wstring(opacity).c_str(),configPath.c_str());WritePrivateProfileStringW(L"Window",L"Topmost",alwaysTop?L"1":L"0",configPath.c_str());WritePrivateProfileStringW(L"Window",L"Taskbar",taskEnabled?L"1":L"0",configPath.c_str());WritePrivateProfileStringW(L"Window",L"TaskOffset",std::to_wstring(taskOffset).c_str(),configPath.c_str());RECT r;GetWindowRect(mainWindow,&r);for(auto k:{L"X",L"Y",L"W",L"H"}){int v=k[0]=='X'?r.left:k[0]=='Y'?r.top:k[0]=='W'?(chatView&&usageWidth?usageWidth:capsule?expandedWidth:r.right-r.left):(chatView&&usageHeight?usageHeight:capsule?expandedHeight:r.bottom-r.top);WritePrivateProfileStringW(L"Window",k,std::to_wstring(v).c_str(),configPath.c_str());}WritePrivateProfileStringW(L"Window",L"Capsule",capsule?L"1":L"0",configPath.c_str());std::lock_guard<std::mutex> lock(pathMutex);WritePrivateProfileStringW(L"Connection",L"CodexPath",cliPath.c_str(),configPath.c_str());}
std::wstring findCli(){wchar_t out[32768];for(auto file:{L"codex.exe",L"codex.cmd"})if(SearchPathW(nullptr,file,nullptr,32768,out,nullptr))return out;wchar_t app[32768];if(GetEnvironmentVariableW(L"APPDATA",app,32768)){auto p=std::wstring(app)+L"\\npm\\codex.cmd";if(GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES)return p;}return L"";}
struct Update{uw::Quota q;bool ok=false;std::wstring message;unsigned readSequence=0;};
void publish(bool ok,const std::wstring&message,const uw::Quota&q={},unsigned readSequence=0){auto*p=new Update{q,ok,message,readSequence};if(!PostMessageW(mainWindow,WM_DATA,0,(LPARAM)p))delete p;}
struct Child {
 HANDLE process=nullptr,job=nullptr,in=nullptr,out=nullptr,errors=nullptr;uw::runtime::Layout runtime;uw::runtime::HostProbe hostProbe;DWORD startError=0;
 ~Child(){close();}void close(){if(in){CloseHandle(in);in=nullptr;}if(job){CloseHandle(job);job=nullptr;}if(process){if(WaitForSingleObject(process,500)==WAIT_TIMEOUT){TerminateProcess(process,0);WaitForSingleObject(process,500);}CloseHandle(process);process=nullptr;}if(out){CloseHandle(out);out=nullptr;}if(errors){CloseHandle(errors);errors=nullptr;}}
 bool start(const std::wstring&path,bool https){SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE childIn=nullptr,childOut=nullptr,err=nullptr;if(!CreatePipe(&childIn,&in,&sa,1024*1024))return false;if(!CreatePipe(&out,&childOut,&sa,0)){CloseHandle(childIn);return false;}SetHandleInformation(in,HANDLE_FLAG_INHERIT,0);SetHandleInformation(out,HANDLE_FLAG_INHERIT,0);if(!CreatePipe(&errors,&err,&sa,0)){CloseHandle(childIn);CloseHandle(childOut);return false;}SetHandleInformation(errors,HANDLE_FLAG_INHERIT,0);runtime=uw::runtime::locate(std::filesystem::path(path),&stopWorker);if(runtime.backend.wstring()!=path&&!discovery::supports(runtime.backend.wstring(),stopWorker)){runtime.backend=std::filesystem::path(path);runtime.backendDir=runtime.backend.parent_path();runtime.host.clear();}auto environment=uw::runtime::childEnvironment(runtime);hostProbe=uw::runtime::probeHost(runtime,environment);
 STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;si.hStdInput=childIn;si.hStdOutput=childOut;si.hStdError=err;PROCESS_INFORMATION pi{};
 auto overrides=uw::runtime::overrides();auto transportOverrides=uw::transport::overrides(https);overrides.insert(overrides.end(),transportOverrides.begin(),transportOverrides.end());std::wstring flags;for(auto&v:overrides)flags+=L" -c \""+wide(v)+L"\"";
 auto launch=runtime.backend.wstring();std::wstring cmd,exe;bool batch=launch.size()>4&&_wcsicmp(launch.substr(launch.size()-4).c_str(),L".cmd")==0;
 if(batch){wchar_t sys[MAX_PATH];GetSystemDirectoryW(sys,MAX_PATH);exe=std::wstring(sys)+L"\\cmd.exe";cmd=L"\""+exe+L"\" /d /s /c \"\""+launch+L"\""+flags+L" app-server\"";}else{exe=launch;cmd=L"\""+launch+L"\""+flags+L" app-server";}
 job=CreateJobObjectW(nullptr,nullptr);if(job){JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji{};ji.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;SetInformationJobObject(job,JobObjectExtendedLimitInformation,&ji,sizeof(ji));}
 BOOL ok=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT,environment.data(),runtime.backendDir.empty()?nullptr:runtime.backendDir.c_str(),&si,&pi);startError=ok?0:GetLastError();CloseHandle(childIn);CloseHandle(childOut);if(err!=INVALID_HANDLE_VALUE)CloseHandle(err);if(!ok)return false;process=pi.hProcess;if(job&&!AssignProcessToJobObject(job,process)){startError=GetLastError();TerminateProcess(process,1);CloseHandle(pi.hThread);return false;}ResumeThread(pi.hThread);CloseHandle(pi.hThread);return true;}
 std::string readErrors(){std::string result;DWORD available=0;char bytes[4096];while(errors&&result.size()<32768&&PeekNamedPipe(errors,nullptr,0,nullptr,&available,nullptr)&&available){DWORD got=0;if(!ReadFile(errors,bytes,std::min<DWORD>(available,sizeof(bytes)),&got,nullptr)||!got)break;result.append(bytes,got);}return result;}
 bool send(const std::string&s){std::string line=s+"\n";DWORD n=0;return in&&WriteFile(in,line.data(),(DWORD)line.size(),&n,nullptr)&&n==line.size();}
};
void backend(){CoInitializeEx(nullptr,COINIT_MULTITHREADED);uw::chat::Session chat;int previousMode=-1;
 auto flush=[&](Child*child){for(auto&e:chat.events)publishChat(e);chat.events.clear();if(child)for(auto&c:chat.commands)if(!child->send(c))wantRestart=true;chat.commands.clear();};
 while(!stopWorker){
 if(wantDetect.exchange(false)){publish(false,L"Detecting desktop Codex...");std::wstring report;auto found=discovery::desktop(stopWorker,report);if(!found.empty()){std::lock_guard<std::mutex>lock(pathMutex);cliPath=found;WritePrivateProfileStringW(L"Connection",L"CodexPath",cliPath.c_str(),configPath.c_str());}wantRestart=false;}
 std::wstring path;{std::lock_guard<std::mutex>lock(pathMutex);path=cliPath;}
 bool runHttps=httpsMode.load();if(previousMode!=(int)runHttps){chat=uw::chat::Session{};previousMode=(int)runHttps;}chat.transportLabel=runHttps?"HTTPS requested; not verified":"Codex default (WebSocket capable)";
 chat.writeOutput=[&](const uw::Json&args){return createOutput(args,chat.context);};
 Child child;bool started=!path.empty()&&child.start(path,runHttps);bool runtimeReady=!child.runtime.host.empty()&&uw::runtime::completePair(child.runtime.backend)&&child.hostProbe.launched&&!child.hostProbe.error;if(started&&child.runtime.backend.wstring()!=path){std::lock_guard<std::mutex>lock(pathMutex);cliPath=child.runtime.backend.wstring();WritePrivateProfileStringW(L"Connection",L"CodexPath",cliPath.c_str(),configPath.c_str());}std::string runtimeReport="Backend runtime\nCompanion host: "+std::string(child.runtime.host.empty()?"not located in selected installation":"located in selected installation")+"\nChild PATH: installation directories prepended; global PATH unchanged\nCode Mode: disabled for widget; direct tools requested\n";if(child.startError)runtimeReport+="Backend process start error: "+std::to_string(child.startError)+"\n";auto runtimeUpdate=[&](const std::string&details){if(!details.empty()&&runtimeReport.find(details)==std::string::npos){runtimeReport+=details+"\n";if(runtimeReport.size()>8000)runtimeReport=runtimeReport.substr(0,8000);}publishChat({uw::chat::Event::Runtime,runtimeReport});};runtimeReport+="Requested backend: "+narrow(uw::runtime::privatePath(path))+"\nBackend executable: "+narrow(uw::runtime::privatePath(child.runtime.backend.wstring()))+"\nRuntime pairing: "+std::string(runtimeReady?"complete pair in the same directory":"incomplete or failed preflight; chat disabled")+"\n";if(!child.runtime.host.empty())runtimeReport+="Host absolute override: "+narrow(uw::runtime::privatePath(child.runtime.host.wstring()))+"\nCODEX_CODE_MODE_HOST_PATH: set in child environment; inherited value replaced\n";else runtimeReport+="Host absolute override: unavailable; no replacement file found\n";if(child.hostProbe.attempted){runtimeReport+="Host preflight (--help, not a tool call): "+std::string(child.hostProbe.launched?"process created":"CreateProcess failed")+"\n";runtimeReport+="Host preflight stage: "+std::string(child.hostProbe.stage)+"\n";if(child.hostProbe.error)runtimeReport+="Host preflight Win32 error: "+std::to_string(child.hostProbe.error)+"\n";if(child.hostProbe.exited)runtimeReport+="Host preflight exit code: "+std::to_string(child.hostProbe.exitCode)+"\n";else if(child.hostProbe.launched&&!child.hostProbe.error)runtimeReport+="Host preflight: no exit within 1.5 seconds; probe stopped\n";}runtimeUpdate("");auto runtimeIssue=[&](const std::string&raw){auto failure=uw::runtime::startupFailure(raw);if(failure.empty())return;runtimeUpdate(failure);auto attempted=uw::runtime::attemptedHostPath(raw);if(!attempted.empty())runtimeUpdate("Backend attempted host: "+narrow(uw::runtime::privatePath(wide(attempted))));};
 if(!started){publish(false,path.empty()?L"Codex not found - open Settings":L"Cannot start Codex - check Settings",{},requestedQuotaRead.exchange(0));for(int i=0;i<50&&!stopWorker&&!wantRestart;i++){std::deque<ChatAction>actions;{std::lock_guard<std::mutex>lock(chatMutex);actions.swap(chatActions);}for(auto&a:actions){if(a.kind==0){chat.bind(a.thread,a.context);chat.submit(a.text,narrow(chatDirectory),GetTickCount64(),a.images);}else if(a.kind==5)chat.bind(a.thread,a.context);else if(a.kind==6)chat.erase(a.text,GetTickCount64());else if(a.kind==8)publishChat({uw::chat::Event::ResetResult,"unknown"});}flush(nullptr);Sleep(100);}wantRestart=false;continue;}
 child.send(R"({"id":1,"method":"initialize","params":{"clientInfo":{"name":"usage_widget","title":"Usage Widget","version":")" UW_VERSION R"("},"capabilities":{"experimentalApi":true}}})");
 uw::transport::Verification transportCheck(runHttps);bool initialized=false,pending=false,resetPending=false;ULONGLONG resetDeadline=0;int quotaId=1000;ULONGLONG deadline=GetTickCount64()+30000;uw::refresh::Schedule refreshSchedule;unsigned pendingRead=0;std::string buffer,stderrBuffer;wantRestart=false;
 while(!stopWorker&&!wantRestart){
 stderrBuffer+=child.readErrors();size_t errorEnd;while((errorEnd=stderrBuffer.find('\n'))!=std::string::npos){runtimeIssue(stderrBuffer.substr(0,errorEnd));stderrBuffer.erase(0,errorEnd+1);}if(stderrBuffer.size()>32768){runtimeIssue(stderrBuffer);stderrBuffer.clear();}
 if(WaitForSingleObject(child.process,0)==WAIT_OBJECT_0){publish(false,L"Codex exited - retrying");break;}
 DWORD available=0;if(!PeekNamedPipe(child.out,nullptr,0,nullptr,&available,nullptr)){publish(false,L"Connection closed - retrying");break;}
 if(available){char chunk[32768];DWORD n=0;if(!ReadFile(child.out,chunk,std::min<DWORD>(available,sizeof(chunk)),&n,nullptr)||!n)break;buffer.append(chunk,n);if(buffer.size()>4*1024*1024){publish(false,L"Invalid oversized server response");break;}
 size_t end;while((end=buffer.find('\n'))!=std::string::npos){auto response=buffer.substr(0,end);buffer.erase(0,end+1);try{auto root=uw::Parser(response).parse();const auto&id=root.at("id");
 if(id.type==uw::Json::Number&&id.number==1){if(root.has("error")){publish(false,L"Codex initialization failed");wantRestart=true;break;}initialized=true;transportCheck.begin(GetTickCount64());child.send(R"({"method":"initialized","params":{}})");child.send(R"({"id":4,"method":"config/read","params":{}})");wantRefresh=true;}
 else if(id.type==uw::Json::Number&&id.number==4){if(transportCheck.observe(root)){auto&effective=root.at("result").at("config");auto&codeMode=effective.at("features").at("code_mode");auto&enabled=codeMode.type==uw::Json::Boolean?codeMode:codeMode.at("enabled");runtimeUpdate(enabled.type==uw::Json::Boolean?(enabled.number?"Effective Code Mode: enabled despite requested override":"Effective Code Mode: disabled (config verified)"):"Effective Code Mode: backend did not report the feature setting");runtimeUpdate(transportCheck.reason());chat.configureIsolation(effective);chat.transportLabel=runHttps?"HTTPS/SSE; provider config verified":"Codex default; live search and isolated tools";chat.connected();}else{runtimeUpdate(transportCheck.reason());chat.transportLabel="Connection configuration rejected: "+transportCheck.reason();publishChat({uw::chat::Event::Error,transportCheck.reason()+(runHttps?". Check Connection settings or try Default (WebSocket).":". Check Connection settings and backend compatibility.")});}}
 else if(id.type==uw::Json::Number&&id.number==500){resetPending=false;if(root.has("error"))publishChat({uw::chat::Event::ResetResult,"unknown"});else{publishChat({uw::chat::Event::ResetResult,root.at("result").at("outcome").str});}}
 else if((pending&&id.type==uw::Json::Number&&id.number==quotaId)||root.at("method").str=="account/rateLimits/updated"){
 const bool reply = id.type == uw::Json::Number;
 const bool ok = !root.has("error") && (root.at("result").type == uw::Json::Object ||
                                      root.at("params").type == uw::Json::Object);
 const unsigned sequence = reply ? pendingRead : 0;
 if (ok) {
     auto credits = uw::data::credits(root);
     if (reply || credits.known) publishCredits(credits);
     auto q = uw::extract(root);
     publish(true, q.five.available || q.week.available ? L"Connected" :
         L"No 5h / weekly window provided", q, sequence);
 } else if (reply) {
     publish(false, L"Quota refresh failed. Check Codex sign-in or connection.", {}, sequence);
 }
 if (reply) {
     pending = false;
     pendingRead = 0;
     refreshSchedule.complete(GetTickCount64(), ok);
 }}
 else{auto recordError=[&](const uw::Json&error){runtimeIssue(error.type==uw::Json::String?error.str:error.at("message").str);};recordError(root.at("error"));recordError(root.at("params").at("error"));recordError(root.at("params").at("item").at("error"));auto method=root.at("method").str;if(method=="warning"||method=="configWarning")recordError(root.at("params"));const auto&item=root.at("params").at("item");if(item.at("type").str=="webSearch"&&(item.at("status").str=="failed"||item.at("status").str=="error"))runtimeUpdate("Web search: backend reported tool failure.");if(item.at("type").str=="dynamicToolCall"&&item.at("tool").str=="create_output"&&item.at("success").type==uw::Json::Boolean)runtimeUpdate(item.at("success").number?"Output tool: backend reported success.":"Output tool: backend reported failure.");if(item.at("type").str=="dynamicToolCall"&&item.at("success").type==uw::Json::Boolean&&!item.at("success").number)for(auto&content:item.at("contentItems").arr){runtimeIssue(content.at("text").str);}chat.handle(root,GetTickCount64());}
 }catch(...){if(!response.empty()&&response[0]=='{')chat.protocolProblem(GetTickCount64());}}}
 auto now=GetTickCount64();if(resetPending&&now>resetDeadline){resetPending=false;publishChat({uw::chat::Event::ResetResult,"unknown"});}if(initialized&&transportCheck.tick(now)){runtimeUpdate(transportCheck.reason());publishChat({uw::chat::Event::Error,transportCheck.reason()+". Restart the widget to retry configuration verification."});}if(!initialized&&now>deadline){publish(false,L"Codex startup timed out");break;}if(pending&&now>deadline){publish(false,L"Quota request timed out.",{},pendingRead);pending=false;pendingRead=0;refreshSchedule.complete(now,false);}
 {std::deque<ChatAction>actions;{std::lock_guard<std::mutex>lock(chatMutex);actions.swap(chatActions);}for(auto&a:actions){if(a.kind==0){if(!runtimeReady){publishChat({uw::chat::Event::Error,"A complete Codex runtime could not be started. Open Codex desktop, then use Detect desktop Codex. Search and file tools were not called.",a.context});continue;}if(!transportCheck.ready()){publishChat({uw::chat::Event::Error,transportCheck.reason()+". Check Connection settings.",a.context});continue;}if(chat.context!=a.context)chat.bind(a.thread,a.context);chat.select(a.model,a.effort);chat.submit(a.text,narrow(chatDirectory),now,a.images);}else if(a.kind==1)chat.stop(now);else if(a.kind==4)chat.refreshModels();else if(a.kind==5)chat.bind(a.thread,a.context);else if(a.kind==6)chat.erase(a.text,now);else if(a.kind==8&&!resetPending){child.send("{\"id\":500,\"method\":\"account/rateLimitResetCredit/consume\",\"params\":{\"idempotencyKey\":"+uw::chat::quote(a.text)+"}}");resetPending=true;resetDeadline=now+30000;}}
 chat.tick(now);flush(&child);if(chat.restart)wantRestart=true;
 if (initialized && !pending) {
     const bool requested = requestedQuotaRead.load() || wantRefresh.load();
     if (refreshSchedule.due(now, automaticQuotaSeconds.load(), requested)) {
         wantRefresh.exchange(false);
         pendingRead = requestedQuotaRead.exchange(0);
         ++quotaId;
         if (!child.send("{\"id\":" + std::to_string(quotaId) +
             ",\"method\":\"account/rateLimits/read\"}")) break;
         pending = true;
         deadline = now + 25000;
     }
 }
 }
 Sleep(30);}
 runtimeIssue(stderrBuffer+child.readErrors());if(pendingRead&&!stopWorker)publish(false,L"Codex connection closed.",{},pendingRead);if(resetPending){publishChat({uw::chat::Event::ResetResult,"unknown"});}chat.disconnected();flush(nullptr);if(!stopWorker)for(int i=0;i<20&&!stopWorker&&!wantRestart;i++)Sleep(100);
 }CoUninitialize();}

Color ink(){return dark?Color(238,242,248):Color(25,33,47);}Color muted(){return dark?Color(165,179,198):Color(101,114,134);}Color line(){return dark?Color(58,72,92):Color(220,227,236);}
void text(Graphics&g,const std::wstring&s,float x,float y,float size,Color c,float w=340){Font f(L"Segoe UI",size,FontStyleRegular,UnitPixel);SolidBrush b(c);StringFormat sf;sf.SetTrimming(StringTrimmingEllipsisCharacter);sf.SetFormatFlags(StringFormatFlagsNoWrap);g.DrawString(s.c_str(),-1,&f,RectF(x,y,w,size*1.6f),&sf,&b);}
void rounded(Graphics&g,RectF r,float radius,Color c){GraphicsPath p;p.AddArc(r.X,r.Y,radius*2,radius*2,180,90);p.AddArc(r.GetRight()-radius*2,r.Y,radius*2,radius*2,270,90);p.AddArc(r.GetRight()-radius*2,r.GetBottom()-radius*2,radius*2,radius*2,0,90);p.AddArc(r.X,r.GetBottom()-radius*2,radius*2,radius*2,90,90);p.CloseFigure();SolidBrush b(c);g.FillPath(&b,&p);}
std::wstring percent(const uw::Window&w){if(!w.available)return L"--";return std::to_wstring((int)std::round(w.remaining))+L"%";}
std::wstring resetText(const uw::Window&w){if(!w.available)return L"Not provided for this account";if(!w.resets)return L"Reset time unavailable";auto delta=w.resets-(int64_t)time(nullptr);if(delta<=0)return L"Reset due - waiting for refresh";int64_t d=delta/86400,h=delta/3600%24,m=delta/60%60;std::wstring out=L"Resets in ";if(d)out+=std::to_wstring(d)+L"d ";out+=std::to_wstring(h)+L"h "+std::to_wstring(m)+L"m";return out;}
void icon(Graphics&g,const std::wstring&s,float x,float y,float size=19){Font f(L"Segoe MDL2 Assets",size,FontStyleRegular,UnitPixel);SolidBrush b(muted());g.DrawString(s.c_str(),-1,&f,PointF(x,y),&b);}
float windowDpi(HWND h){return std::max(1.f,GetDpiForWindow(h)/96.f);}
visual::Layout currentLayout(HWND h){RECT r;GetClientRect(h,&r);float d=windowDpi(h);return visual::Layout(r.right/d,r.bottom/d,capsule);}
#include "chat_ui.hpp"
#include "update_ui.hpp"
void paintMain(HWND h){RECT r;GetClientRect(h,&r);if(r.right<=0||r.bottom<=0)return;visual::Surface surface(r.right,r.bottom);visual::State state;state.dark=dark;state.pinned=alwaysTop;state.fresh=fresh;state.capsule=capsule;state.opacity=opacity;state.chat=chatView;state.quota=quota;if(fresh){auto age=time(nullptr)-lastUpdate;state.footer=age<60?L"Updated just now":L"Updated "+std::to_wstring(age/60)+L"m ago";}else state.footer=status;visual::card(surface,windowDpi(h),state,(int64_t)time(nullptr));if(chatView&&!capsule)paintChat(surface,windowDpi(h));surface.present(h);}
void appearance(){if(mainWindow)InvalidateRect(mainWindow,nullptr,FALSE);if(settingsWindow)RedrawWindow(settingsWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);}
void toggleCapsule(){RECT r;GetWindowRect(mainWindow,&r);if(!capsule){expandedWidth=r.right-r.left;expandedHeight=r.bottom-r.top;}capsule=!capsule;float d=windowDpi(mainWindow);int width=capsule?(int)(280*d):std::clamp(expandedWidth,(int)(368*d),(int)(900*d)),height=capsule?(int)(80*d):std::clamp(expandedHeight,(int)((chatView?560:480)*d),(int)(1100*d));SetWindowPos(mainWindow,nullptr,0,0,width,height,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);appearance();save();}
void updateTray(){std::wstring tip=L"Usage: 5h "+percent(quota.five)+L" | Week "+percent(quota.week);if(!fresh)tip+=L" (offline / stale)";wcsncpy_s(tray.szTip,tip.c_str(),_TRUNCATE);Shell_NotifyIconW(NIM_MODIFY,&tray);InvalidateRect(taskWindow,nullptr,FALSE);}
void addTray(){tray.cbSize=sizeof(tray);tray.hWnd=mainWindow;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=WM_TRAY;tray.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));if(!tray.hIcon)tray.hIcon=LoadIconW(nullptr,IDI_APPLICATION);Shell_NotifyIconW(NIM_ADD,&tray);tray.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&tray);updateTray();}
void positionTask(){HWND bar=FindWindowW(L"Shell_TrayWnd",nullptr);RECT r;if(!bar||!GetWindowRect(bar,&r)||r.bottom-r.top>130||r.right-r.left<400){ShowWindow(taskWindow,SW_HIDE);return;}if(!taskEnabled||!IsWindowVisible(bar)){ShowWindow(taskWindow,SW_HIDE);return;}RECT screen;GetWindowRect(GetDesktopWindow(),&screen);if(r.top>=screen.bottom-2){ShowWindow(taskWindow,SW_HIDE);return;}HWND notify=FindWindowExW(bar,nullptr,L"TrayNotifyWnd",nullptr);RECT nr;int right=r.right-310;if(notify&&GetWindowRect(notify,&nr))right=nr.left-10;float dpi=GetDpiForWindow(bar)/96.f;NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof(metrics);SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0,GetDpiForWindow(bar));metrics.lfMessageFont.lfWeight=FW_NORMAL;metrics.lfMessageFont.lfHeight=-(int)(taskFontSize*dpi);HDC measure=GetDC(taskWindow);HFONT font=CreateFontIndirectW(&metrics.lfMessageFont);auto old=SelectObject(measure,font);auto label=L"5h "+percent(quota.five)+L" | Week "+percent(quota.week)+(fresh?L"":L" *");SIZE measured{};GetTextExtentPoint32W(measure,label.c_str(),(int)label.size(),&measured);SelectObject(measure,old);DeleteObject(font);ReleaseDC(taskWindow,measure);int width=measured.cx+(int)(16*dpi);int left=std::clamp(right-width+taskOffset,(int)r.left+5,(int)r.right-width-5);SetWindowPos(taskWindow,HWND_TOPMOST,left,r.top,width,r.bottom-r.top,SWP_NOACTIVATE|SWP_SHOWWINDOW);}
bool soundEnabled=true,alertFive=true,alertWeek=true;uw::ResetTracker resetFive,resetWeek;
std::wstring pref(const wchar_t*section,const wchar_t*key,const wchar_t*def=L""){wchar_t value[2048];GetPrivateProfileStringW(section,key,def,value,2048,configPath.c_str());return value;}
void notificationPrefs(){for(auto p:{std::pair<const wchar_t*,bool>{L"Sound",soundEnabled},{L"FiveHour",alertFive},{L"Week",alertWeek}})WritePrivateProfileStringW(L"Notifications",p.first,p.second?L"1":L"0",configPath.c_str());}
void loadNotificationPrefs(){
 // Remove data and the credential left by the retired 1.1.0 feature. No credential is read.
 if(GetPrivateProfileIntW(L"Migration",L"LocalSoundOnly",0,configPath.c_str())==0){WritePrivateProfileStringW(L"SMTP",nullptr,nullptr,configPath.c_str());WritePrivateProfileStringW(L"Notifications",L"Email",nullptr,configPath.c_str());CredDeleteW(L"UsageWidget/SMTP",CRED_TYPE_GENERIC,0);WritePrivateProfileStringW(L"Migration",L"LocalSoundOnly",L"1",configPath.c_str());}
 soundEnabled=GetPrivateProfileIntW(L"Notifications",L"Sound",1,configPath.c_str())!=0;alertFive=GetPrivateProfileIntW(L"Notifications",L"FiveHour",1,configPath.c_str())!=0;alertWeek=GetPrivateProfileIntW(L"Notifications",L"Week",1,configPath.c_str())!=0;
 for(auto entry:{std::pair<const wchar_t*,uw::ResetTracker*>{L"ResetFive",&resetFive},{L"ResetWeek",&resetWeek}}){try{auto&t=*entry.second;t.previous.resets=std::stoll(pref(entry.first,L"Deadline",L"0"));t.previous.remaining=std::stod(pref(entry.first,L"Remaining",L"0"));t.notifiedDeadline=std::stoll(pref(entry.first,L"Notified",L"0"));t.previous.available=t.previous.resets>0;}catch(...){*entry.second={};}}
}
void saveResetState(){for(auto e:{std::pair<const wchar_t*,uw::ResetTracker*>{L"ResetFive",&resetFive},{L"ResetWeek",&resetWeek}}){auto&t=*e.second;WritePrivateProfileStringW(e.first,L"Deadline",std::to_wstring(t.previous.resets).c_str(),configPath.c_str());WritePrivateProfileStringW(e.first,L"Remaining",std::to_wstring(t.previous.remaining).c_str(),configPath.c_str());WritePrivateProfileStringW(e.first,L"Notified",std::to_wstring(t.notifiedDeadline).c_str(),configPath.c_str());}}
bool startupEnabled(){DWORD type=0,n=0;return RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",L"UsageWidget",RRF_RT_REG_SZ,&type,nullptr,&n)==ERROR_SUCCESS;}
bool setStartup(bool enabled){HKEY key;auto rc=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);if(rc!=ERROR_SUCCESS)return false;if(enabled){wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);auto value=L"\""+std::wstring(exe)+L"\" --autostart";if(value.size()>259){RegCloseKey(key);return false;}rc=RegSetValueExW(key,L"UsageWidget",0,REG_SZ,(BYTE*)value.c_str(),(DWORD)((value.size()+1)*sizeof(wchar_t)));}else{rc=RegDeleteValueW(key,L"UsageWidget");if(rc==ERROR_FILE_NOT_FOUND)rc=ERROR_SUCCESS;}RegCloseKey(key);return rc==ERROR_SUCCESS;}
void playResetSound(){PlaySoundW(MAKEINTRESOURCEW(201),instance,SND_RESOURCE|SND_ASYNC|SND_NODEFAULT);}
void checkResets(const uw::Quota&q){auto now=(int64_t)time(nullptr);bool five=resetFive.observe(q.five,now),week=resetWeek.observe(q.week,now);saveResetState();five=five&&alertFive;week=week&&alertWeek;if((!five&&!week)||!soundEnabled)return;if(soundEnabled)playResetSound();std::wstring which=five&&week?L"5-hour and weekly":five?L"5-hour":L"Weekly";std::wstring title=which+L" quota reset";std::wstring body=L"Your "+which+L" quota has reset, confirmed by a fresh Codex quota reading.\r\n\r\nRemaining: 5h "+percent(q.five)+L" | Week "+percent(q.week)+L".\r\n\r\nUsage Widget";tray.uFlags=NIF_INFO;wcsncpy_s(tray.szInfoTitle,title.c_str(),_TRUNCATE);wcsncpy_s(tray.szInfo,body.c_str(),_TRUNCATE);tray.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;Shell_NotifyIconW(NIM_MODIFY,&tray);tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;}

void openDashboard(){ShellExecuteW(nullptr,L"open",L"https://chatgpt.com/codex/settings/usage",nullptr,nullptr,SW_SHOWNORMAL);}
void login(){std::wstring p;{std::lock_guard<std::mutex>lock(pathMutex);p=cliPath;}if(p.empty()){MessageBoxW(mainWindow,L"Open Codex desktop, then use Detect desktop Codex in Connection settings.",L"Codex required",MB_OK|MB_ICONINFORMATION);return;}if(p.size()>4&&_wcsicmp(p.substr(p.size()-4).c_str(),L".cmd")==0){wchar_t sys[MAX_PATH];GetSystemDirectoryW(sys,MAX_PATH);auto exe=std::wstring(sys)+L"\\cmd.exe";auto args=L"/d /s /k \"\""+p+L"\" login\"";ShellExecuteW(mainWindow,L"open",exe.c_str(),args.c_str(),nullptr,SW_SHOWNORMAL);}else ShellExecuteW(mainWindow,L"open",p.c_str(),L"login",nullptr,SW_SHOWNORMAL);wantRefresh=true;}
void showSettings();
void command(UINT id){switch(id){case ID_HTTPS:case ID_DEFAULT_TRANSPORT:setTransport(id==ID_HTTPS);break;case ID_CHAT_DIAGNOSTICS:clipboardText(chatDiagnostics+L"\n"+runtimeDiagnostics);break;case ID_CHAT:openChat();break;case ID_STARTUP:if(!setStartup(!startupEnabled()))MessageBoxW(mainWindow,L"Could not update the current-user startup entry.",L"Startup setting",MB_OK|MB_ICONINFORMATION);if(settingsWindow)InvalidateRect(settingsWindow,nullptr,FALSE);break;case ID_CAPSULE:toggleCapsule();break;case ID_LIGHT:autoTheme=false;dark=false;appearance();save();break;case ID_DARK:autoTheme=false;dark=true;appearance();save();break;case ID_DETECT:wantDetect=true;wantRestart=true;break;case ID_REFRESH:wantRefresh=true;break;case ID_LOGIN:login();break;case ID_THEME:autoTheme=false;dark=!dark;appearance();save();break;case ID_AUTO:autoTheme=true;dark=!systemLight();appearance();save();break;case ID_TOP:alwaysTop=!alwaysTop;SetWindowPos(mainWindow,alwaysTop?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);appearance();save();break;case ID_TASKBAR:taskEnabled=!taskEnabled;positionTask();save();break;case ID_RESET:taskOffset=0;positionTask();save();break;case ID_SHOW:ShowWindow(mainWindow,SW_SHOWNORMAL);SetForegroundWindow(mainWindow);break;case ID_SETTINGS:showSettings();break;case ID_DASHBOARD:openDashboard();break;case ID_QUIT:DestroyWindow(mainWindow);break;}}
void menu(HWND hwnd){HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING,ID_SHOW,L"Show widget");AppendMenuW(m,MF_STRING,ID_CAPSULE,capsule?L"Expand widget":L"Collapse to capsule");AppendMenuW(m,MF_STRING,ID_CHAT,L"Quick chat");AppendMenuW(m,MF_STRING,ID_CHAT_DIAGNOSTICS,L"Copy chat diagnostics");AppendMenuW(m,MF_STRING,ID_REFRESH,L"Refresh now");AppendMenuW(m,MF_STRING,ID_SETTINGS,L"Settings...");AppendMenuW(m,MF_STRING,ID_DETECT,L"Detect desktop Codex");AppendMenuW(m,MF_STRING,ID_LOGIN,L"Sign in with ChatGPT...");AppendMenuW(m,MF_STRING,ID_DASHBOARD,L"Open usage dashboard");AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING|(alwaysTop?MF_CHECKED:0),ID_TOP,L"Always on top");AppendMenuW(m,MF_STRING|(taskEnabled?MF_CHECKED:0),ID_TASKBAR,L"Taskbar readout");AppendMenuW(m,MF_STRING,ID_RESET,L"Reset taskbar position");AppendMenuW(m,MF_STRING|(startupEnabled()?MF_CHECKED:0),ID_STARTUP,L"Start with Windows");AppendMenuW(m,MF_STRING,ID_THEME,L"Switch light / dark");AppendMenuW(m,MF_STRING|(autoTheme?MF_CHECKED:0),ID_AUTO,L"Follow system theme");AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,ID_QUIT,L"Exit");POINT p;GetCursorPos(&p);SetForegroundWindow(mainWindow);UINT id=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);DestroyMenu(m);if(id)command(id);PostMessageW(mainWindow,WM_NULL,0,0);}
LRESULT CALLBACK TaskProc(HWND h,UINT msg,WPARAM w,LPARAM l){static POINT initial;static int oldOffset;static bool dragging=false,moved=false;switch(msg){case WM_ERASEBKGND:return 1;case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);RECT r;GetClientRect(h,&r);if(r.right<=0||r.bottom<=0)return 0;visual::Surface surface(r.right,r.bottom);auto label=L"5h "+percent(quota.five)+L" | Week "+percent(quota.week)+(fresh?L"":L" *");UINT dp=GetDpiForWindow(h);NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof(metrics);SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0,dp);float dpi=dp/96.f;std::wstring family=metrics.lfMessageFont.lfFaceName;if(family.empty())family=L"Segoe UI";visual::taskbar(surface,dpi,label,taskLight(),family,(float)taskFontSize);surface.present(h);return 0;}case WM_SIZE:InvalidateRect(h,nullptr,FALSE);return 0;case WM_LBUTTONDOWN:GetCursorPos(&initial);oldOffset=taskOffset;dragging=true;moved=false;SetCapture(h);return 0;case WM_MOUSEMOVE:if(dragging){POINT p;GetCursorPos(&p);if(abs(p.x-initial.x)>3)moved=true;if(moved){taskOffset=oldOffset+p.x-initial.x;positionTask();}}return 0;case WM_LBUTTONUP:if(dragging){dragging=false;ReleaseCapture();save();if(!moved){ShowWindow(mainWindow,IsWindowVisible(mainWindow)?SW_HIDE:SW_SHOW);if(IsWindowVisible(mainWindow))SetForegroundWindow(mainWindow);}}return 0;case WM_CONTEXTMENU:menu(h);return 0;}return DefWindowProcW(h,msg,w,l);}
#include "settings_ui.hpp"
LRESULT CALLBACK MainProc(HWND h,UINT msg,WPARAM w,LPARAM l){if(msg==taskbarCreated){addTray();positionTask();return 0;}switch(msg){case WM_ERASEBKGND:return 1;case WM_NCCALCSIZE:return 0;case WM_NCACTIVATE:return TRUE;case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);paintMain(h);return 0;}case WM_GETMINMAXINFO:{auto*m=(MINMAXINFO*)l;float d=windowDpi(h);m->ptMinTrackSize={(LONG)((capsule?280:chatView?440:368)*d),(LONG)((capsule?80:chatView?620:480)*d)};m->ptMaxTrackSize={(LONG)(900*d),(LONG)((capsule?80:1100)*d)};return 0;}
case WM_NCHITTEST:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);float d=windowDpi(h),x=p.x/d,y=p.y/d;auto layout=currentLayout(h);auto&body=layout.body;float edge=6;if(x<body.X-edge||x>body.GetRight()+edge||y<body.Y-edge||y>body.GetBottom()+edge)return HTTRANSPARENT;
if(!capsule){bool left=x<body.X+edge,right=x>body.GetRight()-edge,top=y<body.Y+edge,bottom=y>body.GetBottom()-edge;if(left)return top?HTTOPLEFT:bottom?HTBOTTOMLEFT:HTLEFT;if(right)return top?HTTOPRIGHT:bottom?HTBOTTOMRIGHT:HTRIGHT;if(top)return HTTOP;if(bottom)return HTBOTTOM;}
if(!capsule&&(layout.tabs.Contains(x,y)||layout.settings.Contains(x,y)||layout.light.Contains(x,y)||layout.dark.Contains(x,y)||layout.pin.Contains(x,y)||layout.refresh.Contains(x,y)))return HTCLIENT;
if(capsule||y<body.Y+65){return HTCAPTION;}
return HTCLIENT;}
case WM_LBUTTONDBLCLK:if(chatView&&!capsule){chatClick(GET_X_LPARAM(l)/windowDpi(h),GET_Y_LPARAM(l)/windowDpi(h));return 0;}command(ID_CAPSULE);return 0;case WM_NCLBUTTONDBLCLK:if(w==HTCAPTION){command(ID_CAPSULE);return 0;}break;
case WM_LBUTTONUP:{if(chatSelecting){chatPointerUp(GET_X_LPARAM(l)/windowDpi(h),GET_Y_LPARAM(l)/windowDpi(h));return 0;}float d=windowDpi(h),x=GET_X_LPARAM(l)/d,y=GET_Y_LPARAM(l)/d;auto layout=currentLayout(h);if(!capsule&&layout.chatTab.Contains(x,y)){openChat();return 0;}if(!capsule&&layout.usage.Contains(x,y)){showUsage();return 0;}if(chatView&&!capsule&&chatClick(x,y))return 0;if(!capsule&&layout.settings.Contains(x,y))showSettings();else if(!capsule&&layout.light.Contains(x,y))command(ID_LIGHT);else if(!capsule&&layout.dark.Contains(x,y))command(ID_DARK);else if(!capsule&&layout.pin.Contains(x,y))command(ID_TOP);else if(!capsule&&layout.refresh.Contains(x,y))command(ID_REFRESH);return 0;}
case WM_LBUTTONDOWN:if(chatView&&!capsule){SetFocus(h);chatPointerDown(GET_X_LPARAM(l)/windowDpi(h),GET_Y_LPARAM(l)/windowDpi(h));}return 0;
case WM_MOUSEMOVE:if(chatSelecting){chatPointerMove(GET_X_LPARAM(l)/windowDpi(h),GET_Y_LPARAM(l)/windowDpi(h));return 0;}break;
case WM_DROPFILES:{HDROP drop=(HDROP)w;openChat();UINT count=DragQueryFileW(drop,0xffffffff,nullptr,0);for(UINT i=0;i<count;i++){wchar_t path[32768];DragQueryFileW(drop,i,path,32768);attachFile(path);}DragFinish(drop);return 0;}
case WM_KEYDOWN:if(chatView&&!capsule&&chatKey(w))return 0;break;
case WM_CHAR:if(chatView&&!capsule){chatChar((wchar_t)w);return 0;}break;
case WM_IME_STARTCOMPOSITION:chatComposing=true;positionIme();break;
case WM_IME_COMPOSITION:if(chatView&&!capsule){chatIme(l);return 0;}break;
case WM_IME_ENDCOMPOSITION:chatComposing=false;chatComposition.clear();InvalidateRect(h,nullptr,FALSE);break;
case WM_MOUSEWHEEL:if(chatView&&!capsule){chatScroll=std::clamp(chatScroll-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*65.f,0.f,chatMaxScroll);chatAutoScroll=chatScroll>=chatMaxScroll-2;InvalidateRect(h,nullptr,FALSE);return 0;}break;
case WM_CHAT:{auto*e=(uw::chat::Event*)l;receiveChat(*e);delete e;InvalidateRect(h,nullptr,FALSE);return 0;}
case WM_EXITSIZEMOVE:save();return 0;case WM_SIZE:InvalidateRect(h,nullptr,FALSE);return 0;case WM_DPICHANGED:{auto*r=(RECT*)l;SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
 case WM_CREDITS:{auto*p=(uw::data::Credits*)l;resetCredits=*p;creditsUpdated=time(nullptr);delete p;if(settingsWindow)InvalidateRect(settingsWindow,nullptr,FALSE);return 0;}
 case WM_APP_UPDATE: {
     std::unique_ptr<AppUpdateResult> result(reinterpret_cast<AppUpdateResult*>(l));
     receiveAppUpdate(*result);
     return 0;
 }
 case WM_DATA: {
     std::unique_ptr<Update> result(reinterpret_cast<Update*>(l));
     fresh = result->ok;
     status = result->message;
     if (result->ok) {
         checkResets(result->q);
         quota = result->q;
         lastUpdate = time(nullptr);
     }
     if (activeQuotaRead && result->readSequence == activeQuotaRead) {
         activeQuotaRead = 0;
         quotaReadMessage = result->ok ? L"Data refreshed." : result->message;
         bool confirmReset = confirmResetAfterRead;
         confirmResetAfterRead = false;
         if (confirmReset) {
             if (result->ok) confirmNewReset();
             else resetMessage = L"Could not verify available reset credits.";
         }
     }
     repaintSettings();
     InvalidateRect(h,nullptr,FALSE);
     updateTray();
     return 0;
 }
 case WM_TIMER:if(historyDirty)saveCurrent();if(fresh&&lastUpdate&&time(nullptr)-lastUpdate>uw::refresh::staleAfter(automaticQuotaSeconds.load())){fresh=false;status=L"Quota data is stale";}if(activeQuotaRead&&GetTickCount64()>=quotaReadDeadline){activeQuotaRead=0;confirmResetAfterRead=false;quotaReadMessage=L"Refresh timed out. Check the Codex connection.";repaintSettings();}tickUpdates();if(autoTheme&&dark==systemLight()){dark=!systemLight();appearance();}InvalidateRect(h,nullptr,FALSE);positionTask();updateTray();return 0;
 case WM_SETTINGCHANGE:if(autoTheme){dark=!systemLight();appearance();}positionTask();return 0;case WM_CONTEXTMENU:menu(h);return 0;case WM_COMMAND:command(LOWORD(w));return 0;
 case WM_TRAY:if(LOWORD(l)==NIN_SELECT||LOWORD(l)==WM_LBUTTONUP)command(ID_SHOW);else if(LOWORD(l)==WM_CONTEXTMENU||LOWORD(l)==WM_RBUTTONUP)menu(h);return 0;
 case WM_CLOSE:ShowWindow(h,SW_HIDE);return 0;case WM_DESTROY:saveCurrent();save();KillTimer(h,1);stopWorker=true;PlaySoundW(nullptr,nullptr,0);Shell_NotifyIconW(NIM_DELETE,&tray);if(settingsWindow)DestroyWindow(settingsWindow);if(taskWindow)DestroyWindow(taskWindow);PostQuitMessage(0);return 0;}return DefWindowProcW(h,msg,w,l);}
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR args,int){instance=hi;if(args&&wcscmp(args,L"--cleanup")==0){HANDLE running=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\UsageWidget_1_0");if(running){CloseHandle(running);return 2;}setStartup(false);CredDeleteW(L"UsageWidget/SMTP",CRED_TYPE_GENERIC,0);return 0;}CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\UsageWidget_1_0");if(GetLastError()==ERROR_ALREADY_EXISTS){HWND other=FindWindowW(L"UsageWidget",nullptr);if(other){ShowWindow(other,SW_SHOW);SetForegroundWindow(other);}if(singleton)CloseHandle(singleton);return 0;}GdiplusStartupInput gi;GdiplusStartup(&graphicsToken,&gi,nullptr);INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_BAR_CLASSES};InitCommonControlsEx(&ic);settingsFont=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 wchar_t app[32768];GetEnvironmentVariableW(L"LOCALAPPDATA",app,32768);std::wstring dir=std::wstring(app)+L"\\UsageWidget";CreateDirectoryW(dir.c_str(),nullptr);configPath=dir+L"\\settings.ini";chatDirectory=dir+L"\\chat-workspace";CreateDirectoryW(chatDirectory.c_str(),nullptr);dataDir=dir;loadQuotaPrefs();loadUpdatePrefs();loadNotificationPrefs();httpsMode=GetPrivateProfileIntW(L"Connection",L"HttpsMode",1,configPath.c_str())!=0;taskFontSize=std::clamp((int)GetPrivateProfileIntW(L"Appearance",L"TaskFontSize",14,configPath.c_str()),12,22);if(GetPrivateProfileIntW(L"Appearance",L"FontSizingVersion",0,configPath.c_str())<4){taskFontSize=14;WritePrivateProfileStringW(L"Appearance",L"FontSizingVersion",L"4",configPath.c_str());WritePrivateProfileStringW(L"Appearance",L"TaskFontSize",std::to_wstring(taskFontSize).c_str(),configPath.c_str());}dark=GetPrivateProfileIntW(L"Appearance",L"Dark",0,configPath.c_str())!=0;autoTheme=GetPrivateProfileIntW(L"Appearance",L"Auto",1,configPath.c_str())!=0;if(autoTheme)dark=!systemLight();opacity=std::clamp((int)GetPrivateProfileIntW(L"Appearance",L"Opacity",95,configPath.c_str()),25,100);alwaysTop=GetPrivateProfileIntW(L"Window",L"Topmost",1,configPath.c_str())!=0;taskEnabled=GetPrivateProfileIntW(L"Window",L"Taskbar",1,configPath.c_str())!=0;taskOffset=(int)GetPrivateProfileIntW(L"Window",L"TaskOffset",0,configPath.c_str());wchar_t path[32768];GetPrivateProfileStringW(L"Connection",L"CodexPath",L"",path,32768,configPath.c_str());cliPath=path;if(cliPath.empty()||GetFileAttributesW(cliPath.c_str())==INVALID_FILE_ATTRIBUTES)cliPath=findCli();if(!cliPath.empty())wantDetect=false;
 WNDCLASSW wc{};wc.style=CS_DBLCLKS;wc.hInstance=hi;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"UsageWidget";wc.lpfnWndProc=MainProc;wc.hIcon=LoadIconW(hi,MAKEINTRESOURCEW(1));RegisterClassW(&wc);wc.lpszClassName=L"UsageTaskbar";wc.lpfnWndProc=TaskProc;RegisterClassW(&wc);wc.lpszClassName=L"UsageSettings";wc.lpfnWndProc=SettingsProc;RegisterClassW(&wc);
 int x=(int)GetPrivateProfileIntW(L"Window",L"X",100,configPath.c_str()),y=(int)GetPrivateProfileIntW(L"Window",L"Y",100,configPath.c_str()),width=std::clamp((int)GetPrivateProfileIntW(L"Window",L"W",420,configPath.c_str()),(int)(368*GetDpiForSystem()/96.f),(int)(900*GetDpiForSystem()/96.f)),height=std::clamp((int)GetPrivateProfileIntW(L"Window",L"H",480,configPath.c_str()),(int)(480*GetDpiForSystem()/96.f),(int)(1100*GetDpiForSystem()/96.f));expandedWidth=width;expandedHeight=height;capsule=GetPrivateProfileIntW(L"Window",L"Capsule",0,configPath.c_str())!=0;if(capsule){width=(int)(280*GetDpiForSystem()/96.f);height=(int)(80*GetDpiForSystem()/96.f);}RECT candidate{x,y,x+width,y+height};if(!MonitorFromRect(&candidate,MONITOR_DEFAULTTONULL)){x=y=100;}
 mainWindow=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|(alwaysTop?WS_EX_TOPMOST:0),L"UsageWidget",L"Usage",WS_POPUP|WS_THICKFRAME,x,y,width,height,nullptr,nullptr,hi,nullptr);if(!mainWindow)return 1;taskWindow=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,L"UsageTaskbar",L"Usage readout",WS_POPUP,0,0,205,48,nullptr,nullptr,hi,nullptr);taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");initializeHistory();restoreLastConversation();DragAcceptFiles(mainWindow,TRUE);appearance();addTray();positionTask();SetTimer(mainWindow,1,1000,nullptr);ShowWindow(mainWindow,SW_SHOW);worker=std::thread(backend);MSG m;while(GetMessageW(&m,nullptr,0,0)>0){if(settingsWindow&&IsDialogMessageW(settingsWindow,&m))continue;TranslateMessage(&m);DispatchMessageW(&m);}stopWorker=true;if(worker.joinable())worker.join();if(updateWorker.joinable())updateWorker.join();if(installerProcess)CloseHandle(installerProcess);downloadedInstaller.reset();while(PeekMessageW(&m,nullptr,WM_APP_UPDATE,WM_APP_UPDATE,PM_REMOVE))delete reinterpret_cast<AppUpdateResult*>(m.lParam);for(auto&t:imageWorkers)if(t.joinable())t.join();while(PeekMessageW(&m,nullptr,WM_DATA,WM_DATA,PM_REMOVE))delete(Update*)m.lParam;while(PeekMessageW(&m,nullptr,WM_CHAT,WM_CHAT,PM_REMOVE))delete(uw::chat::Event*)m.lParam;while(PeekMessageW(&m,nullptr,WM_CREDITS,WM_CREDITS,PM_REMOVE))delete(uw::data::Credits*)m.lParam;imageCache.clear();if(settingsFont)DeleteObject(settingsFont);GdiplusShutdown(graphicsToken);CoUninitialize();if(singleton)CloseHandle(singleton);return 0;}
