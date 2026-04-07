#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define BUTTON_PIN 4

// LED 错误指示配置
#define LED_PIN 13        // LED 引脚
#define LED_BLINK_FAST 100   // 快速闪烁 - 初始化失败
#define LED_BLINK_SLOW 500   // 慢速闪烁 - 芯片未连接

RF24 radio(9, 10);
const byte address[6] = "00001";

bool lastState = HIGH;
unsigned long seq = 0;

// 数据包结构
struct Packet {
  uint8_t keycode;   // 物理按键编号 (固定为0)
  uint8_t state;     // 0=释放, 1=按下
  uint16_t seq;
};

// 发送失败重试次数
#define MAX_RETRY 3

// LED 闪烁函数 - 无限循环表示错误
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
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // 初始化 LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // nRF24 初始化检查
  if (!radio.begin()) {
    errorBlink(LED_BLINK_FAST);  // 快速闪烁
  }
  
  // 检查芯片连接
  if (!radio.isChipConnected()) {
    errorBlink(LED_BLINK_SLOW);  // 慢速闪烁
  }
  
  // 初始化成功，LED 闪烁 2 次后关闭
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();
}

void loop() {
  bool currentState = digitalRead(BUTTON_PIN);

  if (currentState != lastState) {
    Packet p;
    p.keycode = 0;      // 物理按键编号固定为0
    p.state = (currentState == LOW) ? 1 : 0;  // LOW=按下(1), HIGH=释放(0)
    p.seq = seq++;

    // 发送数据，带重试机制
    bool sent = false;
    for (int i = 0; i < MAX_RETRY; i++) {
      if (radio.write(&p, sizeof(p))) {
        sent = true;
        break;  // 发送成功，跳出重试
      }
      delay(5);  // 短暂延迟后重试
    }
    
    // 如果所有重试都失败，可以选择在这里处理错误
    // 例如：记录错误次数、LED 指示等

    delay(10);  // 简单消抖
  }

  lastState = currentState;
}