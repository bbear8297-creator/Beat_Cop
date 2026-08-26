@echo off
chcp 65001 >nul
cd /d "%~dp0"
rem ============================================================
rem  只编译注入 DLL（beatcop\beatcop.dll，32 位）
rem  需要 MSYS2 + 32 位工具链 mingw-w64-i686-gcc
rem ============================================================

rem ---- 自动查找 MSYS2 32 位工具链 ----
set "MINGW32="
if exist "D:\msys2\mingw32\bin\g++.exe" set "MINGW32=D:\msys2\mingw32\bin"
if not defined MINGW32 if exist "C:\msys2\mingw32\bin\g++.exe" set "MINGW32=C:\msys2\mingw32\bin"
if not defined MINGW32 if exist "%LOCALAPPDATA%\Programs\msys2\mingw32\bin\g++.exe" set "MINGW32=%LOCALAPPDATA%\Programs\msys2\mingw32\bin"
if not defined MINGW32 (
    echo [错误] 未找到 MSYS2 32 位工具链（mingw-w64-i686-gcc）
    echo        请先安装 MSYS2，然后在 MSYS2 终端执行：
    echo        pacman -S mingw-w64-i686-gcc
    pause
    exit /b 1
)
set "PATH=%MINGW32%;%PATH%"
set "GXX=%MINGW32%\g++.exe"

echo ========================================
echo   Beat Cop 注入 DLL（32 位）编译
echo ========================================
rem 用 -static 全静态链接，避免 DLL 依赖 libwinpthread 等非系统库导致游戏里加载失败
"%GXX%" -std=c++17 -O2 -shared -static ^
    -Ibeatcop\imgui ^
    -o beatcop\beatcop.dll ^
    beatcop\dllmain.cpp beatcop\money.cpp beatcop\ui.cpp ^
    beatcop\imgui\imgui.cpp beatcop\imgui\imgui_draw.cpp beatcop\imgui\imgui_tables.cpp beatcop\imgui\imgui_widgets.cpp ^
    beatcop\imgui\backends\imgui_impl_win32.cpp beatcop\imgui\backends\imgui_impl_dx11.cpp beatcop\imgui\backends\imgui_impl_dx9.cpp ^
    -luser32 -lgdi32 -ld3d11 -ldxgi -ld3d9 -ldxguid -luuid -ldwmapi -ld3dcompiler
if %errorlevel%==0 (
    echo.
    echo [OK] beatcop\beatcop.dll 编译成功
) else (
    echo.
    echo [失败] 编译出错（游戏开着时 DLL 会被占用，先关游戏再编译）
)
echo.
pause
