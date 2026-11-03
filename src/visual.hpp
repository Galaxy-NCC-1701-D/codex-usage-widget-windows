#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include "model.hpp"
namespace visual {
using namespace Gdiplus;
struct State { bool dark=false,pinned=false,fresh=false,capsule=false,chat=false;int opacity=95;uw::Quota quota;std::wstring footer;};
struct Layout {
 RectF body,light,dark,pin,collapse,settings,refresh,ring,chat,tabs,usage,chatTab;
 Layout(float width,float height,bool capsule){
  body=RectF(16,12,width-32,height-32);
  if(capsule){ring=RectF(body.X+14,body.Y+(body.Height-24)/2,24,24);settings=RectF();collapse=RectF();}
  else{float y=body.Y+20,right=body.GetRight()-18;collapse=RectF();pin=RectF(right-28,y,28,30);dark=RectF(right-64,y,30,30);light=RectF(right-96,y,30,30);ring=RectF(body.X+22,y+1,28,28);settings=RectF(right-30,body.GetBottom()-46,30,30);refresh=RectF(body.X+20,body.GetBottom()-46,body.Width-144,30);chat=RectF();tabs=RectF(body.X+22,body.Y+108,body.Width-44,32);usage=RectF(tabs.X+3,tabs.Y+3,(tabs.Width-6)/2,26);chatTab=RectF(usage.GetRight(),usage.Y,usage.Width,26);refresh.Width=body.Width-80;}
 }
};
inline Color ink(bool dark){return dark?Color(244,246,250):Color(29,35,46);}
inline Color muted(bool dark){return dark?Color(175,187,205):Color(93,106,125);}
inline Color track(bool dark){return dark?Color(65,77,97):Color(218,226,238);}
inline Color accent(bool week){return week?Color(133,140,246):Color(37,195,153);}
inline std::wstring pct(const uw::Window&w){return w.available?std::to_wstring((int)std::round(w.remaining))+L"%":L"--";}
inline GraphicsPath* path(RectF r,float rad){rad=std::max(.01f,std::min(rad,std::min(r.Width,r.Height)/2));auto*p=new GraphicsPath();p->AddArc(r.X,r.Y,rad*2,rad*2,180,90);p->AddArc(r.GetRight()-rad*2,r.Y,rad*2,rad*2,270,90);p->AddArc(r.GetRight()-rad*2,r.GetBottom()-rad*2,rad*2,rad*2,0,90);p->AddArc(r.X,r.GetBottom()-rad*2,rad*2,rad*2,90,90);p->CloseFigure();return p;}
inline void fill(Graphics&g,RectF r,float radius,Color c){auto*p=path(r,radius);SolidBrush b(c);g.FillPath(&b,p);delete p;}
inline void label(Graphics&g,const std::wstring&s,RectF r,float size,Color c,const wchar_t*family=L"Segoe UI",StringAlignment align=StringAlignmentNear){
 Font f(family,size,FontStyleRegular,UnitPixel);SolidBrush b(c);StringFormat sf(StringFormat::GenericTypographic());sf.SetTrimming(StringTrimmingEllipsisCharacter);sf.SetFormatFlags(StringFormatFlagsNoWrap);sf.SetLineAlignment(StringAlignmentCenter);sf.SetAlignment(align);g.DrawString(s.c_str(),-1,&f,r,&sf,&b);
}
inline void opticalLabel(Graphics&g,const std::wstring&s,RectF r,float size,Color c){FontFamily family(L"Segoe UI");Font font(&family,size,FontStyleRegular,UnitPixel);StringFormat sf(StringFormat::GenericTypographic());sf.SetFormatFlags(StringFormatFlagsNoWrap);GraphicsPath glyph;glyph.AddString(s.c_str(),-1,&family,FontStyleRegular,size,PointF(0,0),&sf);RectF b;glyph.GetBounds(&b);SolidBrush brush(c);g.DrawString(s.c_str(),-1,&font,PointF(r.X-b.X,r.Y+r.Height/2-b.Y-b.Height/2),&sf,&brush);}
inline void stroke(Graphics&g,Color c,float x,float y,float x2,float y2,float width=1.4f){Pen p(c,width);p.SetStartCap(LineCapRound);p.SetEndCap(LineCapRound);g.DrawLine(&p,x,y,x2,y2);}
inline void symbol(Graphics&g,RectF r,int kind,Color c,bool active=false){
 float cx=r.X+r.Width/2,cy=r.Y+r.Height/2;Pen p(c,1.35f);p.SetLineJoin(LineJoinRound);p.SetStartCap(LineCapRound);p.SetEndCap(LineCapRound);
 if(kind==0){g.DrawEllipse(&p,cx-3.6f,cy-3.6f,7.2f,7.2f);for(int i=0;i<8;i++){float a=i*3.14159265f/4;stroke(g,c,cx+cosf(a)*6,cy+sinf(a)*6,cx+cosf(a)*8,cy+sinf(a)*8);}}
 else if(kind==1){GraphicsPath moon;moon.AddArc(cx-6,cy-7,13.f,13.f,85.f,270.f);moon.AddBezier(PointF(cx-2,cy-7),PointF(cx-5,cy+2),PointF(cx+1,cy+7),PointF(cx+6,cy+3));g.DrawPath(&p,&moon);}
 else if(kind==2){GraphicsPath pin;pin.AddLine(cx-3,cy-7,cx+3,cy-7);pin.AddLine(cx+3,cy-7,cx+3,cy-2);pin.AddLine(cx+3,cy-2,cx+6,cy+2);pin.AddLine(cx+6,cy+2,cx-6,cy+2);pin.AddLine(cx-6,cy+2,cx-3,cy-2);pin.CloseFigure();if(active){SolidBrush b(c);g.FillPath(&b,&pin);}else g.DrawPath(&p,&pin);stroke(g,c,cx,cy+2,cx,cy+8);}
 else if(kind==3){stroke(g,c,cx-5,cy-2,cx,cy+3);stroke(g,c,cx,cy+3,cx+5,cy-2);}
 else if(kind==4){stroke(g,c,cx-5,cy+2,cx,cy-3);stroke(g,c,cx,cy-3,cx+5,cy+2);}
 else if(kind==5){g.DrawEllipse(&p,cx-4,cy-4,8.f,8.f);for(int i=0;i<8;i++){float a=i*3.14159265f/4;stroke(g,c,cx+cosf(a)*5.5f,cy+sinf(a)*5.5f,cx+cosf(a)*8,cy+sinf(a)*8,1.8f);}g.DrawEllipse(&p,cx-1.4f,cy-1.4f,2.8f,2.8f);}
 else if(kind==6){g.DrawArc(&p,cx-7,cy-7,14.f,14.f,35.f,285.f);stroke(g,c,cx+6,cy-7,cx+6,cy-2);stroke(g,c,cx+6,cy-2,cx+1,cy-3);}
}
inline void ring(Graphics&g,RectF r,const uw::Quota&q,bool dark){
 for(int i=0;i<2;i++){float inset=i?5.5f:1.5f;RectF a(r.X+inset,r.Y+inset,r.Width-inset*2,r.Height-inset*2);Pen bg(track(dark),2.6f);g.DrawEllipse(&bg,a);const auto&w=i?q.week:q.five;if(w.available&&w.remaining>0){Pen p(accent(i!=0),2.6f);p.SetStartCap(LineCapRound);p.SetEndCap(LineCapRound);g.DrawArc(&p,a,-90.f,(float)(360*w.remaining/100));}}
}
inline std::wstring reset(const uw::Window&w,int64_t now){if(!w.available)return L"Window not provided";if(!w.resets)return L"Reset time unavailable";auto delta=w.resets-now;if(delta<=0)return L"Reset due - refreshing";auto d=delta/86400,h=delta/3600%24,m=delta/60%60;return L"Resets in "+(d?std::to_wstring(d)+L"d ":L"")+std::to_wstring(h)+L"h "+std::to_wstring(m)+L"m";}
inline float signedDistance(float x,float y,RectF r,float radius){float qx=std::abs(x-(r.X+r.Width/2))-(r.Width/2-radius),qy=std::abs(y-(r.Y+r.Height/2))-(r.Height/2-radius);return std::sqrt(std::max(qx,0.f)*std::max(qx,0.f)+std::max(qy,0.f)*std::max(qy,0.f))+std::min(std::max(qx,qy),0.f)-radius;}
struct Surface {
 int width,height;void*bits=nullptr;HDC dc=nullptr;HBITMAP dib=nullptr;HGDIOBJ old=nullptr;
 Surface(int w,int h):width(w),height(h){BITMAPINFO b{};b.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);b.bmiHeader.biWidth=w;b.bmiHeader.biHeight=-h;b.bmiHeader.biPlanes=1;b.bmiHeader.biBitCount=32;b.bmiHeader.biCompression=BI_RGB;dc=CreateCompatibleDC(nullptr);dib=CreateDIBSection(dc,&b,DIB_RGB_COLORS,&bits,nullptr,0);if(dib){old=SelectObject(dc,dib);memset(bits,0,(size_t)w*h*4);}}
 ~Surface(){if(old)SelectObject(dc,old);if(dib)DeleteObject(dib);if(dc)DeleteDC(dc);}
 Surface(const Surface&)=delete;Surface&operator=(const Surface&)=delete;
 bool present(HWND h){if(!bits)return false;RECT r;GetWindowRect(h,&r);POINT dst{r.left,r.top},src{0,0};SIZE size{width,height};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};return UpdateLayeredWindow(h,nullptr,&dst,&size,dc,&src,0,&blend,ULW_ALPHA)!=FALSE;}
};
inline void segments(Graphics&g,const Layout&l,bool chat,bool dark){fill(g,l.tabs,16,dark?Color(255,37,49,68):Color(255,232,237,246));auto*p=path(l.tabs,16);Pen pen(track(dark),.8f);g.DrawPath(&pen,p);delete p;fill(g,chat?l.chatTab:l.usage,13,dark?Color(255,69,88,117):Color(255,255,255,255));label(g,L"Usage",l.usage,13,chat?muted(dark):ink(dark),L"Segoe UI",StringAlignmentCenter);label(g,L"Chat",l.chatTab,13,chat?ink(dark):muted(dark),L"Segoe UI",StringAlignmentCenter);}
inline void card(Surface&s,float dpi,const State&state,int64_t now){if(!s.bits)return;float w=s.width/dpi,h=s.height/dpi;Layout l(w,h,state.capsule);float radius=state.capsule?l.body.Height/2:26;
 // The shadow is in the transparent margin; it never darkens the card interior.
 auto*px=(unsigned*)s.bits;for(int y=0;y<s.height;y++)for(int x=0;x<s.width;x++){float dx=x/dpi,dy=y/dpi;if(signedDistance(dx,dy,l.body,radius)>0){auto shadow=l.body;shadow.Y+=4;float d=std::max(0.f,signedDistance(dx,dy,shadow,radius));int a=(int)(48*std::exp(-d*d/70));px[(size_t)y*s.width+x]=(unsigned)a<<24;}}
 Bitmap bitmap(s.width,s.height,s.width*4,PixelFormat32bppPARGB,(BYTE*)s.bits);Graphics g(&bitmap);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.ScaleTransform(dpi,dpi);
 Color body=state.dark?Color((BYTE)(255*state.opacity/100),25,34,49):Color((BYTE)(255*state.opacity/100),248,250,254);fill(g,l.body,radius,body);auto*outline=path(l.body,radius);Pen border(state.dark?Color(100,132,151,180):Color(135,220,228,240),.8f);g.DrawPath(&border,outline);delete outline;
 auto fg=ink(state.dark),secondary=muted(state.dark);ring(g,l.ring,state.quota,state.dark);
 if(state.capsule){float x=l.body.X+47,y=l.body.Y;opticalLabel(g,L"5h",RectF(x,y,21,l.body.Height),13,secondary);opticalLabel(g,pct(state.quota.five),RectF(x+25,y,45,l.body.Height),17,fg);stroke(g,track(state.dark),x+75,y+12,x+75,y+l.body.Height-12,1);opticalLabel(g,L"Week",RectF(x+86,y,34,l.body.Height),13,secondary);opticalLabel(g,pct(state.quota.week),RectF(x+125,y,46,l.body.Height),17,fg);g.Flush(FlushIntentionSync);return;}
 opticalLabel(g,L"Usage",RectF(l.body.X+60,l.light.Y,88,l.light.Height),22,fg);
 RectF toggle(l.light.X-2,l.light.Y-2,66,34);fill(g,toggle,12,state.dark?Color(255,37,49,68):Color(255,232,237,246));fill(g,state.dark?l.dark:l.light,10,state.dark?Color(255,69,88,117):Color(255,255,255,255));symbol(g,l.light,0,state.dark?secondary:fg);symbol(g,l.dark,1,state.dark?fg:secondary);
 if(state.pinned){fill(g,l.pin,10,state.dark?Color(255,48,69,91):Color(255,222,237,246));}
 symbol(g,l.pin,2,state.pinned?Color(31,153,153):secondary,state.pinned);
 stroke(g,track(state.dark),l.body.X+22,l.body.Y+67,l.body.GetRight()-22,l.body.Y+67,.8f);
 SolidBrush dot(state.fresh?Color(38,195,150):Color(221,159,58));g.FillEllipse(&dot,l.body.X+24,l.body.Y+84,7.f,7.f);label(g,state.fresh?L"Connected":L"Offline / connecting",RectF(l.body.X+39,l.body.Y+73,160,28),13,secondary);label(g,L"Codex / Work",RectF(l.body.GetRight()-126,l.body.Y+73,104,28),13,secondary,L"Segoe UI",StringAlignmentFar);
 segments(g,l,state.chat,state.dark);
 if(state.chat){symbol(g,l.settings,5,secondary);g.Flush(FlushIntentionSync);return;}
 float step=(l.body.Height-230)/2;
 for(int i=0;i<2;i++){float x=l.body.X+24,y=l.body.Y+151+i*step;const auto&q=i?state.quota.week:state.quota.five;label(g,i?L"Weekly limit":L"5-hour limit",RectF(x,y,l.body.Width-48,24),16,fg);label(g,pct(q),RectF(x-1,y+24,120,52),40,fg);label(g,L"remaining",RectF(x+128,y+41,l.body.Width-185,28),14,secondary);RectF bar(x,y+82,l.body.Width-48,6);fill(g,bar,3,track(state.dark));if(q.available&&q.remaining>0){bar.Width*=q.remaining/100;fill(g,bar,std::min(3.f,bar.Width/2),accent(i!=0));}label(g,reset(q,now),RectF(x,y+92,l.body.Width-48,24),13,secondary);}
 float fy=l.body.GetBottom()-58;stroke(g,track(state.dark),l.body.X+22,fy,l.body.GetRight()-22,fy,.8f);symbol(g,RectF(l.refresh.X,l.refresh.Y,28,30),6,secondary);label(g,state.footer,RectF(l.refresh.X+35,l.refresh.Y,l.refresh.Width-35,30),13,secondary);symbol(g,l.settings,5,secondary);
 for(int i=0;i<3;i++)stroke(g,secondary,l.body.GetRight()-19+i*4.f,l.body.GetBottom()-10,l.body.GetRight()-10,l.body.GetBottom()-19+i*4.f,.8f);
 g.Flush(FlushIntentionSync);
}
inline void taskbar(Surface&s,float dpi,const std::wstring&labelText,bool light,const std::wstring&family,float size){if(!s.bits)return;Bitmap bitmap(s.width,s.height,s.width*4,PixelFormat32bppPARGB,(BYTE*)s.bits);Graphics g(&bitmap);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.ScaleTransform(dpi,dpi);label(g,labelText,RectF(0,0,s.width/dpi,s.height/dpi),size,light?Color(0,0,0):Color(255,255,255),family.c_str(),StringAlignmentCenter);g.Flush(FlushIntentionSync);}
}
