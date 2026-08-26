// ============================================================
//  Beat Cop 命令行注入器（32 位）
// ============================================================
//  作用：把 beatcop\beatcop.dll 注入到 BeatCop.exe（32 位游戏）进程。
//  这是备用工具；日常使用推荐单文件版 Beat Cop.exe（launcher.cpp，自动注入）。
//
//  为什么必须是 32 位？
//    注入原理是 CreateRemoteThread 调用目标进程的 LoadLibraryA，
//    必须拿到"目标进程内"的 kernel32.LoadLibraryA 地址。
//    游戏是 32 位进程，64 位注入器拿到的地址是 64 位 kernel32 的，
//    在 32 位进程里是无效地址 → 注入失败。所以本程序必须用
//    mingw-w64-i686（32 位）工具链编译。
//
//  编译（需要 MSYS2 + mingw-w64-i686-gcc）：
//    build_injector.bat
//
//  用法：
//    injector.exe                使用默认路径 beatcop\beatcop.dll（相对当前目录）
//    injector.exe D:\xx\a.dll    指定 DLL 绝对路径
// ============================================================
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <cstring>

// 默认 DLL 路径：相对"当前工作目录"。在 inject\ 目录下运行 injector.exe 即可。
static const char* kDefaultDllPath = "beatcop\\beatcop.dll";

// 按进程名找 PID（不区分大小写）
DWORD GetProcessIdByName(const std::wstring& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (name == pe.szExeFile) { CloseHandle(snap); return pe.th32ProcessID; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Beat Cop 注入器（32 位）===" << std::endl;

    // 1. 确定 DLL 路径：优先命令行参数，否则用默认相对路径
    const char* dllPath = (argc > 1) ? argv[1] : kDefaultDllPath;
    char path[MAX_PATH];
    strncpy(path, dllPath, MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';

    // 2. 找游戏进程
    DWORD pid = GetProcessIdByName(L"BeatCop.exe");
    if (!pid) {
        std::cerr << "找不到 BeatCop.exe，请先启动游戏！" << std::endl;
        system("pause");
        return 1;
    }
    std::cout << "游戏 PID: " << pid << std::endl;

    // 3. 打开目标进程（需要管理员权限）
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        std::cerr << "OpenProcess 失败，错误码 " << GetLastError()
                  << "（请以管理员身份运行注入器）" << std::endl;
        system("pause");
        return 1;
    }

    size_t len = strlen(path) + 1;
    std::cout << "注入 DLL: " << path << std::endl;

    // 4. 在目标进程里分配一块内存，写入 DLL 路径字符串
    LPVOID remote = VirtualAllocEx(hProc, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        std::cerr << "VirtualAllocEx 失败，错误码 " << GetLastError() << std::endl;
        CloseHandle(hProc);
        system("pause");
        return 1;
    }
    if (!WriteProcessMemory(hProc, remote, path, len, NULL)) {
        std::cerr << "WriteProcessMemory 失败，错误码 " << GetLastError() << std::endl;
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        system("pause");
        return 1;
    }

    // 5. 取目标进程内 kernel32.LoadLibraryA 地址（32 位进程取 32 位地址）
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLib = GetProcAddress(k32, "LoadLibraryA");
    if (!loadLib) {
        std::cerr << "GetProcAddress(LoadLibraryA) 失败" << std::endl;
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        system("pause");
        return 1;
    }

    // 6. 远程线程：让目标进程执行 LoadLibraryA(路径)，把 DLL 加载进游戏
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)loadLib, remote, 0, NULL);
    if (!hThread) {
        std::cerr << "CreateRemoteThread 失败，错误码 " << GetLastError() << std::endl;
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        system("pause");
        return 1;
    }

    // 7. 等 LoadLibrary 返回；返回值就是 DLL 模块句柄，非 0 说明加载成功
    WaitForSingleObject(hThread, INFINITE);
    DWORD dllHandle = 0;
    GetExitCodeThread(hThread, &dllHandle);
    CloseHandle(hThread);

    if (dllHandle == 0) {
        std::cerr << "注入失败：LoadLibrary 返回 0（DLL 加载出错）" << std::endl;
    } else {
        std::cout << "注入成功！DLL 句柄 0x" << std::hex << dllHandle << std::endl;
        std::cout << "UI 已画进游戏里，去游戏里看吧。" << std::endl;
    }

    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    system("pause");
    return dllHandle ? 0 : 1;
}
