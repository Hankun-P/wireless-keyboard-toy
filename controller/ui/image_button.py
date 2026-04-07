from PySide6.QtWidgets import QPushButton
from PySide6.QtGui import QPixmap, QPainter
from PySide6.QtCore import Qt

from utils.path import resource_path


class ImageButton(QPushButton):
    """图像按钮组件，支持三种状态：正常、悬停、按下"""
    
    def __init__(self, normal_img, hover_img=None, pressed_img=None, parent=None):
        super().__init__(parent)
        
        # 加载图片
        self.normal_pixmap = QPixmap(resource_path(normal_img))
        self.hover_pixmap = QPixmap(resource_path(hover_img)) if hover_img else self.normal_pixmap
        self.pressed_pixmap = QPixmap(resource_path(pressed_img)) if pressed_img else self.normal_pixmap
        
        # 设置按钮尺寸为图片尺寸
        if not self.normal_pixmap.isNull():
            self.setFixedSize(self.normal_pixmap.size())
        
        # 无边框、透明背景
        self.setFlat(True)
        self.setStyleSheet("background: transparent; border: none;")
        
        # 状态标记
        self._is_pressed = False
        self._is_hovered = False
        
    def enterEvent(self, event):
        """鼠标进入"""
        self._is_hovered = True
        self.update()
        super().enterEvent(event)
        
    def leaveEvent(self, event):
        """鼠标离开"""
        self._is_hovered = False
        self.update()
        super().leaveEvent(event)
        
    def mousePressEvent(self, event):
        """鼠标按下"""
        if event.button() == Qt.LeftButton:
            self._is_pressed = True
            self.update()
        super().mousePressEvent(event)
        
    def mouseReleaseEvent(self, event):
        """鼠标释放"""
        if event.button() == Qt.LeftButton:
            self._is_pressed = False
            self.update()
        super().mouseReleaseEvent(event)
        
    def paintEvent(self, event):
        """绘制按钮"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.SmoothPixmapTransform)
        
        # 根据状态选择图片
        if self._is_pressed:
            pixmap = self.pressed_pixmap
        elif self._is_hovered:
            pixmap = self.hover_pixmap
        else:
            pixmap = self.normal_pixmap
            
        # 绘制图片
        if not pixmap.isNull():
            painter.drawPixmap(self.rect(), pixmap)
        
        painter.end()
