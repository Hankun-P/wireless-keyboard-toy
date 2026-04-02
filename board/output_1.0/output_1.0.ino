#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define BUTTON_PIN 2

RF24 radio(9, 10);
const byte address[6] = "00001";

bool lastState = HIGH;
unsigned long seq = 0;

struct Packet {
  uint8_t keycode;
  uint8_t state;
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
    p.keycode = 0; // 这个键编号
    p.state = (currentState == LOW) ? 1 : 0;
    p.seq = seq++;

    radio.write(&p, sizeof(p));

    delay(10);
  }

  lastState = currentState;
}