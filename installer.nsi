Unicode True
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!define WIDGET_VERSION "1.7.2"
Name "Usage Widget"
OutFile "dist/UsageWidget-Setup-${WIDGET_VERSION}-x64.exe"
VIProductVersion "${WIDGET_VERSION}.0"
VIAddVersionKey "ProductName" "Usage Widget"
VIAddVersionKey "FileDescription" "Usage Widget Setup"
VIAddVersionKey "FileVersion" "${WIDGET_VERSION}"
VIAddVersionKey "ProductVersion" "${WIDGET_VERSION}"
VIAddVersionKey "LegalCopyright" "Usage Widget contributors"
InstallDir "$LOCALAPPDATA\Programs\UsageWidget"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "InstallLocation"
RequestExecutionLevel user
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\UsageWidget.exe"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

; Close only this current-user widget. Its mutex remains held until its worker
; and privately launched backend have exited. Never kill Codex desktop.
!macro CloseWidget PREFIX
Function ${PREFIX}CloseWidget
 Push $0
 Push $1
 Push $2
 retry_close:
 FindWindow $0 "UsageWidget"
 ${If} $0 != 0
  SendMessage $0 0x0111 107 0 /TIMEOUT=2000
 ${EndIf}
 StrCpy $2 0
 wait_close:
 System::Call 'kernel32::OpenMutexW(i 0x00100000, i 0, w "Local\UsageWidget_1_0") p.r1'
 ${If} $1 == 0
  Goto close_done
 ${EndIf}
 System::Call 'kernel32::CloseHandle(p r1)'
 IntOp $2 $2 + 1
 ${If} $2 < 100
  Sleep 100
  Goto wait_close
 ${EndIf}
 MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Usage Widget is still closing. Wait for it to finish, then choose Retry." IDRETRY retry_close
 Pop $2
 Pop $1
 Pop $0
 Abort
 close_done:
 Pop $2
 Pop $1
 Pop $0
FunctionEnd
!macroend
!insertmacro CloseWidget ""
!insertmacro CloseWidget "un."

Function .onInit
 ${IfNot} ${RunningX64}
  MessageBox MB_OK "Usage Widget requires 64-bit Windows."
  Abort
 ${EndIf}
 ; Earlier installers did not write InstallLocation. Reuse their custom path.
 ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "InstallLocation"
 ${If} $0 == ""
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "DisplayIcon"
  ${If} $0 != ""
   ${GetParent} "$0" $INSTDIR
  ${EndIf}
 ${EndIf}
FunctionEnd

Section "Usage Widget" SEC_MAIN
 SetShellVarContext current
 Call CloseWidget
 SetOutPath "$INSTDIR"
 ClearErrors
 File "dist/UsageWidget.exe"
 File "README.md"
 File "EXPORTS.md"
 File "VALIDATION.txt"
 ${If} ${Errors}
  MessageBox MB_OK|MB_ICONSTOP "Could not write the application files. Check the selected folder and retry installation."
  Abort
 ${EndIf}
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 CreateDirectory "$SMPROGRAMS\Usage Widget"
 CreateShortcut "$SMPROGRAMS\Usage Widget\Usage Widget.lnk" "$INSTDIR\UsageWidget.exe"
 CreateShortcut "$SMPROGRAMS\Usage Widget\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
 CreateShortcut "$DESKTOP\Usage Widget.lnk" "$INSTDIR\UsageWidget.exe"
 ; Preserve startup opt-in while correcting the path after a moved installation.
 ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "UsageWidget"
 ${If} $0 != ""
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "UsageWidget" '$\"$INSTDIR\UsageWidget.exe$\" --autostart'
 ${EndIf}
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "DisplayName" "Usage Widget"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "DisplayVersion" "${WIDGET_VERSION}"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "InstallLocation" "$INSTDIR"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "DisplayIcon" "$INSTDIR\UsageWidget.exe"
 WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "NoModify" 1
 WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget" "NoRepair" 1
SectionEnd
Section "Uninstall"
 SetShellVarContext current
 Call un.CloseWidget
 ; Missing application files must not prevent recovery/uninstallation.
 ClearErrors
 IfFileExists "$INSTDIR\UsageWidget.exe" 0 removed_exe
 Delete "$INSTDIR\UsageWidget.exe"
 ${If} ${Errors}
  MessageBox MB_OK|MB_ICONSTOP "Could not remove UsageWidget.exe. Check folder permissions and retry."
  Abort
 ${EndIf}
 removed_exe:
 DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "UsageWidget"
 System::Call 'advapi32::CredDeleteW(w "UsageWidget/SMTP", i 1, i 0)'
 Delete "$INSTDIR\README.md"
 Delete "$INSTDIR\EXPORTS.md"
 Delete "$INSTDIR\VALIDATION.txt"
 Delete "$DESKTOP\Usage Widget.lnk"
 Delete "$SMPROGRAMS\Usage Widget\Usage Widget.lnk"
 Delete "$SMPROGRAMS\Usage Widget\Uninstall.lnk"
 RMDir "$SMPROGRAMS\Usage Widget"
 DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UsageWidget"
 ; Keep preferences for reinstall; remove only the known private empty directory.
 RMDir "$LOCALAPPDATA\UsageWidget\chat-workspace"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
SectionEnd
