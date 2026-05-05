"""
设置对话框模块 - 快捷键/保存路径/自启动配置
"""
from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QLineEdit, QCheckBox, QFileDialog, QGroupBox, QMessageBox,
    QWidget
)
from PyQt6.QtGui import QFont, QKeySequence, QColor
from PyQt6.QtCore import Qt, pyqtSignal

from src.autostart import is_autostart_enabled, set_autostart


class HotkeyEdit(QLineEdit):
    """快捷键录入输入框"""

    hotkey_changed = pyqtSignal(str)

    # keyboard 库使用的修饰键名称映射
    MODIFIER_MAP = {
        Qt.Key.Key_Control: "ctrl",
        Qt.Key.Key_Shift: "shift",
        Qt.Key.Key_Alt: "alt",
        Qt.Key.Key_Meta: "win",
    }

    KEY_NAME_MAP = {
        Qt.Key.Key_F1: "f1", Qt.Key.Key_F2: "f2", Qt.Key.Key_F3: "f3",
        Qt.Key.Key_F4: "f4", Qt.Key.Key_F5: "f5", Qt.Key.Key_F6: "f6",
        Qt.Key.Key_F7: "f7", Qt.Key.Key_F8: "f8", Qt.Key.Key_F9: "f9",
        Qt.Key.Key_F10: "f10", Qt.Key.Key_F11: "f11", Qt.Key.Key_F12: "f12",
        Qt.Key.Key_Space: "space", Qt.Key.Key_Return: "enter",
        Qt.Key.Key_Tab: "tab", Qt.Key.Key_Escape: "esc",
        Qt.Key.Key_Delete: "delete", Qt.Key.Key_Backspace: "backspace",
        Qt.Key.Key_Insert: "insert", Qt.Key.Key_Home: "home",
        Qt.Key.Key_End: "end", Qt.Key.Key_PageUp: "page up",
        Qt.Key.Key_PageDown: "page down",
        Qt.Key.Key_Up: "up", Qt.Key.Key_Down: "down",
        Qt.Key.Key_Left: "left", Qt.Key.Key_Right: "right",
        Qt.Key.Key_Print: "print screen",
    }

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setReadOnly(True)
        self.setPlaceholderText("点击此处，然后按下快捷键组合...")
        self._recording = False
        self.setStyleSheet("""
            QLineEdit {
                background: #383838;
                color: #e0e0e0;
                border: 2px solid #555;
                border-radius: 6px;
                padding: 4px 10px;
                font-size: 14px;
                font-family: "Microsoft YaHei UI";
                min-height: 24px;
            }
            QLineEdit:focus {
                border-color: #1e90ff;
                background: #404040;
            }
        """)

    def set_hotkey_text(self, hotkey_str: str):
        """显示热键文本（格式化显示）"""
        display = hotkey_str.replace("+", " + ").title()
        self.setText(display)

    def mousePressEvent(self, event):
        self._recording = True
        self.setText("请按下快捷键组合...")
        super().mousePressEvent(event)

    def keyPressEvent(self, event):
        if not self._recording:
            return

        key = event.key()
        modifiers = event.modifiers()

        # 忽略单独的修饰键按下
        if key in self.MODIFIER_MAP:
            return

        parts = []
        if modifiers & Qt.KeyboardModifier.ControlModifier:
            parts.append("ctrl")
        if modifiers & Qt.KeyboardModifier.ShiftModifier:
            parts.append("shift")
        if modifiers & Qt.KeyboardModifier.AltModifier:
            parts.append("alt")

        # 获取主键名
        if key in self.KEY_NAME_MAP:
            key_name = self.KEY_NAME_MAP[key]
        elif 0x20 <= key <= 0x7e:
            key_name = chr(key).lower()
        else:
            return

        # 必须包含至少一个修饰键
        if not parts:
            self.setText("请使用修饰键组合（如 Ctrl+Shift+...）")
            return

        parts.append(key_name)
        hotkey_str = "+".join(parts)

        self._recording = False
        self.set_hotkey_text(hotkey_str)
        self.hotkey_changed.emit(hotkey_str)

    def focusOutEvent(self, event):
        self._recording = False
        super().focusOutEvent(event)


class SettingsDialog(QDialog):
    """设置对话框"""

    hotkey_updated = pyqtSignal(str)

    def __init__(self, config, parent=None):
        super().__init__(parent)
        self.config = config
        self.setWindowTitle("SnapCapture 设置")
        self.setFixedSize(480, 420)
        self.setWindowFlags(
            self.windowFlags() & ~Qt.WindowType.WindowContextHelpButtonHint
        )
        self._init_ui()
        self._load_config()

    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setSpacing(16)
        layout.setContentsMargins(20, 20, 20, 20)

        # 标题
        title = QLabel("⚙ 设置")
        title.setFont(QFont("Microsoft YaHei UI", 18, QFont.Weight.Bold))
        title.setStyleSheet("color: #e0e0e0; margin-bottom: 8px;")
        layout.addWidget(title)

        # --- 快捷键设置 ---
        hotkey_group = self._create_group("快捷键设置")
        hotkey_layout = QVBoxLayout()

        hint = QLabel("点击输入框后按下快捷键组合来设置截屏快捷键")
        hint.setStyleSheet("color: #888; font-size: 12px;")
        hotkey_layout.addWidget(hint)

        row = QHBoxLayout()
        self.hotkey_edit = HotkeyEdit()
        self.hotkey_edit.hotkey_changed.connect(self._on_hotkey_changed)
        row.addWidget(self.hotkey_edit)

        btn_reset = QPushButton("恢复默认")
        btn_reset.setFixedWidth(90)
        btn_reset.setStyleSheet(self._button_style("#555", "#666"))
        btn_reset.clicked.connect(self._reset_hotkey)
        row.addWidget(btn_reset)

        hotkey_layout.addLayout(row)
        hotkey_group.setLayout(hotkey_layout)
        layout.addWidget(hotkey_group)

        # --- 保存路径设置 ---
        save_group = self._create_group("保存路径")
        save_layout = QHBoxLayout()

        self.path_edit = QLineEdit()
        self.path_edit.setReadOnly(True)
        self.path_edit.setStyleSheet("""
            QLineEdit {
                background: #383838;
                color: #e0e0e0;
                border: 2px solid #555;
                border-radius: 6px;
                padding: 8px 12px;
                font-size: 13px;
            }
        """)
        save_layout.addWidget(self.path_edit)

        btn_browse = QPushButton("浏览...")
        btn_browse.setFixedWidth(90)
        btn_browse.setStyleSheet(self._button_style("#1e90ff", "#1a7ad9"))
        btn_browse.clicked.connect(self._browse_folder)
        save_layout.addWidget(btn_browse)

        save_group.setLayout(save_layout)
        layout.addWidget(save_group)

        # --- 启动设置 ---
        startup_group = self._create_group("启动设置")
        startup_layout = QVBoxLayout()

        self.chk_autostart = QCheckBox("  开机自动启动 SnapCapture")
        self.chk_autostart.setStyleSheet("""
            QCheckBox {
                color: #e0e0e0;
                font-size: 14px;
                spacing: 8px;
            }
        """)
        self.chk_autostart.toggled.connect(self._on_autostart_changed)
        startup_layout.addWidget(self.chk_autostart)

        startup_group.setLayout(startup_layout)
        layout.addWidget(startup_group)

        layout.addStretch()

        # 底部按钮
        bottom = QHBoxLayout()
        bottom.addStretch()
        btn_close = QPushButton("关闭")
        btn_close.setFixedSize(100, 36)
        btn_close.setStyleSheet(self._button_style("#555", "#666"))
        btn_close.clicked.connect(self.close)
        bottom.addWidget(btn_close)
        layout.addLayout(bottom)

        # 对话框样式
        self.setStyleSheet("""
            QDialog {
                background-color: #2b2b2b;
            }
        """)

    def _create_group(self, title: str) -> QGroupBox:
        group = QGroupBox(title)
        group.setStyleSheet("""
            QGroupBox {
                color: #aaa;
                font-size: 13px;
                font-weight: bold;
                border: 1px solid #444;
                border-radius: 8px;
                margin-top: 12px;
                padding-top: 16px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 12px;
                padding: 0 6px;
            }
        """)
        return group

    @staticmethod
    def _button_style(bg: str, hover_bg: str) -> str:
        return f"""
            QPushButton {{
                background-color: {bg};
                color: white;
                border: none;
                border-radius: 6px;
                padding: 8px 16px;
                font-size: 13px;
                font-family: "Microsoft YaHei UI";
            }}
            QPushButton:hover {{
                background-color: {hover_bg};
            }}
        """

    def _load_config(self):
        self.hotkey_edit.set_hotkey_text(self.config.hotkey)
        self.path_edit.setText(self.config.save_path)
        self.chk_autostart.setChecked(is_autostart_enabled())

    def _on_hotkey_changed(self, hotkey_str: str):
        self.config.set("hotkey", hotkey_str)
        self.hotkey_updated.emit(hotkey_str)

    def _reset_hotkey(self):
        default = self.config.DEFAULT_HOTKEY
        self.hotkey_edit.set_hotkey_text(default)
        self.config.reset_hotkey()
        self.hotkey_updated.emit(default)

    def _browse_folder(self):
        folder = QFileDialog.getExistingDirectory(
            self, "选择截图保存文件夹", self.config.save_path
        )
        if folder:
            self.path_edit.setText(folder)
            self.config.set("save_path", folder)

    def _on_autostart_changed(self, checked: bool):
        success = set_autostart(checked)
        if success:
            self.config.set("auto_start", checked)
        else:
            self.chk_autostart.blockSignals(True)
            self.chk_autostart.setChecked(not checked)
            self.chk_autostart.blockSignals(False)
            QMessageBox.warning(self, "错误", "自启动设置失败，请以管理员身份运行。")
