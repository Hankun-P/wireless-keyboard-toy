// nRF24 回环测试 - 一个板子同时测试发送和接收
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(7, 9);  // CE=D7, CSN=D9

// 两个地址，一个用于发送，一个用于接收
const byte tx_address[6] = "00001";
const byte rx_address[6] = "00002";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== nRF24 Loopback Test ===");
  Serial.println("This test sends data and tries to receive it back");
  
  if (!radio.begin()) {
    Serial.println("RF24 init FAILED!");
    while(1);
  }
  
  Serial.println("RF24 init OK");
  
  // 配置
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(100);
  radio.setRetries(15, 15);
  radio.setAutoAck(true);
  
  // 打开读取管道
  radio.openReadingPipe(0, tx_address);
  radio.startListening();
  
  Serial.println("Ready! Press any key to send test packet...");
}

void loop() {
  // 检查串口输入
  if (Serial.available()) {
    Serial.read();  // 清空输入
    
    // 停止监听，切换到发送模式
    radio.stopListening();
    radio.openWritingPipe(tx_address);
    
    const char text[] = "TEST";
    Serial.print("Sending: ");
    Serial.println(text);
    
    bool result = radio.write(&text, sizeof(text));
    
    if (result) {
      Serial.println("Send OK - ACK received!");
    } else {
      Serial.println("Send FAIL - No ACK!");
    }
    
    // 切换回接收模式
    radio.openReadingPipe(0, tx_address);
    radio.startListening();
    
    Serial.println("Switched back to receive mode");
  }
  
  // 尝试接收数据
  if (radio.available()) {
    char text[32] = {0};
    radio.read(&text, sizeof(text));
    
    Serial.print("[RX] Received: ");
    Serial.println(text);
  }
  
  // 心跳
  static unsigned long lastHB = 0;
  if (millis() - lastHB > 5000) {
    lastHB = millis();
    Serial.print("[HB] available=");
    Serial.print(radio.available());
    Serial.print(", isChipConnected=");
    Serial.println(radio.isChipConnected());
  }
}
