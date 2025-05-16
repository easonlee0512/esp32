// 通用版本，不使用ESP32特定PWM功能
// 改用數字輸出模擬PWM效果

#include <Arduino.h>

int ledPin = 26;          // LED接腳
int lightSensorPin = 27;  // 光敏電阻接腳

// 光敏電阻閾值設定
int MIN_LIGHT = 0;     // 亮環境的值（較低讀數）
int MAX_LIGHT = 500;  // 暗環境的值（較高讀數）

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(lightSensorPin, INPUT);
}

// 增強版PWM函數，提高亮度對比度
void customAnalogWrite(int pin, int value) {
  // 提高對比度，使亮度變化更明顯
  int onTime = map(value, 0, 255, 0, 20);  // 增加最大時間以增強亮度
  int offTime = 20 - onTime;
  
  digitalWrite(pin, HIGH);
  delayMicroseconds(onTime * 200);  // 增加延遲以使亮度變化更明顯
  digitalWrite(pin, LOW);
  delayMicroseconds(offTime * 200);
}

void loop() {
  // 讀取光敏電阻值
  int lightValue = analogRead(lightSensorPin);
  
  // 輸出讀數方便調試
  Serial.print("環境光線強度: ");
  Serial.println(lightValue);
  
  // 直接反比映射 - 光線越暗，LED越亮
  // 大多數光敏電阻在暗處時電阻值較高，讀數較高
  int brightness = map(lightValue, MIN_LIGHT, MAX_LIGHT, 0, 255);
  
  // 限制亮度在0-255範圍內
  brightness = constrain(brightness, 0, 255);
  
  // 使用我們的自定義PWM函數控制LED
  for (int i = 0; i < 100; i++) {  // 增加重複次數以產生更平滑的效果
    customAnalogWrite(ledPin, brightness);
  }
  
  // 輸出LED亮度值
  Serial.print("LED亮度設定為: ");
  Serial.println(brightness);
}