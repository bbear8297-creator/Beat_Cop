// ============================================================
//  beatcop.dll 入口：D3D11 / D3D9 的 Present / EndScene 挂钩
// ============================================================
//  被注入到游戏进程后：
//   1) DllMain 启动 HookThread，等待游戏加载 d3d11.dll / d3d9.dll；
//   2) 创建"虚拟"D3D11/D3D9 设备，拿到 IDXGISwapChain /
//      IDirect3DDevice9 的 vtable（所有同类对象共享同一份 vtable）；
//   3) 直接改写共享 vtable 里的 Present（第 8 项）/ EndScene（第 42 项）
//      为我们的函数——不用扫描进程内存找对象（扫描易误判导致崩溃）；
//   4) 每次 Present / EndScene 被调用时，执行 UI_RenderFrame() 画 UI。
//  渲染内容见 ui.cpp，金币读写见 money.cpp。
// ============================================================
#include "beatcop.h"
#include <d3d11.h>
#include <dxgi.h>
#include <d3d9.h>

// ==================== D3D11 Present 挂钩 ====================
typedef HRESULT(STDMETHODCALLTYPE* PresentFn)(IDXGISwapChain*, UINT, UINT);
static PresentFn oPresent = NULL;
static bool g_hooked11 = false;

HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* chain, UINT sync, UINT flags) {
    if (!g_hooked11) {
        g_hooked11 = true;
        LogMsg("[present] 第一次 Present，初始化 ImGui...");
        ID3D11Device* dev = NULL;
        ID3D11DeviceContext* ctx = NULL;
        if (chain->GetDevice(IID_ID3D11Device, (void**)&dev) == S_OK && dev) {
            dev->GetImmediateContext(&ctx);
            bool ok = UI_InitDX11(FindGameWindowSelf(), dev, ctx);
            LogMsg("[present] UI_InitDX11 = %s", ok ? "OK" : "FAIL");
        } else {
            LogMsg("[present] GetDevice 失败");
        }
    }
    UI_RenderFrame();
    return oPresent(chain, sync, flags);
}

// ==================== D3D9 EndScene 挂钩 ====================
typedef HRESULT(STDMETHODCALLTYPE* EndSceneFn)(IDirect3DDevice9*);
static EndSceneFn oEndScene = NULL;
static bool g_hooked9 = false;

HRESULT STDMETHODCALLTYPE hkEndScene(IDirect3DDevice9* dev) {
    if (!g_hooked9) {
        g_hooked9 = true;
        UI_InitDX9(FindGameWindowSelf(), dev);
    }
    UI_RenderFrame();
    return oEndScene(dev);
}

// ==================== vtable 挂钩 ====================
// 所有同接口对象（如 IDXGISwapChain）共享同一个 vtable，直接改 vtable 里的
// 函数指针即可 hook 所有对象，无需扫描进程内存找对象（扫描易误判导致崩溃）
static void PatchVtableEntry(void** vtbl, int index, DWORD newFn) {
    DWORD oldProt;
    VirtualProtect(&vtbl[index], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProt);
    vtbl[index] = (void*)newFn;
    VirtualProtect(&vtbl[index], sizeof(DWORD), oldProt, &oldProt);
}

// ==================== 安装 D3D11 挂钩 ====================
static bool HookDX11() {
    if (oPresent) return true;
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 8, 8, NULL, NULL, NULL, NULL);
    ID3D11Device* dummyDev = NULL;
    ID3D11DeviceContext* dummyCtx = NULL;
    IDXGISwapChain* dummyChain = NULL;
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width = 8; sd.BufferDesc.Height = 8;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferCount = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
                                               NULL, 0, D3D11_SDK_VERSION, &sd,
                                               &dummyChain, &dummyDev, NULL, &dummyCtx);
    if (FAILED(hr))   // WARP 兜底
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0,
                                           NULL, 0, D3D11_SDK_VERSION, &sd,
                                           &dummyChain, &dummyDev, NULL, &dummyCtx);
    if (FAILED(hr)) { if (hwnd) DestroyWindow(hwnd); LogMsg("[hook] 创建 dummy 交换链失败 hr=%08X", (unsigned)hr); return false; }

    void** vtbl = *(void***)dummyChain;
    DWORD dummyVtbl = (DWORD)(uintptr_t)vtbl;
    oPresent = (PresentFn)vtbl[8];   // IDXGISwapChain::Present = vtable[8]
    LogMsg("[hook] dummyVtbl=%08X  Present=%08X", dummyVtbl, (DWORD)(uintptr_t)vtbl[8]);

    dummyChain->Release(); dummyDev->Release(); dummyCtx->Release();
    if (hwnd) DestroyWindow(hwnd);

    // 共享 vtable 直接挂钩（所有交换链都走这里）
    PatchVtableEntry(vtbl, 8, (DWORD)(uintptr_t)hkPresent);
    LogMsg("[hook] D3D11 挂钩完成");
    return true;
}

// ==================== 安装 D3D9 挂钩 ====================
static bool HookDX9() {
    if (oEndScene) return true;
    HMODULE m9 = GetModuleHandleW(L"d3d9.dll");
    if (!m9) return false;
    typedef IDirect3D9* (WINAPI* Create9Fn)(UINT);
    Create9Fn create9 = (Create9Fn)GetProcAddress(m9, "Direct3DCreate9");
    if (!create9) return false;
    IDirect3D9* d9 = create9(D3D_SDK_VERSION);
    if (!d9) return false;

    HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 8, 8, NULL, NULL, NULL, NULL);
    IDirect3DDevice9* dummyDev = NULL;
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferFormat = D3DFMT_A8R8G8B8; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE;
    HRESULT hr = d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dummyDev);
    if (FAILED(hr))
        hr = d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                              D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dummyDev);
    if (FAILED(hr)) { d9->Release(); if (hwnd) DestroyWindow(hwnd); return false; }

    void** vtbl = *(void***)dummyDev;
    DWORD dummyVtbl = (DWORD)(uintptr_t)vtbl;
    oEndScene = (EndSceneFn)vtbl[42];   // IDirect3DDevice9::EndScene = vtable[42]

    dummyDev->Release(); d9->Release(); if (hwnd) DestroyWindow(hwnd);

    // 共享 vtable 直接挂钩（所有设备都走这里）
    PatchVtableEntry(vtbl, 42, (DWORD)(uintptr_t)hkEndScene);
    return true;
}

// ==================== 挂钩线程 ====================
static DWORD WINAPI HookThread(LPVOID) {
    LogMsg("[dll] hook 线程启动");
    for (int i = 0; i < 400; ++i) {   // 最多等 20 秒
        if (GetModuleHandleW(L"d3d11.dll")) {
            LogMsg("[dll] 检测到 d3d11.dll，尝试 HookDX11");
            if (HookDX11()) return 0;
            LogMsg("[dll] HookDX11 失败，继续等 d3d9");
        }
        if (GetModuleHandleW(L"d3d9.dll"))  { if (HookDX9()) return 0; }
        Sleep(50);
    }
    LogMsg("[dll] 20 秒超时，hook 未装好");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        LogMsg("[dll] DLL_PROCESS_ATTACH");
        DisableThreadLibraryCalls(hMod);
        HANDLE h = CreateThread(NULL, 0, HookThread, NULL, 0, NULL);
        if (h) CloseHandle(h);
    } else if (reason == DLL_PROCESS_DETACH) {
        UI_Shutdown();
    }
    return TRUE;
}
