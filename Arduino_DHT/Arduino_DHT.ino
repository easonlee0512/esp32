#include <Arduino.h>
#include <Ultrasonic.h>
#include "DHT.h"
#include <LedControlMS.h>
#include <WiFi.h>          // 加入WiFi庫
#include <PubSubClient.h>  // 加入MQTT庫
#include "pitches.h"       // 加入pitches.h

// LED矩陣設定
#define DATA_PIN 19  // DIN腳位
#define CLK_PIN 21   // CLK腳位
#define CS_PIN 18    // CS腳位
#define NBR_MTX 1    // 1個8x8矩陣
LedControl lc = LedControl(DATA_PIN, CLK_PIN, CS_PIN, NBR_MTX);

// 蜂鳴器設定
#define BUZZER_PIN 4  // 蜂鳴器腳位
// 蜂鳴器音樂陣列
int melody[] = {
  NOTE_C6, NOTE_D6, NOTE_E6, NOTE_F6, NOTE_G6, NOTE_A6, NOTE_B6
};
int noteDuration = 300;  // 每個音符持續時間

// WiFi設定
const char* ssid = "李翊辰的iPhone";         // 請修改為您的WiFi名稱
const char* password = "11000000";     // 請修改為您的WiFi密碼

// MQTT設定
const char* mqtt_server = "172.20.10.5";   // 修改為您電腦的實際IP地址
const int mqtt_port = 1883;                // MQTT伺服器埠號
const char* mqtt_client_id = "ESP32-SmartLamp"; // MQTT客戶端ID
const char* mqtt_user = "";                // MQTT使用者名稱，如果有需要
const char* mqtt_password = "";            // MQTT密碼，如果有需要

// MQTT主題
const char* topic_environment = "smartlamp/environment"; // 環境數據（溫濕度、光敏）
const char* topic_reminder = "smartlamp/reminder";      // 久坐提醒狀態
const char* topic_settings = "smartlamp/settings/sitting_reminder"; // 久坐提醒時間設定

// 建立WiFi和MQTT客戶端
WiFiClient espClient;
PubSubClient client(espClient);

// 上次發布MQTT消息的時間
unsigned long lastMqttPublishTime = 0;
const long mqttPublishInterval = 5000;  // 每5秒發布一次數據

// LED矩陣跑馬燈相關變數
String scrollText = "";          // 跑馬燈顯示文字
int textPosition = 0;            // 當前顯示位置
unsigned long scrollDelay = 300; // 跑馬燈延遲時間(毫秒)
bool isTemperature = true;       // 是否顯示溫度(否則顯示濕度)

// 字符定義 (0-9 C, %)
byte digits[10][8] = {
  {B00111000, B01000100, B01000100, B01000100, B01000100, B01000100, B00111000, B00000000}, // 0
  {B00010000, B00110000, B00010000, B00010000, B00010000, B00010000, B00111000, B00000000}, // 1
  {B00111000, B01000100, B00000100, B00001000, B00010000, B00100000, B01111100, B00000000}, // 2
  {B00111000, B01000100, B00000100, B00011000, B00000100, B01000100, B00111000, B00000000}, // 3
  {B00000100, B00001100, B00010100, B00100100, B01111100, B00000100, B00000100, B00000000}, // 4
  {B01111100, B01000000, B01111000, B00000100, B00000100, B01000100, B00111000, B00000000}, // 5
  {B00111000, B01000100, B01000000, B01111000, B01000100, B01000100, B00111000, B00000000}, // 6
  {B01111100, B00000100, B00001000, B00010000, B00100000, B00100000, B00100000, B00000000}, // 7
  {B00111000, B01000100, B01000100, B00111000, B01000100, B01000100, B00111000, B00000000}, // 8
  {B00111000, B01000100, B01000100, B00111100, B00000100, B01000100, B00111000, B00000000}  // 9
};

// 修改符號定義
byte symbols[2][8] = {
  // 百分比符號 (%)，使用用戶提供的新定義
  {
    B11100001,
    B10100010,
    B11100100,
    B00001000,
    B00010000,
    B00100111,
    B01000101,
    B10000111
  },
  // 度數符號 (°)，使用用戶提供的新定義
  {
    B11100000,
    B10100000,
    B11100000,
    B00011111,
    B00010000,
    B00010000,
    B00010000,
    B00011111
  }
};

// DHT溫濕度感測器設定
#define DHTPIN 25         // DHT11連接到25腳位
#define DHTTYPE DHT11     // 感測器類型為DHT11
DHT dht1(DHTPIN, DHTTYPE);

// 超音波感測器設定（久坐提醒功能）
Ultrasonic ultrasonic(16, 17);  // 使用16做為Trig腳位，17做為Echo腳位

// LED設定（光控功能）
int ledPin = 26;          // LED接腳
int lightSensorPin = 34;  // 光敏電阻接腳

// 紅燈設定（久坐提醒功能）
const int redLedPin = 5;  // 紅燈接腳位5

// 使用軟體模擬PWM的變數
unsigned long lastPwmTime = 0;
const int pwmCycleTime = 20; // PWM週期 (毫秒)

// 光敏電阻閾值設定
int MIN_LIGHT = 0;     // 亮環境的值（較低讀數）
int MAX_LIGHT = 500;   // 暗環境的值（較高讀數）

// 久坐提醒相關變數
const int DISTANCE_THRESHOLD = 50;       // 距離閾值（小於50cm被視為坐下）
unsigned long SITTING_TIME_THRESHOLD = 600000;  // 久坐時間閾值（預設10分鐘）
unsigned long sittingStartTime = 0;      // 開始坐下的時間
bool isSitting = false;                  // 目前是否坐著
bool alarmActive = false;                // 警報是否啟動

// 計時變數（控制功能執行頻率）
unsigned long lastLightCheckTime = 0;    // 上次檢查光線的時間
unsigned long lastDistanceCheckTime = 0; // 上次檢查距離的時間
unsigned long lastDHTReadTime = 0;       // 上次讀取溫濕度的時間
unsigned long lastScrollTime = 0;        // 上次捲動文字的時間
unsigned long lastTextChangeTime = 0;    // 上次切換顯示內容的時間
unsigned long lastLedUpdateTime = 0;     // 上次更新LED亮度的時間

// 存儲當前LED亮度
int currentBrightness = 0;

// 存儲溫濕度數據
float temperature = 0;
float humidity = 0;

// 新增：用於顯示的溫濕度數據
float displayTemperature = 0;
float displayHumidity = 0;

// 連接WiFi網路
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("連接到WiFi網路: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi已連接");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
}

// MQTT回調函數，處理接收到的消息
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("收到消息 [");
  Serial.print(topic);
  Serial.print("] ");
  
  // 將payload轉換為字串
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // 處理久坐提醒時間設定
  if (String(topic) == topic_settings) {
    // 嘗試解析JSON格式的設定
    // 格式預期為 {"reminder_time": 秒數}
    int startPos = message.indexOf("reminder_time");
    if (startPos > 0) {
      int valuePos = message.indexOf(":", startPos);
      int endPos = message.indexOf("}", valuePos);
      if (valuePos > 0 && endPos > 0) {
        // 提取數值
        String valueStr = message.substring(valuePos + 1, endPos);
        valueStr.trim();
        
        // 直接使用秒數
        unsigned long newThreshold = valueStr.toInt(); // 不乘以1000
        
        // 更新久坐時間閾值（檢查秒數範圍）
        if (newThreshold >= 30 && newThreshold <= 3600) { // 30秒到60分鐘的範圍
          SITTING_TIME_THRESHOLD = newThreshold * 1000; // 存儲時轉換為毫秒
          Serial.print("已更新久坐提醒時間閾值為 ");
          Serial.print(newThreshold);
          Serial.println(" 秒");
          
          // 重置久坐狀態
          if (isSitting) {
            sittingStartTime = millis(); // 重置計時
            alarmActive = false;         // 關閉警報
          }
        } else {
          Serial.println("久坐提醒時間超出有效範圍！有效範圍為30-3600秒");
        }
      }
    }
  }
}

// 重新連接MQTT伺服器
void reconnect() {
  // 設定重連嘗試次數
  int attempts = 0;
  
  // 嘗試連接，但限制最大重試次數，避免長時間阻塞
  while (!client.connected() && attempts < 3) {
    attempts++;
    Serial.print("嘗試MQTT連接...");
    // 嘗試連接
    if (client.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("已連接");
      
      // 發送連接成功消息
      client.publish(topic_environment, "ESP32已連接");
      
      // 訂閱設定主題
      client.subscribe(topic_settings);
      Serial.println("已訂閱久坐提醒時間設定主題");
      
      return;  // 連接成功直接返回
    } else {
      Serial.print("連接失敗, rc=");
      Serial.print(client.state());
      Serial.print(" 嘗試: ");
      Serial.print(attempts);
      Serial.println("/3");
      delay(1000);  // 短暫延遲後重試
    }
  }
  
  if (!client.connected()) {
    Serial.println("MQTT連接失敗，將在下一循環重試");
  }
}

// 使用軟體模擬PWM控制LED亮度
void softPWM(int pin, int brightness) {
  unsigned long currentTime = millis();
  
  // 計算應該開啟的時間比例
  int onTime = map(brightness, 0, 255, 0, pwmCycleTime);
  
  // 在一個PWM週期內
  if (currentTime - lastPwmTime < pwmCycleTime) {
    // 在onTime時間內保持高電平
    if (currentTime - lastPwmTime < onTime) {
      digitalWrite(pin, HIGH);
    } else {
      digitalWrite(pin, LOW);
    }
  } else {
    // 重置PWM週期
    lastPwmTime = currentTime;
  }
}

void setup() {
  Serial.begin(115200);
  
  // 初始化WiFi
  setup_wifi();
  
  // 設置MQTT伺服器和回調函數
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);  // 設置回調函數處理接收到的消息
  
  // 初始化DHT溫濕度感測器
  dht1.begin();
  
  // 初始化LED矩陣
  for (int i = 0; i < NBR_MTX; i++) {
    lc.shutdown(i, false);  // 喚醒顯示
    lc.setIntensity(i, 8);  // 設定亮度 (0-15)
    lc.clearDisplay(i);     // 清除顯示
  }
  
  // 光控LED設定
  pinMode(ledPin, OUTPUT);
  pinMode(lightSensorPin, INPUT);
  
  // 久坐提醒設定
  pinMode(redLedPin, OUTPUT);
  digitalWrite(redLedPin, LOW);  // 初始化時確保紅燈關閉
  
  // 初始化蜂鳴器
  pinMode(BUZZER_PIN, OUTPUT);
  
  // 初始化顯示切換時間
  lastTextChangeTime = millis();
  
  Serial.println("系統初始化完成...");
  delay(1000);
  
  // 初始化跑馬燈文字
  updateScrollText();
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
        Serial.print("閾值(秒): ");
        Serial.println(SITTING_TIME_THRESHOLD / 1000);
        
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
    //Serial.println("距離測量異常，請檢查感測器連接");
  }
}

// 處理DHT11溫濕度感測
void handleDHTSensor() {
  // 讀取溫濕度
  humidity = dht1.readHumidity();
  temperature = dht1.readTemperature();
  
  // 檢查是否讀取失敗
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("無法從DHT感測器讀取數據!");
    return;
  }
  
  // 讀取成功後，更新顯示用的溫濕度變數
  displayTemperature = temperature;
  displayHumidity = humidity;
  
  // 顯示溫濕度
  Serial.print("相對溼度: ");
  Serial.print(humidity);
  Serial.println(" %");
  
  Serial.print("攝氏溫度: ");
  Serial.print(temperature);
  Serial.println(" °C");
  
  Serial.println("--------------------");
  
  // 溫度高於25度時發出蜂鳴器警報
  static bool hasBeeped = false;
  if (temperature > 24.0 && !hasBeeped) {
    // 發出兩聲警報
    tone(BUZZER_PIN, NOTE_C6, 300);
    delay(500);
    tone(BUZZER_PIN, NOTE_C6, 300);
    hasBeeped = true;
    Serial.println("溫度過高警報！");
  } else if (temperature <=24.0) {
    // 溫度回到閾值以下，重置警報狀態
    hasBeeped = false;
  }
  
  // 更新跑馬燈顯示文字
  updateScrollText();
}

// 更新跑馬燈顯示文字
void updateScrollText() {
  if (isTemperature) {
    // 只顯示溫度數值和單位，使用顯示用的溫度變數
    scrollText = String((int)displayTemperature) + "C";
  } else {
    // 只顯示濕度數值和單位，使用顯示用的濕度變數
    scrollText = String((int)displayHumidity) + "%";
  }
  
  // 重置文字位置
  textPosition = 0;
  
  Serial.print("更新跑馬燈文字: ");
  Serial.println(scrollText);
}

// 在LED矩陣上顯示一個字符
void displayChar(int addr, char c) {
  lc.clearDisplay(addr);
  
  byte charPattern[8]; // 暫存字符圖案
  
  if (c >= '0' && c <= '9') {
    // 數字 0-9
    int index = c - '0';
    // 複製圖案到暫存陣列
    for (int i = 0; i < 8; i++) {
      charPattern[i] = digits[index][i];
    }
  } else if (c == '%') {
    // 百分比符號
    for (int i = 0; i < 8; i++) {
      charPattern[i] = symbols[0][i];
    }
  } else if (c == 'C') {
    // 攝氏度符號，使用度數符號
    for (int i = 0; i < 8; i++) {
      charPattern[i] = symbols[1][i];
    }
  } else if (c == ':') {
    // 冒號的特殊處理
    memset(charPattern, 0, 8); // 清空字符圖案
    lc.setLed(addr, 3, 7-2, true); // 上下翻轉
    lc.setLed(addr, 3, 7-5, true); // 上下翻轉
    return;
  } else {
    return; // 不支援的字符，直接返回
  }
  
  // 向右旋轉90度並顯示
  // 旋轉方法：將8x8矩陣中的(x,y)變成(y,7-x)
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (bitRead(charPattern[row], 7-col)) { // 檢查原始圖案的位元
        lc.setLed(addr, col, 7-row, true); // 旋轉後設置LED，並上下翻轉
      }
    }
  }
}

// 顯示跑馬燈文字
void scrollMatrixText() {
  if (scrollText.length() == 0) return;
  
  // 顯示當前位置的字符
  if (textPosition < scrollText.length()) {
    char currentChar = scrollText.charAt(textPosition);
    displayChar(0, currentChar);
    
    Serial.print("顯示字符: ");
    Serial.println(currentChar);
  } else {
    lc.clearDisplay(0);  // 清除顯示
  }
  
  // 更新位置
  textPosition++;
  
  // 如果文字已經全部顯示完，循環次數計數器增加
  static int displayCycles = 0;
  if (textPosition >= scrollText.length() + 2) {  // 加2是為了在末尾增加一些空白時間
    textPosition = 0;
    displayCycles++;
    
    // 每顯示完2次完整內容，切換到另一種顯示
    if (displayCycles >= 1) {
      displayCycles = 0;
      isTemperature = !isTemperature;
      updateScrollText();
      Serial.println("切換顯示內容: " + String(isTemperature ? "溫度" : "濕度"));
    }
  }
}

// 發布環境數據到MQTT
void publishEnvironmentData() {
  // 建立JSON格式的環境數據
  String jsonData = "{";
  jsonData += "\"temperature\":" + String(temperature) + ",";
  jsonData += "\"humidity\":" + String(humidity) + ",";
  jsonData += "\"light\":" + String(analogRead(lightSensorPin));
  jsonData += "}";
  
  // 發布到環境數據主題
  client.publish(topic_environment, jsonData.c_str());
  Serial.println("已發布環境數據到MQTT");
}

// 發布久坐提醒狀態到MQTT
void publishReminderStatus() {
  String status = "{";
  status += "\"sitting\":" + String(isSitting ? "true" : "false") + ",";
  status += "\"alarm\":" + String(alarmActive ? "true" : "false") + ",";
  
  if (isSitting) {
    unsigned long sittingDuration = millis() - sittingStartTime;
    status += "\"duration\":" + String(sittingDuration / 1000) + ",";
  } else {
    status += "\"duration\":0,";
  }
  
  // 新增：發布當前的久坐時間閾值
  status += "\"threshold\":" + String(SITTING_TIME_THRESHOLD / 1000);
  
  status += "}";
  
  client.publish(topic_reminder, status.c_str());
  Serial.println("已發布久坐提醒狀態到MQTT");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // 檢查MQTT連接狀態
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // 處理光敏電阻控制LED亮度 (每1000ms執行一次)
  if (currentMillis - lastLightCheckTime >= 1000) {
    lastLightCheckTime = currentMillis;
    handleLightControl();
  }
  
  // 使用軟體模擬PWM控制LED亮度 (每1ms更新一次)
  if (currentMillis - lastLedUpdateTime >= 1) {
    lastLedUpdateTime = currentMillis;
    softPWM(ledPin, currentBrightness);
  }
  
  // 處理超聲波感測器久坐提醒 (每1000ms執行一次)
  if (currentMillis - lastDistanceCheckTime >= 1000) {
    lastDistanceCheckTime = currentMillis;
    handleSittingReminder();
  }
  
  // 處理DHT11溫濕度感測 (每5000ms執行一次)
  if (currentMillis - lastDHTReadTime >= 5000) {
    lastDHTReadTime = currentMillis;
    handleDHTSensor();
  }
  
  // 更新LED矩陣顯示 (每800ms捲動一次)
  if (currentMillis - lastScrollTime >= 800) {
    lastScrollTime = currentMillis;
    scrollMatrixText();
  }
  
  // 發布MQTT數據 (每5秒發布一次)
  if (currentMillis - lastMqttPublishTime >= mqttPublishInterval) {
    lastMqttPublishTime = currentMillis;
    
    // 發布各種數據
    publishEnvironmentData();
    publishReminderStatus();
  }
  
  // 如果警報啟動，持續執行閃爍
  if (alarmActive) {
    blinkRedLed();
  }
  
  // 極短暫延遲，避免CPU過度佔用但不影響軟PWM精度
  delayMicroseconds(100);
}