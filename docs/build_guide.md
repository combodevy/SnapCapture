# SnapCapture 构建指南

本文档介绍如何从源代码构建 SnapCapture 可执行程序和安装包。

## 环境要求

- **操作系统**: Windows 10 及以上
- **Python**: 3.10 或更高版本
- **NSIS**: 3.x（仅生成安装程序时需要）

## 步骤一：准备环境

```bash
# 克隆代码
git clone https://github.com/YOUR_USERNAME/SnapCapture.git
cd SnapCapture

# 创建虚拟环境（推荐）
python -m venv venv
venv\Scripts\activate

# 安装依赖
pip install -r requirements.txt

# 安装 PyInstaller（如果 requirements.txt 中已包含则跳过）
pip install pyinstaller
```

## 步骤二：生成图标

```bash
python generate_icon.py
```

这将在 `src/resources/icons/` 下生成 `app.ico` 和 `app.png`。

## 步骤三：打包为可执行文件

```bash
python build.py
```

构建成功后，可执行文件位于 `dist/SnapCapture/SnapCapture.exe`。

### 打包选项说明

构建脚本使用 PyInstaller 的 `--onedir` 模式（目录模式），相比 `--onefile` 模式：
- ✅ 启动速度更快
- ✅ 更新时只需替换变化的文件
- ❌ 文件数量较多

## 步骤四：生成安装程序（可选）

1. 安装 [NSIS](https://nsis.sourceforge.io/Download)
2. 确保已完成步骤三（打包为可执行文件）
3. 运行：

```bash
# 方式一：命令行
makensis installer\installer.nsi

# 方式二：右键 installer.nsi → 选择 "Compile NSIS Script"
```

4. 安装程序生成于 `dist/SnapCapture_Setup.exe`

## 目录结构说明

```
构建后:
dist/
├── SnapCapture/          # PyInstaller 输出目录
│   ├── SnapCapture.exe   # 主程序
│   ├── src/resources/    # 资源文件
│   └── ...               # 运行时依赖
└── SnapCapture_Setup.exe # NSIS 安装程序（步骤四）
```

## 故障排除

### PyInstaller 打包失败
- 确认所有依赖已安装：`pip list`
- 查看 `build/` 目录下的日志文件
- 尝试添加 `--debug all` 参数获取详细信息

### 运行时找不到模块
- 检查 `build.py` 中的 `--hidden-import` 参数
- 确保资源文件已正确添加到 `--add-data`

### 图标不显示
- 确认 `generate_icon.py` 已成功运行
- 检查 `src/resources/icons/app.ico` 文件是否存在
