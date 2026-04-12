// nRF24 简单测试 - 发送端 (Output)
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);
const byte address[6] = "00001";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== nRF24 Sender Test ===");
  
  if (!radio.begin()) {
    Serial.println("RF24 init FAILED!");
    while(1);
  }
  
  Serial.println("RF24 init OK");
  
  // 使用 ACK 模式，确保发送成功
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MAX);  // 最大功率
  radio.setDataRate(RF24_1MBPS);  // 1Mbps
  radio.setChannel(100);  // 换信道，避免干扰
  radio.setRetries(15, 15);  // 自动重试
  radio.setAutoAck(true);  // 启用 ACK
  radio.stopListening();
  
  Serial.println("Sender ready!");
}

void loop() {
  const char text[] = "Hello";
  
  Serial.print("Sending: ");
  Serial.println(text);
  
  bool result = radio.write(&text, sizeof(text));
  
  if (result) {
    Serial.println("Send OK (ACK received)");
  } else {
    Serial.println("Send FAIL (no ACK)");
  }
  
  delay(1000);
}
