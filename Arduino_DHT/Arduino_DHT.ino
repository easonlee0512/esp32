#include <Arduino.h>
#include <Ultrasonic.h>

// 超音波感測器設定（久坐提醒功能）
Ultrasonic ultrasonic(16, 17);  // 使用16做為Trig腳位，17做為Echo腳位

// LED設定（光控功能）
int ledPin = 26;          // LED接腳
int lightSensorPin = 27;  // 光敏電阻接腳

// Arduino analogWrite模擬 (ESP32沒有原生的analogWrite)
void analogWriteESP(uint8_t pin, uint8_t value) {
  // 用1kHz頻率生成PWM
  int freq = 1000;
  int high = value * freq / 255;
  int low = freq - high;
  
  // 若設定為0或255，直接設置高低電平即可
  if (value == 0) {
    digitalWrite(pin, LOW);
    return;
  } else if (value == 255) {
    digitalWrite(pin, HIGH);
    return;
  }
  
  // 保持高電平
  digitalWrite(pin, HIGH);
  delayMicroseconds(high);
  // 保持低電平
  digitalWrite(pin, LOW);
  delayMicroseconds(low);
}

// 紅燈設定（久坐提醒功能）
const int redLedPin = 5;  // 紅燈接腳位5

// 光敏電阻閾值設定
int MIN_LIGHT = 0;     // 亮環境的值（較低讀數）
int MAX_LIGHT = 500;   // 暗環境的值（較高讀數）

// 久坐提醒相關變數
const int DISTANCE_THRESHOLD = 50;       // 距離閾值（小於50cm被視為坐下）
const unsigned long SITTING_TIME_THRESHOLD = 10000;  // 久坐時間閾值（10秒用於測試）
unsigned long sittingStartTime = 0;      // 開始坐下的時間
bool isSitting = false;                  // 目前是否坐著
bool alarmActive = false;                // 警報是否啟動

// 計時變數（控制功能執行頻率）
unsigned long lastLightCheckTime = 0;    // 上次檢查光線的時間
unsigned long lastDistanceCheckTime = 0; // 上次檢查距離的時間
unsigned long lastPWMTime = 0;           // 上次PWM更新時間

// 存儲當前LED亮度
int currentBrightness = 0;

void setup() {
  Serial.begin(115200);
  
  // 光控LED設定
  pinMode(ledPin, OUTPUT);
  pinMode(lightSensorPin, INPUT);
  
  // 久坐提醒設定
  pinMode(redLedPin, OUTPUT);
  digitalWrite(redLedPin, LOW);  // 初始化時確保紅燈關閉
  
  Serial.println("系統初始化完成...");
  delay(1000);
}

// 閃爍紅燈函數
void blinkRedLed() {
  static unsigned long lastBlinkTime = 0;
  static bool ledState = false;
  
  // 每250毫秒切換一次LED狀態（快速閃爍）
  if (millis() - lastBlinkTime > 250) {
    lastBlinkTime = millis();
    ledState = !ledState;
    digitalWrite(redLedPin, ledState);
  }
}

// 處理光敏電阻控制LED亮度
void handleLightControl() {
  // 讀取光敏電阻值
  int lightValue = analogRead(lightSensorPin);
  
  // 輸出讀數方便調試
  Serial.print("環境光線強度: ");
  Serial.println(lightValue);
  
  // 反比映射 - 光線越亮，LED越暗；光線越暗，LED越亮
  // 將輸出範圍從(0,255)改為(255,0)實現反向映射
  int brightness = map(lightValue, MIN_LIGHT, MAX_LIGHT, 255, 0);
  
  // 限制亮度在0-255範圍內
  brightness = constrain(brightness, 0, 255);
  
  // 保存當前亮度，供主迴圈使用
  currentBrightness = brightness;
  
  // 輸出LED亮度值
  Serial.print("LED亮度設定為: ");
  Serial.println(brightness);
}

// 處理超聲波感測器久坐提醒
void handleSittingReminder() {
  // 讀取超聲波感測時間（微秒）
  long timing = ultrasonic.timing();
  
  // 將時間轉換為距離（公分）
  float distance = ultrasonic.convert(timing, Ultrasonic::CM);
  
  // 輸出測量詳情
  Serial.print("經過時間(單位µs): ");
  Serial.println(timing);
  Serial.print("感測距離(單位cm): ");
  Serial.println(distance);
  
  // 檢查距離是否在合理範圍內
  if (timing > 0 && distance <= 400) { // HC-SR04最大測量距離約400cm
    // 判斷是否坐下（距離小於閾值）
    if (distance < DISTANCE_THRESHOLD) {
      if (!isSitting) {
        // 剛坐下，記錄開始時間
        isSitting = true;
        sittingStartTime = millis();
        Serial.println("偵測到人員坐下，開始計時");
      } else {
        // 持續坐著，檢查是否達到久坐時間
        unsigned long sittingDuration = millis() - sittingStartTime;
        
        // 輸出已坐時間
        Serial.print("已坐時間(秒): ");
        Serial.println(sittingDuration / 1000);
        
        if (sittingDuration >= SITTING_TIME_THRESHOLD && !alarmActive) {
          // 達到久坐時間閾值，啟動警報
          alarmActive = true;
          Serial.println("警告：已久坐超過設定時間！");
        } else if (!alarmActive) {
          // 計時中，確保紅燈關閉
          digitalWrite(redLedPin, LOW);
        }
      }
    } else {
      // 未偵測到人員坐下，重置狀態
      if (isSitting) {
        Serial.println("偵測到人員起身，重置計時");
      }
      isSitting = false;
      alarmActive = false;
      digitalWrite(redLedPin, LOW);  // 確保LED關閉
    }
    
    // 如果警報啟動，閃爍紅燈
    if (alarmActive) {
      blinkRedLed();
    }
  } else {
    // 距離測量異常
    Serial.println("距離測量異常，請檢查感測器連接");
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // 處理光敏電阻控制LED亮度 (每500ms執行一次)
  if (currentMillis - lastLightCheckTime >= 500) {
    lastLightCheckTime = currentMillis;
    handleLightControl();
  }
  
  // 處理超聲波感測器久坐提醒 (每1000ms執行一次)
  if (currentMillis - lastDistanceCheckTime >= 1000) {
    lastDistanceCheckTime = currentMillis;
    handleSittingReminder();
  }
  
  // 持續更新LED PWM (每1ms更新一次，保持恆亮效果)
  if (currentMillis - lastPWMTime >= 1) {
    lastPWMTime = currentMillis;
    analogWriteESP(ledPin, currentBrightness);
  }
  
  // 如果警報啟動，持續執行閃爍
  if (alarmActive) {
    blinkRedLed();
  }
  
  // 短暫延遲，避免CPU過度佔用
  delayMicroseconds(100);
}