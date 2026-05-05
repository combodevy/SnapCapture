"""
主应用类 - 协调所有模块的生命周期和信号连接
"""
import os
import gc
from datetime import datetime
from pathlib import Path

from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import QObject, pyqtSignal, QTimer

from src.config import Config
from src.hotkey_manager import HotkeyManager
from src.screenshot_overlay import ScreenshotOverlay
from src.tray_icon import TrayIcon
from src.settings_dialog import SettingsDialog
from src.clipboard_manager import copy_pixmap_to_clipboard


class SnapCaptureApp(QObject):
    """SnapCapture 应用主控制器"""

    def __init__(self):
        super().__init__()

        # 初始化配置
        self.config = Config()

        # 初始化各模块
        self.overlay = ScreenshotOverlay()
        self.tray = TrayIcon()
        self.hotkey_manager = HotkeyManager(self.config)
        self.settings_dialog = None

        # 连接信号
        self._connect_signals()

        # 确保保存目录存在
        save_path = self.config.save_path
        if save_path:
            os.makedirs(save_path, exist_ok=True)

        # 内存清理定时器（每60秒）
        self._gc_timer = QTimer()
        self._gc_timer.timeout.connect(self._gc_collect)
        self._gc_timer.start(60000)

    def _connect_signals(self):
        """连接所有模块之间的信号"""
        # 快捷键 → 截屏
        self.hotkey_manager.hotkey_activated.connect(self._start_capture)

        # 托盘菜单 → 各功能
        self.tray.capture_requested.connect(self._start_capture)
        self.tray.settings_requested.connect(self._show_settings)
        self.tray.quit_requested.connect(self._quit)

        # 截屏结果处理
        self.overlay.screenshot_confirmed.connect(self._on_screenshot_confirmed)
        self.overlay.screenshot_save.connect(self._on_screenshot_save)

    def start(self):
        """启动应用"""
        self.tray.show()
        self.hotkey_manager.start()
        self.tray.show_message(
            "SnapCapture 已启动",
            f"按 {self.config.hotkey.replace('+', ' + ').title()} 开始截屏",
            3000
        )

    def _start_capture(self):
        """触发截屏"""
        # 如果覆盖层已显示，忽略
        if self.overlay.isVisible():
            return
        self.overlay.start_capture()

    def _on_screenshot_confirmed(self, pixmap):
        """截图确认：复制到剪贴板"""
        success = copy_pixmap_to_clipboard(pixmap)
        if success:
            self.tray.show_message("截图完成", "已复制到剪贴板", 1500)
        else:
            self.tray.show_message("截图失败", "复制到剪贴板失败", 2000)

    def _on_screenshot_save(self, pixmap):
        """截图保存：保存到文件 + 复制到剪贴板"""
        # 复制到剪贴板
        copy_pixmap_to_clipboard(pixmap)

        # 保存到文件
        save_dir = self.config.save_path
        os.makedirs(save_dir, exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"SnapCapture_{timestamp}.png"
        filepath = os.path.join(save_dir, filename)

        try:
            pixmap.save(filepath, "PNG")
            self.tray.show_message(
                "截图已保存",
                f"文件: {filename}\n路径: {save_dir}",
                2500
            )
        except Exception as e:
            self.tray.show_message("保存失败", str(e), 3000)

    def _show_settings(self):
        """打开设置对话框"""
        if self.settings_dialog is None:
            self.settings_dialog = SettingsDialog(self.config)
        self.settings_dialog.show()
        self.settings_dialog.raise_()
        self.settings_dialog.activateWindow()

    def _gc_collect(self):
        """定期垃圾回收"""
        gc.collect()

    def _quit(self):
        """退出应用"""
        self.hotkey_manager.stop()
        self.tray.hide()
        self._gc_timer.stop()
        QApplication.quit()
