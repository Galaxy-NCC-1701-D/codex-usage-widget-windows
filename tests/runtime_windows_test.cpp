// Native Windows-only integration check. Run after compiling this test on Windows.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../src/backend_runtime.hpp"
#include <cassert>
#include <iostream>
std::wstring envPath(){DWORD n=GetEnvironmentVariableW(L"PATH",nullptr,0);std::wstring result(n,L'\0');if(n){GetEnvironmentVariableW(L"PATH",result.data(),n);result.resize(wcslen(result.c_str()));}return result;}
int wmain(int argc,wchar_t**argv){using namespace uw::runtime;if(argc==2&&std::wstring(argv[1])==L"--help")return 0;if(argc==2&&std::wstring(argv[1])==L"--probe"){wchar_t self[32768];GetModuleFileNameW(nullptr,self,32768);auto dir=fs::path(self).parent_path().wstring();wchar_t explicitHost[32768];if(!GetEnvironmentVariableW(L"CODEX_CODE_MODE_HOST_PATH",explicitHost,32768)||_wcsicmp(self,explicitHost)!=0)return 8;return envPath().rfind(dir+L";",0)==0?0:7;}
 wchar_t temp[32768],self[32768],system[32768];GetTempPathW(32768,temp);GetModuleFileNameW(nullptr,self,32768);GetSystemDirectoryW(system,32768);auto root=fs::path(temp)/(L"Usage Widget runtime test "+std::to_wstring(GetCurrentProcessId()));fs::create_directories(root/L"resources"/L"bin");fs::create_directories(root/L"resources"/L"runtime");auto backend=root/L"resources"/L"bin"/L"codex.exe",host=root/L"resources"/L"runtime"/L"codex-code-mode-host.exe";assert(CopyFileW(self,backend.c_str(),TRUE));assert(CopyFileW(self,host.c_str(),TRUE));auto pairedBackend=host.parent_path()/L"codex.exe";assert(CopyFileW(self,pairedBackend.c_str(),TRUE));auto before=envPath();auto layout=locate(backend);assert(layout.host==host&&layout.backend==pairedBackend&&layout.backendDir==host.parent_path());auto block=childEnvironment(layout);auto probe=probeHost(layout,block);assert(probe.attempted&&probe.launched&&probe.exited&&probe.exitCode==0&&!probe.error);auto exe=fs::path(system)/L"cmd.exe";std::wstring command=L"\""+exe.wstring()+L"\" /d /c codex-code-mode-host.exe --probe";STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};assert(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,block.data(),layout.backendDir.c_str(),&si,&pi));assert(WaitForSingleObject(pi.hProcess,10000)==WAIT_OBJECT_0);DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);assert(code==0);assert(envPath()==before);fs::remove_all(root);std::cout<<"PASS native Windows companion spawn by child-only PATH with unchanged parent environment\n";return 0;}
