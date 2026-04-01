import sys
import os
from PySide6.QtWidgets import (
    QApplication, QWidget, QLabel, QPushButton, QVBoxLayout
)
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QPainter, QPixmap, QKeySequence

def resource_path(path):
    if hasattr(sys, "_MEIPASS"):
        base = sys._MEIPASS   # PyInstaller 临时解压目录
    else:
        base = os.path.dirname(__file__)  # 正常运行

    return os.path.join(base, path)

# ================= 自定义进度条 =================
class CustomProgressBar(QWidget):


    def __init__(self):
        super().__init__()
        self.value = 0

        self.bg = QPixmap(resource_path("assets/bar_bg.png"))
        self.fill = QPixmap(resource_path("assets/bar_fill.png"))
        self.setFixedWidth(180)
        self.setFixedHeight(36)

    def setValue(self, v):
        self.value = v
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)

    # 背景
        painter.drawPixmap(self.rect(), self.bg)

    # 计算显示宽度
        w = int(self.width() * self.value / 100)

        if w <= 0:
            return

    # 只裁剪 fill 图片的一部分
        cropped = self.fill.copy(0, 0, w, self.fill.height())

        painter.drawPixmap(0, 0, cropped)

# ================= 模拟设备 =================
class FakeDevice:
    def __init__(self):
        self.battery = 82
        self.key = "F13"

    def set_key(self, key):
        print("绑定按键:", key)
        self.key = key

    def get_status(self):
        return {"battery": self.battery, "key": self.key}


# ================= 主界面 =================
class App(QWidget):
    def __init__(self):
        super().__init__()

        self.device = FakeDevice()

        self.state = "IDLE"
        self.current_pressed_key = None

        self.setWindowTitle("ToyKey Controller")
        self.resize(320, 220)

        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        # ===== 原UI =====
        self.status = QLabel("状态：已连接")
        self.battery = QLabel()
        self.key = QLabel()

        self.btn = QPushButton("修改按键")
        self.btn.clicked.connect(self.enter_binding_mode)

        # ===== 绑定UI =====
        self.hint = QLabel("长按键盘按键进行绑定")
        self.hint.hide()

        self.progress = CustomProgressBar()
        self.progress.hide()

        # 精灵
        self.sprite = QLabel(self)
        self.sprite.resize(32, 32)
        self.sprite.hide()

        self.frames = [
            resource_path("assets/walk1.png"),
            resource_path("assets/walk2.png"),
            resource_path("assets/walk3.png"),
            resource_path("assets/walk4.png")
        ]
        self.frame_index = 0

        # ===== 布局 =====
        layout = QVBoxLayout()
        layout.addWidget(self.status)
        layout.addWidget(self.battery)
        layout.addWidget(self.key)
        layout.addWidget(self.btn)
        layout.addWidget(self.hint)
        layout.addWidget(self.progress)
        self.setLayout(layout)

        # ===== 定时器 =====
        self.progress_timer = QTimer()
        self.progress_timer.timeout.connect(self.update_progress)

        self.anim_timer = QTimer()
        self.anim_timer.timeout.connect(self.update_sprite)

        self.refresh()

    # ================= 状态切换 =================
    def enter_binding_mode(self):
        self.state = "BINDING"

        self.key.hide()
        self.btn.hide()

        self.hint.show()
        self.progress.show()
        self.sprite.show()

        self.progress.setValue(0)
        self.setFocus()

    def reset_ui(self):
        self.state = "IDLE"

        self.hint.hide()
        self.progress.hide()
        self.sprite.hide()

        self.key.show()
        self.btn.show()

        self.progress.setValue(0)

    # ================= 刷新设备 =================
    def refresh(self):
        data = self.device.get_status()
        self.battery.setText(f"电量：{data['battery']}%")
        self.key.setText(f"当前按键：{data['key']}")

    # ================= 键盘事件 =================
    def keyPressEvent(self, event):
        if self.state != "BINDING":
            return

        if event.isAutoRepeat():
            return

        self.current_pressed_key = event.key()

        print("按下:", event.text())

        self.progress.setValue(0)
        self.progress_timer.start(50)
        self.anim_timer.start(200)

    def keyReleaseEvent(self, event):
        if self.state != "BINDING":
            return

        if event.isAutoRepeat():
            return

        print("松开:", event.text())

        self.progress_timer.stop()
        self.anim_timer.stop()

        success = self.progress.value >= 100

        self.play_disappear(success)

    # ================= 进度更新 =================
    def update_progress(self):
        value = self.progress.value + 2
        self.progress.setValue(value)

        # 精灵移动
        x = int(self.progress.width() * value / 100)

        px = self.progress.x()
        py = self.progress.y()

        self.sprite.move(px + x, py - 10)

        if value >= 100:
            self.progress_timer.stop()
            self.anim_timer.stop()
            self.play_disappear(True)

    # ================= 精灵动画 =================
    def update_sprite(self):
        self.frame_index = (self.frame_index + 1) % len(self.frames)
        self.sprite.setPixmap(QPixmap(self.frames[self.frame_index]))

    # ================= 消失动画 =================
    def play_disappear(self, success):
        self.success = success

        self.disappear_frames = [
            resource_path("assets/disappear1.png"),
            resource_path("assets/disappear2.png")
        ]
        self.disappear_index = 0

        self.dis_timer = QTimer()
        self.dis_timer.timeout.connect(self.update_disappear)
        self.dis_timer.start(100)

    def update_disappear(self):
        if self.disappear_index < len(self.disappear_frames):
            self.sprite.setPixmap(QPixmap(self.disappear_frames[self.disappear_index]))
            self.disappear_index += 1
        else:
            self.dis_timer.stop()

            if self.success:
                self.finish_binding()
            else:
                self.reset_ui()

    # ================= 完成绑定 =================
    def finish_binding(self):
        if self.current_pressed_key is not None:
            key_name = QKeySequence(self.current_pressed_key).toString()
            self.device.set_key(key_name)

        self.refresh()
        self.reset_ui()


# ================= 启动 =================
app = QApplication(sys.argv)
window = App()
window.show()
sys.exit(app.exec())