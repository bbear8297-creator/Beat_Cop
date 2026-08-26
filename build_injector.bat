@echo off
chcp 65001 >nul
cd /d "%~dp0"
rem ============================================================
rem  编译命令行注入器 injector.exe（32 位，备用工具）
rem  注意：必须用 32 位编译器（mingw32）。
rem        注入器用 CreateRemoteThread 调 LoadLibraryA，必须拿到
rem        32 位进程内 kernel32.LoadLibraryA 的地址（游戏是 32 位）；
rem        64 位注入器拿到的地址是错的。
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
echo   Beat Cop 命令行注入器（32 位）编译
echo ========================================
"%GXX%" -std=c++17 -O2 -static -o injector.exe injector.cpp
if %errorlevel%==0 (
    echo.
    echo [OK] injector.exe 编译成功
    echo      用法: injector.exe            （默认注入 beatcop\beatcop.dll）
    echo            injector.exe D:\xx\a.dll（指定 DLL 路径）
) else (
    echo.
    echo [失败] 编译出错，请根据上方错误信息排查
)
echo.
pause
