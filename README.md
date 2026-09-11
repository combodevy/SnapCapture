# SnapCapture (CaptureTool)

[English Version](README_EN.md)

SnapCapture 是一款基于 C++11、Win32 API 和 GDI/GDI+ 实现的 Windows 轻量级原生截图与标注工具。

## 项目特点

*   **轻量无依赖**：纯 Win32 SDK 与 GDI+ 绘制，无第三方 UI 框架依赖，可静态编译为单一的独立可执行文件（约 2.6 MB）。
*   **内存开销小**：支持双缓冲渲染与局部刷新，运行内存占用约为 2.5 MB。
*   **DPI 感知适配**：支持系统级高 DPI 缩放与多监视器环境，确保截图与标注的高清渲染。

## 核心功能

1.  **截图模式与区域选择**
    *   **窗口探测**：利用 DWM (Desktop Window Manager) 接口获取精确的窗口物理边界（排除了系统阴影），支持悬停高亮和一键捕获。
    *   **自由框选**：支持鼠标左键拖拽框选任意区域，并支持八向手柄缩放和全局平移。
2.  **矢量标注功能**
    *   **基础图形**：支持画笔自由绘制、矩形（圆角半径可自定义且持久化）、椭圆、矢量箭头。
    *   **属性调节**：可在浮动工具栏中实时调节画笔粗细与标注颜色（提供预设颜色和自定义取色板）。
    *   **撤销支持**：支持以历史栈形式逐步撤销已添加的标注。
3.  **文本标注引擎**
    *   **内联编辑**：支持双击已添加的文本框直接原地进入编辑状态。支持键盘左右方向键、Home/End 键移动光标以及 Backspace/Delete 退格和删除，支持任意中英文插入。
    *   **样式与调整**：支持通过系统字体对话框自定义选择字体名、字号及粗斜体；支持左上、右上、左下等控制点等比缩放文本大小，或通过右下角旋转控制点在 0°~360° 间任意旋转。
4.  **系统与保存选项**
    *   **全局快捷键**：支持自定义配置键盘组合键或鼠标侧键（Mouse4/Mouse5）唤起截图，支持按键吞噬（Suppress）以防与其他应用冲突。
    *   **剪贴板与保存**：点击确定后自动将截图以 `CF_DIB`（传统位图）和 `PNG`（保留透明通道）双格式存入剪贴板，支持另存为本地 PNG 文件。
    *   **开机自启动**：可通过设置面板写入注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 实现自启动（无需管理员权限）。

## 配置文件 (config.json)

配置文件将自动生成在程序同级目录下，包含以下关键字段：

```json
{
  "hotkey": {
    "type": "mouse",           // 触发类型: "keyboard" 或 "mouse"
    "keys": [],                // 键盘快捷键组合 (例如 ["ctrl", "shift", "a"])
    "mouse_button": "x1",      // 鼠标侧键名称 (例如 "x1" 或 "x2")
    "suppress": true           // 是否阻止该快捷键被传递给其他应用程序
  },
  "auto_start": true,          // 是否开机自启动
  "save_to_clipboard": true,   // 是否在截图后复制到剪贴板
  "save_directory": "",        // 点击确定后自动保存 PNG 的目录
  "notification": true,        // 是否显示托盘通知
  "round_radius": 10,          // 矩形标注的圆角半径 (0-30 像素)
  "capture_mode": "region"     // 默认截图模式: "region"(区域) 或 "window"(窗口)
}
```

## 编译方法

### 环境要求
*   **操作系统**：Windows 7 或更高版本
*   **编译器**：支持 C++11 标准的 MinGW-w64 11.x 或更高版本 GCC
*   **构建环境**：PowerShell

### 编译步骤
1. 打开 PowerShell 终端，进入项目根目录。
2. 运行构建脚本：
   ```powershell
   .\build.ps1
   ```

### 构建脚本参数

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `-OutputName` | 输出的可执行文件名 | `CaptureTool.exe` |
| `-Architecture` | 目标架构，可选 `x64` 或 `x86` | `x64` |
| `-Strip` | 链接后剥离符号表，减小体积 | 关闭 |
| `-Run` | 编译成功后立即启动程序 | 关闭 |

示例：

```powershell
.\build.ps1 -Strip -Run
```

### 编译命令说明

脚本内部等价于执行：

```powershell
g++ -std=c++11 -O3 -mwindows -static main.cpp -lgdi32 -lgdiplus -lshlwapi -luser32 -lshell32 -lole32 -lcomdlg32 -ldwmapi -o CaptureTool.exe
```

*   `-O3`：开启编译器最大优化。
*   `-static`：静态链接运行时库，使生成文件独立运行。
*   `-mwindows`：隐藏命令行窗口。

## 自动构建与发布

项目使用 GitHub Actions 完成持续集成与发布：

*   **Build Check**（`.github/workflows/build.yml`）：每次 push / PR 都会在 `windows-latest` + MinGW-w64 环境中真实编译一次，并上传 `CaptureTool.exe` 作为构建产物，确保改动不会破坏编译。
*   **Release**（`.github/workflows/release.yml`）：推送 `v*` 标签时自动编译、剥离符号，并创建 GitHub Release，附件为 `CaptureTool.exe`。

发布新版本：

```powershell
git tag v1.0.0
git push origin v1.0.0
```

也可以在 GitHub 上手动运行 Release 工作流并指定 tag。

> 注意：可执行文件不再提交进仓库，请从 Releases 页面或 Actions 构建产物中获取。

## 使用说明

1.  **运行**：双击启动 `CaptureTool.exe`，程序将在系统托盘后台运行。
2.  **唤起截图**：按下配置的快捷键（默认鼠标侧键 X1）即可开始截图。
3.  **截图操作**：
    *   在目标窗口上移动，悬停时会自动探测并高亮，左键点击即可完成该窗口截图。
    *   在任意区域拖拽可手动拉出截图框。
4.  **标注编辑**：
    *   在下方工具栏选择矩形、圆形、箭头、画笔、文字工具，拖拽即可进行标注。
    *   双击文本框可原地编辑文字，使用手柄可以平移、缩放或旋转标注元素。
5.  **输出**：
    *   **确定（勾号）**：按配置复制到剪贴板；若设置了自动保存目录，则同时保存 PNG 到该目录。
    *   **保存（软盘）**：弹出保存对话框另存为 PNG，并按配置决定是否同时复制到剪贴板。
    *   **取消（叉号）/ Esc 键**：退出截图不保存。

## 许可证

本项目基于 [MIT License](LICENSE) 开源。
