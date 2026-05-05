"""
操作工具栏模块 - 截屏确认/保存/取消按钮
"""
from PyQt6.QtWidgets import QWidget, QHBoxLayout, QPushButton
from PyQt6.QtGui import QPainter, QColor, QPen, QIcon, QPixmap, QFont
from PyQt6.QtCore import Qt, pyqtSignal, QSize, QPoint


class ToolButton(QPushButton):
    """自定义工具栏按钮"""

    def __init__(self, text: str, color: str, hover_color: str, parent=None):
        super().__init__(text, parent)
        self._color = color
        self._hover_color = hover_color
        self.setFixedSize(36, 36)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setStyleSheet(f"""
            QPushButton {{
                background-color: {color};
                border: none;
                border-radius: 6px;
                color: white;
                font-size: 16px;
                font-weight: bold;
                font-family: "Segoe UI Symbol", "Segoe UI Emoji";
            }}
            QPushButton:hover {{
                background-color: {hover_color};
            }}
            QPushButton:pressed {{
                background-color: {color};
            }}
        """)


class Toolbar(QWidget):
    """截屏操作工具栏"""

    confirm_clicked = pyqtSignal()   # 确认（复制到剪贴板）
    save_clicked = pyqtSignal()      # 保存到文件
    cancel_clicked = pyqtSignal()    # 取消

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedHeight(48)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(6)

        # 取消按钮
        self.btn_cancel = ToolButton("✕", "#e74c3c", "#c0392b", self)
        self.btn_cancel.setToolTip("取消截屏 (Esc)")
        self.btn_cancel.clicked.connect(self.cancel_clicked.emit)

        # 保存按钮
        self.btn_save = ToolButton("💾", "#3498db", "#2980b9", self)
        self.btn_save.setToolTip("保存到文件")
        self.btn_save.clicked.connect(self.save_clicked.emit)

        # 确认按钮（复制到剪贴板）
        self.btn_confirm = ToolButton("✓", "#2ecc71", "#27ae60", self)
        self.btn_confirm.setToolTip("复制到剪贴板")
        self.btn_confirm.clicked.connect(self.confirm_clicked.emit)

        layout.addWidget(self.btn_cancel)
        layout.addWidget(self.btn_save)
        layout.addWidget(self.btn_confirm)

        self.adjustSize()
        self.hide()

    def update_position(self, selection_rect, parent_width: int, parent_height: int):
        """根据选区位置更新工具栏位置"""
        if selection_rect is None:
            self.hide()
            return

        toolbar_w = self.width()
        toolbar_h = self.height()

        # 默认在选区右下角下方
        x = selection_rect.right() - toolbar_w
        y = selection_rect.bottom() + 8

        # 如果超出底部，放到选区内部底部
        if y + toolbar_h > parent_height:
            y = selection_rect.bottom() - toolbar_h - 4

        # 如果超出右边
        if x + toolbar_w > parent_width:
            x = parent_width - toolbar_w - 4

        # 防止超出左侧/顶部
        x = max(4, x)
        y = max(4, y)

        self.move(x, y)

    def paintEvent(self, event):
        """绘制半透明背景"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setBrush(QColor(40, 40, 40, 200))
        painter.setPen(QPen(QColor(80, 80, 80), 1))
        painter.drawRoundedRect(self.rect().adjusted(1, 1, -1, -1), 8, 8)
        painter.end()
