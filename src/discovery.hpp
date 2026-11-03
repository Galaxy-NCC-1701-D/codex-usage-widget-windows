#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <atomic>
#include "runtime_paths.hpp"
namespace discovery {
inline std::wstring lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return (wchar_t)towlower(c);});return s;}
inline bool candidate(const std::wstring&p){auto s=lower(p);auto pos=s.find_last_of(L"\\/");auto name=s.substr(pos==std::wstring::npos?0:pos+1);return (name==L"codex.exe"||name==L"codex-cli.exe")&&(s.find(L"\\resources\\")!=std::wstring::npos||s.find(L"\\bin\\")!=std::wstring::npos);}
inline std::string capture(const std::wstring&exe,const std::wstring&args,DWORD timeout,std::atomic<bool>&stop){
 HANDLE read=nullptr,write=nullptr,input=nullptr,job=nullptr;SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};if(!CreatePipe(&read,&write,&sa,0))return "";SetHandleInformation(read,HANDLE_FLAG_INHERIT,0);input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
 STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=input;si.hStdOutput=write;si.hStdError=write;PROCESS_INFORMATION pi{};std::wstring command=L"\""+exe+L"\" "+args;
 job=CreateJobObjectW(nullptr,nullptr);if(job){JOBOBJECT_EXTENDED_LIMIT_INFORMATION j{};j.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;SetInformationJobObject(job,JobObjectExtendedLimitInformation,&j,sizeof(j));}
 BOOL started=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&si,&pi);CloseHandle(write);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
 if(!started){CloseHandle(read);if(job)CloseHandle(job);return "";}if(!job||!AssignProcessToJobObject(job,pi.hProcess)){TerminateProcess(pi.hProcess,1);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);CloseHandle(read);if(job)CloseHandle(job);return "";}ResumeThread(pi.hThread);CloseHandle(pi.hThread);
 std::string out;ULONGLONG end=GetTickCount64()+timeout;bool done=false;
 while(!stop&&GetTickCount64()<end){DWORD available=0;if(PeekNamedPipe(read,nullptr,0,nullptr,&available,nullptr)&&available){char buf[4096];DWORD n=0;if(ReadFile(read,buf,std::min<DWORD>(available,sizeof(buf)),&n,nullptr))out.append(buf,n);if(out.size()>256*1024)break;continue;}if(WaitForSingleObject(pi.hProcess,0)==WAIT_OBJECT_0){done=true;break;}Sleep(30);}
 DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(job);WaitForSingleObject(pi.hProcess,300);CloseHandle(pi.hProcess);CloseHandle(read);return done&&code==0?out:"";
}
inline bool supports(const std::wstring&p,std::atomic<bool>&stop){auto out=capture(p,L"--help",3000,stop);return out.find("app-server")!=std::string::npos;}
inline void scan(const std::wstring&dir,int depth,std::vector<std::wstring>&paths,int&budget,std::atomic<bool>&stop){if(depth<0||budget<=0||stop||paths.size()>=12)return;WIN32_FIND_DATAW fd;HANDLE h=FindFirstFileW((dir+L"\\*").c_str(),&fd);if(h==INVALID_HANDLE_VALUE)return;do{if(--budget<=0||stop)break;std::wstring n=fd.cFileName;if(n==L"."||n==L"..")continue;auto p=dir+L"\\"+n;if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))scan(p,depth-1,paths,budget,stop);}else if(candidate(p))paths.push_back(p);}while(paths.size()<12&&FindNextFileW(h,&fd));FindClose(h);}
inline std::wstring desktop(std::atomic<bool>&stop,std::wstring&report){
 report=L"Usage Widget desktop discovery\r\nPackage: OpenAI.Codex (current Windows user only)\r\n\r\n";std::vector<std::wstring> paths;std::set<std::wstring>seen;std::wstring incomplete;
 HANDLE shot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(shot!=INVALID_HANDLE_VALUE){PROCESSENTRY32W pe{};pe.dwSize=sizeof(pe);if(Process32FirstW(shot,&pe))do{if(stop)break;auto n=lower(pe.szExeFile);if(n!=L"codex.exe"&&n!=L"codex-cli.exe")continue;HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pe.th32ProcessID);if(p){wchar_t buf[32768];DWORD size=32768;if(QueryFullProcessImageNameW(p,0,buf,&size)&&candidate(buf))paths.emplace_back(buf);CloseHandle(p);}}while(Process32NextW(shot,&pe));CloseHandle(shot);}
 for(auto&p:paths)if(seen.insert(lower(p)).second){report+=L"Running backend candidate: "+p+L"\r\n";if(supports(p,stop)){if(uw::runtime::completePair(std::filesystem::path(p))){report+=L"Verified: app-server with co-located matching-architecture host.\r\n";return p;}if(incomplete.empty())incomplete=p;report+=L"Backend verified but companion missing beside it; continuing pair discovery.\r\n";continue;}}
 wchar_t sys[MAX_PATH];GetSystemDirectoryW(sys,MAX_PATH);auto ps=std::wstring(sys)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
 const std::wstring args=L"-NoLogo -NoProfile -NonInteractive -Command \"[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false); $ErrorActionPreference='Stop'; Get-AppxPackage -Name OpenAI.Codex | Sort-Object Version -Descending | ForEach-Object { $_.InstallLocation }\"";
 auto output=capture(ps,args,12000,stop);if(output.size()>=3&&(unsigned char)output[0]==0xef&&(unsigned char)output[1]==0xbb&&(unsigned char)output[2]==0xbf)output.erase(0,3);
 size_t pos=0;int budget=4000;paths.clear();while(pos<output.size()&&!stop){auto e=output.find('\n',pos);auto s=output.substr(pos,e==std::string::npos?std::string::npos:e-pos);if(!s.empty()&&s.back()=='\r')s.pop_back();pos=e==std::string::npos?output.size():e+1;int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);if(!n)continue;std::wstring root(n,L' ');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),root.data(),n);DWORD attr=GetFileAttributesW(root.c_str());if(attr==INVALID_FILE_ATTRIBUTES||!(attr&FILE_ATTRIBUTE_DIRECTORY))continue;report+=L"Package location: "+root+L"\r\n";scan(root,8,paths,budget,stop);}
 for(auto&p:paths)if(seen.insert(lower(p)).second){report+=L"Bundled backend candidate: "+p+L"\r\n";if(supports(p,stop)){if(uw::runtime::completePair(std::filesystem::path(p))){report+=L"Verified: app-server with co-located matching-architecture host.\r\n";return p;}if(incomplete.empty())incomplete=p;report+=L"Backend verified but companion missing beside it; continuing pair discovery.\r\n";continue;}report+=L"Not verified, inaccessible, or help probe timed out.\r\n";}
 if(!incomplete.empty()&&!stop){report+=L"No complete pair found; retaining backend for quota-only access.\r\n";return incomplete;}report+=L"No usable bundled backend found. Open Codex desktop and try Detect desktop Codex again.\r\nNo WindowsApps permissions were changed. No credentials were read.\r\n";return L"";
}
}
