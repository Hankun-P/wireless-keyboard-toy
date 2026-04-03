import sys
from PySide6.QtWidgets import QWidget, QLabel, QPushButton, QComboBox, QMessageBox
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QPixmap, QKeySequence

from core.device import ArduinoDevice
from core.state import UIState
from ui.progress_bar import CustomProgressBar
from utils.path import resource_path


class App(QWidget):
    def __init__(self):
        super().__init__()

        # ===== 基础 =====
        self.setWindowTitle("ToyKey Controller")
        self.setFixedSize(320, 260)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        # 创建设备实例 (未连接状态)
        self.device = ArduinoDevice()
        self.state = UIState.IDLE

        # ===== UI =====
        self.status = QLabel("状态：未连接", self)
        self.status.move(10, 10)

        # 串口选择下拉框
        self.port_label = QLabel("串口:", self)
        self.port_label.move(10, 35)
        
        self.port_combo = QComboBox(self)
        self.port_combo.setFixedWidth(120)
        self.port_combo.move(50, 33)
        self.refresh_ports()
        
        # 连接按钮
        self.connect_btn = QPushButton("连接", self)
        self.connect_btn.move(180, 33)
        self.connect_btn.clicked.connect(self.toggle_connection)

        self.battery = QLabel(self)
        self.battery.move(10, 65)

        self.key = QLabel(self)
        self.key.move(10, 90)

        self.btn = QPushButton("修改按键", self)
        self.btn.move(100, 120)
        self.btn.clicked.connect(self.enter_binding)
        self.btn.setEnabled(False)  # 未连接时禁用

        self.hint = QLabel("长按键盘按键进行绑定", self)
        self.hint.move(60, 160)
        self.hint.hide()

        self.progress = CustomProgressBar()
        self.progress.setParent(self)
        self.progress.move(70, 190)
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

    # ================= 串口连接 =================
    def refresh_ports(self):
        """刷新可用串口列表"""
        self.port_combo.clear()
        ports = ArduinoDevice.list_ports()
        if ports:
            self.port_combo.addItems(ports)
        else:
            self.port_combo.addItem("无可用串口")
    
    def toggle_connection(self):
        """连接/断开设备"""
        if self.device.serial and self.device.serial.is_open:
            # 断开连接
            self.device.disconnect()
            self.status.setText("状态：未连接")
            self.connect_btn.setText("连接")
            self.port_combo.setEnabled(True)
            self.btn.setEnabled(False)
            self.battery.setText("")
            self.key.setText("")
        else:
            # 连接设备
            port = self.port_combo.currentText()
            if port == "无可用串口":
                QMessageBox.warning(self, "警告", "没有可用的串口")
                return
            
            if self.device.connect(port):
                self.status.setText("状态：已连接")
                self.connect_btn.setText("断开")
                self.port_combo.setEnabled(False)
                self.btn.setEnabled(True)
                self.refresh()
            else:
                QMessageBox.critical(self, "错误", f"无法连接到 {port}")

    # ================= 状态控制 =================
    def enter_binding(self):
        self.state = UIState.BINDING

        self.key.hide()
        self.btn.hide()
        self.port_label.hide()
        self.port_combo.hide()
        self.connect_btn.hide()

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
        self.port_label.show()
        self.port_combo.show()
        self.connect_btn.show()

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