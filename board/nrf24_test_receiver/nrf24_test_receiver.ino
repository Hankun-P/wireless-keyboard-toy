// nRF24 简单测试 - 接收端 (Input)
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);
const byte address[6] = "00001";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== nRF24 Receiver Test ===");
  
  if (!radio.begin()) {
    Serial.println("RF24 init FAILED!");
    while(1);
  }
  
  Serial.println("RF24 init OK");
  
  // 与发送端相同配置
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MAX);  // 最大功率
  radio.setDataRate(RF24_1MBPS);  // 1Mbps
  radio.setChannel(100);  // 换信道
  radio.setAutoAck(true);  // 启用 ACK
  radio.startListening();
  
  Serial.println("Receiver ready!");
}

void loop() {
  // 心跳
  static unsigned long lastHB = 0;
  if (millis() - lastHB > 3000) {
    lastHB = millis();
    Serial.print("[HB] available=");
    Serial.print(radio.available());
    Serial.print(", isChipConnected=");
    Serial.println(radio.isChipConnected());
  }
  
  // 接收数据
  if (radio.available()) {
    char text[32] = {0};
    radio.read(&text, sizeof(text));
    
    Serial.print("[RX] Received: ");
    Serial.println(text);
  }
}
