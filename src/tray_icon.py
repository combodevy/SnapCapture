"""
系统托盘图标模块 - 提供快速访问菜单
"""
import os
from PyQt6.QtWidgets import QSystemTrayIcon, QMenu
from PyQt6.QtGui import QIcon, QPixmap, QPainter, QColor, QFont, QAction
from PyQt6.QtCore import pyqtSignal, QSize


def create_default_icon() -> QIcon:
    """加载应用图标，如果 ICO 文件存在则使用，否则程序化生成"""
    # 尝试加载生成的图标文件
    icon_dir = os.path.join(os.path.dirname(__file__), "resources", "icons")
    for ext in ("app.ico", "app.png"):
        path = os.path.join(icon_dir, ext)
        if os.path.exists(path):
            return QIcon(path)

    # 回退：程序化生成
    size = 64
    pixmap = QPixmap(size, size)
    pixmap.fill(QColor(0, 0, 0, 0))

    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)

    painter.setBrush(QColor(30, 144, 255))
    painter.setPen(QColor(20, 100, 200))
    painter.drawRoundedRect(4, 4, size - 8, size - 8, 12, 12)

    painter.setPen(QColor(255, 255, 255))
    font = QFont("Segoe UI", 28, QFont.Weight.Bold)
    painter.setFont(font)
    painter.drawText(pixmap.rect(), 0x0084, "S")

    painter.end()
    return QIcon(pixmap)


class TrayIcon(QSystemTrayIcon):
    """系统托盘图标"""

    capture_requested = pyqtSignal()
    settings_requested = pyqtSignal()
    quit_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setIcon(create_default_icon())
        self.setToolTip("SnapCapture 截屏工具")

        # 创建右键菜单
        self.menu = QMenu()
        self.menu.setStyleSheet("""
            QMenu {
                background-color: #2b2b2b;
                color: #e0e0e0;
                border: 1px solid #555;
                border-radius: 4px;
                padding: 4px;
                font-family: "Microsoft YaHei UI";
                font-size: 13px;
            }
            QMenu::item {
                padding: 6px 24px 6px 12px;
                border-radius: 3px;
            }
            QMenu::item:selected {
                background-color: #1e90ff;
                color: white;
            }
            QMenu::separator {
                height: 1px;
                background: #555;
                margin: 4px 8px;
            }
        """)

        # 菜单项
        action_capture = self.menu.addAction("📷  截取屏幕")
        action_capture.triggered.connect(self.capture_requested.emit)

        self.menu.addSeparator()

        action_settings = self.menu.addAction("⚙  设置")
        action_settings.triggered.connect(self.settings_requested.emit)

        self.menu.addSeparator()

        action_quit = self.menu.addAction("✕  退出")
        action_quit.triggered.connect(self.quit_requested.emit)

        self.setContextMenu(self.menu)

        # 双击托盘图标触发截屏
        self.activated.connect(self._on_activated)

    def _on_activated(self, reason):
        if reason == QSystemTrayIcon.ActivationReason.DoubleClick:
            self.capture_requested.emit()

    def show_message(self, title: str, message: str, duration_ms: int = 2000):
        """显示托盘通知气泡"""
        self.showMessage(title, message, QSystemTrayIcon.MessageIcon.Information, duration_ms)
