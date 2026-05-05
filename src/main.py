"""
SnapCapture - 专业截屏工具
应用入口点
"""
import sys
import os

# 高 DPI 支持（必须在创建 QApplication 之前设置）
os.environ["QT_ENABLE_HIGHDPI_SCALING"] = "1"

from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QGuiApplication

from src.app import SnapCaptureApp


def check_single_instance():
    """检查是否已有实例运行（简单的文件锁方式）"""
    import tempfile
    lock_file = os.path.join(tempfile.gettempdir(), "snapcapture.lock")
    try:
        if os.path.exists(lock_file):
            # 检查文件是否过期（超过10秒视为过期）
            import time
            if time.time() - os.path.getmtime(lock_file) > 10:
                os.remove(lock_file)
            else:
                # 尝试读取 PID 判断进程是否存在
                try:
                    with open(lock_file, "r") as f:
                        pid = int(f.read().strip())
                    import ctypes
                    kernel32 = ctypes.windll.kernel32
                    handle = kernel32.OpenProcess(0x0400, False, pid)
                    if handle:
                        kernel32.CloseHandle(handle)
                        return False  # 已有实例运行
                    else:
                        os.remove(lock_file)
                except (ValueError, OSError):
                    os.remove(lock_file)

        # 写入当前 PID
        with open(lock_file, "w") as f:
            f.write(str(os.getpid()))
        return True
    except OSError:
        return True


def cleanup_lock():
    """清理锁文件"""
    import tempfile
    lock_file = os.path.join(tempfile.gettempdir(), "snapcapture.lock")
    try:
        os.remove(lock_file)
    except OSError:
        pass


def main():
    # 单实例检查
    if not check_single_instance():
        print("[SnapCapture] 已有一个实例在运行")
        sys.exit(0)

    # 创建应用
    app = QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)  # 关闭窗口不退出，保持托盘运行
    app.setApplicationName("SnapCapture")
    app.setApplicationVersion("1.0.0")

    # 高 DPI 设置
    QApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
    )

    # 启动主应用
    snap_app = SnapCaptureApp()
    snap_app.start()

    # 运行事件循环
    try:
        exit_code = app.exec()
    finally:
        cleanup_lock()

    sys.exit(exit_code)


if __name__ == "__main__":
    main()
