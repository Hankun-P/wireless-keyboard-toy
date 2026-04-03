#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Keyboard.h>

// ========== 测试模式配置 ==========
#define TEST_MODE_DIRECT  // 定义此宏启用直连测试模式，注释掉则使用无线模式
#define TEST_BUTTON_PIN 4 // 测试按键引脚 (根据你的接线修改)

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

// 发送 HID 按键事件
void sendHIDKey(uint8_t physKey, uint8_t state) {
  uint8_t hidKey = keymap[physKey];
  
  if (state == 1) {
    Keyboard.press(hidKey);
  } else {
    Keyboard.release(hidKey);
  }
  
  // 同时通过串口输出调试信息
  Serial.print("HID:");
  Serial.print(physKey);
  Serial.print(",");
  Serial.print(state);
  Serial.print(",0x");
  Serial.println(hidKey, HEX);
}

void setup() {
  Serial.begin(115200);
  
  // 初始化 HID 键盘
  Keyboard.begin();
  
  // 加载按键映射
  loadKeymap();
  
  #ifdef TEST_MODE_DIRECT
  // 测试模式: 配置直连按键
  pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("[TEST MODE] 直连测试模式已启用");
  Serial.print("[TEST MODE] 按键引脚: ");
  Serial.println(TEST_BUTTON_PIN);
  #else
  // 无线模式: 初始化 nRF24
  radio.begin();
  radio.openReadingPipe(0, address);
  radio.startListening();
  Serial.println("[RF MODE] 无线模式已启用");
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