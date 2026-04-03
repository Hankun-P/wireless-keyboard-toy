#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define BUTTON_PIN 2

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

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  radio.begin();
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

    radio.write(&p, sizeof(p));

    delay(10);  // 简单消抖
  }

  lastState = currentState;
}