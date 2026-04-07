#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Keyboard.h>

// ========== 测试模式配置 ==========
#define TEST_MODE_DIRECT  // 定义此宏启用直连测试模式，注释掉则使用无线模式
#define TEST_BUTTON_PIN 4 // 测试按键引脚 (根据你的接线修改)

// LED 错误指示配置
#define LED_PIN 13        // LED 引脚（大多数 Arduino 板载 LED 在 13 脚）
#define LED_BLINK_FAST 100   // 快速闪烁间隔（ms）- nRF24 初始化失败
#define LED_BLINK_SLOW 500   // 慢速闪烁间隔（ms）- 芯片未连接

#ifndef TEST_MODE_DIRECT
RF24 radio(9, 10);
const byte address[6] = "00001";
#endif

// 数据包结构
struct Packet {
  uint8_t keycode;   // 物理按键编号
  uint8_t state;     // 0=释放, 1=按下
  uint16_t seq;
};

Packet p;

// 测试模式变量
#ifdef TEST_MODE_DIRECT
bool lastButtonState = HIGH;
unsigned long testSeq = 0;
#endif

// EEPROM 地址定义
const int EEPROM_ADDR_KEYMAP = 0;  // 按键映射表起始地址

// 默认映射: 物理按键0 -> F13 (0x68)
// HID 键码参考: https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf
const uint8_t DEFAULT_KEYMAP[] = {0x68};

// 当前按键映射表 (物理按键 -> HID 键码)
uint8_t keymap[1];

// 加载 EEPROM 中的映射表
void loadKeymap() {
  // 检查是否已初始化 (第一个字节为 0xFF 表示未初始化)
  if (EEPROM.read(EEPROM_ADDR_KEYMAP) == 0xFF) {
    // 使用默认值并保存
    keymap[0] = DEFAULT_KEYMAP[0];
    saveKeymap();
  } else {
    keymap[0] = EEPROM.read(EEPROM_ADDR_KEYMAP);
  }
}

// 保存映射表到 EEPROM
void saveKeymap() {
  EEPROM.update(EEPROM_ADDR_KEYMAP, keymap[0]);
}

// 解析串口指令并处理
// 格式: "SET:0,0x68\n" - 设置物理按键0映射到 HID 键码 0x68
// 格式: "GET\n" - 获取当前映射
void processSerialCommand() {
  static String cmdBuffer = "";
  
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      cmdBuffer.trim();
      
      if (cmdBuffer.startsWith("SET:")) {
        // 解析 SET:phys,hid
        int commaIdx = cmdBuffer.indexOf(',');
        if (commaIdx > 4) {
          uint8_t physKey = cmdBuffer.substring(4, commaIdx).toInt();
          String hidStr = cmdBuffer.substring(commaIdx + 1);
          uint8_t hidKey = strtol(hidStr.c_str(), NULL, 0);
          
          if (physKey == 0) {
            keymap[0] = hidKey;
            saveKeymap();
            Serial.print("OK:");
            Serial.print(physKey);
            Serial.print("->0x");
            Serial.println(hidKey, HEX);
          } else {
            Serial.println("ERR:INVALID_KEY");
          }
        }
      } else if (cmdBuffer == "GET") {
        Serial.print("MAP:0->0x");
        Serial.println(keymap[0], HEX);
      }
      
      cmdBuffer = "";
    } else {
      cmdBuffer += c;
    }
  }
}

// HID 键码到 ASCII 的映射表（用于 Keyboard.write）
uint8_t hidToAscii(uint8_t hidKey) {
  // 字母键 A-Z (0x04-0x1D)
  if (hidKey >= 0x04 && hidKey <= 0x1D) {
    return 'a' + (hidKey - 0x04);  // A=0x04, B=0x05, ...
  }
  // 数字键 1-9 (0x1E-0x26)
  if (hidKey >= 0x1E && hidKey <= 0x26) {
    return '1' + (hidKey - 0x1E);  // 1=0x1E, 2=0x1F, ...
  }
  // 数字键 0 (0x27)
  if (hidKey == 0x27) return '0';
  // 空格 (0x2C)
  if (hidKey == 0x2C) return ' ';
  // 回车 (0x28)
  if (hidKey == 0x28) return '\n';
  // Tab (0x2B)
  if (hidKey == 0x2B) return '\t';
  // 其他键直接返回原值
  return hidKey;
}

// 发送 HID 按键事件
void sendHIDKey(uint8_t physKey, uint8_t state) {
  uint8_t hidKey = keymap[physKey];
  
  if (state == 1) {
    // 使用 Keyboard.write 发送 ASCII 字符（绕过输入法）
    uint8_t ascii = hidToAscii(hidKey);
    if (ascii >= 32 && ascii <= 126) {
      // 可打印字符
      Keyboard.write(ascii);
    } else {
      // 控制字符或功能键，使用原来的方式
      Keyboard.press(hidKey);
      Keyboard.release(hidKey);
    }
    // 调试输出已禁用，避免干扰 controller
    // Serial.print("SEND:");
    // Serial.print(hidKey, HEX);
    // Serial.print("->");
    // Serial.println((char)ascii);
  }
}

// LED 闪烁函数 - 进入无限闪烁循环表示错误
void errorBlink(int interval) {
  pinMode(LED_PIN, OUTPUT);
  while (1) {
    digitalWrite(LED_PIN, HIGH);
    delay(interval);
    digitalWrite(LED_PIN, LOW);
    delay(interval);
  }
}

void setup() {
  Serial.begin(115200);
  // while (!Serial);  // 禁用等待串口，让 Arduino 独立运行
  // Serial.println("BOOT OK");  // 禁用启动输出，避免干扰 controller
 
  // 初始化 LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // 初始关闭
  
  // 初始化 HID 键盘（延后一点）
  delay(500);

  // 初始化 HID 键盘
  Keyboard.begin();
  
  // 加载按键映射
  loadKeymap();
  
  // 启动调试输出已禁用，避免干扰 controller
  // Serial.print("[KEYMAP] 当前映射: 0x");
  // Serial.println(keymap[0], HEX);
  
  #ifdef TEST_MODE_DIRECT
  // 测试模式: 配置直连按键
  pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
  // Serial.println("[TEST MODE] 直连测试模式已启用");
  // Serial.print("[TEST MODE] 按键引脚: ");
  // Serial.println(TEST_BUTTON_PIN);
  #else
  // 无线模式: 初始化 nRF24
  if (!radio.begin()) {
    // nRF24 初始化失败 - LED 快速闪烁
    errorBlink(LED_BLINK_FAST);
  }
  
  // 检查 nRF24 芯片是否连接正常
  if (!radio.isChipConnected()) {
    // nRF24 芯片未连接 - LED 慢速闪烁
    errorBlink(LED_BLINK_SLOW);
  }
  
  // 初始化成功，LED 常亮 1 秒后关闭
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);  // 设置发射功率
  radio.startListening();
  // Serial.println("[RF MODE] 无线模式已启用");
  #endif
}

void loop() {
  // 处理串口指令 (改键)
  processSerialCommand();
  
  #ifdef TEST_MODE_DIRECT
  // 测试模式: 直接读取按键
  bool currentState = digitalRead(TEST_BUTTON_PIN);
  
  if (currentState != lastButtonState) {
    p.keycode = 0;  // 物理按键编号
    p.state = (currentState == LOW) ? 1 : 0;  // LOW=按下, HIGH=释放
    p.seq = testSeq++;
    
    // 发送 HID 按键事件
    sendHIDKey(p.keycode, p.state);
    
    delay(10);  // 简单消抖
  }
  
  lastButtonState = currentState;
  
  #else
  // 无线模式: 处理无线按键事件
  if (radio.available()) {
    radio.read(&p, sizeof(p));
    
    // 发送 HID 按键事件给 PC
    sendHIDKey(p.keycode, p.state);
  }
  #endif
}