#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);
const byte address[6] = "00001";

struct Packet {
  uint8_t keycode;
  uint8_t state;
  uint16_t seq;
};

Packet p;

void setup() {
  Serial.begin(115200);

  radio.begin();
  radio.openReadingPipe(0, address);
  radio.startListening();
}

void loop() {
  if (radio.available()) {
    radio.read(&p, sizeof(p));

    // 👉 只转发，不解释
    Serial.print(p.keycode);
    Serial.print(",");
    Serial.print(p.state);
    Serial.print(",");
    Serial.println(p.seq);
  }
}