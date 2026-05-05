"""
放大镜预览模块 - 在鼠标附近显示放大的屏幕内容
"""
from PyQt6.QtWidgets import QWidget
from PyQt6.QtGui import QPainter, QPixmap, QColor, QPen, QFont, QFontMetrics
from PyQt6.QtCore import Qt, QRect, QPoint


class Magnifier(QWidget):
    """放大镜组件，跟随鼠标显示放大的截图区域"""

    SIZE = 150       # 放大镜窗口尺寸（像素）
    SAMPLE = 19      # 采样区域大小（奇数，保证中心像素）
    OFFSET = 25      # 与鼠标的偏移距离

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(self.SIZE, self.SIZE + 40)  # 额外空间显示信息
        self.source_pixmap = None
        self.mouse_pos = QPoint()
        self.scale_x = 1.0
        self.scale_y = 1.0
        self.hide()

    def set_source(self, pixmap: QPixmap, scale_x: float, scale_y: float):
        """设置截图源和缩放比例"""
        self.source_pixmap = pixmap
        self.scale_x = scale_x
        self.scale_y = scale_y

    def update_position(self, mouse_pos: QPoint, parent_width: int, parent_height: int):
        """更新放大镜位置，自动避开屏幕边缘"""
        self.mouse_pos = mouse_pos
        total_h = self.SIZE + 40

        # 默认在鼠标右下方
        x = mouse_pos.x() + self.OFFSET
        y = mouse_pos.y() + self.OFFSET

        # 靠近右边缘则翻到左边
        if x + self.SIZE > parent_width:
            x = mouse_pos.x() - self.SIZE - self.OFFSET
        # 靠近下边缘则翻到上方
        if y + total_h > parent_height:
            y = mouse_pos.y() - total_h - self.OFFSET

        # 防止超出左上角
        x = max(0, x)
        y = max(0, y)

        self.move(x, y)
        self.update()

    def _get_physical_pos(self):
        """将逻辑坐标转换为物理像素坐标"""
        px = int(self.mouse_pos.x() * self.scale_x)
        py = int(self.mouse_pos.y() * self.scale_y)
        if self.source_pixmap:
            px = max(0, min(px, self.source_pixmap.width() - 1))
            py = max(0, min(py, self.source_pixmap.height() - 1))
        return px, py

    def _get_color_at_cursor(self):
        """获取鼠标位置的像素颜色"""
        if self.source_pixmap is None:
            return QColor(0, 0, 0)
        px, py = self._get_physical_pos()
        img = self.source_pixmap.toImage()
        return QColor(img.pixel(px, py))

    def paintEvent(self, event):
        if self.source_pixmap is None:
            return

        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, False)
        painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform, False)

        px, py = self._get_physical_pos()
        half = self.SAMPLE // 2

        # 计算采样区域（物理坐标）
        src_x = px - half
        src_y = py - half

        # 钳制到有效范围
        src_x = max(0, min(src_x, self.source_pixmap.width() - self.SAMPLE))
        src_y = max(0, min(src_y, self.source_pixmap.height() - self.SAMPLE))

        # 绘制放大区域
        source_rect = QRect(src_x, src_y, self.SAMPLE, self.SAMPLE)
        target_rect = QRect(0, 0, self.SIZE, self.SIZE)
        painter.drawPixmap(target_rect, self.source_pixmap, source_rect)

        # 十字准星
        cell_size = self.SIZE / self.SAMPLE
        center_x = int(half * cell_size + cell_size / 2)
        center_y = int(half * cell_size + cell_size / 2)

        pen = QPen(QColor(0, 255, 0, 180))
        pen.setWidth(1)
        painter.setPen(pen)
        painter.drawLine(center_x, 0, center_x, self.SIZE)
        painter.drawLine(0, center_y, self.SIZE, center_y)

        # 中心像素高亮框
        cx = int(half * cell_size)
        cy = int(half * cell_size)
        pen.setColor(QColor(255, 50, 50, 220))
        pen.setWidth(2)
        painter.setPen(pen)
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawRect(int(cx), int(cy), int(cell_size), int(cell_size))

        # 外边框
        pen.setColor(QColor(100, 100, 100))
        pen.setWidth(2)
        painter.setPen(pen)
        painter.drawRect(0, 0, self.SIZE - 1, self.SIZE - 1)

        # 信息区域背景
        info_rect = QRect(0, self.SIZE, self.SIZE, 40)
        painter.fillRect(info_rect, QColor(30, 30, 30, 230))
        painter.setPen(QColor(100, 100, 100))
        painter.drawRect(0, self.SIZE, self.SIZE - 1, 39)

        # 坐标和颜色信息
        color = self._get_color_at_cursor()
        font = QFont("Microsoft YaHei UI", 9)
        painter.setFont(font)
        painter.setPen(QColor(220, 220, 220))

        coord_text = f"坐标: ({px}, {py})"
        color_text = f"RGB: ({color.red()}, {color.green()}, {color.blue()})"

        painter.drawText(6, self.SIZE + 16, coord_text)

        # 颜色方块 + RGB文字
        color_box_x = 6
        color_box_y = self.SIZE + 22
        painter.fillRect(color_box_x, color_box_y, 12, 12, color)
        painter.setPen(QColor(150, 150, 150))
        painter.drawRect(color_box_x, color_box_y, 12, 12)
        painter.setPen(QColor(220, 220, 220))
        painter.drawText(color_box_x + 16, self.SIZE + 34, color_text)

        painter.end()
