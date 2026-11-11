#pragma once
// Render into the same per-pixel surface: opacity affects the background only.
void appearance();void toggleCapsule();
#include "chat_store.hpp"
bool chatBusy=false,chatComposing=false,chatSelectAll=false,chatAutoScroll=true;
std::wstring runtimeDiagnostics=L"No backend runtime diagnostics yet.";std::wstring chatError;std::wstring chatInput,chatComposition,chatStatus=L"Uses your signed-in Codex session";size_t chatCaret=0;
std::vector<uw::chat::Model>chatModels;std::string selectedModel,selectedEffort;bool modelPrefsLoaded=false,modelPreferenceSaved=false;ULONGLONG chatStageAt=0,chatSentAt=0;std::wstring chatTiming,chatDiagnostics=L"No chat diagnostics collected yet. Open Chat and send a message first.";
void loadModelPrefs(){if(modelPrefsLoaded)return;wchar_t value[256];GetPrivateProfileStringW(L"Chat",L"Model",L"__unset__",value,256,configPath.c_str());modelPreferenceSaved=wcscmp(value,L"__unset__")!=0;selectedModel=modelPreferenceSaved?narrow(value):"";GetPrivateProfileStringW(L"Chat",L"Effort",L"",value,256,configPath.c_str());selectedEffort=narrow(value);modelPrefsLoaded=true;}
void saveModelPrefs(){modelPreferenceSaved=true;WritePrivateProfileStringW(L"Chat",L"Model",wide(selectedModel).c_str(),configPath.c_str());WritePrivateProfileStringW(L"Chat",L"Effort",wide(selectedEffort).c_str(),configPath.c_str());}
const uw::chat::Model*currentModel(){for(auto&m:chatModels)if(m.id==selectedModel)return &m;return nullptr;}
std::string quickEffort(const uw::chat::Model&m){for(auto v:{"low","minimal","none"})if(std::find(m.efforts.begin(),m.efforts.end(),v)!=m.efforts.end())return v;return std::find(m.efforts.begin(),m.efforts.end(),m.defaultEffort)!=m.efforts.end()?m.defaultEffort:"";}
#include "picker_ui.hpp"

std::wstring elapsedStatus(){if(!chatBusy)return chatStatus;return chatStatus+L"  "+std::to_wstring((GetTickCount64()-chatStageAt)/1000)+L"s";}
float chatScroll=0,chatMaxScroll=0;int chatInputFirstLine=0;
extern HWND imageWindow;void clearDraftAttachments();void deleteConversation(const std::string&);void clearRenderedImages();void receiveRemoteImage(const uw::chat::Event&);bool copyChatSelection();extern bool inputFocused;extern size_t selectionStart,selectionEnd;extern std::wstring chatPlainText;
struct ChatMessage {bool user;std::wstring text;std::vector<uw::data::Attachment>attachments;};std::vector<ChatMessage>chatMessages;
struct InputLine {size_t start,length;float width;};std::vector<InputLine>chatInputLines;
struct ChatLayout {RectF usage,chat,newChat,deleteChat,attach,copy,transcript,composer,send,stop,input,model,effort;ChatLayout(const visual::Layout&l){float x=l.body.X+22,w=l.body.Width-44;usage=l.usage;chat=l.chatTab;newChat=RectF(x+w-88,l.body.Y+147,88,26);deleteChat=RectF(x+w-178,l.body.Y+147,88,26);composer=RectF(x,l.body.GetBottom()-169,w,110);attach=RectF(x+8,composer.Y+7,28,28);input=RectF(x+43,composer.Y+10,w-55,59);send=RectF(x+w-65,composer.GetBottom()-32,55,25);stop=RectF(x+w-125,send.Y,54,25);model=RectF(x,l.body.Y+180,w-112,28);effort=RectF(model.GetRight()+6,model.Y,106,28);transcript=RectF(x,l.body.Y+218,w,composer.Y-l.body.Y-230-((pendingAttachments.size()+1)/2)*44.f);copy=RectF(x,l.body.GetBottom()-46,88,30);}};
ChatLayout chatLayout(){return ChatLayout(currentLayout(mainWindow));}
void showUsage(){if(!chatView)return;chatView=false;if(usageWidth&&usageHeight)SetWindowPos(mainWindow,nullptr,0,0,usageWidth,usageHeight,SWP_NOMOVE|SWP_NOZORDER);appearance();}
void openChat(){loadModelPrefs();if(capsule)toggleCapsule();if(!chatView){RECT r;GetWindowRect(mainWindow,&r);usageWidth=r.right-r.left;usageHeight=r.bottom-r.top;chatView=true;float d=windowDpi(mainWindow);SetWindowPos(mainWindow,nullptr,0,0,std::max((int)(480*d),(int)(r.right-r.left)),std::max((int)(620*d),(int)(r.bottom-r.top)),SWP_NOMOVE|SWP_NOZORDER);}
 SetForegroundWindow(mainWindow);SetFocus(mainWindow);appearance();}
void clipboardText(const std::wstring&s){if(!OpenClipboard(mainWindow))return;HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,(s.size()+1)*sizeof(wchar_t));if(mem){void*p=GlobalLock(mem);if(p){memcpy(p,s.c_str(),(s.size()+1)*sizeof(wchar_t));GlobalUnlock(mem);EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,mem))GlobalFree(mem);}else GlobalFree(mem);}CloseClipboard();}
void insertChat(const std::wstring&value){std::wstring clean;for(wchar_t c:value)if(c>=32||c=='\n'||c=='\t')clean+=c;if(chatSelectAll){chatInput.clear();chatCaret=0;chatSelectAll=false;}if(chatInput.size()+clean.size()>8000){chatStatus=L"Message limit: 8,000 characters";return;}chatInput.insert(chatCaret,clean);chatCaret+=clean.size();InvalidateRect(mainWindow,nullptr,FALSE);}
size_t priorChat(size_t p){if(!p)return 0;p--;if(p&&chatInput[p]>=0xdc00&&chatInput[p]<=0xdfff&&chatInput[p-1]>=0xd800&&chatInput[p-1]<=0xdbff)p--;return p;}
size_t nextChat(size_t p){if(p>=chatInput.size())return p;if(chatInput[p]>=0xd800&&chatInput[p]<=0xdbff&&p+1<chatInput.size())return p+2;return p+1;}
void sendChat(){if(updateInstalling())return;if(chatBusy||chatComposing||!pendingDeleteId.empty()||(chatInput.find_first_not_of(L" \t\r\n")==std::wstring::npos&&pendingAttachments.empty()))return;auto prompt=chatInput+attachmentText;if(prompt.empty())prompt=L"Please describe the attached image.";if(narrow(prompt).size()>256*1024){chatError=L"Attached text is too large. Split the files first.";return;}if(auto*m=currentModel())if(!m->supportsImages&&std::any_of(pendingAttachments.begin(),pendingAttachments.end(),[](auto&a){return a.image;})){chatError=L"This model does not accept images. Choose an image-capable model.";return;}chatMessages.push_back({true,chatInput,pendingAttachments});chatMessages.push_back({false,L"",{}});chatBusy=true;chatError.clear();chatAutoScroll=true;auto*c=currentSaved();std::vector<std::string>images;for(auto&a:pendingAttachments)if(a.image)images.push_back(a.path);queueSend(narrow(prompt),selectedModel,selectedEffort,currentChatId,c?c->thread:"",images);pendingAttachments.clear();pendingFileText.clear();attachmentText.clear();chatInput.clear();chatCaret=0;chatSelectAll=false;historyDirty=true;saveCurrent();InvalidateRect(mainWindow,nullptr,FALSE);}

void receiveChat(const uw::chat::Event&e){
 if(e.kind==uw::chat::Event::Runtime){runtimeDiagnostics=wide(e.text);return;}
 if(e.kind==uw::chat::Event::RemoteImage){receiveRemoteImage(e);return;}
 if(e.kind==uw::chat::Event::ResetResult){resetBusy=false;if(!uw::data::confirmedResetOutcome(e.text)){resetMessage=L"Result unknown. Retry uses the same request ID.";}else{pendingResetKey.clear();WritePrivateProfileStringW(L"Usage",L"PendingReset",nullptr,configPath.c_str());resetMessage=e.text=="reset"||e.text=="alreadyRedeemed"?L"Reset confirmed. Refreshing quota...":e.text=="noCredit"?L"No reset credits available.":e.text=="nothingToReset"?L"No eligible quota window to reset.":L"Reset was not confirmed.";}wantRefresh=true;if(settingsWindow)InvalidateRect(settingsWindow,nullptr,FALSE);return;}
 if(e.kind==uw::chat::Event::ThreadBound){for(auto&c:savedChats)if(c.id==e.context){c.thread=e.text;atomicText(chatFile(c.id),uw::data::serialize(c));break;}return;}
 if(e.kind==uw::chat::Event::Deleted){auto id=pendingDeleteId;pendingDeleteId.clear();if(!id.empty()){clearRenderedImages();if(imageWindow)DestroyWindow(imageWindow);removeSaved(id);if(currentChatId==id){chatMessages.clear();chatBusy=false;currentChatId=newId();chatInput.clear();chatCaret=0;chatError.clear();chatScroll=0;chatAutoScroll=true;clearDraftAttachments();queueBind(currentChatId,"");historyDirty=false;WritePrivateProfileStringW(L"Chat",L"LastConversation",wide(currentChatId).c_str(),configPath.c_str());}}if(settingsWindow)InvalidateRect(settingsWindow,nullptr,FALSE);return;}
 if(e.kind==uw::chat::Event::File){auto j=uw::Parser(e.text).parse();uw::data::Attachment file{j.at("name").str,j.at("path").str,j.at("image").number!=0};if(e.context==currentChatId){if(chatMessages.empty()||chatMessages.back().user)chatMessages.push_back({false,L"",{}});chatMessages.back().attachments.push_back(file);historyDirty=true;}return;}
 if(!e.context.empty()&&e.context!=currentChatId&&e.kind!=uw::chat::Event::Models&&e.kind!=uw::chat::Event::Diagnostic){if(e.kind==uw::chat::Event::Error&&!pendingDeleteId.empty()){pendingDeleteId.clear();chatError=wide(e.text);}return;}

 if(e.kind==uw::chat::Event::Diagnostic){chatDiagnostics=wide(e.text);return;}
 if(e.kind==uw::chat::Event::Models){loadModelPrefs();chatModels=uw::chat::modelsFrom(uw::Parser(e.text).parse());if(!chatModels.empty()&&!selectedModel.empty()&&!currentModel()){selectedModel.clear();selectedEffort.clear();}if(!chatModels.empty()&&selectedModel.empty()){const auto*chosen=&chatModels.front();for(auto&m:chatModels)if(m.recommended){chosen=&m;break;}selectedModel=chosen->id;selectedEffort=quickEffort(*chosen);}if(auto*m=currentModel())if(!selectedEffort.empty()&&std::find(m->efforts.begin(),m->efforts.end(),selectedEffort)==m->efforts.end())selectedEffort=quickEffort(*m);if(!chatModels.empty())saveModelPrefs();return;}
 if(e.kind==uw::chat::Event::Timing){auto j=uw::Parser(e.text).parse();auto seconds=[](double ms){std::wostringstream out;out<<std::fixed<<std::setprecision(1)<<ms/1000;return out.str()+L"s";};chatTiming=L"Setup "+seconds(j.at("setupMs").number)+L"  |  First text "+(j.at("firstMs").number<0?L"--":seconds(j.at("firstMs").number))+L"  |  Total "+seconds(j.at("totalMs").number);return;}
 if(e.kind==uw::chat::Event::Text){if(chatBusy&&!chatMessages.empty())chatMessages.back().text=wide(e.text);historyDirty=true;}
 else{if(e.kind==uw::chat::Event::Error&&!chatBusy&&e.text.find("Connection lost.")!=0)chatError=wide(e.text);chatStatus=wide(e.text);chatStageAt=GetTickCount64();if(e.kind==uw::chat::Event::Done||e.kind==uw::chat::Event::Error){if(e.kind==uw::chat::Event::Error&&chatBusy&&!chatMessages.empty()){auto&last=chatMessages.back().text;if(!last.empty())last+=L"\n\n";last+=L"["+wide(e.text)+L"]";}chatBusy=false;historyDirty=true;if(e.kind==uw::chat::Event::Error){pendingDeleteId.clear();deleteAfterStop=false;}else if(deleteAfterStop){deleteAfterStop=false;queueChat(6,currentSaved()?currentSaved()->thread:"");}wantRefresh=true;}}
}
void positionIme(){if(!mainWindow||!chatView)return;auto l=chatLayout();float d=windowDpi(mainWindow);HIMC ctx=ImmGetContext(mainWindow);if(ctx){COMPOSITIONFORM form{};form.dwStyle=CFS_POINT;form.ptCurrentPos={(LONG)((l.input.X+8)*d),(LONG)((l.input.Y+22)*d)};ImmSetCompositionWindow(ctx,&form);CANDIDATEFORM candidates{};candidates.dwStyle=CFS_CANDIDATEPOS;candidates.ptCurrentPos=form.ptCurrentPos;ImmSetCandidateWindow(ctx,&candidates);LOGFONTW f{};f.lfHeight=-(LONG)(14*d);wcscpy_s(f.lfFaceName,L"Segoe UI");ImmSetCompositionFontW(ctx,&f);ImmReleaseContext(mainWindow,ctx);}}
void chatIme(LPARAM flags){HIMC ctx=ImmGetContext(mainWindow);if(!ctx)return;auto get=[&](DWORD kind){LONG n=ImmGetCompositionStringW(ctx,kind,nullptr,0);std::wstring value(n>0?n/2:0,L' ');if(n>0)ImmGetCompositionStringW(ctx,kind,value.data(),n);return value;};if(flags&GCS_RESULTSTR){insertChat(get(GCS_RESULTSTR));chatComposition.clear();}if(flags&GCS_COMPSTR)chatComposition=get(GCS_COMPSTR);ImmReleaseContext(mainWindow,ctx);InvalidateRect(mainWindow,nullptr,FALSE);}
bool chatKey(WPARAM key){bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;if(ctrl){if(key=='A'){if(!inputFocused){selectionStart=0;selectionEnd=chatPlainText.size();InvalidateRect(mainWindow,nullptr,FALSE);return true;}chatSelectAll=true;InvalidateRect(mainWindow,nullptr,FALSE);return true;}if(key=='C'){if(!copyChatSelection()&&chatSelectAll)clipboardText(chatInput);return true;}if(key=='X'){if(chatSelectAll){clipboardText(chatInput);chatInput.clear();chatCaret=0;chatSelectAll=false;}InvalidateRect(mainWindow,nullptr,FALSE);return true;}if(key=='V'){if(OpenClipboard(mainWindow)){auto h=GetClipboardData(CF_UNICODETEXT);auto*p=h?(const wchar_t*)GlobalLock(h):nullptr;std::wstring text;if(p){text=p;GlobalUnlock(h);}CloseClipboard();insertChat(text);}return true;}}
 if(chatComposing)return false;
 if(key==VK_RETURN){if(GetKeyState(VK_SHIFT)&0x8000)insertChat(L"\n");else sendChat();return true;}
 if(key==VK_LEFT){chatCaret=chatSelectAll?0:priorChat(chatCaret);chatSelectAll=false;}
 else if(key==VK_RIGHT){chatCaret=chatSelectAll?chatInput.size():nextChat(chatCaret);chatSelectAll=false;}
 else if(key==VK_HOME){chatCaret=0;chatSelectAll=false;}
 else if(key==VK_END){chatCaret=chatInput.size();chatSelectAll=false;}
 else if(key==VK_DELETE){if(chatSelectAll){chatInput.clear();chatCaret=0;chatSelectAll=false;}else if(chatCaret<chatInput.size())chatInput.erase(chatCaret,nextChat(chatCaret)-chatCaret);}
 else if(key==VK_BACK){if(chatSelectAll){chatInput.clear();chatCaret=0;chatSelectAll=false;}else if(chatCaret){auto prev=priorChat(chatCaret);chatInput.erase(prev,chatCaret-prev);chatCaret=prev;}}
 else if(key==VK_ESCAPE){chatComposition.clear();chatSelectAll=false;}
 else {return false;}
 InvalidateRect(mainWindow,nullptr,FALSE);return true;
}
void chatChar(wchar_t c){if(c>=32)inputFocused=true;if(c>=32&&!(GetKeyState(VK_CONTROL)&0x8000))insertChat(std::wstring(1,c));}
float chatWidth(Graphics&g,const std::wstring&s,size_t start,size_t length){if(!length)return 0;Font f(L"Segoe UI",14,FontStyleRegular,UnitPixel);StringFormat sf(StringFormat::GenericTypographic());sf.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces|StringFormatFlagsNoWrap);RectF bound;g.MeasureString(s.c_str()+start,(INT)length,&f,PointF(0,0),&sf,&bound);return bound.Width;}
std::vector<InputLine> inputLines(Graphics&g,float width){std::vector<InputLine>lines;size_t start=0,pos=0;float used=0;while(pos<chatInput.size()){size_t next=nextChat(pos);if(chatInput[pos]=='\n'){lines.push_back({start,pos-start,used});pos=next;start=pos;used=0;continue;}float advance=chatWidth(g,chatInput,pos,next-pos);if(used+advance>width&&pos>start){lines.push_back({start,pos-start,used});start=pos;used=0;}used+=advance;pos=next;}lines.push_back({start,pos-start,used});return lines;}
#include "chat_render.hpp"
void paintChat(visual::Surface&s,float dpi){Bitmap bitmap(s.width,s.height,s.width*4,PixelFormat32bppPARGB,(BYTE*)s.bits);Graphics g(&bitmap);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.ScaleTransform(dpi,dpi);visual::Layout base(s.width/dpi,s.height/dpi,false);ChatLayout l(base);auto fg=visual::ink(dark),secondary=visual::muted(dark),track=visual::track(dark);auto bg=dark?Color(255,37,49,68):Color(255,232,237,246);
 auto button=[&](const std::wstring&label,RectF r,bool active,bool enabled=true){if(active)visual::fill(g,r,10,bg);visual::label(g,label,r,13,enabled?fg:secondary,L"Segoe UI",StringAlignmentCenter);};
 button(L"New chat",l.newChat,false,!chatBusy);button(L"Delete chat",l.deleteChat,false,!chatBusy);
 visual::label(g,L"5h "+percent(quota.five)+L"  |  Week "+percent(quota.week),RectF(l.usage.X,l.newChat.Y,l.composer.Width-185,25),12,secondary);
 auto*m=currentModel();visual::fill(g,l.model,10,bg);visual::fill(g,l.effort,10,bg);visual::label(g,L"Model: "+(m?wide(m->name):selectedModel.empty()?L"Select model":L"Unavailable")+L" ▾",RectF(l.model.X+9,l.model.Y,l.model.Width-18,28),12,chatBusy?secondary:fg);visual::label(g,(selectedEffort.empty()?L"Default":wide(selectedEffort))+L" ▾",l.effort,12,chatBusy?secondary:fg,L"Segoe UI",StringAlignmentCenter);

 if(!chatError.empty())visual::label(g,chatError,RectF(l.transcript.X,l.transcript.Y-4,l.transcript.Width,28),12,Color(200,90,90));
 renderRichTranscript(g,l.transcript,false);g.SetClip(l.transcript);renderRichTranscript(g,l.transcript,true);g.ResetClip();if(chatMessages.empty()){visual::label(g,L"Search, ask, or attach a file",RectF(l.transcript.X,l.transcript.Y+35,l.transcript.Width,30),16,fg,L"Segoe UI",StringAlignmentCenter);}
 if(chatMaxScroll>0){float thumb=std::max(20.f,l.transcript.Height*l.transcript.Height/(chatMaxScroll+l.transcript.Height));visual::fill(g,RectF(l.transcript.GetRight()-2,l.transcript.Y+(l.transcript.Height-thumb)*chatScroll/chatMaxScroll,2,thumb),1,track);}
 float attachmentY=l.composer.Y-((pendingAttachments.size()+1)/2)*44.f;for(size_t i=0;i<pendingAttachments.size();i++){float width=(l.composer.Width-6)/2;RectF chip(l.composer.X+(i%2)*(width+6),attachmentY+(i/2)*44,width,38);visual::fill(g,chip,10,bg);if(pendingAttachments[i].image)drawImageFit(g,wide(pendingAttachments[i].path),RectF(chip.X+4,chip.Y+3,32,32));visual::label(g,wide(pendingAttachments[i].name),RectF(chip.X+40,chip.Y,chip.Width-65,38),11,fg);visual::label(g,L"×",RectF(chip.GetRight()-24,chip.Y,24,38),15,secondary);chatLinks.push_back({RectF(chip.GetRight()-26,chip.Y,26,38),6,std::to_wstring(i),L"",L""});}

 visual::fill(g,l.composer,15,dark?Color(255,31,43,60):Color(255,242,245,251));auto*p=visual::path(l.composer,15);Pen pen(track,.8f);g.DrawPath(&pen,p);delete p;
 visual::label(g,L"+",l.attach,22,secondary,L"Segoe UI",StringAlignmentCenter);
 chatInputLines=inputLines(g,l.input.Width-2);int caretLine=0;for(size_t i=0;i<chatInputLines.size();i++)if(chatCaret>=chatInputLines[i].start)caretLine=(int)i;chatInputFirstLine=std::max(0,caretLine-2);
 g.SetClip(l.input);if(chatInput.empty()&&chatComposition.empty())visual::label(g,L"Message Codex...",RectF(l.input.X,l.input.Y,l.input.Width,22),14,secondary);
 Font f(L"Segoe UI",14,FontStyleRegular,UnitPixel);StringFormat inputFormat(StringFormat::GenericTypographic());inputFormat.SetFormatFlags(StringFormatFlagsNoWrap|StringFormatFlagsMeasureTrailingSpaces);SolidBrush inputBrush(fg);
 for(int i=chatInputFirstLine;i<(int)chatInputLines.size()&&i<chatInputFirstLine+3;i++){auto&v=chatInputLines[i];float yy=l.input.Y+(i-chatInputFirstLine)*19;if(chatSelectAll)visual::fill(g,RectF(l.input.X,yy,std::max(2.f,v.width),19),1,dark?Color(255,57,81,111):Color(255,200,222,250));g.DrawString(chatInput.c_str()+v.start,(INT)v.length,&f,PointF(l.input.X,yy),&inputFormat,&inputBrush);}
 if(GetFocus()==mainWindow&&inputFocused){auto&v=chatInputLines[caretLine];float cx=l.input.X+chatWidth(g,chatInput,v.start,chatCaret-v.start),cy=l.input.Y+(caretLine-chatInputFirstLine)*19;if(!chatComposition.empty())visual::label(g,chatComposition,RectF(cx,cy,l.input.GetRight()-cx,21),14,fg);else if(GetTickCount64()%1200<650)visual::stroke(g,fg,cx,cy+2,cx,cy+17,1);}
 g.ResetClip();button(L"Send",l.send,true,!chatBusy);if(chatBusy)button(L"Stop",l.stop,false);visual::label(g,L"Enter to send  ·  Shift+Enter for a new line",RectF(l.input.X,l.send.Y,l.composer.Width-150,25),10,secondary);
 button(L"Copy reply",l.copy,false,!chatMessages.empty());g.Flush(FlushIntentionSync);
}
bool chatClick(float x,float y){for(auto&hit:chatLinks)if(hit.kind==6&&hit.r.Contains(x,y)){auto i=(size_t)std::stoul(hit.target);if(i<pendingAttachments.size()){imageCache.erase(wide(pendingAttachments[i].path));if(ownedAttachment(wide(pendingAttachments[i].path)))DeleteFileW(wide(pendingAttachments[i].path).c_str());pendingAttachments.erase(pendingAttachments.begin()+i);pendingFileText.erase(pendingFileText.begin()+i);attachmentText.clear();for(auto&t:pendingFileText)attachmentText+=t;}InvalidateRect(mainWindow,nullptr,FALSE);return true;}auto l=chatLayout();if(l.model.Contains(x,y)){modelMenu(false);return true;}if(l.effort.Contains(x,y)){modelMenu(true);return true;}if(l.usage.Contains(x,y)){showUsage();return true;}if(l.attach.Contains(x,y)){chooseAttachments();return true;}if(l.deleteChat.Contains(x,y)){deleteConversation(currentChatId);return true;}if(l.newChat.Contains(x,y)){if(!chatBusy&&pendingDeleteId.empty())newConversation();return true;}if(l.send.Contains(x,y)){sendChat();return true;}if(l.stop.Contains(x,y)){if(chatBusy){queueChat(1);chatStatus=L"Stopping...";}return true;}if(l.copy.Contains(x,y)){for(auto i=chatMessages.rbegin();i!=chatMessages.rend();++i)if(!i->user&&!i->text.empty()){clipboardText(i->text);chatStatus=L"Reply copied";break;}InvalidateRect(mainWindow,nullptr,FALSE);return true;}
 if(l.input.Contains(x,y)){inputFocused=true;SetFocus(mainWindow);chatSelectAll=false;if(!chatInputLines.empty()){int line=std::clamp(chatInputFirstLine+(int)((y-l.input.Y)/19),0,(int)chatInputLines.size()-1);auto&v=chatInputLines[line];HDC dc=GetDC(mainWindow);Graphics g(dc);float target=x-l.input.X,best=1e9f;chatCaret=v.start;for(size_t p=v.start;p<=v.start+v.length;){float distance=std::abs(chatWidth(g,chatInput,v.start,p-v.start)-target);if(distance<best){best=distance;chatCaret=p;}if(p==v.start+v.length)break;p=nextChat(p);}ReleaseDC(mainWindow,dc);}positionIme();InvalidateRect(mainWindow,nullptr,FALSE);return true;}
 if(l.transcript.Contains(x,y)){chatAutoScroll=chatScroll>=chatMaxScroll-5;return true;}return false;}

void setTransport(bool https){if(httpsMode.load()==https)return;if(chatBusy||!pendingDeleteId.empty()){MessageBoxW(mainWindow,L"Finish the current operation before changing the connection mode.",L"Connection",MB_OK|MB_ICONINFORMATION);return;}closePicker();httpsMode=https;WritePrivateProfileStringW(L"Connection",L"HttpsMode",https?L"1":L"0",configPath.c_str());saveCurrent();currentChatId=newId();chatMessages.clear();chatError.clear();chatTiming.clear();queueBind(currentChatId,"");wantRestart=true;appearance();}

void saveCurrent(){auto*c=currentSaved();if(!c){uw::data::Conversation fresh;fresh.id=currentChatId;fresh.https=httpsMode.load();savedChats.push_back(fresh);c=&savedChats.back();}if(chatMessages.empty()&&c->thread.empty()){historyDirty=false;return;}c->messages.clear();for(auto&m:chatMessages)c->messages.push_back({m.user,narrow(m.text),m.attachments});if(c->title.empty()){for(auto&m:chatMessages)if(m.user){c->title=narrow(m.text.substr(0,60));if(c->title.empty()&&!m.attachments.empty())c->title=m.attachments.front().name;break;}}c->model=selectedModel;c->effort=selectedEffort;c->updated=time(nullptr);c->interrupted=chatBusy;if(!atomicText(chatFile(c->id),uw::data::serialize(*c)))chatError=L"Could not save conversation history.";WritePrivateProfileStringW(L"Chat",L"LastConversation",wide(currentChatId).c_str(),configPath.c_str());historyDirty=false;}
void clearDraftAttachments(){for(auto&a:pendingAttachments){imageCache.erase(wide(a.path));if(ownedAttachment(wide(a.path)))DeleteFileW(wide(a.path).c_str());}pendingAttachments.clear();pendingFileText.clear();attachmentText.clear();}
void newConversation(){if(chatBusy||!pendingDeleteId.empty())return;saveCurrent();clearDraftAttachments();clearRenderedImages();currentChatId=newId();chatMessages.clear();chatInput.clear();chatCaret=0;chatError.clear();chatScroll=0;chatAutoScroll=true;queueBind(currentChatId,"");InvalidateRect(mainWindow,nullptr,FALSE);}
void loadConversation(const std::string&id){if(chatBusy||!pendingDeleteId.empty()){MessageBoxW(mainWindow,L"Stop the current response before opening another conversation.",L"History",MB_OK);return;}saveCurrent();auto i=std::find_if(savedChats.begin(),savedChats.end(),[&](auto&c){return c.id==id;});if(i==savedChats.end())return;clearDraftAttachments();auto record=*i;clearRenderedImages();if(httpsMode.load()!=record.https){httpsMode=record.https;WritePrivateProfileStringW(L"Connection",L"HttpsMode",record.https?L"1":L"0",configPath.c_str());wantRestart=true;}currentChatId=id;chatMessages.clear();for(auto&m:record.messages)chatMessages.push_back({m.user,wide(m.text),m.attachments});if(record.interrupted){if(chatMessages.empty()||chatMessages.back().user)chatMessages.push_back({false,L"",{}});chatMessages.back().text+=L"\n[Interrupted after restart. Your message was not resent.]";}selectedModel=record.model;selectedEffort=record.effort;modelPrefsLoaded=true;saveModelPrefs();chatError.clear();chatInput.clear();chatCaret=0;chatAutoScroll=true;queueBind(id,record.thread);historyDirty=true;saveCurrent();openChat();}
void restoreLastConversation(){wchar_t last[128];GetPrivateProfileStringW(L"Chat",L"LastConversation",L"",last,128,configPath.c_str());std::string id=narrow(last);currentChatId=newId();if(id.empty()&&!savedChats.empty())id=savedChats.front().id;if(!id.empty())loadConversation(id);wchar_t key[128];GetPrivateProfileStringW(L"Usage",L"PendingReset",L"",key,128,configPath.c_str());pendingResetKey=narrow(key);}
void deleteConversation(const std::string&id){if(updateInstalling()||!pendingDeleteId.empty())return;auto i=std::find_if(savedChats.begin(),savedChats.end(),[&](auto&c){return c.id==id;});if(id==currentChatId){saveCurrent();i=std::find_if(savedChats.begin(),savedChats.end(),[&](auto&c){return c.id==id;});}if(i==savedChats.end())return;if(MessageBoxW(mainWindow,L"Permanently delete this conversation from Codex and remove its widget history and attachment copies?",L"Delete chat",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return;pendingDeleteId=id;if(chatBusy){if(id!=currentChatId){pendingDeleteId.clear();MessageBoxW(mainWindow,L"Stop the active response first.",L"History",MB_OK);return;}deleteAfterStop=true;queueChat(1);}else queueChat(6,i->thread);}
void submitReset() {
    resetBusy = true;
    resetMessage = L"Reset requested...";
    queueChat(8, pendingResetKey);
}

void confirmNewReset() {
    if (resetBusy || resetConfirmationOpen || !pendingResetKey.empty() || updateInstalling()) return;
    if (!resetCredits.known) {
        resetMessage = L"Codex did not report available reset credits.";
        return;
    }
    if (resetCredits.count <= 0) {
        resetMessage = L"No reset credits available.";
        return;
    }
    resetConfirmationOpen = true;
    int choice = MessageBoxW(settingsWindow ? settingsWindow : mainWindow,
        L"Use one available reset credit? Codex determines which quota windows are eligible.",
        L"Use one reset credit", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    resetConfirmationOpen = false;
    if (choice != IDYES) {
        resetMessage = L"Reset cancelled.";
        return;
    }
    if (updateInstalling() || !resetCredits.known || !resetCredits.count ||
        std::time(nullptr) - creditsUpdated > 150) {
        resetMessage = L"Credit data changed or expired. Check again before resetting.";
        return;
    }
    auto key = newId();
    if (!WritePrivateProfileStringW(L"Usage", L"PendingReset", wide(key).c_str(), configPath.c_str()) ||
        !WritePrivateProfileStringW(nullptr, nullptr, nullptr, configPath.c_str())) {
        resetMessage = L"Could not save the reset request ID.";
        return;
    }
    pendingResetKey = key;
    submitReset();
}

void resetUsage() {
    if (resetBusy || confirmResetAfterRead || resetConfirmationOpen || updateInstalling()) return;
    if (!pendingResetKey.empty()) {
        resetConfirmationOpen = true;
        int choice = MessageBoxW(settingsWindow ? settingsWindow : mainWindow,
            L"Retry the existing reset request using the same request ID?",
            L"Retry reset", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        resetConfirmationOpen = false;
        if (choice == IDYES) submitReset();
        return;
    }
    resetMessage = L"Checking available reset credits...";
    requestQuotaRead(true);
}

void clearRenderedImages(){imageCache.clear();remoteImages.clear();selectionStart=selectionEnd=0;}

void receiveRemoteImage(const uw::chat::Event&e){auto j=uw::Parser(e.text).parse();auto url=wide(j.at("url").str),path=wide(j.at("path").str);imageLoading.erase(url);if(path.empty()){if(e.context==currentChatId)chatError=wide(j.at("error").str);return;}if(e.context==currentChatId)remoteImages[url]=path;int index=(int)j.at("messageIndex").number;uw::data::Attachment a{"image.png",narrow(path),true,narrow(url)};if(e.context==currentChatId&&index>=0&&index<(int)chatMessages.size()){chatMessages[index].attachments.push_back(a);historyDirty=true;}else{bool found=false;for(auto&c:savedChats)if(c.id==e.context&&index>=0&&index<(int)c.messages.size()){c.messages[index].attachments.push_back(a);atomicText(chatFile(c.id),uw::data::serialize(c));found=true;break;}if(!found)DeleteFileW(path.c_str());}}
