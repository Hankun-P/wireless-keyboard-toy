import serial
import serial.tools.list_ports

# Qt Key 到 HID Keycode 的映射表
QT_TO_HID = {
    # 功能键
    16777264: 0x3A,  # F1
    16777265: 0x3B,  # F2
    16777266: 0x3C,  # F3
    16777267: 0x3D,  # F4
    16777268: 0x3E,  # F5
    16777269: 0x3F,  # F6
    16777270: 0x40,  # F7
    16777271: 0x41,  # F8
    16777272: 0x42,  # F9
    16777273: 0x43,  # F10
    16777274: 0x44,  # F11
    16777275: 0x45,  # F12
    16777276: 0x68,  # F13
    16777277: 0x69,  # F14
    16777278: 0x6A,  # F15
    16777279: 0x6B,  # F16
    16777280: 0x6C,  # F17
    16777281: 0x6D,  # F18
    16777282: 0x6E,  # F19
    16777283: 0x6F,  # F20
    16777284: 0x70,  # F21
    16777285: 0x71,  # F22
    16777286: 0x72,  # F23
    16777287: 0x73,  # F24
    
    # 数字键
    48: 0x27,  # 0
    49: 0x1E,  # 1
    50: 0x1F,  # 2
    51: 0x20,  # 3
    52: 0x21,  # 4
    53: 0x22,  # 5
    54: 0x23,  # 6
    55: 0x24,  # 7
    56: 0x25,  # 8
    57: 0x26,  # 9
    
    # 字母键
    65: 0x04,  # A
    66: 0x05,  # B
    67: 0x06,  # C
    68: 0x07,  # D
    69: 0x08,  # E
    70: 0x09,  # F
    71: 0x0A,  # G
    72: 0x0B,  # H
    73: 0x0C,  # I
    74: 0x0D,  # J
    75: 0x0E,  # K
    76: 0x0F,  # L
    77: 0x10,  # M
    78: 0x11,  # N
    79: 0x12,  # O
    80: 0x13,  # P
    81: 0x14,  # Q
    82: 0x15,  # R
    83: 0x16,  # S
    84: 0x17,  # T
    85: 0x18,  # U
    86: 0x19,  # V
    87: 0x1A,  # W
    88: 0x1B,  # X
    89: 0x1C,  # Y
    90: 0x1D,  # Z
    
    # 特殊键
    32: 0x2C,   # Space
    16777219: 0x2A,  # Backspace
    16777220: 0x28,  # Enter/Return
    16777217: 0x2B,  # Tab
    16777216: 0x29,  # Escape
    
    # 方向键
    16777235: 0x52,  # Up
    16777237: 0x51,  # Down
    16777234: 0x50,  # Left
    16777236: 0x4F,  # Right
    
    # 修饰键
    16777248: 0xE1,  # Shift (Left)
    16777249: 0xE0,  # Ctrl (Left)
    16777250: 0xE2,  # Alt (Left)
    16777251: 0xE3,  # Meta/Win (Left)
}


def qt_key_to_hid(qt_key):
    """将 Qt Key 转换为 HID Keycode"""
    return QT_TO_HID.get(qt_key, None)


def hid_to_key_name(hid_code):
    """将 HID Keycode 转换为可读名称"""
    for qt_key, hid in QT_TO_HID.items():
        if hid == hid_code:
            # 反向查找名称
            from PySide6.QtGui import QKeySequence
            return QKeySequence(qt_key).toString()
    return f"0x{hid_code:02X}"


class ArduinoDevice:
    """与 Arduino Input 设备通信的类"""
    
    def __init__(self, port=None, baudrate=115200):
        self.battery = 82  # 模拟电量
        self.key = "F13"   # 当前映射的按键名称
        self.hid_code = 0x68  # F13 的 HID 码
        self.serial = None
        self.port = port
        
        if port:
            self.connect(port, baudrate)
    
    def connect(self, port, baudrate=115200):
        """连接串口"""
        try:
            self.serial = serial.Serial(port, baudrate, timeout=1)
            self.port = port
            # 读取当前映射
            self._read_current_keymap()
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            return False
    
    def disconnect(self):
        """断开连接"""
        if self.serial:
            self.serial.close()
            self.serial = None
    
    def _send_command(self, cmd):
        """发送命令并等待响应"""
        if not self.serial:
            return None
        
        self.serial.write((cmd + '\n').encode())
        self.serial.flush()
        
        # 读取响应
        try:
            response = self.serial.readline().decode().strip()
            return response
        except:
            return None
    
    def _read_current_keymap(self):
        """读取当前按键映射"""
        response = self._send_command("GET")
        if response and response.startswith("MAP:0->0x"):
            try:
                hid_hex = response.split("0x")[1]
                self.hid_code = int(hid_hex, 16)
                self.key = hid_to_key_name(self.hid_code)
            except:
                pass
    
    def set_key(self, qt_key):
        """设置按键映射 (传入 Qt Key)"""
        hid_code = qt_key_to_hid(qt_key)
        if hid_code is None:
            print(f"不支持的按键: {qt_key}")
            return False
        
        # 发送 SET 命令: SET:0,0x68
        cmd = f"SET:0,0x{hid_code:02X}"
        response = self._send_command(cmd)
        
        if response and response.startswith("OK:"):
            self.hid_code = hid_code
            from PySide6.QtGui import QKeySequence
            self.key = QKeySequence(qt_key).toString()
            print(f"绑定按键: {self.key} (HID: 0x{hid_code:02X})")
            return True
        else:
            print(f"绑定失败: {response}")
            return False
    
    def get_status(self):
        """获取设备状态"""
        return {"battery": self.battery, "key": self.key}
    
    @staticmethod
    def list_ports():
        """列出可用串口"""
        return [p.device for p in serial.tools.list_ports.comports()]


# 保持向后兼容
FakeDevice = ArduinoDevice