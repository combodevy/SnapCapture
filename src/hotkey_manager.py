"""
全局快捷键管理模块 - 使用 keyboard 库实现系统级热键监听
"""
import keyboard
import mouse
from PyQt6.QtCore import QObject, pyqtSignal, QThread


class HotkeyListener(QThread):
    """在独立线程中运行 keyboard/mouse 监听，避免阻塞主线程"""

    hotkey_triggered = pyqtSignal()

    def __init__(self):
        super().__init__()
        self._hotkey_str = ""
        self._running = False
        self._hooked_obj = None  # 保存用于注销的对象 (keyboard hook 或者 mouse callback)

    def set_hotkey(self, hotkey_str: str):
        """设置/更新热键"""
        self._unregister()
        self._hotkey_str = hotkey_str
        self._register()

    def _register(self):
        """注册热键"""
        if not self._hotkey_str:
            return

        try:
            if self._hotkey_str.startswith("mouse_"):
                # 处理鼠标热键
                button_map = {
                    "mouse_middle": "middle",
                    "mouse_x1": "x",
                    "mouse_x2": "x2"
                }
                btn = button_map.get(self._hotkey_str, "middle")
                # mouse.on_button 返回挂载的回调函数，可用于 unhook
                self._hooked_obj = mouse.on_button(
                    self._on_hotkey, 
                    buttons=(btn,), 
                    types=('down',)
                )
            else:
                # 处理键盘热键
                # suppress=True 解决与其他软件的冲突，独占按键
                self._hooked_obj = keyboard.add_hotkey(
                    self._hotkey_str,
                    self._on_hotkey,
                    suppress=True
                )
        except Exception as e:
            print(f"[SnapCapture] 快捷键注册失败 '{self._hotkey_str}': {e}")

    def _unregister(self):
        """注销当前热键"""
        if self._hooked_obj:
            try:
                if self._hotkey_str.startswith("mouse_"):
                    mouse.unhook(self._hooked_obj)
                else:
                    keyboard.remove_hotkey(self._hooked_obj)
            except Exception:
                pass
            self._hooked_obj = None

    def _on_hotkey(self):
        """热键回调（在后台线程中执行）"""
        self.hotkey_triggered.emit()

    def run(self):
        """线程主循环"""
        self._running = True
        self._register()
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
