#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Keyboard.h>

// ========== 测试模式配置 ==========
//#define TEST_MODE_DIRECT  // 定义此宏启用直连测试模式，注释掉则使用无线模式
//#define TEST_BUTTON_PIN 4 // 测试按键引脚 (根据你的接线修改)

// LED 引脚定义（Pro Micro 板载 LED 通常在 D17/TXLED 或 D30/RXLED）
#define LED_PIN 17  // Pro Micro TX LED

#ifndef TEST_MODE_DIRECT
RF24 radio(9, 10);
const byte address[6] = "00001";
#endif

// 数据包结构（与 output 端保持一致）
// 全部使用 uint8_t 避免对齐问题，seq 拆分为高字节和低字节
struct Packet {
  uint8_t keycode;   // 物理按键编号
  uint8_t state;     // 0=释放, 1=按下
  uint8_t seq_low;   // 序列号低字节
  uint8_t seq_high;  // 序列号高字节
  uint8_t battery;   // 电量档位 0-3
};

Packet p;

// 测试模式变量
#ifdef TEST_MODE_DIRECT
bool lastButtonState = HIGH;
unsigned long testSeq = 0;
#endif

// EEPROM 地址定义
const int EEPROM_ADDR_KEYMAP = 0;  // 按键映射表起始地址

// 默认映射: 物理按键0 -> 'a' (0x04)
// HID 键码参考: https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf
const uint8_t DEFAULT_KEYMAP[] = {0x04};

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

void setup() {
  Serial.begin(115200);
  delay(1000);  // 等待串口就绪
  // while (!Serial);  // 禁用等待串口，让 Arduino 独立运行
  // Serial.println("BOOT OK");  // 禁用启动输出，避免干扰 controller
 
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
    Serial.println("RF24_INIT_FAILED");
    // LED 快速闪烁表示错误
    pinMode(LED_PIN, OUTPUT);
    while (1) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }
  Serial.println("RF24_OK");
  Serial.print("[INIT] Packet size=");
  Serial.println(sizeof(Packet));
  radio.setPALevel(RF24_PA_HIGH);  // 提高功率
  radio.setDataRate(RF24_250KBPS);  // 降低速率增加稳定性
  radio.setChannel(76);  // 固定信道
  radio.setAutoAck(false);  // 禁用自动应答
  radio.openReadingPipe(0, address);
  radio.startListening();
  #endif
}

void loop() {
  // 处理串口指令 (改键)
  processSerialCommand();
  
  // 调试：周期性输出心跳（每5秒）
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    Serial.print("[HB] radio.available()=");
    Serial.print(radio.available());
    Serial.print(", isChipConnected=");
    Serial.println(radio.isChipConnected());
  }
  
  #ifdef TEST_MODE_DIRECT
  // 测试模式: 直接读取按键
  bool currentState = digitalRead(TEST_BUTTON_PIN);
  
  if (currentState != lastButtonState) {
    p.keycode = 0;  // 物理按键编号
    p.state = (currentState == LOW) ? 1 : 0;  // LOW=按下, HIGH=释放
    p.seq_low = testSeq & 0xFF;
    p.seq_high = (testSeq >> 8) & 0xFF;
    testSeq++;
    
    // 发送 HID 按键事件
    sendHIDKey(p.keycode, p.state);
    
    delay(10);  // 简单消抖
  }
  
  lastButtonState = currentState;
  
  #else
  // 无线模式: 处理无线按键事件
  uint8_t pipeNum;
  if (radio.available(&pipeNum)) {
    uint8_t payloadSize = radio.getPayloadSize();
    Serial.print("[RX] pipe=");
    Serial.print(pipeNum);
    Serial.print(", payloadSize=");
    Serial.print(payloadSize);
    Serial.print(", sizeof(Packet)=");
    Serial.println(sizeof(Packet));
    
    radio.read(&p, sizeof(p));
    
    // 调试输出（临时启用）
    uint16_t fullSeq = p.seq_low | (p.seq_high << 8);
    Serial.print("RX_DATA:");
    Serial.print(p.keycode);
    Serial.print(",");
    Serial.print(p.state);
    Serial.print(",");
    Serial.print(fullSeq);
    Serial.print(",");
    Serial.println(p.battery);
    
    // 发送 HID 按键事件给 PC
    Serial.print("HID_SEND:");
    Serial.print(p.keycode);
    Serial.print(",");
    Serial.println(p.state);
    sendHIDKey(p.keycode, p.state);
  }
  #endif
}