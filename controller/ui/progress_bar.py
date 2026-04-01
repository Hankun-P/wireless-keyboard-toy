from PySide6.QtWidgets import QWidget
from PySide6.QtGui import QPainter, QPixmap
from utils.path import resource_path

class CustomProgressBar(QWidget):
    def __init__(self):
        super().__init__()
        self.value = 0

        self.bg = QPixmap(resource_path("assets/bar_bg.png"))
        self.fill = QPixmap(resource_path("assets/bar_fill.png"))

        self.setFixedSize(180, 36)

    def setValue(self, v):
        self.value = max(0, min(100, v))
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.drawPixmap(self.rect(), self.bg)

        w = int(self.width() * self.value / 100)
        if w <= 0:
            return

        cropped = self.fill.copy(0, 0, w, self.fill.height())
        painter.drawPixmap(0, 0, cropped)