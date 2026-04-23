#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define BUTTON_PIN 4

// LED 错误指示配置
#define LED_PIN 13        // LED 引脚
#define LED_BLINK_FAST 100   // 快速闪烁 - 初始化失败
#define LED_BLINK_SLOW 500   // 慢速闪烁 - 芯片未连接

// 电池电量检测配置
#define BATTERY_PIN A0      // 电池电压检测引脚
// 3.7V 锂电池电压范围：3.3V(0%) ~ 4.2V(100%)
// 通过分压电阻后接 A0，需要根据实际电路调整这些值
#define BATTERY_MIN_RAW 600   // 3.3V 对应的 ADC 值（需校准）
#define BATTERY_MAX_RAW 770   // 4.2V 对应的 ADC 值（需校准）
#define BATTERY_SEND_INTERVAL 120000  // 每 120 秒（2分钟）发送一次电量（减少耗电）

// WS2812B 连接状态指示
#include <Adafruit_NeoPixel.h>
#define LED_STATUS_PIN 5      // WS2812B 数据引脚
#define LED_NUM 1             // WS2812B 灯珠数量
Adafruit_NeoPixel pixels(LED_NUM, LED_STATUS_PIN, NEO_GRB + NEO_KHZ800);

// 连接状态管理
#define ACK_SUCCESS_THRESHOLD 1  // 1次成功即认为连接
#define ACK_FAIL_THRESHOLD 3     // 连续3次失败认为断开

// 省电模式下的发送间隔
#define BATTERY_SEND_INTERVAL_CONNECTED 120000      // 已连接：2分钟
#define BATTERY_SEND_INTERVAL_DISCONNECTED 300000   // 未连接：5分钟

RF24 radio(7, 9);  // CE=D7, CSN=D9
const byte address[6] = "00001";

bool lastState = HIGH;
bool initialized = false;  // 初始化标志
unsigned long seq = 0;

// 连接状态管理
bool isConnected = false;       // 连接状态
uint8_t ackSuccessCount = 0;    // ACK成功计数
uint8_t ackFailCount = 0;       // ACK失败计数

// 数据包结构（与 input 端保持一致）
// 全部使用 uint8_t 避免对齐问题，seq 拆分为高字节和低字节
struct Packet {
  uint8_t keycode;   // 物理按键编号 (固定为0)
  uint8_t state;     // 0=释放, 1=按下
  uint8_t seq_low;   // 序列号低字节
  uint8_t seq_high;  // 序列号高字节
  uint8_t battery;   // 电量百分比 0-100
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
  Serial.begin(115200);
  delay(1000);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // 初始化电池检测
  pinMode(BATTERY_PIN, INPUT);
  
  // 初始化 LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // 初始化 WS2812B
  pixels.begin();
  pixels.clear();
  pixels.show();

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
  radio.setPALevel(RF24_PA_MAX);  // 最大功率
  radio.setDataRate(RF24_1MBPS);  // 1Mbps
  radio.setChannel(100);  // 信道100（与测试程序一致）
  radio.setAutoAck(true);  // 启用ACK
  radio.setRetries(15, 15);  // 重试设置
  radio.stopListening();
}

// 读取电池电量（四档：0, 1, 2, 3 对应 0-25, 25-50, 50-75, 75-100）
uint8_t readBatteryLevel() {
  int raw = analogRead(BATTERY_PIN);
  
  // 将 ADC 值映射到 0-100
  int percent = map(raw, BATTERY_MIN_RAW, BATTERY_MAX_RAW, 0, 100);
  percent = constrain(percent, 0, 100);
  
  // 转换为四档
  if (percent >= 75) return 3;
  if (percent >= 50) return 2;
  if (percent >= 25) return 1;
  return 0;
}

// 上次发送电量的时间
unsigned long lastBatterySend = 0;
// 上次的电量值（用于检测变化）
uint8_t lastBatteryLevel = 255;

void loop() {
  bool currentState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();
  uint8_t currentBattery = readBatteryLevel();

  // 首次运行时初始化状态，避免误触发
  if (!initialized) {
    lastState = currentState;
    lastBatteryLevel = currentBattery;
    initialized = true;
    return;  // 跳过第一次循环
  }

  // 检查是否需要发送数据
  bool shouldSend = false;
  bool isKeyChange = (currentState != lastState);
  unsigned long batteryInterval = isConnected ? BATTERY_SEND_INTERVAL_CONNECTED : BATTERY_SEND_INTERVAL_DISCONNECTED;
  bool isBatteryTime = (now - lastBatterySend > batteryInterval);
  
  // 按键状态变化时发送
  if (isKeyChange) {
    shouldSend = true;
  }
  
  // 电量定时上报（只保留定时，未连接时不发送）
  if (isConnected && isBatteryTime) {
    shouldSend = true;
  }
  
  if (shouldSend) {
    Packet p;
    p.keycode = 0;
    p.state = (currentState == LOW) ? 1 : 0;  // LOW=按下(1), HIGH=释放(0)
    uint16_t currentSeq = seq++;
    p.seq_low = currentSeq & 0xFF;
    p.seq_high = (currentSeq >> 8) & 0xFF;
    p.battery = currentBattery;  // 0-3 四档电量

    // 发送数据并检查ACK
    bool ack = radio.write(&p, sizeof(p));
    
    // 更新连接状态
    if (ack) {
      ackSuccessCount++;
      ackFailCount = 0;
      if (ackSuccessCount >= ACK_SUCCESS_THRESHOLD) {
        isConnected = true;
        pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // 连接成功：绿色
        pixels.show();
      }
    } else {
      ackFailCount++;
      ackSuccessCount = 0;
      if (ackFailCount >= ACK_FAIL_THRESHOLD) {
        isConnected = false;
        pixels.clear();  // 断开：熄灭
        pixels.show();
      }
    }
    
    // 更新发送记录
    lastBatterySend = now;
    lastBatteryLevel = currentBattery;

    delay(10);  // 简单消抖
  }

  // 只在发送后才更新按键状态，确保每次变化都能检测到
  if (shouldSend) {
    lastState = currentState;
  }
  
  // 未连接时降低轮询频率以省电
  if (!isConnected) {
    delay(50);
  }
}