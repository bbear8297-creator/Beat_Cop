#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>

// ---------------- 金币逻辑（进程内直接读写） ----------------
// 取自身进程指定模块基址
uintptr_t GetModuleBaseSelf(const wchar_t* name);
// 解析金币地址（mono.dll + 0x001F62CC，偏移 54 2D8 0 14 24）
uintptr_t ResolveMoneyAddr();
// 读取当前金币；失败返回 -1
int  ReadMoney();
// 写入金币；成功返回 true
bool WriteMoney(int val);

// ---------------- UI（ImGui） ----------------
// 初始化（D3D11 版）
bool UI_InitDX11(HWND hwnd, void* dev, void* ctx);
// 初始化（D3D9 版）
bool UI_InitDX9(HWND hwnd, void* dev);
// 每帧渲染
void UI_RenderFrame();
// 卸载
void UI_Shutdown();
// 找自身进程的主窗口
HWND FindGameWindowSelf();

// 调试日志（写到 C:\beatcop.log）
void LogMsg(const char* fmt, ...);
