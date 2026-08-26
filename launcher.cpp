// ============================================================
//  Beat Cop 一键注入器（32 位 GUI）——★ 最终交付版
// ============================================================
//  作用：把 beatcop.dll 以"资源"形式内嵌进本 exe，运行后自动
//        检测 Beat Cop 进程并注入，全程无需任何环境、无需外部 DLL。
//  产物：build_launcher.bat 打包生成单文件 Beat Cop.exe
//        （把 Beat Cop.exe 交付给使用者，双击即用，仅需 Windows 10）。
//  注意：本程序必须用 32 位编译器（mingw32）编译，原因同 injector.cpp
//        （要拿 32 位进程内 kernel32.LoadLibraryA 的地址）。
// ============================================================
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>

#define IDR_BEATCOP_DLL 101
#define IDC_STATUS       1001
#define IDC_BTN_INJECT   1002

static HWND g_status = NULL;
static HWND g_btn    = NULL;
static DWORD g_injectedPid = 0;

// ---------- 进程 / 模块工具 ----------
static DWORD FindGamePid() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"BeatCop.exe") == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static bool IsAlreadyInjected(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{ sizeof(me) };
    bool found = false;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, L"beatcop.dll") == 0) { found = true; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

// ---------- 解压内嵌 DLL ----------
static std::wstring ExtractDll() {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_BEATCOP_DLL), (LPCWSTR)RT_RCDATA);
    if (!hRes) return L"";
    HGLOBAL hGlob = LoadResource(NULL, hRes);
    if (!hGlob) return L"";
    void* data = LockResource(hGlob);
    DWORD size = SizeofResource(NULL, hRes);

    wchar_t tmp[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tmp)) return L"";
    std::wstring dir = std::wstring(tmp) + L"Beat Cop\\";
    CreateDirectoryW(dir.c_str(), NULL);

    std::wstring path = dir + L"beatcop.dll";
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return L"";
    DWORD written = 0;
    WriteFile(f, data, size, &written, NULL);
    CloseHandle(f);
    return (written == size) ? path : L"";
}

// ---------- 注入 ----------
static bool Inject(DWORD pid, const wchar_t* dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return false;

    char path[MAX_PATH];
    if (wcstombs(path, dllPath, MAX_PATH) == (size_t)-1) { CloseHandle(hProc); return false; }
    SIZE_T len = strlen(path) + 1;

    LPVOID remote = VirtualAllocEx(hProc, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { CloseHandle(hProc); return false; }
    if (!WriteProcessMemory(hProc, remote, path, len, NULL)) {
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE); CloseHandle(hProc); return false;
    }

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLib = GetProcAddress(k32, "LoadLibraryA");
    if (!loadLib) { VirtualFreeEx(hProc, remote, 0, MEM_RELEASE); CloseHandle(hProc); return false; }

    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)loadLib, remote, 0, NULL);
    if (!hThread) {
        DWORD err = GetLastError();
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return err == 0 ? false : false;
    }
    WaitForSingleObject(hThread, INFINITE);
    DWORD code = 0;
    GetExitCodeThread(hThread, &code);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return code != 0;
}

// ---------- 主流程：检查并注入 ----------
static void CheckAndInject() {
    DWORD pid = FindGamePid();
    if (!pid) {
        SetWindowTextW(g_status, L"等待游戏启动... 启动 Beat Cop 后自动注入");
        g_injectedPid = 0;
        return;
    }
    if (IsAlreadyInjected(pid)) {
        g_injectedPid = pid;
        SetWindowTextW(g_status, L"✓ 已注入！UI 已经画进游戏里了");
        return;
    }
    // 未注入：解压 + 注入
    std::wstring dllPath = ExtractDll();
    if (dllPath.empty()) { SetWindowTextW(g_status, L"✗ 解压 DLL 失败"); return; }
    if (Inject(pid, dllPath.c_str())) {
        g_injectedPid = pid;
        SetWindowTextW(g_status, L"✓ 注入成功！UI 已经画进游戏里了");
    } else {
        SetWindowTextW(g_status, L"✗ 注入失败，请以管理员身份运行本程序");
    }
}

// ---------- 界面 ----------
static HBRUSH bgBrush;
static HBRUSH fgWhite;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        bgBrush = CreateSolidBrush(RGB(22, 22, 28));
        fgWhite = CreateSolidBrush(RGB(242, 242, 247));
        g_status = CreateWindowExW(0, L"STATIC", L"正在启动...",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   20, 24, 300, 48, hwnd, (HMENU)IDC_STATUS, NULL, NULL);
        g_btn = CreateWindowExW(0, L"BUTTON", L"重新注入",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                20, 88, 120, 32, hwnd, (HMENU)IDC_BTN_INJECT, NULL, NULL);
        SetTimer(hwnd, 1, 1000, NULL);   // 每秒轮询
        return 0;
    }
    case WM_TIMER:
        CheckAndInject();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_INJECT) {
            SetWindowTextW(g_status, L"正在注入...");
            CheckAndInject();
        }
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)lParam;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(242, 242, 247));
        return (LRESULT)bgBrush;
    }
    case WM_CTLCOLORBTN:
        return (LRESULT)bgBrush;
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(RGB(22, 22, 28));
        FillRect((HDC)wParam, &rc, br);
        DeleteObject(br);
        return 1;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"BeatCopLauncher";
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"BeatCopLauncher", L"Beat Cop 金币修改器",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 360, 170,
                                NULL, NULL, hInst, NULL);
    if (!hwnd) return 0;

    // 底部水印
    CreateWindowExW(0, L"STATIC", L"BearLing特供", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                    20, 122, 300, 18, hwnd, NULL, NULL, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    CheckAndInject();   // 立即检查一次

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DeleteObject(bgBrush);
    DeleteObject(fgWhite);
    return 0;
}
