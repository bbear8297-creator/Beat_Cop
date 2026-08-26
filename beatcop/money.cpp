// ============================================================
//  金币读写逻辑（DLL 注入后，在游戏进程内直接访问内存）
// ============================================================
//  原理：mono.dll + 0x001F62CC 处存着一个指针，按偏移链
//        0x54 → 0x2D8 → 0x0 → 0x14 → 0x24 逐层解引用，
//        最后一层偏移只做加法，得到金币地址。
//  ★ 游戏更新后偏移会失效：用 Cheat Engine 重新找，改下面两处即可：
//      - mb + 0x001F62CC            （静态地址，在 ResolveMoneyAddr 里）
//      - offs[] = { 0x54, 0x2D8, ...}（偏移链，顺序不能颠倒）
// ============================================================
#include "beatcop.h"
#include <tlhelp32.h>
#include <cstdio>
#include <cstdarg>

// 调试日志：追加到 %TEMP%\beatcop.log
void LogMsg(const char* fmt, ...) {
    static char path[MAX_PATH] = "";
    if (!path[0]) {
        char tmp[MAX_PATH] = "";
        DWORD n = GetTempPathA(MAX_PATH, tmp);
        if (n && n < MAX_PATH - 20)
            snprintf(path, MAX_PATH, "%sbeatcop.log", tmp);
        else
            snprintf(path, MAX_PATH, "beatcop.log");
    }
    FILE* f = fopen(path, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

// 自身进程的模块基址（注入 DLL 后，GetCurrentProcessId() 就是游戏 PID）
uintptr_t GetModuleBaseSelf(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me{ sizeof(me) };
    uintptr_t result = 0;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(name, me.szModule) == 0) { result = (uintptr_t)me.modBaseAddr; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return result;
}

// 解析金币地址：mono.dll + 0x001F62CC 先解引用，再逐层"加偏移→解引用"，
// 最后一层偏移只做加法。游戏 32 位，指针 4 字节。
uintptr_t ResolveMoneyAddr() {
    uintptr_t mb = GetModuleBaseSelf(L"mono.dll");
    if (!mb) return 0;

    uintptr_t addr = mb + 0x001F62CC;
    if (IsBadReadPtr((const void*)addr, 4)) return 0;
    DWORD p = *(DWORD*)addr;            // 基址解引用
    if (!p) return 0;
    addr = p;

    static const DWORD offs[] = { 0x54, 0x2D8, 0x0, 0x14, 0x24 };
    for (int i = 0; i < 5; ++i) {
        if (i == 4) { addr += offs[i]; break; }
        DWORD readAddr = (DWORD)(addr + offs[i]);
        if (IsBadReadPtr((const void*)(uintptr_t)readAddr, 4)) return 0;
        DWORD q = *(DWORD*)readAddr;
        if (!q) return 0;
        addr = q;
    }
    return addr;
}

int ReadMoney() {
    uintptr_t a = ResolveMoneyAddr();
    if (!a) return -1;
    if (IsBadReadPtr((const void*)a, 4)) return -1;
    return *(int*)a;
}

bool WriteMoney(int val) {
    uintptr_t a = ResolveMoneyAddr();
    if (!a) return false;
    if (IsBadWritePtr((void*)a, 4)) return false;
    *(int*)a = val;
    return true;
}
