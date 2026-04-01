#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Keyboard.h>

RF24 radio(9, 10); // CE, CSN
const byte address[6] = "00001";

char text[32] = "";

void setup() {
  Serial.begin(9600);
  Keyboard.begin();

  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening();
}

void loop() {
  if (radio.available()) {
    radio.read(&text, sizeof(text));
    Serial.print("Received: ");
    Serial.println(text);

    if (strcmp(text, "DOWN") == 0) {
      Keyboard.press('a');   // 👉 可以改成任意键
    }
    else if (strcmp(text, "UP") == 0) {
      Keyboard.release('a');
    }
  }
}