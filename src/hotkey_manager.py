"""
全局快捷键管理模块 - 使用 keyboard 库实现系统级热键监听
"""
import keyboard
from PyQt6.QtCore import QObject, pyqtSignal, QThread


class HotkeyListener(QThread):
    """在独立线程中运行 keyboard 监听，避免阻塞主线程"""

    hotkey_triggered = pyqtSignal()

    def __init__(self):
        super().__init__()
        self._hotkey_str = ""
        self._running = False

    def set_hotkey(self, hotkey_str: str):
        """设置/更新热键"""
        # 先移除旧的
        self._unregister()
        self._hotkey_str = hotkey_str
        self._register()

    def _register(self):
        """注册热键"""
        if self._hotkey_str:
            try:
                keyboard.add_hotkey(
                    self._hotkey_str,
                    self._on_hotkey,
                    suppress=False
                )
            except Exception as e:
                print(f"[SnapCapture] 快捷键注册失败 '{self._hotkey_str}': {e}")

    def _unregister(self):
        """注销当前热键"""
        if self._hotkey_str:
            try:
                keyboard.remove_hotkey(self._hotkey_str)
            except (KeyError, ValueError):
                pass

    def _on_hotkey(self):
        """热键回调（在 keyboard 线程中执行）"""
        self.hotkey_triggered.emit()

    def run(self):
        """线程主循环"""
        self._running = True
        self._register()
        # keyboard.wait() 会阻塞直到程序退出
        while self._running:
            self.msleep(100)

    def stop(self):
        """停止监听"""
        self._running = False
        self._unregister()
        try:
            keyboard.unhook_all()
        except Exception:
            pass
        self.quit()
        self.wait(2000)


class HotkeyManager(QObject):
    """快捷键管理器"""

    hotkey_activated = pyqtSignal()

    def __init__(self, config, parent=None):
        super().__init__(parent)
        self.config = config
        self.listener = HotkeyListener()
        self.listener.hotkey_triggered.connect(
            self.hotkey_activated.emit,
        )
        self.config.config_changed.connect(self._on_config_changed)

    def start(self):
        """启动热键监听"""
        hotkey = self.config.hotkey
        self.listener.set_hotkey(hotkey)
        self.listener.start()

    def stop(self):
        """停止热键监听"""
        self.listener.stop()

    def update_hotkey(self, new_hotkey: str):
        """更新快捷键"""
        self.listener.set_hotkey(new_hotkey)
        self.config.set("hotkey", new_hotkey)

    def _on_config_changed(self, key, value):
        if key == "hotkey":
            self.listener.set_hotkey(value)
