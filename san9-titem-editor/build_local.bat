@echo off
rem Build with MinGW (32-bit). Run in this folder.
where gcc >nul 2>nul || (echo [ERROR] gcc not found. Install MinGW-w64 i686. & pause & exit /b 1)
gcc -O2 -mwindows -fexec-charset=GBK -o San9TItemEditor.exe titem_editor.c -lgdi32 -luser32 -lkernel32
if errorlevel 1 (echo [FALLBACK] UTF-8 path failed, trying GBK source & gcc -O2 -mwindows -o San9TItemEditor.exe titem_editor_gbk.c -lgdi32 -luser32 -lkernel32)
if exist San9TItemEditor.exe (echo [OK] San9TItemEditor.exe & dir San9TItemEditor.exe) else (echo [FAIL] no exe)
pause
