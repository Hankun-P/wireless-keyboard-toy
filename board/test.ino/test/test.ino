#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("开始初始化...");
  
  // 先测试 SPI
  Serial.println("SPI 开始");
  SPI.begin();
  Serial.println("SPI 完成");
  
  // 再测试 nRF24
  Serial.println("nRF24 开始");
  bool result = radio.begin();
  Serial.print("nRF24 结果: ");
  Serial.println(result ? "成功" : "失败");
}

void loop() {}