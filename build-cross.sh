#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p dist
(cd src && x86_64-w64-mingw32-windres app.rc -o app-resource.o)
x86_64-w64-mingw32-g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -municode -mwindows src/main.cpp src/app-resource.o -o dist/UsageWidget.exe -lgdiplus -ldwmapi -lcomctl32 -lshell32 -luser32 -lgdi32 -ladvapi32 -lwinmm -limm32 -lole32 -luuid -lcomdlg32 -lwinhttp -lbcrypt
makensis installer.nsi
