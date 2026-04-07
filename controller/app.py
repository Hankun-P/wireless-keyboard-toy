import sys
import os
from PySide6.QtWidgets import QWidget, QLabel, QPushButton, QComboBox, QMessageBox
from PySide6.QtCore import QTimer, Qt, QPoint
from PySide6.QtGui import QPixmap, QKeySequence, QPainter, QFontDatabase, QFont

from core.device import ArduinoDevice
from core.state import UIState
from ui.progress_bar import CustomProgressBar
from ui.image_button import ImageButton
from utils.path import resource_path


class App(QWidget):
    # 窗口尺寸（根据背景图调整）
    WINDOW_WIDTH = 800
    WINDOW_HEIGHT = 600
    
    # 资源路径配置（后期替换为实际资源）
    BG_IMAGE = "assets/bg.png"                    # 背景图
    FONT_FILE = "assets/font.ttf"                 # 自定义字体
    BTN_BIND_NORMAL = "assets/btn_bind_normal.png"    # 绑定按钮-正常
    BTN_BIND_HOVER = "assets/btn_bind_hover.png"      # 绑定按钮-悬停
    BTN_BIND_PRESSED = "assets/btn_bind_pressed.png"  # 绑定按钮-按下
    
    # 关闭按钮图片
    BTN_CLOSE_NORMAL = "assets/btn_close_normal.png"    # 关闭按钮-正常
    BTN_CLOSE_HOVER = "assets/btn_close_hover.png"      # 关闭按钮-悬停
    BTN_CLOSE_PRESSED = "assets/btn_close_pressed.png"  # 关闭按钮-按下
    
    # 连接状态指示图
    STATUS_CONNECTING = [                      # 连接中 - 两帧动画
        "assets/status_connecting_1.png",
        "assets/status_connecting_2.png"
    ]
    STATUS_CONNECTED = "assets/status_connected.png"    # 已连接
    STATUS_DISCONNECTED = "assets/status_disconnected.png"  # 无设备/断开
    
    # 电量状态图片（四档）
    BATTERY_IMAGES = {
        0: "assets/battery_0.png",      # 0-25%
        1: "assets/battery_1.png",      # 26-50%
        2: "assets/battery_2.png",      # 51-75%
        3: "assets/battery_3.png",      # 76-100%
    }
    
    def __init__(self):
        super().__init__()

        # ===== 基础 =====
        self.setWindowTitle("ToyKey Controller")
        self.setFixedSize(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        
        # 设置无边框窗口
        self.setWindowFlags(Qt.FramelessWindowHint)
        
        # 用于窗口拖动的变量
        self._drag_pos = None
        
        # 加载自定义字体
        self.load_custom_font()
        
        # 加载背景图
        self.bg_pixmap = QPixmap(resource_path(self.BG_IMAGE))
        # 如果背景图不存在，使用默认尺寸和颜色
        if self.bg_pixmap.isNull():
            print(f"[WARN] 背景图未找到: {self.BG_IMAGE}")
            self.setStyleSheet("background-color: #2d2d2d;")
        else:
            # 根据背景图调整窗口大小
            self.setFixedSize(self.bg_pixmap.size())

        # 创建设备实例 (未连接状态)
        self.device = ArduinoDevice()
        self.state = UIState.IDLE

        # ===== UI - 分布式布局 =====
        # 所有元素根据窗口尺寸动态定位
        margin = 30  # 边距
        
        # 左上角：电量显示
        self.battery_icon = QLabel(self)
        self.battery_icon.setFixedSize(48, 24)
        self.battery_icon.move(margin, margin)
        
        self.battery_label = QLabel("当前以太量", self)
        self.battery_label.move(margin, margin + 30)
        self.apply_font(self.battery_label, 10)
        
        # 右上角：关闭按钮
        self.close_btn = ImageButton(
            self.BTN_CLOSE_NORMAL,
            self.BTN_CLOSE_HOVER,
            self.BTN_CLOSE_PRESSED,
            self
        )
        self.close_btn.move(self.width() - margin - self.close_btn.width(), margin)
        self.close_btn.clicked.connect(self.close)
        
        # 加载电量图片
        self._load_battery_images()
        
        # 左下角：当前绑定按键
        self.key_label = QLabel("当前绑定", self)
        self.key_label.move(margin, self.height() - margin - 50)
        self.apply_font(self.key_label, 10)
        
        self.key = QLabel("--", self)
        self.key.move(margin, self.height() - margin - 30)
        self.apply_font(self.key, 16, bold=True)
        
        # 右下角：连接状态
        self.status_icon = QLabel(self)
        self.status_icon.setFixedSize(24, 24)
        self.status_icon.move(self.width() - margin - 24, self.height() - margin - 30)
        
        # 动态文字标签（每个字单独一个 QLabel，用于实现跳跃效果）
        self.praying_labels = []
        self.praying_text = "仙女祈祷中"
        self.praying_base_x = self.width() - margin - 100
        self.praying_base_y = self.height() - margin - 25
        
        for i, char in enumerate(self.praying_text):
            label = QLabel(char, self)
            label.move(self.praying_base_x + i * 16, self.praying_base_y)
            self.apply_font(label, 11)
            self.praying_labels.append(label)
        
        # 省略号标签
        self.dots_label = QLabel(".", self)
        self.dots_label.move(self.praying_base_x + len(self.praying_text) * 16, self.praying_base_y)
        self.apply_font(self.dots_label, 11)
        
        # 加载状态图片
        self._load_status_images()
        
        # 连接动画定时器
        self.status_anim_timer = QTimer(self)
        self.status_anim_timer.timeout.connect(self._update_connecting_anim)
        self.status_anim_frame = 0
        
        # 文字跳跃动画定时器
        self.pray_anim_timer = QTimer(self)
        self.pray_anim_timer.timeout.connect(self._update_praying_anim)
        self.pray_char_index = 0  # 当前跳跃的字索引
        self.pray_dots_count = 1  # 当前省略号数量
        
        # 自动连接定时器
        self.auto_connect_timer = QTimer(self)
        self.auto_connect_timer.timeout.connect(self.try_auto_connect)
        self.auto_connect_timer.start(2000)
        
        # 初始尝试连接
        self._set_status_connecting()
        
        # 中央：主要操作按钮（大尺寸）
        self.btn = ImageButton(
            self.BTN_BIND_NORMAL,
            self.BTN_BIND_HOVER,
            self.BTN_BIND_PRESSED,
            self
        )
        # 按钮居中显示
        btn_x = (self.width() - self.btn.width()) // 2
        btn_y = (self.height() - self.btn.height()) // 2
        self.btn.move(btn_x, btn_y)
        self.btn.clicked.connect(self.enter_binding)
        self.btn.setEnabled(False)
        
        # 中央按钮下方提示文字（默认隐藏，进入绑定后显示）
        self.hint = QLabel("长按键盘按键进行绑定", self)
        self.hint.move(self.width()//2 - 80, btn_y + self.btn.height() + 20)
        self.apply_font(self.hint, 14)
        self.hint.hide()
        
        # 取消按钮（绑定界面）- 共用连接按钮的图片
        self.cancel_btn = ImageButton(
            self.BTN_BIND_NORMAL,
            self.BTN_BIND_HOVER,
            self.BTN_BIND_PRESSED,
            self
        )
        self.cancel_btn.move(self.width()//2 - 50, self.height()//2 + 80)
        self.cancel_btn.clicked.connect(self.cancel_binding)
        self.cancel_btn.hide()

        # 进度条
        self.progress = CustomProgressBar()
        self.progress.setParent(self)
        self.progress.move(self.width()//2 - 90, self.height()//2)
        self.progress.hide()

        # 精灵动画
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

    def paintEvent(self, event):
        """绘制背景图"""
        painter = QPainter(self)
        if not self.bg_pixmap.isNull():
            painter.drawPixmap(self.rect(), self.bg_pixmap)
        painter.end()

    def _load_status_images(self):
        """加载连接状态图片"""
        # 连接中动画帧
        self.connecting_frames = []
        for path in self.STATUS_CONNECTING:
            pixmap = QPixmap(resource_path(path))
            if not pixmap.isNull():
                self.connecting_frames.append(pixmap)
        
        # 已连接图片
        self.connected_pixmap = QPixmap(resource_path(self.STATUS_CONNECTED))
        
        # 断开/无设备图片
        self.disconnected_pixmap = QPixmap(resource_path(self.STATUS_DISCONNECTED))
    
    def _load_battery_images(self):
        """加载电量状态图片"""
        self.battery_pixmaps = {}
        for level, path in self.BATTERY_IMAGES.items():
            pixmap = QPixmap(resource_path(path))
            if not pixmap.isNull():
                self.battery_pixmaps[level] = pixmap
    
    def _update_battery_display(self, battery_percent):
        """更新电量显示"""
        if battery_percent <= 25:
            level = 0
        elif battery_percent <= 50:
            level = 1
        elif battery_percent <= 75:
            level = 2
        else:
            level = 3
        
        if level in self.battery_pixmaps:
            self.battery_icon.setPixmap(self.battery_pixmaps[level])
        else:
            self.battery_icon.clear()
    
    def _update_connecting_anim(self):
        """更新连接中动画"""
        if self.connecting_frames:
            self.status_anim_frame = (self.status_anim_frame + 1) % len(self.connecting_frames)
            self.status_icon.setPixmap(self.connecting_frames[self.status_anim_frame])
    
    def _set_status_connecting(self):
        """设置状态为连接中 - 启动仙女祈祷动画"""
        # 显示祈祷文字
        for label in self.praying_labels:
            label.show()
        self.dots_label.show()
        
        # 启动动画定时器
        self.pray_anim_timer.start(200)  # 每200ms更新一次动画
        
        if self.connecting_frames:
            self.status_icon.setPixmap(self.connecting_frames[0])
            self.status_anim_timer.start(500)
        else:
            self.status_icon.clear()
    
    def _update_praying_anim(self):
        """更新祈祷文字动画"""
        # 重置所有字的位置
        for i, label in enumerate(self.praying_labels):
            label.move(self.praying_base_x + i * 16, self.praying_base_y)
        
        # 当前字向上跳跃
        if self.praying_labels:
            current_label = self.praying_labels[self.pray_char_index]
            current_label.move(current_label.x(), self.praying_base_y - 5)
        
        # 更新省略号
        dots = "." * self.pray_dots_count
        self.dots_label.setText(dots)
        
        # 更新索引
        self.pray_char_index = (self.pray_char_index + 1) % len(self.praying_labels)
        self.pray_dots_count = (self.pray_dots_count % 3) + 1  # 1->2->3->1
    
    def _set_status_connected(self):
        """设置状态为已连接"""
        self.status_anim_timer.stop()
        self.pray_anim_timer.stop()
        
        # 隐藏祈祷文字，显示已连接
        for label in self.praying_labels:
            label.hide()
        self.dots_label.hide()
        
        if not self.connected_pixmap.isNull():
            self.status_icon.setPixmap(self.connected_pixmap)
        else:
            self.status_icon.clear()
    
    def _set_status_disconnected(self):
        """设置状态为断开/无设备"""
        self.status_anim_timer.stop()
        self.pray_anim_timer.stop()
        
        # 隐藏祈祷文字
        for label in self.praying_labels:
            label.hide()
        self.dots_label.hide()
        
        if not self.disconnected_pixmap.isNull():
            self.status_icon.setPixmap(self.disconnected_pixmap)
        else:
            self.status_icon.clear()

    def load_custom_font(self):
        """加载自定义字体"""
        font_path = resource_path(self.FONT_FILE)
        if os.path.exists(font_path):
            font_id = QFontDatabase.addApplicationFont(font_path)
            if font_id != -1:
                self.font_family = QFontDatabase.applicationFontFamilies(font_id)[0]
                print(f"[FONT] 加载字体: {self.font_family}")
            else:
                print(f"[WARN] 字体加载失败: {self.FONT_FILE}")
                self.font_family = "Microsoft YaHei"  #  fallback
        else:
            print(f"[WARN] 字体文件未找到: {self.FONT_FILE}")
            self.font_family = "Microsoft YaHei"  # fallback
    
    def apply_font(self, widget, size=12, bold=False):
        """应用自定义字体到控件"""
        font = QFont(self.font_family, size)
        font.setBold(bold)
        widget.setFont(font)
        # 设置文字颜色（根据背景图调整）
        widget.setStyleSheet("color: white; background: transparent;")

    # ================= 串口自动连接 =================
    def try_auto_connect(self):
        """尝试自动连接设备"""
        # 如果已经连接，检查连接是否仍然有效
        if self.device.serial and self.device.serial.is_open:
            try:
                # 简单检查串口是否仍然可用
                self.device.serial.in_waiting
                return  # 连接正常，不需要重连
            except:
                # 连接已断开，需要重连
                self.device.disconnect()
                self.on_disconnected()
        
        # 获取可用串口列表
        ports = ArduinoDevice.list_ports()
        
        if not ports:
            self._set_status_disconnected()
            return
        
        # 尝试连接每个串口
        for port in ports:
            # 跳过系统保留端口（可选）
            if "Bluetooth" in port or "Wireless" in port:
                continue
                
            if self.device.connect(port):
                self.on_connected(port)
                return
        
        # 连接失败，保持连接中状态
        pass
    
    def on_connected(self, port):
        """连接成功回调"""
        self._set_status_connected()
        self.btn.setEnabled(True)
        self.refresh()
        print(f"[AUTO] 已自动连接到 {port}")
    
    def on_disconnected(self):
        """断开连接回调"""
        self._set_status_disconnected()
        self.btn.setEnabled(False)
        self.battery_icon.clear()
        self.key.setText("--")

    # ================= 状态控制 =================
    def enter_binding(self):
        self.state = UIState.BINDING

        # 隐藏四个角落的元素
        self.close_btn.hide()
        self.battery_label.hide()
        self.battery_icon.hide()
        self.key_label.hide()
        self.key.hide()
        self.status_icon.hide()
        self.status_text.hide()

        self.hint.show()
        self.progress.show()
        self.sprite.show()
        self.cancel_btn.show()

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

        # 隐藏绑定界面元素
        self.hint.hide()
        self.progress.hide()
        self.sprite.hide()
        self.cancel_btn.hide()

        # 恢复四个角落的元素
        self.close_btn.show()
        self.battery_label.show()
        self.battery_icon.show()
        self.key_label.show()
        self.key.show()
        self.status_icon.show()
        # 根据当前状态显示对应的文字
        if self.device.serial and self.device.serial.is_open:
            # 已连接状态，祈祷文字保持隐藏
            pass
        else:
            # 未连接状态，显示祈祷文字
            for label in self.praying_labels:
                label.show()
            self.dots_label.show()
        
        # 恢复中央按钮
        self.btn.show()

        self.progress.setValue(0)
    
    def cancel_binding(self):
        """取消绑定，返回初始界面"""
        # 停止所有定时器
        self.progress_timer.stop()
        self.anim_timer.stop()
        if hasattr(self, 'dis_timer'):
            self.dis_timer.stop()
        
        # 重置状态
        self.progress_value = 0
        self.current_pressed_key = None
        
        # 退出绑定界面
        self.exit_binding()
    
    # ================= 窗口拖动 =================
    def mousePressEvent(self, event):
        """鼠标按下，开始拖动"""
        if event.button() == Qt.LeftButton:
            # 检查是否点击在按钮上，如果是则不处理拖动
            if self.childAt(event.pos()) in [self.close_btn, self.btn, self.cancel_btn]:
                super().mousePressEvent(event)
                return
            self._drag_pos = event.globalPosition().toPoint() - self.frameGeometry().topLeft()
            event.accept()
    
    def mouseMoveEvent(self, event):
        """鼠标移动，拖动窗口"""
        if event.buttons() == Qt.LeftButton and self._drag_pos is not None:
            self.move(event.globalPosition().toPoint() - self._drag_pos)
            event.accept()
    
    def mouseReleaseEvent(self, event):
        """鼠标释放，结束拖动"""
        self._drag_pos = None
        super().mouseReleaseEvent(event)

    # ================= 键盘 =================
    def keyPressEvent(self, event):
        if self.state != UIState.BINDING:
            return
        if event.isAutoRepeat():
            return

        self.current_pressed_key = event.key()
        print(f"[DEBUG] keyPressEvent: key={event.key()}, hex={hex(event.key())}")

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
            self.device.set_key(self.current_pressed_key)

        self.refresh()
        self.exit_binding()

    # ================= 刷新 =================
    def refresh(self):
        data = self.device.get_status()
        self._update_battery_display(data['battery'])
        self.key.setText(f"当前按键：{data['key']}")