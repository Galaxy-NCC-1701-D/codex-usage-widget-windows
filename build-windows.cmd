@echo off
setlocal
cd /d "%~dp0"
if not exist dist mkdir dist
where cl >nul 2>nul
if errorlevel 1 (
 echo Open x64 Native Tools Command Prompt for Visual Studio, then run this file.
 exit /b 1
)
cd src
rc /nologo app.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /DUNICODE /D_UNICODE main.cpp app.res /Fe:..\dist\UsageWidget.exe /link /SUBSYSTEM:WINDOWS gdiplus.lib dwmapi.lib comctl32.lib shell32.lib user32.lib gdi32.lib advapi32.lib winmm.lib imm32.lib ole32.lib uuid.lib comdlg32.lib winhttp.lib bcrypt.lib
if errorlevel 1 exit /b 1
cd ..
where makensis >nul 2>nul
if errorlevel 1 (
 echo Application built. Install NSIS and run makensis installer.nsi to build the installer.
 exit /b 0
)
makensis installer.nsi
