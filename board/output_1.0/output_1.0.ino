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

RF24 radio(9, 10);
const byte address[6] = "00001";

bool lastState = HIGH;
bool initialized = false;  // 初始化标志
unsigned long seq = 0;

// 数据包结构（与 input 端保持一致）
struct Packet {
  uint8_t keycode;   // 物理按键编号 (固定为0)
  uint8_t state;     // 0=释放, 1=按下
  uint16_t seq;
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

  // nRF24 初始化检查
  if (!radio.begin()) {
    Serial.println("RF24_INIT_FAILED");
    errorBlink(LED_BLINK_FAST);  // 快速闪烁
  }
  
  // 检查芯片连接
  if (!radio.isChipConnected()) {
    Serial.println("RF24_NOT_CONNECTED");
    errorBlink(LED_BLINK_SLOW);  // 慢速闪烁
  }
  
  Serial.println("RF24_OK");
  
  // 初始化成功，LED 闪烁 2 次后关闭
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_HIGH);  // 提高功率
  radio.setDataRate(RF24_250KBPS);  // 降低速率增加稳定性
  radio.setChannel(76);  // 固定信道
  radio.setAutoAck(false);  // 禁用自动应答（不需要ACK）
  radio.stopListening();
}

// 读取电池电量（简化：固定返回3，表示满电）
uint8_t readBatteryLevel() {
  // 暂时禁用 ADC 读取，避免 A0 悬空导致问题
  return 3;  // 固定返回满电
}

// 上次发送电量的时间
unsigned long lastBatterySend = 0;
// 上次的电量值（用于检测变化）
uint8_t lastBatteryLevel = 255;

void loop() {
  static unsigned long lastDebug = 0;
  unsigned long now = millis();
  
  // 每秒输出一次心跳，确认程序还在运行
  if (now - lastDebug > 1000) {
    Serial.print(".");
    lastDebug = now;
  }
  
  bool currentState = digitalRead(BUTTON_PIN);
  uint8_t currentBattery = readBatteryLevel();

  // 首次运行时初始化状态，避免误触发
  if (!initialized) {
    lastState = currentState;
    lastBatteryLevel = currentBattery;
    initialized = true;
    Serial.println("INIT_DONE");
    return;  // 跳过第一次循环
  }

  // 检查是否需要发送数据
  bool shouldSend = false;
  bool isKeyChange = (currentState != lastState);
  bool isBatteryTime = (now - lastBatterySend > BATTERY_SEND_INTERVAL);
  bool isBatteryChange = (currentBattery != lastBatteryLevel);
  
  // 按键状态变化、定时发送电量、电量变化时发送
  if (isKeyChange || isBatteryTime || isBatteryChange) {
    shouldSend = true;
  }
  
  if (shouldSend) {
    Packet p;
    p.keycode = 0;
    p.state = (currentState == LOW) ? 1 : 0;  // LOW=按下(1), HIGH=释放(0)
    p.seq = seq++;
    p.battery = currentBattery;  // 0-3 四档电量

    // 调试输出
    Serial.print("BTN:");
    Serial.print(p.state);
    Serial.print(",BAT:");
    Serial.print(p.battery);
    Serial.print(",SEQ:");
    Serial.println(p.seq);

    // 发送数据，带重试机制
    bool sent = false;
    Serial.print("WRITE_START ");
    for (int i = 0; i < MAX_RETRY; i++) {
      Serial.print(i);
      if (radio.write(&p, sizeof(p))) {
        sent = true;
        break;
      }
      delay(5);
    }
    Serial.print(" ");
    Serial.println(sent ? "SEND_OK" : "SEND_FAIL");
    
    // 更新发送记录
    lastBatterySend = now;
    lastBatteryLevel = currentBattery;

    delay(10);  // 简单消抖
  }

  // 只在发送后才更新按键状态，确保每次变化都能检测到
  if (shouldSend) {
    lastState = currentState;
  }
}