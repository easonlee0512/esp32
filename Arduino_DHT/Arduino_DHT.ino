// 通用版本，不使用ESP32特定PWM功能
// 改用數字輸出模擬PWM效果

#include <Arduino.h>

int ledPin = 26;          // LED接腳
int lightSensorPin = 27;  // 光敏電阻接腳

// 光敏電阻閾值設定
int MIN_LIGHT = 100;    // 非常暗的環境
int MAX_LIGHT = 2000;   // 明亮的環境

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

// 非線性映射函數，使暗環境時亮度變化更大
int exponentialMap(int value, int in_min, int in_max, int out_min, int out_max) {
  // 確保輸入值在範圍內
  value = constrain(value, in_min, in_max);
  
  // 計算輸入值在輸入範圍內的相對位置（0-1）
  float normalized = (float)(value - in_min) / (float)(in_max - in_min);
  
  // 應用指數函數使暗環境（低值）時的變化更明顯
  // 使用平方來增強變化
  normalized = 1.0 - normalized;  // 反轉，因為我們希望光線越弱，亮度越強
  normalized = normalized * normalized;  // 平方增強效果
  
  // 映射回輸出範圍
  return (int)(normalized * (out_max - out_min)) + out_min;
}

void loop() {
  // 讀取光敏電阻值
  int lightValue = analogRead(lightSensorPin);
  
  // 輸出讀數方便調試
  Serial.print("環境光線強度: ");
  Serial.println(lightValue);
  
  // 使用指數映射增強暗環境亮度變化
  int brightness = exponentialMap(lightValue, MIN_LIGHT, MAX_LIGHT, 0, 255);
  
  // 限制亮度在0-255範圍內
  brightness = constrain(brightness, 0, 255);
  
  // 使用我們的自定義PWM函數控制LED
  for (int i = 0; i < 30; i++) {  // 增加重複次數以產生更平滑的效果
    customAnalogWrite(ledPin, brightness);
  }
  
  // 輸出LED亮度值
  Serial.print("LED亮度設定為: ");
  Serial.println(brightness);
}