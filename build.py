"""
PyInstaller 构建脚本 - 打包 SnapCapture 为 Windows 可执行文件
"""
import os
import sys
import subprocess
import shutil

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(PROJECT_DIR, "src")
ICON_PATH = os.path.join(SRC_DIR, "resources", "icons", "app.ico")
DIST_DIR = os.path.join(PROJECT_DIR, "dist")
BUILD_DIR = os.path.join(PROJECT_DIR, "build")


def build():
    """执行 PyInstaller 打包"""
    print("=" * 50)
    print("  SnapCapture 构建工具")
    print("=" * 50)

    # 检查图标文件
    icon_arg = ""
    if os.path.exists(ICON_PATH):
        icon_arg = f"--icon={ICON_PATH}"
        print(f"  图标: {ICON_PATH}")
    else:
        print("  警告: 未找到图标文件，将使用默认图标")

    # 资源文件
    resources_dir = os.path.join(SRC_DIR, "resources")
    add_data = []
    if os.path.exists(resources_dir):
        add_data.append(f"--add-data={resources_dir};src/resources")

    # 构建命令
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--name=SnapCapture",
        "--windowed",           # 无控制台窗口
        "--onedir",             # 目录模式（启动更快）
        "--noconfirm",          # 覆盖已有输出
        "--clean",              # 清理临时文件
        f"--distpath={DIST_DIR}",
        f"--workpath={BUILD_DIR}",
    ]

    if icon_arg:
        cmd.append(icon_arg)

    for data in add_data:
        cmd.append(data)

    # 隐藏导入
    cmd.extend([
        "--hidden-import=mss",
        "--hidden-import=keyboard",
        "--hidden-import=PIL",
    ])

    # 入口点
    cmd.append(os.path.join(SRC_DIR, "main.py"))

    print(f"\n  命令: {' '.join(cmd)}\n")
    print("  开始构建...\n")

    result = subprocess.run(cmd, cwd=PROJECT_DIR)

    if result.returncode == 0:
        exe_path = os.path.join(DIST_DIR, "SnapCapture", "SnapCapture.exe")
        print(f"\n  [OK] Build Success!")
        print(f"  Output: {exe_path}")
        if os.path.exists(exe_path):
            size_mb = os.path.getsize(exe_path) / 1024 / 1024
            print(f"  Size: {size_mb:.1f} MB")
    else:
        print(f"\n  [FAIL] Build Failed (code: {result.returncode})")
        sys.exit(1)


if __name__ == "__main__":
    build()
