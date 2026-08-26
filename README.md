# Beat Cop 金币修改器

基于 **DLL 注入 + ImGui** 的 Beat Cop（Unity 5.6，32 位）游戏内存修改工具：把金币修改面板直接绘制进游戏画面（由游戏自身的 D3D11 渲染），不抢占焦点、不打断操作。

## 直接使用（推荐，无需任何环境）

**仓库里已附构建好的成品 `Beat Cop.exe`**（单文件，DLL 已内嵌），下载后直接可用：

- 只需要 **Windows 10**，不需要安装任何东西（不需要 MSYS2、不需要 .NET、不需要额外 DLL）
- 使用方法：
  1. 启动游戏（Beat Cop）
  2. 右键 `Beat Cop.exe` → **以管理员身份运行**
  3. 自动检测并注入 → 游戏画面左上角出现修改面板
  4. 输入金币数量 → 点击「修 改」或按回车

> 注入器会每秒轮询：游戏中途关闭再重开时自动再次注入。

## 从源码构建（可选）

**只有想自己编译/改代码的人才需要这一步**；直接使用完全不需要。

构建需要 **MSYS2**（为什么：项目用 GCC 编译，MSYS2 是 Windows 上最方便的 GCC 工具链分发。它只参与"编译"这一步，与运行时无关——成品 exe 不依赖 MSYS2 的任何文件）。

### 环境准备（一次性）

1. 安装 [MSYS2](https://www.msys2.org)（安装包约几十 MB，装完即可）
2. 在 MSYS2 终端安装 32 位工具链：

   ```bash
   pacman -S mingw-w64-i686-gcc
   ```

### 编译（双击脚本即可）

脚本会自动查找 MSYS2（装到 `D:\msys2`、`C:\msys2` 或 `%LOCALAPPDATA%\Programs\msys2` 均可），项目文件夹可放在任意位置：

| 脚本 | 产物 | 说明 |
|------|------|------|
| `build_launcher.bat` | `Beat Cop.exe` | 一键打包单文件版（推荐） |
| `build_dll.bat` | `beatcop/beatcop.dll` | 只编译注入 DLL |
| `build_injector.bat` | `injector.exe` | 命令行注入器（备用） |

> 打包前请先关闭游戏（DLL 被占用会导致编译跳过 DLL 一步；DLL 缺失时脚本会明确报错，不会产出损坏的 exe）。

## 文件结构

```
Beat Cop/
├── Beat Cop.exe          ★ 已构建好的成品（可直接下载使用）
├── launcher.cpp          单文件注入器源码（主程序，GUI）
├── injector.cpp          命令行注入器源码（备用工具）
├── resource.rc           资源脚本：把 DLL 以 RCDATA 资源嵌入 exe
├── build_launcher.bat    ★ 一键打包：编译 DLL → 嵌入资源 → 生成单文件 exe
├── build_dll.bat         只编译注入 DLL
├── build_injector.bat    编译命令行注入器
├── README.md             本文档
└── beatcop/              注入 DLL 源码工程
    ├── dllmain.cpp       DLL 入口：D3D11 Present / D3D9 EndScene 挂钩
    ├── money.cpp         金币读写逻辑（游戏更新后改偏移的位置）
    ├── ui.cpp            ImGui 界面 + 鼠标键盘输入注入
    ├── beatcop.h         公共头文件
    └── imgui/            ImGui 开源库（v1.90.9，构建依赖）
```

> 除 `Beat Cop.exe`（成品，入库）外，`*.dll` / `*.o` 等构建产物均被 `.gitignore` 排除，由脚本自动生成。

## 使用说明（面板）

```
┌──────────────────────────────┐
│ Beat Cop 金币修改器      [×] │
│ 当前金币: 10000151            │
│ [新金币: 输入数字      ]      │
│ [修 改]  [重置为当前值]        │
│                              │
│ ✓ 已修改                     │
└──────────────────────────────┘
```

- **当前金币**：实时读取游戏中的金币数
- **新金币**：输入要改成的数字
- **修 改**：写入游戏（或按回车）；**重置为当前值**：清空输入框

> 金币指针在进入任务界面后才有效；未生效时面板会提示"金币指针尚未生效..."。

## 配置（偏移 / 目标游戏）

金币偏移配置在 `beatcop/money.cpp`：

```cpp
uintptr_t addr = mb + 0x001F62CC;                                // mono.dll + 0x001F62CC
static const DWORD offs[] = { 0x54, 0x2D8, 0x0, 0x14, 0x24 };    // 偏移链（CE 顺序）
```

目标游戏进程名在 `launcher.cpp`、`injector.cpp` 中搜索 `BeatCop.exe` 修改。

## 常见问题

| 问题 | 原因 / 解决 |
|------|------------|
| 提示"找不到 BeatCop.exe" | 游戏未启动，先启动游戏 |
| 提示"需要管理员" | 右键 → 以管理员身份运行 |
| 提示"解压 DLL 失败" | `resource.rc` 的 `IDR_BEATCOP_DLL` 必须为 101（与 launcher.cpp 一致）；或 DLL 缺失（重新运行 `build_launcher.bat`） |
| 注入成功但无 UI | 确认游戏使用 D3D11 / D3D9 |
| 面板显示"金币指针尚未生效..." | 进入任务界面后自动可用 |
| 修改后金币未变化 | 偏移失效（游戏更新），重新定位偏移 |
| 编译找不到编译器 | 未安装 MSYS2 或 32 位工具链（仅编译需要） |

调试日志：`%TEMP%\beatcop.log`（记录挂钩与 ImGui 初始化过程）。

## 更新偏移（游戏更新后）

1. 使用 Cheat Engine 附加到游戏进程
2. 定位金币地址 → 指针扫描
3. 将静态地址（`mono.dll + 偏移`）与偏移链按顺序填入 `beatcop/money.cpp`
4. 重新编译 DLL 并打包

> 偏移顺序不可颠倒，`offs[0]` 最先应用。

## 技术原理

- **注入**：`CreateRemoteThread` + `LoadLibraryA` 将 DLL 注入游戏进程（注入器须为 32 位，以获取 32 位进程内 `kernel32.LoadLibraryA` 地址）。
- **挂钩**：创建虚拟 D3D11 设备获取 `IDXGISwapChain` 的共享 vtable，改写其 `Present`（第 8 项）；D3D9 同理挂钩 `EndScene`（第 42 项）。
- **渲染**：每次 `Present` 调用时以 ImGui 渲染面板。
- **输入**：以 `GetCursorPos` / `GetAsyncKeyState` 手动注入鼠标键盘状态，绕开游戏 WndProc。
- **改金币**：注入后于进程内解引用 `mono.dll+0x001F62CC` → 偏移链 → 读写。

## 第三方依赖

- [ImGui](https://github.com/ocornut/imgui) v1.90.9（MIT License），位于 `beatcop/imgui/`。
