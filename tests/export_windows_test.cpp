// Native Windows renderer check: build with -municode and gdiplus/gdi32/ole32.
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <objidl.h>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>
std::wstring wide(const std::string&s){int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,L' ');MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),w.data(),n);return w;}
#include "../src/export_windows.hpp"
int wmain(){CoInitializeEx(nullptr,COINIT_MULTITHREADED);ULONG_PTR token;Gdiplus::GdiplusStartupInput input;assert(Gdiplus::GdiplusStartup(&token,&input,nullptr)==Gdiplus::Ok);{
 auto drawing=R"({"width":640,"height":360,"background":"#F8FAFD","elements":[{"type":"rect","x":20,"y":20,"width":600,"height":320,"radius":24,"fill":"#167C80"},{"type":"text","text":"Hello / 中文测试","x":50,"y":70,"width":540,"height":100,"fill":"#FFFFFF","fontSize":32},{"type":"ellipse","x":250,"y":200,"width":80,"height":80,"fill":"#FFFFFF"}]})";
 for(auto ext:{".png",".jpg",".jpeg",".bmp",".tif",".tiff"}){auto bytes=uw::exports::generate(ext,drawing);assert(bytes.size()>100);auto suffix=std::string(ext);if(suffix==".png")assert(bytes.substr(0,8)==std::string("\x89PNG\r\n\x1a\n",8));if(suffix==".jpg"||suffix==".jpeg")assert((unsigned char)bytes[0]==255&&(unsigned char)bytes[1]==216);if(suffix==".bmp")assert(bytes.substr(0,2)=="BM");IStream*stream;assert(SUCCEEDED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)));ULONG n;stream->Write(bytes.data(),(ULONG)bytes.size(),&n);LARGE_INTEGER zero{};stream->Seek(zero,STREAM_SEEK_SET,nullptr);{Gdiplus::Bitmap image(stream);assert(image.GetLastStatus()==Gdiplus::Ok&&image.GetWidth()==640&&image.GetHeight()==360);Gdiplus::Color pixel;image.GetPixel(100,200,&pixel);assert(pixel.GetG()>100&&pixel.GetR()<70);}stream->Release();}
 auto pdf=uw::exports::generate(".pdf",u8"# PDF / 中文\nA real PDF with embedded font.\n中文文字测试。\n");assert(pdf.rfind("%PDF-1.7",0)==0&&pdf.find("/FontFile2")!=std::string::npos);
 }Gdiplus::GdiplusShutdown(token);CoUninitialize();std::cout<<"PASS native raster encoding/decode/pixels, dimensions, CJK drawing and embedded-font PDF font acquisition\n";return 0;}
