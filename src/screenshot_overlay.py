"""
截屏覆盖层模块 - 全屏覆盖，支持矩形区域选择
核心逻辑：
1. 使用 mss 捕获物理像素级截图
2. 将截图显示为覆盖层背景
3. 用户拖拽选择区域
4. 分辨率显示始终基于物理像素坐标计算，避免 DPI 缩放误差
"""
import mss
import mss.tools
from PyQt6.QtWidgets import QWidget, QApplication
from PyQt6.QtGui import (
    QPainter, QPixmap, QColor, QPen, QFont, QImage,
    QCursor, QGuiApplication
)
from PyQt6.QtCore import Qt, QRect, QPoint, pyqtSignal, QTimer

from src.magnifier import Magnifier
from src.toolbar import Toolbar


class ScreenshotOverlay(QWidget):
    """全屏截屏覆盖层"""

    # 信号
    screenshot_confirmed = pyqtSignal(QPixmap)   # 确认截图（复制到剪贴板）
    screenshot_save = pyqtSignal(QPixmap)         # 保存截图到文件
    capture_cancelled = pyqtSignal()              # 取消截屏

    def __init__(self):
        super().__init__()

        # 窗口属性
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose, False)
        self.setMouseTracking(True)
        self.setCursor(Qt.CursorShape.CrossCursor)

        # 截图数据
        self.screenshot_pixmap = None      # 物理像素级原始截图
        self._screenshot_bytes = None      # 保持引用防止GC
        self.scale_x = 1.0                 # 物理/逻辑 水平缩放比
        self.scale_y = 1.0                 # 物理/逻辑 垂直缩放比

        # mss 截图区域信息（物理坐标偏移）
        self._mss_left = 0
        self._mss_top = 0

        # 选区状态
        self.start_point = None            # 拖拽起点（逻辑坐标）
        self.current_point = None          # 当前鼠标位置（逻辑坐标）
        self.is_selecting = False          # 是否正在拖拽选择
        self.selection_done = False        # 选择完成（松开鼠标后）

        # 子组件
        self.magnifier = Magnifier(self)
        self.toolbar = Toolbar(self)
        self.toolbar.confirm_clicked.connect(self._on_confirm)
        self.toolbar.save_clicked.connect(self._on_save)
        self.toolbar.cancel_clicked.connect(self._on_cancel)

    def start_capture(self):
        """开始截屏：捕获屏幕并显示覆盖层"""
        self._reset_state()
        self._capture_screen()
        if self.screenshot_pixmap is None:
            return

        # 设置覆盖层几何（覆盖虚拟桌面 = 所有显示器）
        virtual_geo = QGuiApplication.primaryScreen().virtualGeometry()
        self.setGeometry(virtual_geo)

        # 计算物理↔逻辑缩放比
        self.scale_x = self.screenshot_pixmap.width() / virtual_geo.width()
        self.scale_y = self.screenshot_pixmap.height() / virtual_geo.height()

        self.magnifier.set_source(self.screenshot_pixmap, self.scale_x, self.scale_y)

        self.showFullScreen()
        self.activateWindow()
        self.setFocus()

    def _capture_screen(self):
        """使用 mss 高速捕获全屏截图（物理像素）"""
        try:
            with mss.mss() as sct:
                monitor = sct.monitors[0]  # 所有显示器合并区域
                self._mss_left = monitor["left"]
                self._mss_top = monitor["top"]

                screenshot = sct.grab(monitor)
                w, h = screenshot.width, screenshot.height

                # mss 返回 BGRA 数据
                # 在 little-endian 系统上，Qt Format_ARGB32 内存布局即为 BGRA
                self._screenshot_bytes = bytes(screenshot.raw)
                qimg = QImage(
                    self._screenshot_bytes, w, h,
                    w * 4, QImage.Format.Format_ARGB32
                )
                self.screenshot_pixmap = QPixmap.fromImage(qimg)
        except Exception as e:
            print(f"[SnapCapture] 截屏失败: {e}")
            self.screenshot_pixmap = None

    def _reset_state(self):
        """重置所有状态"""
        self.start_point = None
        self.current_point = None
        self.is_selecting = False
        self.selection_done = False
        self.toolbar.hide()
        self.magnifier.hide()

    def _get_selection_rect(self) -> QRect | None:
        """获取当前选区矩形（逻辑坐标）"""
        if self.start_point is None or self.current_point is None:
            return None
        return QRect(self.start_point, self.current_point).normalized()

    def _get_physical_rect(self, logical_rect: QRect) -> QRect:
        """将逻辑坐标选区转换为物理像素坐标"""
        x = int(logical_rect.x() * self.scale_x)
        y = int(logical_rect.y() * self.scale_y)
        w = int(logical_rect.width() * self.scale_x)
        h = int(logical_rect.height() * self.scale_y)
        # 钳制到截图范围
        if self.screenshot_pixmap:
            x = max(0, min(x, self.screenshot_pixmap.width() - 1))
            y = max(0, min(y, self.screenshot_pixmap.height() - 1))
            w = min(w, self.screenshot_pixmap.width() - x)
            h = min(h, self.screenshot_pixmap.height() - y)
        return QRect(x, y, w, h)

    def _crop_selection(self) -> QPixmap | None:
        """从原始截图中裁切选中区域（物理像素精度）"""
        sel = self._get_selection_rect()
        if sel is None or sel.width() < 1 or sel.height() < 1:
            return None
        phys = self._get_physical_rect(sel)
        if phys.width() < 1 or phys.height() < 1:
            return None
        return self.screenshot_pixmap.copy(phys)

    # ---- 鼠标事件 ----

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            pos = event.position().toPoint()
            if self.selection_done:
                # 检查是否点在选区外，如果是则重新选择
                sel = self._get_selection_rect()
                if sel and sel.contains(pos):
                    return  # 点在选区内，忽略
                # 重新开始选择
                self.selection_done = False
                self.toolbar.hide()

            self.start_point = pos
            self.current_point = pos
            self.is_selecting = True
            self.magnifier.show()
            self.update()

    def mouseMoveEvent(self, event):
        pos = event.position().toPoint()

        if self.is_selecting:
            self.current_point = pos
            self.update()

        # 更新放大镜
        if not self.selection_done:
            self.magnifier.update_position(pos, self.width(), self.height())
            if not self.magnifier.isVisible():
                self.magnifier.show()
            self.magnifier.raise_()

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton and self.is_selecting:
            self.current_point = event.position().toPoint()
            self.is_selecting = False
            self.magnifier.hide()

            sel = self._get_selection_rect()
            if sel and sel.width() > 3 and sel.height() > 3:
                self.selection_done = True
                self.toolbar.update_position(sel, self.width(), self.height())
                self.toolbar.show()
                self.toolbar.raise_()
            else:
                # 选区太小，重置
                self.start_point = None
                self.current_point = None

            self.update()

    def mouseDoubleClickEvent(self, event):
        """双击全屏截图"""
        if event.button() == Qt.MouseButton.LeftButton:
            self.start_point = QPoint(0, 0)
            self.current_point = QPoint(self.width(), self.height())
            self.selection_done = True
            self.is_selecting = False
            self._on_confirm()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Escape:
            self._on_cancel()
        elif event.key() == Qt.Key.Key_Return or event.key() == Qt.Key.Key_Enter:
            if self.selection_done:
                self._on_confirm()

    # ---- 绘制 ----

    def paintEvent(self, event):
        if self.screenshot_pixmap is None:
            return

        painter = QPainter(self)

        # 1. 绘制截图背景
        painter.drawPixmap(self.rect(), self.screenshot_pixmap)

        # 2. 半透明遮罩
        overlay_color = QColor(0, 0, 0, 100)

        sel = self._get_selection_rect()

        if sel and (sel.width() > 0 and sel.height() > 0):
            # 绘制四周遮罩（选区外部分）
            region = self.rect()

            # 上方
            painter.fillRect(
                QRect(region.left(), region.top(), region.width(), sel.top() - region.top()),
                overlay_color
            )
            # 下方
            painter.fillRect(
                QRect(region.left(), sel.bottom() + 1, region.width(), region.bottom() - sel.bottom()),
                overlay_color
            )
            # 左侧
            painter.fillRect(
                QRect(region.left(), sel.top(), sel.left() - region.left(), sel.height() + 1),
                overlay_color
            )
            # 右侧
            painter.fillRect(
                QRect(sel.right() + 1, sel.top(), region.right() - sel.right(), sel.height() + 1),
                overlay_color
            )

            # 3. 选区边框
            pen = QPen(QColor(30, 144, 255), 2, Qt.PenStyle.SolidLine)
            painter.setPen(pen)
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawRect(sel)

            # 4. 八个调整手柄点（视觉参考）
            handle_color = QColor(30, 144, 255)
            handle_size = 6
            handles = [
                sel.topLeft(), QPoint(sel.center().x(), sel.top()), sel.topRight(),
                QPoint(sel.left(), sel.center().y()), QPoint(sel.right(), sel.center().y()),
                sel.bottomLeft(), QPoint(sel.center().x(), sel.bottom()), sel.bottomRight()
            ]
            painter.setBrush(handle_color)
            painter.setPen(QPen(QColor(255, 255, 255), 1))
            for h in handles:
                painter.drawRect(
                    h.x() - handle_size // 2, h.y() - handle_size // 2,
                    handle_size, handle_size
                )

            # 5. 分辨率信息标签（物理像素，确保准确）
            phys = self._get_physical_rect(sel)
            phys_w = phys.width()
            phys_h = phys.height()

            size_text = f"{phys_w} × {phys_h}"

            font = QFont("Microsoft YaHei UI", 11, QFont.Weight.Bold)
            painter.setFont(font)
            fm = painter.fontMetrics()
            text_w = fm.horizontalAdvance(size_text) + 16
            text_h = fm.height() + 8

            # 标签位置：选区左上方
            label_x = sel.left()
            label_y = sel.top() - text_h - 4

            # 如果上方空间不足，显示在选区内部顶部
            if label_y < 0:
                label_y = sel.top() + 4

            # 背景
            label_rect = QRect(label_x, label_y, text_w, text_h)
            painter.fillRect(label_rect, QColor(30, 30, 30, 200))
            painter.setPen(QColor(80, 80, 80))
            painter.drawRect(label_rect)

            # 文字
            painter.setPen(QColor(30, 200, 255))
            painter.drawText(
                label_rect,
                Qt.AlignmentFlag.AlignCenter,
                size_text
            )
        else:
            # 没有选区时，整个屏幕覆盖半透明遮罩
            painter.fillRect(self.rect(), overlay_color)

        painter.end()

    # ---- 操作回调 ----

    def _on_confirm(self):
        """确认截图：复制到剪贴板"""
        pixmap = self._crop_selection()
        if pixmap:
            self.screenshot_confirmed.emit(pixmap)
        self._cleanup_and_hide()

    def _on_save(self):
        """保存截图到文件"""
        pixmap = self._crop_selection()
        if pixmap:
            self.screenshot_save.emit(pixmap)
        self._cleanup_and_hide()

    def _on_cancel(self):
        """取消截屏"""
        self.capture_cancelled.emit()
        self._cleanup_and_hide()

    def _cleanup_and_hide(self):
        """清理资源并隐藏覆盖层"""
        self.hide()
        self._reset_state()
        # 释放大截图内存
        self.screenshot_pixmap = None
        self._screenshot_bytes = None
        self.magnifier.source_pixmap = None
