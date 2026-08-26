// ============================================================
//  ImGui UI：游戏内悬浮面板 + 输入注入
// ============================================================
//  - 用 ImGui 画出"金币修改器"面板——真正画进游戏画面的 UI，
//    点击不会切掉游戏、不会抢焦点；
//  - 中文字体直接加载 C:\Windows\Fonts 下的微软雅黑（退回黑体）；
//  - 鼠标/键盘用 GetCursorPos / GetAsyncKeyState 手动注入，
//    绕开游戏 WndProc（游戏不转发输入时也能点按钮、打字）。
// ============================================================
#include "beatcop.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_dx9.h"
#include <d3d11.h>
#include <d3d9.h>
#include <cstdlib>
#include <cstring>

static bool g_uiReady = false;
static bool g_uiDX11 = true;
static bool g_lastApplyOk = false;
static float g_lastApplyTime = -10.0f;
static HWND g_hwnd = NULL;
static BYTE g_lastKeys[256] = { 0 };

struct SelfWndCtx { DWORD pid; HWND result; };

static BOOL CALLBACK EnumSelfProc(HWND hwnd, LPARAM lp) {
    SelfWndCtx* c = (SelfWndCtx*)lp;
    DWORD wp = 0;
    GetWindowThreadProcessId(hwnd, &wp);
    if (wp == c->pid && IsWindowVisible(hwnd)) { c->result = hwnd; return FALSE; }
    return TRUE;
}

// 找自身进程的主窗口
HWND FindGameWindowSelf() {
    SelfWndCtx ctx = { GetCurrentProcessId(), NULL };
    EnumWindows(EnumSelfProc, (LPARAM)&ctx);
    return ctx.result;
}

// 加载中文字体（微软雅黑，找不到就退回默认字体）
static void LoadChineseFont() {
    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseFull();
    if (ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f, NULL, ranges))
        return;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simhei.ttf", 16.0f, NULL, ranges);
}

// 手动注入鼠标/键盘（绕开游戏 WndProc，游戏不转发输入时仍可用）
static void InjectInput() {
    if (!g_hwnd) return;
    ImGuiIO& io = ImGui::GetIO();
    // 鼠标位置（屏幕→窗口客户区）
    POINT pt;
    if (GetCursorPos(&pt)) {
        ScreenToClient(g_hwnd, &pt);
        io.AddMousePosEvent((float)pt.x, (float)pt.y);
    }
    io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);

    // 键盘：数字 0-9、退格、回车（用 GetAsyncKeyState 读物理键盘，不依赖线程消息队列）
    auto keyState = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    for (int vk = 0x30; vk <= 0x39; ++vk) {
        bool pressed = keyState(vk);
        if (pressed && !g_lastKeys[vk])
            io.AddInputCharacter((unsigned short)vk);   // '0'..'9'
        g_lastKeys[vk] = pressed ? 1 : 0;
    }
    auto keyEv = [&](int vk, ImGuiKey k) {
        bool pressed = keyState(vk);
        if (pressed != (g_lastKeys[vk] != 0))
            io.AddKeyEvent(k, pressed);
        g_lastKeys[vk] = pressed ? 1 : 0;
    };
    keyEv(VK_BACK, ImGuiKey_Backspace);
    keyEv(VK_RETURN, ImGuiKey_Enter);
}

bool UI_InitDX11(HWND hwnd, void* dev, void* ctx) {
    if (g_uiReady) return true;
    g_hwnd = hwnd;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.FrameRounding = 4.0f; st.WindowRounding = 6.0f; st.WindowBorderSize = 1.0f;
    LoadChineseFont();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init((ID3D11Device*)dev, (ID3D11DeviceContext*)ctx);
    g_uiDX11 = true;
    g_uiReady = true;
    return true;
}

bool UI_InitDX9(HWND hwnd, void* dev) {
    if (g_uiReady) return true;
    g_hwnd = hwnd;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.FrameRounding = 4.0f; st.WindowRounding = 6.0f; st.WindowBorderSize = 1.0f;
    LoadChineseFont();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init((IDirect3DDevice9*)dev);
    g_uiDX11 = false;
    g_uiReady = true;
    return true;
}

void UI_Shutdown() {
    if (!g_uiReady) return;
    if (g_uiDX11) ImGui_ImplDX11_Shutdown(); else ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_uiReady = false;
}

static void DrawUIPanel() {
    int money = ReadMoney();

    // 主面板
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 230), ImGuiCond_FirstUseEver);
    ImGui::Begin("Beat Cop 金币修改器", NULL,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    if (money < 0) {
        ImGui::TextColored(ImVec4(1, 0.7f, 0.4f, 1), "金币指针尚未生效...");
        ImGui::Text("进入任务界面后自动可用");
        ImGui::End();
        return;
    }

    ImGui::Text("当前金币:  ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1, 1), "%d", money);

    ImGui::Spacing();
    static char buf[32] = "";
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("新金币", buf, sizeof(buf), ImGuiInputTextFlags_CharsDecimal);

    if (ImGui::Button("修 改") || (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
        long long val = _atoi64(buf);
        if (val > 0 && WriteMoney((int)val)) {
            g_lastApplyOk = true;
            g_lastApplyTime = (float)ImGui::GetTime();
            buf[0] = 0;
        } else {
            g_lastApplyOk = false;
            g_lastApplyTime = (float)ImGui::GetTime();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("重置为当前值")) { buf[0] = 0; }

    if ((float)ImGui::GetTime() - g_lastApplyTime < 2.0f) {
        ImGui::TextColored(g_lastApplyOk ? ImVec4(0.3f, 0.9f, 0.5f, 1) : ImVec4(1, 0.4f, 0.4f, 1),
                           g_lastApplyOk ? "✓ 已修改" : "✗ 写入失败");
    }

    ImGui::End();

    // 右下角水印
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 140, io.DisplaySize.y - 34), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.25f);
    ImGui::Begin("##wm", NULL,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.7f, 0.9f), "BearLing特供");
    ImGui::End();
}

void UI_RenderFrame() {
    if (!g_uiReady) return;
    if (g_uiDX11) ImGui_ImplDX11_NewFrame(); else ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    InjectInput();          // 手动注入鼠标/键盘，确保能点输入框
    ImGui::NewFrame();
    DrawUIPanel();
    ImGui::Render();
    if (g_uiDX11) ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    else ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}
