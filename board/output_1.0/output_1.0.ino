#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define BUTTON_PIN 2

RF24 radio(9, 10); // CE, CSN
const byte address[6] = "00001";

bool lastState = HIGH;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(9600);

  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();
}

void loop() {
  bool currentState = digitalRead(BUTTON_PIN);

  // 状态变化才发送（防抖 + 减少通信量）
  if (currentState != lastState) {
    if (currentState == LOW) {
      const char text[] = "DOWN";
      radio.write(&text, sizeof(text));
      Serial.println("Send: DOWN");
    } else {
      const char text[] = "UP";
      radio.write(&text, sizeof(text));
      Serial.println("Send: UP");
    }
    delay(20); // 简单防抖
  }

  lastState = currentState;
}