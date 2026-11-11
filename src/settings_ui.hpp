#pragma once
// Included after application state/actions. All coordinates below are DPI-independent.
std::vector<std::string>historyVisibleIds;int historyPage=0;
int settingsTab=0;bool opacityDragging=false;
constexpr int settingsTabCount = 6;
constexpr int creditRowsPerPage = 4;
int creditPage = 0;
HWND refreshEdit = nullptr;
std::wstring intervalMessage;
std::vector<std::pair<Gdiplus::RectF,int>> settingsActions;
enum { ACT_TAB=500,ACT_CLOSE=510,ACT_OPACITY=511,ACT_FONT_MINUS=512,ACT_FONT_PLUS,ACT_SOUND,ACT_FIVE,ACT_WEEK,ACT_TEST_SOUND,ACT_RESET_USAGE=550,ACT_REFRESH_CREDITS,ACT_HISTORY_PREV,ACT_HISTORY_NEXT,
 ACT_REFRESH_TOGGLE,ACT_REFRESH_SAVE,ACT_CREDITS_PREV,ACT_CREDITS_NEXT,
 ACT_UPDATE_AUTO,ACT_UPDATE_CHECK,ACT_UPDATE_INSTALL,ACT_UPDATE_PAGE,ACT_UPDATE_NOTES };
std::wstring localDate(std::int64_t timestamp, bool includeTime = false) {
    if (timestamp <= 0) return L"Unknown";
    auto value = static_cast<std::time_t>(timestamp);
    tm local{};
    if (localtime_s(&local, &value)) return L"Unknown";
    wchar_t date[32]{};
    if (!wcsftime(date, ARRAYSIZE(date), includeTime ? L"%Y-%m-%d %H:%M" : L"%Y-%m-%d", &local))
        return L"Unknown";
    return date;
}

void saveRefreshInterval() {
    if (!refreshEdit) return;
    wchar_t value[16]{};
    GetWindowTextW(refreshEdit, value, ARRAYSIZE(value));
    auto seconds = uw::refresh::parseInterval(value);
    if (!seconds) {
        intervalMessage = L"Enter a whole number from 5 to 1800.";
        SetFocus(refreshEdit);
        SendMessageW(refreshEdit, EM_SETSEL, 0, -1);
        return;
    }
    if (!WritePrivateProfileStringW(L"Usage", L"RefreshSeconds",
            std::to_wstring(*seconds).c_str(), configPath.c_str())) {
        intervalMessage = L"Could not save the refresh interval.";
        return;
    }
    refreshIntervalSeconds = *seconds;
    if (automaticQuotaSeconds.load()) automaticQuotaSeconds = *seconds;
    SetWindowTextW(refreshEdit, std::to_wstring(*seconds).c_str());
    intervalMessage = L"Refresh interval saved.";
}

void toggleQuotaRefresh() {
    const bool enable = !automaticQuotaSeconds.load();
    if (!WritePrivateProfileStringW(L"Usage", L"AutoRefresh", enable ? L"1" : L"0",
                                    configPath.c_str())) {
        intervalMessage = L"Could not save auto-refresh settings.";
        return;
    }
    automaticQuotaSeconds = enable ? refreshIntervalSeconds : 0;
    intervalMessage = enable ? L"Auto-refresh enabled." : L"Auto-refresh paused.";
}

void openUpdateRelease() {
    auto page = availableRelease ? availableRelease->page() :
        "https://" + std::string(uw::updates::downloadHost) + "/" +
        uw::updates::repository + "/releases";
    auto url = wide(page);
    ShellExecuteW(settingsWindow, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void settingsAction(int id){if(id>=1000&&id<1020){auto row=(id-1000)/2;if(row<(int)historyVisibleIds.size()){auto selected=historyVisibleIds[row];if((id-1000)%2)deleteConversation(selected);else loadConversation(selected);}InvalidateRect(settingsWindow,nullptr,FALSE);return;}if(id>=ACT_TAB&&id<ACT_TAB+settingsTabCount){settingsTab=id-ACT_TAB;ShowWindow(refreshEdit,settingsTab==3?SW_SHOWNA:SW_HIDE);InvalidateRect(settingsWindow,nullptr,FALSE);return;}switch(id){case ACT_RESET_USAGE:resetUsage();break;case ACT_REFRESH_CREDITS:requestQuotaRead();break;
case ACT_REFRESH_SAVE:saveRefreshInterval();break;
case ACT_REFRESH_TOGGLE:toggleQuotaRefresh();break;
case ACT_CREDITS_PREV:creditPage=std::max(0,creditPage-1);break;
case ACT_CREDITS_NEXT:++creditPage;break;
case ACT_UPDATE_AUTO:saveAutomaticUpdates();break;
case ACT_UPDATE_CHECK:startUpdateWorker(true);break;
case ACT_UPDATE_INSTALL:downloadOrInstallUpdate();break;
case ACT_UPDATE_PAGE:openUpdateRelease();break;
case ACT_UPDATE_NOTES:if(availableRelease){
    auto notes=wide(availableRelease->notes);
    if(notes.empty())notes=L"No release notes provided.";
    if(notes.size()>1800)notes=notes.substr(0,1800)+L"\n\nOpen the Release page for the full notes.";
    MessageBoxW(settingsWindow,notes.c_str(),(L"Release notes - "+wide(availableRelease->tag)).c_str(),
                MB_OK|MB_ICONINFORMATION);
}break;case ACT_HISTORY_PREV:historyPage=std::max(0,historyPage-1);break;case ACT_HISTORY_NEXT:historyPage++;break;case ACT_CLOSE:DestroyWindow(settingsWindow);return;case ACT_FONT_MINUS:taskFontSize=std::max(12,taskFontSize-1);positionTask();updateTray();save();break;case ACT_FONT_PLUS:taskFontSize=std::min(22,taskFontSize+1);positionTask();updateTray();save();break;case ACT_SOUND:soundEnabled=!soundEnabled;notificationPrefs();break;case ACT_FIVE:alertFive=!alertFive;notificationPrefs();break;case ACT_WEEK:alertWeek=!alertWeek;notificationPrefs();break;case ACT_TEST_SOUND:playResetSound();break;default:command((UINT)id);break;}if(settingsWindow)InvalidateRect(settingsWindow,nullptr,FALSE);}
void drawSettings(HWND h){PAINTSTRUCT ps;HDC screen=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);Bitmap bm(std::max(1L,r.right),std::max(1L,r.bottom),PixelFormat32bppARGB);Graphics g(&bm);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.Clear(dark?Color(24,33,48):Color(247,249,253));float d=windowDpi(h);g.ScaleTransform(d,d);auto fg=visual::ink(dark),secondary=visual::muted(dark);Color panel=dark?Color(33,45,63):Color(255,255,255),highlight=dark?Color(49,66,88):Color(226,239,247);settingsActions.clear();
 auto label=[&](const std::wstring&t,float x,float y,float w,float size=14,Color c=Color(0,0,0)){if(c.GetValue()==Color(0,0,0).GetValue())c=fg;visual::label(g,t,RectF(x,y,w,30),size,c);};
 auto button=[&](int id,const std::wstring&t,RectF b,bool selected=false){visual::fill(g,b,11,selected?highlight:panel);visual::label(g,t,b,14,fg,L"Segoe UI",StringAlignmentCenter);settingsActions.push_back({b,id});};
 auto toggle=[&](int id,const std::wstring&t,const std::wstring&desc,float y,bool on){label(t,240,y,355,16);label(desc,240,y+25,370,12,secondary);RectF b(618,y+8,42,24);visual::fill(g,b,12,on?Color(40,184,154):visual::track(dark));SolidBrush knob(Color(255,255,255));g.FillEllipse(&knob,on?640.f:621.f,y+11,18.f,18.f);settingsActions.push_back({RectF(230,y,442,57),id});};
 label(L"Settings",28,25,170,25);visual::stroke(g,secondary,675,30,685,40,1.4f);visual::stroke(g,secondary,685,30,675,40,1.4f);settingsActions.push_back({RectF(665,20,30,30),ACT_CLOSE});
 const wchar_t* tabs[]={L"Appearance",L"Notifications",L"Connection",L"Usage",L"History",L"Updates"};for(int i=0;i<settingsTabCount;i++){RectF b(20,100+i*48.f,174,40);if(settingsTab==i)visual::fill(g,b,12,highlight);label(tabs[i],38,105+i*48.f,145,14);settingsActions.push_back({b,ACT_TAB+i});}label(L"Usage Widget " UW_VERSION_W,28,516,170,12,secondary);label(automaticQuotaSeconds.load()?L"Refresh every "+std::to_wstring(refreshIntervalSeconds)+L" sec":L"Auto-refresh off",28,541,170,12,secondary);label(tabs[settingsTab],230,28,405,23);label(L"Changes stay on this computer.",230,60,430,12,secondary);
 if(settingsTab==0){visual::fill(g,RectF(222,103,464,180),18,panel);label(L"Theme",240,116,130,16);button(ID_AUTO,L"System",RectF(414,115,80,32),autoTheme);button(ID_LIGHT,L"Light",RectF(496,115,80,32),!autoTheme&&!dark);button(ID_DARK,L"Dark",RectF(578,115,80,32),!autoTheme&&dark);label(L"Background opacity",240,179,330,16);label(std::to_wstring(opacity)+L"%",610,179,60,15,secondary);RectF rail(240,238,420,5);visual::fill(g,rail,2.5f,visual::track(dark));float x=240+(opacity-25)/75.f*420;visual::fill(g,RectF(240,238,std::max(1.f,x-240),5),2.5f,Color(40,184,154));SolidBrush thumb(Color(40,184,154));g.FillEllipse(&thumb,x-7,233.f,14.f,14.f);label(L"Text and icons always stay opaque.",240,252,420,12,secondary);toggle(ID_TOP,L"Always on top",L"Keep the widget above other windows.",304,alwaysTop);toggle(ID_TASKBAR,L"Taskbar readout",L"Plain system-colored text beside the tray.",371,taskEnabled);toggle(ID_STARTUP,L"Start with Windows",L"Launch when you sign in to this Windows account.",438,startupEnabled());label(L"Taskbar font size",240,513,300,15);button(ACT_FONT_MINUS,L"-",RectF(534,513,34,32));label(std::to_wstring(taskFontSize)+L" px",578,513,50,14);button(ACT_FONT_PLUS,L"+",RectF(632,513,34,32));}
 else if(settingsTab==1){toggle(ACT_SOUND,L"Sound on reset",L"Clear triad - three bright chimes.",111,soundEnabled);toggle(ACT_FIVE,L"5-hour quota",L"Alert when this quota window is confirmed reset.",211,alertFive);toggle(ACT_WEEK,L"Weekly quota",L"Alert when the weekly window is confirmed reset.",311,alertWeek);button(ACT_TEST_SOUND,L"Test sound - Clear triad",RectF(240,440,420,36));label(L"Alerts follow a fresh server reading, not just the clock.",240,493,440,12,secondary);label(L"The widget must be running and connected.",240,519,440,12,secondary);}
 else if(settingsTab==2){struct Row{int id;const wchar_t*title;const wchar_t*desc;};Row rows[]={{ID_DETECT,L"Detect desktop Codex",L"Find a usable backend in your installed desktop app."},{ID_LOGIN,L"Sign in with ChatGPT",L"Use Codex's official local sign-in flow."},{ID_DASHBOARD,L"Open usage dashboard",L"View OpenAI's account usage page in your browser."}};for(int i=0;i<3;i++){RectF b(230,110+i*70.f,450,68);visual::fill(g,b,14,panel);label(rows[i].title,246,b.Y+5,410,15);label(rows[i].desc,246,b.Y+31,410,12,secondary);settingsActions.push_back({b,rows[i].id});}label(L"Chat connection",230,334,450,16);button(ID_HTTPS,L"HTTPS",RectF(230,376,218,36),httpsMode.load());button(ID_DEFAULT_TRANSPORT,L"Default (WebSocket)",RectF(458,376,222,36),!httpsMode.load());label(L"Applies to this widget only. Starts a new conversation.",230,425,450,12,secondary);label(L"Default mode follows Codex transport and fallback rules.",230,450,450,12,secondary);label(L"Open Codex desktop if reconnection is needed.",230,529,450,12,secondary);}
 else if (settingsTab == 3) {
     toggle(ACT_REFRESH_TOGGLE, L"Auto-refresh", L"Refresh quota and reset credits periodically.",
            92, automaticQuotaSeconds.load() != 0);
     label(L"Interval (5-1800 seconds)", 240, 155, 220, 14);
     visual::fill(g, RectF(469,153,86,30), 7, panel);
     button(ACT_REFRESH_SAVE, L"Apply", RectF(566,151,94,34));
     label(intervalMessage, 240, 184, 430, 12, secondary);

     visual::fill(g, RectF(230,218,450,222), 16, panel);
     label(resetCredits.known ? L"Available resets: " + std::to_wstring(resetCredits.count) :
                                L"Available resets: Unavailable", 246, 225, 420, 17);
     visual::fill(g, RectF(244,261,422,28), 6, highlight);
     label(L"#", 255, 260, 44, 13);
     label(L"Granted on", 311, 260, 150, 13);
     label(L"Expires on", 474, 260, 180, 13);

     const int rowCount = static_cast<int>(resetCredits.rows.size());
     const int pageCount = std::max(1, (rowCount + creditRowsPerPage - 1) / creditRowsPerPage);
     creditPage = std::clamp(creditPage, 0, pageCount - 1);
     const int startRow = creditPage * creditRowsPerPage;
     for (int row = startRow; row < std::min(startRow + creditRowsPerPage, rowCount); ++row) {
         const auto& credit = resetCredits.rows[row];
         const float y = 290 + (row - startRow) * 25.f;
         label(std::to_wstring(row + 1), 255, y, 44, 13);
         label(localDate(credit.granted), 311, y, 150, 13);
         label(credit.noExpiration ? L"No expiration" : localDate(credit.expires), 474, y, 180, 13);
     }
     if (!rowCount) label(resetCredits.known && resetCredits.count == 0 ?
         L"No available reset credits." : resetCredits.detailsKnown ? L"No credit rows returned." : L"Credit details unavailable.", 252, 297, 395, 13, secondary);
     if (pageCount > 1) {
         if (creditPage) button(ACT_CREDITS_PREV, L"Previous", RectF(470,397,86,30));
         if (creditPage + 1 < pageCount) button(ACT_CREDITS_NEXT, L"Next", RectF(566,397,92,30));
     }
     if (rowCount) label(L"Showing " + std::to_wstring(startRow + 1) + L"-" +
         std::to_wstring(std::min(startRow + creditRowsPerPage, rowCount)) + L" of " +
         std::to_wstring(resetCredits.count), 246, 398, 218, 12, secondary);

     button(ACT_REFRESH_CREDITS, activeQuotaRead ? L"Refreshing..." : L"Refresh data",
         RectF(230,452,210,36));
     button(ACT_RESET_USAGE, resetBusy ? L"Reset requested..." :
         confirmResetAfterRead ? L"Checking credits..." :
         pendingResetKey.empty() ? L"Use 1 reset credit..." : L"Retry previous reset",
         RectF(450,452,230,36));
     label(lastUpdate ? L"Last successful read: " + localDate(lastUpdate, true) :
         L"Waiting for the first Codex reading.", 230, 494, 448, 12, secondary);
     label(quotaReadMessage, 230, 520, 448, 12, secondary);
     label(resetMessage, 230, 546, 448, 12, secondary);
 }
 else if(settingsTab==4){std::vector<uw::data::Conversation*>rows;for(auto&c:savedChats)if(!c.messages.empty()||!c.thread.empty())rows.push_back(&c);std::sort(rows.begin(),rows.end(),[](auto*a,auto*b){return a->updated>b->updated;});historyPage=std::clamp(historyPage,0,std::max(0,((int)rows.size()-1)/6));historyVisibleIds.clear();for(int row=0;row<6;row++){int index=historyPage*6+row;if(index>=(int)rows.size())break;auto&c=*rows[index];historyVisibleIds.push_back(c.id);RectF box(230,105+row*65.f,450,57);visual::fill(g,box,12,c.id==currentChatId?highlight:panel);label(c.title.empty()?L"Untitled conversation":wide(c.title),244,box.Y+1,340,14);time_t timestamp=c.updated;tm local{};localtime_s(&local,&timestamp);wchar_t value[64];wcsftime(value,64,L"%Y-%m-%d %H:%M",&local);label(value,244,box.Y+25,330,11,secondary);settingsActions.push_back({RectF(box.X,box.Y,box.Width-75,box.Height),1000+row*2});button(1001+row*2,L"Delete",RectF(box.GetRight()-72,box.Y+12,62,32));}if(rows.empty())label(L"Your widget conversations will appear here.",230,140,450,14,secondary);button(ACT_HISTORY_PREV,L"Previous",RectF(230,518,100,34));label(std::to_wstring(historyPage+1),428,518,60,14);button(ACT_HISTORY_NEXT,L"Next",RectF(580,518,100,34));}
 else if (settingsTab == 5) {
     label(L"Installed version: " UW_VERSION_W, 230, 108, 440, 16);
     toggle(ACT_UPDATE_AUTO, L"Automatically check for updates",
            L"Check GitHub for stable releases once a day.", 153, automaticUpdates);
     button(ACT_UPDATE_CHECK, L"Check for updates", RectF(230,231,215,36));
     button(ACT_UPDATE_PAGE, L"Open Release page", RectF(458,231,222,36));
     label(updatesMessage, 230, 281, 450, 13, secondary);
     if (lastUpdateCheck) label(L"Last checked: " + localDate(lastUpdateCheck, true),
         230, 315, 450, 12, secondary);
     if (availableRelease) {
         visual::fill(g, RectF(230,360,450,150), 16, panel);
         label(L"Available version: " + wide(availableRelease->tag), 246, 371, 420, 16);
         button(ACT_UPDATE_NOTES, L"Release notes", RectF(246,417,175,35));
         if (availableRelease->digest)
             button(ACT_UPDATE_INSTALL, downloadedInstaller ? L"Install update" : L"Download & Install",
                 RectF(431,417,232,35));
         label(L"Install after saving your current work.", 246, 463, 416, 12, secondary);
     }
 }
 g.ResetTransform();Graphics output(screen);output.DrawImage(&bm,0,0);EndPaint(h,&ps);}
void setOpacityFromPointer(HWND h,int x){float d=windowDpi(h);opacity=std::clamp((int)std::round(25+(x/d-240)/420*75),25,100);appearance();save();}
LRESULT CALLBACK SettingsProc(HWND h,UINT msg,WPARAM w,LPARAM l){switch(msg){case WM_CREATE:{
 float dpi=windowDpi(h);
 refreshEdit=CreateWindowExW(0,L"EDIT",std::to_wstring(refreshIntervalSeconds).c_str(),
     WS_CHILD|WS_TABSTOP|ES_NUMBER|ES_CENTER|ES_AUTOHSCROLL,
     static_cast<int>(475*dpi),static_cast<int>(157*dpi),
     static_cast<int>(74*dpi),static_cast<int>(22*dpi),h,nullptr,instance,nullptr);
 if(refreshEdit){
     SendMessageW(refreshEdit,WM_SETFONT,reinterpret_cast<WPARAM>(settingsFont),TRUE);
     SendMessageW(refreshEdit,EM_SETLIMITTEXT,12,0);
     ShowWindow(refreshEdit,settingsTab==3?SW_SHOWNA:SW_HIDE);
 }
 return 0;
}case WM_CTLCOLOREDIT:{
 if(reinterpret_cast<HWND>(l)!=refreshEdit)break;
 auto dc=reinterpret_cast<HDC>(w);
 auto background=dark?RGB(33,45,63):RGB(255,255,255);
 SetDCBrushColor(dc,background);SetBkColor(dc,background);
 SetTextColor(dc,dark?RGB(238,242,248):RGB(25,33,47));
 return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
}case WM_NCCALCSIZE:return 0;case WM_ERASEBKGND:return 1;case WM_PAINT:drawSettings(h);return 0;case WM_NCHITTEST:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);float d=windowDpi(h);if(p.y/d<75&&p.x/d<650)return HTCAPTION;return HTCLIENT;}case WM_LBUTTONDOWN:{float d=windowDpi(h);if(settingsTab==0&&RectF(230,223,440,32).Contains(GET_X_LPARAM(l)/d,GET_Y_LPARAM(l)/d)){opacityDragging=true;SetCapture(h);setOpacityFromPointer(h,GET_X_LPARAM(l));}return 0;}case WM_MOUSEMOVE:if(opacityDragging)setOpacityFromPointer(h,GET_X_LPARAM(l));return 0;case WM_LBUTTONUP:{if(opacityDragging){opacityDragging=false;ReleaseCapture();return 0;}float d=windowDpi(h),x=GET_X_LPARAM(l)/d,y=GET_Y_LPARAM(l)/d;for(auto a:settingsActions)if(a.first.Contains(x,y)){settingsAction(a.second);break;}return 0;}case WM_COMMAND:if(LOWORD(w)==IDCANCEL){DestroyWindow(h);return 0;}break;case WM_DPICHANGED:DestroyWindow(h);PostMessageW(mainWindow,WM_COMMAND,ID_SETTINGS,0);return 0;case WM_CLOSE:DestroyWindow(h);return 0;case WM_DESTROY:refreshEdit=nullptr;settingsWindow=nullptr;return 0;}return DefWindowProcW(h,msg,w,l);}
void showSettings(){if(settingsWindow){SetForegroundWindow(settingsWindow);return;}RECT main;GetWindowRect(mainWindow,&main);float d=windowDpi(mainWindow);int width=(int)(720*d),height=(int)(610*d);HMONITOR mon=MonitorFromWindow(mainWindow,MONITOR_DEFAULTTONEAREST);MONITORINFO mi{};mi.cbSize=sizeof(mi);GetMonitorInfoW(mon,&mi);int x=std::clamp((int)main.left+20,(int)mi.rcWork.left,std::max((int)mi.rcWork.left,(int)mi.rcWork.right-width));int y=std::clamp((int)main.top+20,(int)mi.rcWork.top,std::max((int)mi.rcWork.top,(int)mi.rcWork.bottom-height));settingsWindow=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT|(alwaysTop?WS_EX_TOPMOST:0),L"UsageSettings",L"Usage - Settings",WS_POPUP|WS_THICKFRAME|WS_CLIPCHILDREN,x,y,width,height,mainWindow,nullptr,instance,nullptr);int corner=2;DwmSetWindowAttribute(settingsWindow,33,&corner,sizeof(corner));// Explicit shape supports Windows 10 as well.
 SetWindowRgn(settingsWindow,CreateRoundRectRgn(0,0,width+1,height+1,(int)(40*d),(int)(40*d)),TRUE);ShowWindow(settingsWindow,SW_SHOW);}
