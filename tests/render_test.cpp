#define UNICODE
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <ctime>
#include <cassert>
#include <iostream>
#include "../src/visual.hpp"
int differences(visual::Surface&a,visual::Surface&b,RECT r){int n=0;auto*x=(unsigned*)a.bits,*y=(unsigned*)b.bits;for(int j=r.top;j<r.bottom;j++)for(int i=r.left;i<r.right;i++)n+=x[j*a.width+i]!=y[j*a.width+i];return n;}
int opaque(visual::Surface&a,RECT r){int n=0;auto*x=(unsigned*)a.bits;for(int j=r.top;j<r.bottom;j++)for(int i=r.left;i<r.right;i++)n+=(x[j*a.width+i]>>24)==255;return n;}
int main(){ULONG_PTR token;Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);{
 visual::State state;state.fresh=true;state.quota.five={true,19,1900000000};state.quota.week={true,54,1900000000};state.footer=L"Updated just now";state.opacity=25;
 visual::Surface low(420,480),full(420,480),changed(420,480);visual::card(low,1,state,time(nullptr));state.opacity=100;visual::card(full,1,state,time(nullptr));
 assert((((unsigned*)low.bits)[120*420+360]>>24)<100);assert((((unsigned*)full.bits)[120*420+360]>>24)==255);
 assert(opaque(low,{42,145,155,198})>30);assert(opaque(full,{42,145,155,198})>30);
 auto firstRing=state.quota.five.remaining;state.quota.five.remaining=80;visual::card(changed,1,state,time(nullptr));assert(differences(full,changed,{38,30,70,62})>10);state.quota.five.remaining=firstRing;
 state.pinned=true;visual::Surface pin(420,480);visual::card(pin,1,state,time(nullptr));auto layout=visual::Layout(420,480,false);RECT pinRect{(LONG)layout.pin.X,(LONG)layout.pin.Y,(LONG)layout.pin.GetRight(),(LONG)layout.pin.GetBottom()};assert(differences(full,pin,pinRect)>10);
 visual::Surface small(368,448),dpi(552,672),capsule(280,80);visual::card(small,1,state,time(nullptr));visual::card(dpi,1.5f,state,time(nullptr));state.capsule=true;visual::card(capsule,1,state,time(nullptr));assert(opaque(capsule,{60,22,240,60})>30);
 std::cout<<"PASS per-pixel background alpha, opaque text, quota rings, pin state, minimum size, DPI, capsule render\n";
 }Gdiplus::GdiplusShutdown(token);}
