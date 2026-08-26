@echo off
chcp 65001 >nul
cd /d "%~dp0"
rem ============================================================
rem  Beat Cop 一键打包：编译 DLL → 内嵌进 exe → 生成单文件 Beat Cop.exe
rem  产物 Beat Cop.exe 可直接交付使用（仅需 Windows 10，无需任何环境）
rem  需要 MSYS2 + 32 位工具链 mingw-w64-i686-gcc（安装方法见 README）
rem ============================================================

rem ---- 自动查找 MSYS2 32 位工具链（mingw32）----
rem 装到 D:\msys2、C:\msys2 或 %LOCALAPPDATA%\Programs\msys2 都能被找到
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
set "WINDRES=%MINGW32%\windres.exe"

echo ========================================
echo   Beat Cop 一键注入器（单文件版）打包
echo ========================================

rem 1. 编译注入 DLL（如果游戏正开着占用 DLL，这步会失败，可沿用现有 DLL）
"%GXX%" -std=c++17 -O2 -shared -static ^
    -Ibeatcop\imgui ^
    -o beatcop\beatcop.dll ^
    beatcop\dllmain.cpp beatcop\money.cpp beatcop\ui.cpp ^
    beatcop\imgui\imgui.cpp beatcop\imgui\imgui_draw.cpp beatcop\imgui\imgui_tables.cpp beatcop\imgui\imgui_widgets.cpp ^
    beatcop\imgui\backends\imgui_impl_win32.cpp beatcop\imgui\backends\imgui_impl_dx11.cpp beatcop\imgui\backends\imgui_impl_dx9.cpp ^
    -luser32 -lgdi32 -ld3d11 -ldxgi -ld3d9 -ldxguid -luuid -ldwmapi -ld3dcompiler
if errorlevel 1 (
    echo [注意] DLL 编译失败（可能游戏占用），尝试使用现有 beatcop.dll
)
rem 若 DLL 不存在，windres 会产出空资源，打包出的 exe 注入时必然报"解压 DLL 失败"，
rem 这里直接检查并报错拦截，避免悄悄生成坏 exe
if not exist beatcop\beatcop.dll (
    echo [错误] beatcop\beatcop.dll 不存在！
    echo        请先关闭游戏，再重新运行本脚本（或先用 build_dll.bat 编译 DLL）
    goto fail
)

rem 2. 编译资源（把 DLL 字节嵌进 exe 的资源段）
"%WINDRES%" -i resource.rc -o resource.o
if errorlevel 1 goto fail

rem 3. 编译单文件 exe（launcher.cpp 运行时自动解压资源里的 DLL 并注入）
"%GXX%" -std=c++17 -O2 -static -mwindows launcher.cpp resource.o -o "Beat Cop.exe"
if errorlevel 1 goto fail

echo.
echo [OK] 打包完成：Beat Cop.exe（单文件，含内嵌 DLL）
echo      把这个 exe 交付给使用者，双击运行即可（仅需 Windows 10）
echo.
pause
exit /b 0

:fail
echo.
echo [失败] 打包出错
echo.
pause
