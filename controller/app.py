import sys
from PySide6.QtWidgets import QWidget, QLabel, QPushButton
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QPixmap, QKeySequence

from core.device import FakeDevice
from core.state import UIState
from ui.progress_bar import CustomProgressBar
from utils.path import resource_path


class App(QWidget):
    def __init__(self):
        super().__init__()

        # ===== 基础 =====
        self.setWindowTitle("ToyKey Controller")
        self.setFixedSize(320, 220)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        self.device = FakeDevice()
        self.state = UIState.IDLE

        # ===== UI =====
        self.status = QLabel("状态：已连接", self)
        self.status.move(10, 10)

        self.battery = QLabel(self)
        self.battery.move(10, 35)

        self.key = QLabel(self)
        self.key.move(10, 60)

        self.btn = QPushButton("修改按键", self)
        self.btn.move(100, 90)
        self.btn.clicked.connect(self.enter_binding)

        self.hint = QLabel("长按键盘按键进行绑定", self)
        self.hint.move(60, 130)
        self.hint.hide()

        self.progress = CustomProgressBar()
        self.progress.setParent(self)
        self.progress.move(70, 160)
        self.progress.hide()

        self.sprite = QLabel(self)
        self.sprite.resize(32, 32)
        self.sprite.hide()

        # ===== 动画资源（预加载）=====
        self.frames = [
            QPixmap(resource_path(f"assets/walk{i}.png"))
            for i in range(1, 5)
        ]

        self.disappear_frames = [
            QPixmap(resource_path("assets/disappear1.png")),
            QPixmap(resource_path("assets/disappear2.png"))
        ]

        # ===== 动画状态 =====
        self.frame_index = 0
        self.progress_value = 0
        self.current_pressed_key = None

        # ===== 定时器 =====
        self.anim_timer = QTimer()
        self.anim_timer.timeout.connect(self.update_animation)

        self.progress_timer = QTimer()
        self.progress_timer.timeout.connect(self.update_progress)

        self.refresh()

    # ================= 状态控制 =================
    def enter_binding(self):
        self.state = UIState.BINDING

        self.key.hide()
        self.btn.hide()

        self.hint.show()
        self.progress.show()
        self.sprite.show()

        # 🔥 强制重置（关键）
        self.progress_value = 0
        self.progress.setValue(0)

        self.frame_index = 0
        self.sprite.setPixmap(self.frames[0])

        # 放到起点
        px, py = self.progress.x(), self.progress.y()
        self.sprite.move(px, py - 10)

        self.setFocus()

    def exit_binding(self):
        self.state = UIState.IDLE

        self.hint.hide()
        self.progress.hide()
        self.sprite.hide()

        self.key.show()
        self.btn.show()

        self.progress.setValue(0)

    # ================= 键盘 =================
    def keyPressEvent(self, event):
        if self.state != UIState.BINDING:
            return
        if event.isAutoRepeat():
            return

        self.current_pressed_key = event.key()

        self.progress_value = 0
        self.progress_timer.start(30)
        self.anim_timer.start(150)

    def keyReleaseEvent(self, event):
        if self.state != UIState.BINDING:
            return
        if event.isAutoRepeat():
            return

        self.progress_timer.stop()
        self.anim_timer.stop()

        if self.progress_value >= 100:
            self.play_success()
        else:
            self.reset_binding()

    # ================= 动画 =================
    def update_animation(self):
        if self.state != UIState.BINDING:
            return

        self.frame_index = (self.frame_index + 1) % len(self.frames)
        self.sprite.setPixmap(self.frames[self.frame_index])

    # ================= 进度 =================
    def update_progress(self):
        self.progress_value = min(self.progress_value + 2, 100)
        self.progress.setValue(self.progress_value)

        ratio = self.progress_value / 100
        x = int(self.progress.width() * ratio)

        px, py = self.progress.x(), self.progress.y()
        self.sprite.move(px + x, py - 10)

        if self.progress_value >= 100:
            self.progress_timer.stop()
            self.anim_timer.stop()
            self.play_success()

    # ================= 成功动画 =================
    def play_success(self):
        self.dis_index = 0
        self.dis_timer = QTimer()
        self.dis_timer.timeout.connect(self.update_success_anim)
        self.dis_timer.start(100)

    def update_success_anim(self):
        if self.dis_index < len(self.disappear_frames):
            self.sprite.setPixmap(self.disappear_frames[self.dis_index])
            self.dis_index += 1
        else:
            self.dis_timer.stop()
            self.finish_binding()

    # ================= 重置 =================
    def reset_binding(self):
        self.progress_value = 0
        self.progress.setValue(0)

        self.frame_index = 0
        self.sprite.setPixmap(self.frames[0])

        px, py = self.progress.x(), self.progress.y()
        self.sprite.move(px, py - 10)

    # ================= 完成绑定 =================
    def finish_binding(self):
        if self.current_pressed_key:
            key_name = QKeySequence(self.current_pressed_key).toString()
            self.device.set_key(key_name)

        self.refresh()
        self.exit_binding()

    # ================= 刷新 =================
    def refresh(self):
        data = self.device.get_status()
        self.battery.setText(f"电量：{data['battery']}%")
        self.key.setText(f"当前按键：{data['key']}")