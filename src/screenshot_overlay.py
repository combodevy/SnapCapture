"""
截屏覆盖层模块 - 全屏覆盖，支持矩形区域选择、拖拽缩放、自动窗口识别
"""
import mss
import mss.tools
import ctypes
from ctypes import wintypes
from PyQt6.QtWidgets import QWidget, QApplication
from PyQt6.QtGui import (
    QPainter, QPixmap, QColor, QPen, QFont, QImage,
    QCursor, QGuiApplication
)
from PyQt6.QtCore import Qt, QRect, QPoint, pyqtSignal, QTimer

from src.magnifier import Magnifier
from src.toolbar import Toolbar

user32 = ctypes.windll.user32

class RECT(ctypes.Structure):
    _fields_ = [
        ("left", ctypes.c_long),
        ("top", ctypes.c_long),
        ("right", ctypes.c_long),
        ("bottom", ctypes.c_long)
    ]

class POINT(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_long),
        ("y", ctypes.c_long)
    ]

def get_window_rect_under_cursor(scale_x, scale_y, offset_x, offset_y):
    """获取鼠标当前所在窗口的逻辑矩形区域"""
    pt = POINT()
    user32.GetCursorPos(ctypes.byref(pt))
    hwnd = user32.WindowFromPoint(pt)
    if not hwnd:
        return None
    
    # 获取根窗口
    GA_ROOT = 2
    root_hwnd = user32.GetAncestor(hwnd, GA_ROOT)
    
    rect = RECT()
    user32.GetWindowRect(root_hwnd, ctypes.byref(rect))
    
    # 转换为逻辑坐标 (减去虚拟桌面偏移，再除以缩放比)
    x = int((rect.left - offset_x) / scale_x)
    y = int((rect.top - offset_y) / scale_y)
    w = int((rect.right - rect.left) / scale_x)
    h = int((rect.bottom - rect.top) / scale_y)
    
    # 略微内缩以排除阴影，或者直接返回
    return QRect(x, y, w, h)


class ScreenshotOverlay(QWidget):
    """全屏截屏覆盖层"""

    screenshot_confirmed = pyqtSignal(QPixmap)
    screenshot_save = pyqtSignal(QPixmap)
    capture_cancelled = pyqtSignal()

    HANDLE_SIZE = 8
    HANDLE_MARGIN = 4

    def __init__(self):
        super().__init__()
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose, False)
        self.setMouseTracking(True)
        self.setCursor(Qt.CursorShape.CrossCursor)

        self.screenshot_pixmap = None
        self._screenshot_bytes = None
        self.scale_x = 1.0
        self.scale_y = 1.0

        self._mss_left = 0
        self._mss_top = 0

        self.start_point = None
        self.current_point = None
        self.is_selecting = False
        self.selection_done = False
        
        # 拖拽调整
        self.drag_mode = None  # 'tl', 't', 'tr', 'l', 'r', 'bl', 'b', 'br', 'move'
        self.drag_offset = QPoint()
        
        # 自动窗口识别
        self.auto_window_rect = None

        self.magnifier = Magnifier(self)
        self.toolbar = Toolbar(self)
        self.toolbar.confirm_clicked.connect(self._on_confirm)
        self.toolbar.save_clicked.connect(self._on_save)
        self.toolbar.cancel_clicked.connect(self._on_cancel)

    def start_capture(self):
        self._reset_state()
        self._capture_screen()
        if self.screenshot_pixmap is None:
            return

        virtual_geo = QGuiApplication.primaryScreen().virtualGeometry()
        self.setGeometry(virtual_geo)

        self.scale_x = self.screenshot_pixmap.width() / virtual_geo.width()
        self.scale_y = self.screenshot_pixmap.height() / virtual_geo.height()

        self.magnifier.set_source(self.screenshot_pixmap, self.scale_x, self.scale_y)
        self.showFullScreen()
        self.activateWindow()
        self.setFocus()

    def _capture_screen(self):
        try:
            with mss.mss() as sct:
                monitor = sct.monitors[0]
                self._mss_left = monitor["left"]
                self._mss_top = monitor["top"]

                screenshot = sct.grab(monitor)
                w, h = screenshot.width, screenshot.height

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
        self.start_point = None
        self.current_point = None
        self.is_selecting = False
        self.selection_done = False
        self.drag_mode = None
        self.auto_window_rect = None
        self.setCursor(Qt.CursorShape.CrossCursor)
        self.toolbar.hide()
        self.magnifier.hide()

    def _get_selection_rect(self) -> QRect | None:
        if self.start_point is None or self.current_point is None:
            return None
        return QRect(self.start_point, self.current_point).normalized()

    def _get_physical_rect(self, logical_rect: QRect) -> QRect:
        x = int(logical_rect.x() * self.scale_x)
        y = int(logical_rect.y() * self.scale_y)
        w = int(logical_rect.width() * self.scale_x)
        h = int(logical_rect.height() * self.scale_y)
        if self.screenshot_pixmap:
            x = max(0, min(x, self.screenshot_pixmap.width() - 1))
            y = max(0, min(y, self.screenshot_pixmap.height() - 1))
            w = min(w, self.screenshot_pixmap.width() - x)
            h = min(h, self.screenshot_pixmap.height() - y)
        return QRect(x, y, w, h)

    def _crop_selection(self) -> QPixmap | None:
        sel = self._get_selection_rect()
        if sel is None or sel.width() < 1 or sel.height() < 1:
            return None
        phys = self._get_physical_rect(sel)
        if phys.width() < 1 or phys.height() < 1:
            return None
        return self.screenshot_pixmap.copy(phys)

    def _get_handle_rects(self, sel: QRect):
        hs = self.HANDLE_SIZE
        hm = self.HANDLE_MARGIN
        return {
            'tl': QRect(sel.left() - hm, sel.top() - hm, hs, hs),
            't': QRect(sel.center().x() - hs//2, sel.top() - hm, hs, hs),
            'tr': QRect(sel.right() - hm + 1, sel.top() - hm, hs, hs),
            'l': QRect(sel.left() - hm, sel.center().y() - hs//2, hs, hs),
            'r': QRect(sel.right() - hm + 1, sel.center().y() - hs//2, hs, hs),
            'bl': QRect(sel.left() - hm, sel.bottom() - hm + 1, hs, hs),
            'b': QRect(sel.center().x() - hs//2, sel.bottom() - hm + 1, hs, hs),
            'br': QRect(sel.right() - hm + 1, sel.bottom() - hm + 1, hs, hs)
        }

    def _update_cursor_shape(self, pos: QPoint):
        if not self.selection_done:
            self.setCursor(Qt.CursorShape.CrossCursor)
            return

        sel = self._get_selection_rect()
        if not sel:
            return

        handles = self._get_handle_rects(sel)
        
        if handles['tl'].contains(pos) or handles['br'].contains(pos):
            self.setCursor(Qt.CursorShape.SizeFDiagCursor)
        elif handles['tr'].contains(pos) or handles['bl'].contains(pos):
            self.setCursor(Qt.CursorShape.SizeBDiagCursor)
        elif handles['l'].contains(pos) or handles['r'].contains(pos):
            self.setCursor(Qt.CursorShape.SizeHorCursor)
        elif handles['t'].contains(pos) or handles['b'].contains(pos):
            self.setCursor(Qt.CursorShape.SizeVerCursor)
        elif sel.contains(pos):
            self.setCursor(Qt.CursorShape.SizeAllCursor)
        else:
            self.setCursor(Qt.CursorShape.CrossCursor)

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            pos = event.position().toPoint()
            
            if self.selection_done:
                sel = self._get_selection_rect()
                handles = self._get_handle_rects(sel)
                
                # 检查是否点击了控制点
                for mode, rect in handles.items():
                    if rect.contains(pos):
                        self.drag_mode = mode
                        self.start_point = sel.topLeft()
                        self.current_point = sel.bottomRight()
                        self.toolbar.hide()
                        return

                # 检查是否点击了选区内部 (移动)
                if sel.contains(pos):
                    self.drag_mode = 'move'
                    self.drag_offset = pos - sel.topLeft()
                    self.toolbar.hide()
                    return

                # 点击了选区外，重新选择
                self.selection_done = False
                self.toolbar.hide()

            # 开始新的选择
            self.start_point = pos
            self.current_point = pos
            self.is_selecting = True
            self.auto_window_rect = None
            self.magnifier.show()
            self.update()

    def mouseMoveEvent(self, event):
        pos = event.position().toPoint()
        self._update_cursor_shape(pos)

        if self.is_selecting:
            self.current_point = pos
            self.update()
        elif self.drag_mode:
            sel = self._get_selection_rect()
            if self.drag_mode == 'move':
                new_top_left = pos - self.drag_offset
                w = sel.width()
                h = sel.height()
                # 防止移出屏幕
                new_x = max(0, min(new_top_left.x(), self.width() - w))
                new_y = max(0, min(new_top_left.y(), self.height() - h))
                self.start_point = QPoint(new_x, new_y)
                self.current_point = QPoint(new_x + w, new_y + h)
            else:
                # 调整大小
                x1, y1 = self.start_point.x(), self.start_point.y()
                x2, y2 = self.current_point.x(), self.current_point.y()
                
                # 保证 start 是左上，current 是右下
                x_min, x_max = min(x1, x2), max(x1, x2)
                y_min, y_max = min(y1, y2), max(y1, y2)
                
                if 'l' in self.drag_mode: x_min = pos.x()
                if 'r' in self.drag_mode: x_max = pos.x()
                if 't' in self.drag_mode: y_min = pos.y()
                if 'b' in self.drag_mode: y_max = pos.y()
                
                self.start_point = QPoint(x_min, y_min)
                self.current_point = QPoint(x_max, y_max)
            self.update()
        else:
            # 未选择状态，自动识别窗口
            if not self.selection_done:
                rect = get_window_rect_under_cursor(self.scale_x, self.scale_y, self._mss_left, self._mss_top)
                if rect:
                    # 钳制在屏幕范围内
                    rect = rect.intersected(self.rect())
                    if rect != self.auto_window_rect:
                        self.auto_window_rect = rect
                        self.update()

        # 更新放大镜
        if not self.selection_done and not self.drag_mode:
            self.magnifier.update_position(pos, self.width(), self.height())
            if not self.magnifier.isVisible():
                self.magnifier.show()
            self.magnifier.raise_()

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            if self.is_selecting:
                self.current_point = event.position().toPoint()
                self.is_selecting = False
                self.magnifier.hide()

                # 如果没有拖拽（只是点击），且有自动识别的窗口，则直接选中该窗口
                sel = self._get_selection_rect()
                if sel and sel.width() < 5 and sel.height() < 5 and self.auto_window_rect:
                    self.start_point = self.auto_window_rect.topLeft()
                    self.current_point = self.auto_window_rect.bottomRight()
                    sel = self._get_selection_rect()
                
                if sel and sel.width() > 3 and sel.height() > 3:
                    self.selection_done = True
                    self.toolbar.update_position(sel, self.width(), self.height())
                    self.toolbar.show()
                    self.toolbar.raise_()
                else:
                    self.start_point = None
                    self.current_point = None
                    
            elif self.drag_mode:
                self.drag_mode = None
                sel = self._get_selection_rect()
                if sel:
                    self.selection_done = True
                    self.toolbar.update_position(sel, self.width(), self.height())
                    self.toolbar.show()
                    self.toolbar.raise_()

            self.update()

    def mouseDoubleClickEvent(self, event):
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

    def paintEvent(self, event):
        if self.screenshot_pixmap is None:
            return

        painter = QPainter(self)
        painter.drawPixmap(self.rect(), self.screenshot_pixmap)

        overlay_color = QColor(0, 0, 0, 100)
        sel = self._get_selection_rect()

        if sel and (sel.width() > 0 and sel.height() > 0):
            region = self.rect()
            painter.fillRect(QRect(region.left(), region.top(), region.width(), sel.top() - region.top()), overlay_color)
            painter.fillRect(QRect(region.left(), sel.bottom() + 1, region.width(), region.bottom() - sel.bottom()), overlay_color)
            painter.fillRect(QRect(region.left(), sel.top(), sel.left() - region.left(), sel.height() + 1), overlay_color)
            painter.fillRect(QRect(sel.right() + 1, sel.top(), region.right() - sel.right(), sel.height() + 1), overlay_color)

            pen = QPen(QColor(30, 144, 255), 2, Qt.PenStyle.SolidLine)
            painter.setPen(pen)
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawRect(sel)

            handles = self._get_handle_rects(sel)
            painter.setBrush(QColor(30, 144, 255))
            painter.setPen(QPen(QColor(255, 255, 255), 1))
            for rect in handles.values():
                painter.drawRect(rect)

            phys = self._get_physical_rect(sel)
            size_text = f"{phys.width()} × {phys.height()}"

            font = QFont("Microsoft YaHei UI", 11, QFont.Weight.Bold)
            painter.setFont(font)
            fm = painter.fontMetrics()
            text_w = fm.horizontalAdvance(size_text) + 16
            text_h = fm.height() + 8

            label_x = sel.left()
            label_y = sel.top() - text_h - 4
            if label_y < 0:
                label_y = sel.top() + 4

            label_rect = QRect(label_x, label_y, text_w, text_h)
            painter.fillRect(label_rect, QColor(30, 30, 30, 200))
            painter.setPen(QColor(80, 80, 80))
            painter.drawRect(label_rect)

            painter.setPen(QColor(30, 200, 255))
            painter.drawText(label_rect, Qt.AlignmentFlag.AlignCenter, size_text)
            
        elif self.auto_window_rect and not self.is_selecting and not self.selection_done:
            # 绘制全屏遮罩
            painter.fillRect(self.rect(), overlay_color)
            # 抠出自动识别的窗口区域
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Clear)
            painter.fillRect(self.auto_window_rect, Qt.GlobalColor.transparent)
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
            
            # 画一个蓝色边框
            pen = QPen(QColor(30, 144, 255), 2, Qt.PenStyle.SolidLine)
            painter.setPen(pen)
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawRect(self.auto_window_rect)
            
            # 分辨率
            phys = self._get_physical_rect(self.auto_window_rect)
            size_text = f"{phys.width()} × {phys.height()}"
            font = QFont("Microsoft YaHei UI", 11, QFont.Weight.Bold)
            painter.setFont(font)
            fm = painter.fontMetrics()
            text_w = fm.horizontalAdvance(size_text) + 16
            text_h = fm.height() + 8

            label_x = self.auto_window_rect.left()
            label_y = self.auto_window_rect.top() - text_h - 4
            if label_y < 0:
                label_y = self.auto_window_rect.top() + 4

            label_rect = QRect(label_x, label_y, text_w, text_h)
            painter.fillRect(label_rect, QColor(30, 30, 30, 200))
            painter.setPen(QColor(80, 80, 80))
            painter.drawRect(label_rect)
            painter.setPen(QColor(30, 200, 255))
            painter.drawText(label_rect, Qt.AlignmentFlag.AlignCenter, size_text)
            
        else:
            painter.fillRect(self.rect(), overlay_color)

        painter.end()

    def _on_confirm(self):
        pixmap = self._crop_selection()
        if pixmap:
            self.screenshot_confirmed.emit(pixmap)
        self._cleanup_and_hide()

    def _on_save(self):
        pixmap = self._crop_selection()
        if pixmap:
            self.screenshot_save.emit(pixmap)
        self._cleanup_and_hide()

    def _on_cancel(self):
        self.capture_cancelled.emit()
        self._cleanup_and_hide()

    def _cleanup_and_hide(self):
        self.hide()
        self._reset_state()
        self.screenshot_pixmap = None
        self._screenshot_bytes = None
        self.magnifier.source_pixmap = None
