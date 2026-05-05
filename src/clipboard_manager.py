"""
剪贴板管理模块 - 将截图复制到系统剪贴板
"""
from PyQt6.QtWidgets import QApplication
from PyQt6.QtGui import QPixmap


def copy_pixmap_to_clipboard(pixmap: QPixmap) -> bool:
    """
    将 QPixmap 复制到系统剪贴板
    返回是否成功
    """
    try:
        clipboard = QApplication.clipboard()
        if clipboard is None:
            return False
        clipboard.setPixmap(pixmap)
        return True
    except Exception as e:
        print(f"[SnapCapture] 剪贴板复制失败: {e}")
        return False
