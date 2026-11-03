#pragma once
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cwctype>
#include <cctype>
#include <atomic>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace uw::runtime {
namespace fs=std::filesystem;
inline std::wstring fold(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return (wchar_t)std::towlower(c);});return s;}
inline std::wstring envKey(const std::wstring&s){auto at=s.find(L'=',s.empty()||s[0]!=L'='?0:1);return fold(s.substr(0,at));}
inline std::vector<wchar_t> environment(const std::vector<std::wstring>&in,const std::vector<std::wstring>&dirs,const std::wstring&host=L""){std::vector<std::wstring>entries;std::wstring path;for(auto&s:in){if(!host.empty()&&envKey(s)==L"codex_code_mode_host_path")continue;if(envKey(s)==L"path"){auto at=s.find(L'=');if(at!=std::wstring::npos)path=s.substr(at+1);}else entries.push_back(s);}std::wstring prefix;std::vector<std::wstring>seen;for(auto&dir:dirs){if(dir.empty()||dir.find_first_of(L";\r\n")!=std::wstring::npos)continue;auto name=fold(dir);if(std::find(seen.begin(),seen.end(),name)!=seen.end())continue;seen.push_back(name);prefix+=dir+L";";}entries.push_back(L"PATH="+prefix+path);if(!host.empty())entries.push_back(L"CODEX_CODE_MODE_HOST_PATH="+host);std::sort(entries.begin(),entries.end(),[](auto&a,auto&b){return fold(a)<fold(b);});std::vector<wchar_t>block;for(auto&s:entries){block.insert(block.end(),s.begin(),s.end());block.push_back(0);}block.push_back(0);if(block.size()<2)block.push_back(0);return block;}
inline unsigned peMachine(const fs::path&p){std::ifstream f(p,std::ios::binary);unsigned char header[64]{};if(!f.read((char*)header,64)||header[0]!='M'||header[1]!='Z')return 0;unsigned offset=(unsigned)header[60]|((unsigned)header[61]<<8)|((unsigned)header[62]<<16)|((unsigned)header[63]<<24);if(offset>1024*1024)return 0;f.seekg(offset);unsigned char pe[6]{};if(!f.read((char*)pe,6)||pe[0]!='P'||pe[1]!='E'||pe[2]||pe[3])return 0;return pe[4]|(pe[5]<<8);}
struct Layout {fs::path backend,backendDir,host,root;int examined=0;};
inline bool redirected(const fs::path&p){
#ifdef _WIN32
 auto attr=GetFileAttributesW(p.c_str());return attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_REPARSE_POINT);
#else
 std::error_code ec;return fs::is_symlink(p,ec);
#endif
}
inline bool regular(const fs::path&p){std::error_code ec;return fs::is_regular_file(p,ec)&&!ec;}
inline bool completePair(const fs::path&backend){auto host=backend.parent_path()/L"codex-code-mode-host.exe";auto machine=peMachine(backend);return machine&&regular(backend)&&regular(host)&&!redirected(backend)&&!redirected(host)&&peMachine(host)==machine;}
inline Layout locate(const fs::path&requested,const std::atomic<bool>*stop=nullptr){Layout result;result.backend=requested;result.backendDir=requested.parent_path();result.root=result.backendDir;auto machine=peMachine(requested);if(completePair(requested)){result.host=result.backendDir/L"codex-code-mode-host.exe";return result;}
 auto cursor=result.backendDir;fs::path binRoot;bool resourcesFound=false;for(int i=0;i<8&&!cursor.empty();i++){auto name=fold(cursor.filename().wstring());if(name==L"resources"){result.root=cursor.parent_path();resourcesFound=true;break;}if(name==L"bin"&&binRoot.empty())binRoot=cursor.parent_path();auto parent=cursor.parent_path();if(parent==cursor)break;cursor=parent;}
 if(!resourcesFound&&!binRoot.empty()){result.root=binRoot;}
 std::vector<fs::path>pairs;std::error_code ec;fs::recursive_directory_iterator it(result.root,fs::directory_options::skip_permission_denied,ec),end;while(!ec&&it!=end&&result.examined++<5000&&!(stop&&stop->load())){auto candidate=it->path();std::error_code statusError;bool link=it->is_symlink(statusError)||redirected(candidate);if(it.depth()>=6||link)it.disable_recursion_pending();auto name=fold(candidate.filename().wstring());if(!link&&(name==L"codex.exe"||name==L"codex-cli.exe")&&completePair(candidate)&&(!machine||peMachine(candidate)==machine))pairs.push_back(candidate);it.increment(ec);}
 if(stop&&stop->load()){return result;}
 std::sort(pairs.begin(),pairs.end(),[](const fs::path&a,const fs::path&b){std::error_code ea,eb;auto ta=fs::last_write_time(a,ea),tb=fs::last_write_time(b,eb);if(!ea&&!eb&&ta!=tb)return ta>tb;return a.native()<b.native();});if(!pairs.empty()){result.backend=pairs.front();result.backendDir=result.backend.parent_path();result.host=result.backendDir/L"codex-code-mode-host.exe";}return result;}
inline std::vector<std::string> overrides(){return {"features.code_mode.enabled=false"};}
inline std::string attemptedHostPath(const std::string&s){auto low=s;std::transform(low.begin(),low.end(),low.begin(),[](unsigned char c){return (char)std::tolower(c);});const std::string filename="codex-code-mode-host.exe";auto end=low.find(filename);if(end==std::string::npos)return "";auto slash=s.rfind(":\\",end),forward=s.rfind(":/",end);auto colon=slash==std::string::npos?forward:forward==std::string::npos?slash:std::max(slash,forward);if(colon==std::string::npos||colon==0||!std::isalpha((unsigned char)s[colon-1]))return "";auto path=s.substr(colon-1,end+filename.size()-colon+1);if(path.size()>2048||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<32;}))return "";return path;}
// Classify only actual backend error fields/stderr. Never persist arbitrary text.
inline std::string startupFailure(const std::string&s){std::string low=s;std::transform(low.begin(),low.end(),low.begin(),[](unsigned char c){return (char)std::tolower(c);});if(low.find("codex-code-mode-host")==std::string::npos)return "";if(low.find("os error 2")!=std::string::npos||low.find("error=2")!=std::string::npos||low.find("error 2)")!=std::string::npos||low.find("not found")!=std::string::npos||low.find("cannot find")!=std::string::npos)return "Code Mode host launch failed: file/path not found (OS error 2).";if(low.find("os error 5")!=std::string::npos||low.find("access is denied")!=std::string::npos)return "Code Mode host launch failed: access denied (OS error 5).";if(low.find("failed")!=std::string::npos||low.find("error")!=std::string::npos)return "Code Mode host reported a runtime error.";return "";}
}
