# SnapCapture 截屏工具

<p align="center">
  <strong>轻量级 Windows 专业截屏工具</strong>
</p>

## ✨ 功能特性

- 🎯 **精准截屏** — 矩形区域选择，实时显示分辨率（物理像素）
- 🔍 **放大镜预览** — 鼠标附近放大显示，辅助精确定位
- ⌨️ **全局快捷键** — 可自定义快捷键，系统全局响应
- 📋 **一键复制** — 截图自动复制到剪贴板
- 💾 **自定义保存** — 支持自定义保存路径
- 🖥️ **系统托盘** — 后台静默运行，托盘图标快速访问
- 🚀 **开机自启** — 可选开机自动启动
- 📐 **高DPI支持** — 完美适配 4K/高分辨率显示器
- 🖥️ **多显示器** — 支持多显示器截屏

## 📷 快速使用

1. 启动 SnapCapture 后，程序驻留在系统托盘
2. 按下 `Ctrl + Shift + A`（默认快捷键）开始截屏
3. 鼠标拖拽选择截图区域
4. 点击 ✓ 复制到剪贴板，或点击 💾 保存到文件
5. 按 `ESC` 取消截屏

## 🔧 系统要求

- Windows 10 及以上
- Python 3.10+（从源码运行时）

## 📦 安装

### 方式一：安装程序
下载 `SnapCapture_Setup.exe` 并运行安装向导。

### 方式二：从源码运行

```bash
# 克隆仓库
git clone https://github.com/YOUR_USERNAME/SnapCapture.git
cd SnapCapture

# 安装依赖
pip install -r requirements.txt

# 运行
python -m src.main
```

## 🏗️ 构建

```bash
# 安装依赖
pip install -r requirements.txt

# 生成图标
python generate_icon.py

# 打包为 exe
python build.py

# 输出位于 dist/SnapCapture/SnapCapture.exe
```

### 生成安装程序
1. 安装 [NSIS](https://nsis.sourceforge.io/)
2. 先执行 `python build.py` 完成打包
3. 运行 `makensis installer/installer.nsi`
4. 安装程序生成于 `dist/SnapCapture_Setup.exe`

## ⚙️ 配置

配置文件位于 `%APPDATA%/SnapCapture/config.json`

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `hotkey` | `ctrl+shift+a` | 全局截屏快捷键 |
| `save_path` | `~/Pictures/SnapCapture` | 截图保存路径 |
| `auto_start` | `false` | 开机自启动 |

## 📁 项目结构

```
SnapCapture/
├── src/
│   ├── main.py                 # 应用入口
│   ├── app.py                  # 主应用控制器
│   ├── config.py               # 配置管理
│   ├── screenshot_overlay.py   # 截屏覆盖层（核心）
│   ├── magnifier.py            # 放大镜预览
│   ├── toolbar.py              # 操作工具栏
│   ├── hotkey_manager.py       # 全局快捷键
│   ├── tray_icon.py            # 系统托盘
│   ├── settings_dialog.py      # 设置对话框
│   ├── clipboard_manager.py    # 剪贴板管理
│   ├── autostart.py            # 自启动管理
│   └── resources/icons/        # 图标资源
├── installer/
│   └── installer.nsi           # NSIS 安装脚本
├── docs/
│   ├── user_manual.md          # 用户手册
│   └── build_guide.md          # 构建指南
├── build.py                    # 构建脚本
├── requirements.txt            # Python 依赖
└── README.md
```

## 📝 许可证

MIT License

## 🙏 致谢

- [PyQt6](https://www.riverbankcomputing.com/software/pyqt/) — GUI 框架
- [mss](https://github.com/BoboTiG/python-mss) — 高速屏幕截取
- [keyboard](https://github.com/boppreh/keyboard) — 全局键盘监听
