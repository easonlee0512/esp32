#include <Arduino.h>
#include <Ultrasonic.h>
#include "DHT.h"
#include <LedControlMS.h>
#include <WiFi.h>          // 加入WiFi庫
#include <PubSubClient.h>  // 加入MQTT庫
#include "pitches.h"       // 加入pitches.h
#include <time.h>          // 加入時間庫
#include <HTTPClient.h>    // 加入HTTP客戶端庫
#include <ArduinoJson.h>   // 加入JSON處理庫

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
const char* mqtt_server = "0.0.0.0";   // 修改為您電腦的實際IP地址
const int mqtt_port = 1883;                // MQTT伺服器埠號
const char* mqtt_client_id = "ESP32-SmartLamp"; // MQTT客戶端ID
const char* mqtt_user = "";                // MQTT使用者名稱，如果有需要
const char* mqtt_password = "";            // MQTT密碼，如果有需要

// MQTT主題
const char* topic_environment = "smartlamp/environment"; // 環境數據（溫濕度、光敏）
const char* topic_reminder = "smartlamp/reminder";      // 久坐提醒狀態
const char* topic_settings = "smartlamp/settings/sitting_reminder"; // 久坐提醒時間設定
const char* topic_alarm = "smartlamp/settings/alarm";   // 鬧鐘設定
const char* topic_light_control = "smartlamp/light/control"; // 新增：燈光控制主題

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
bool lightEnabled = true; // 新增：控制燈光開關的變數

// 存儲溫濕度數據
float temperature = 0;
float humidity = 0;

// 新增：用於顯示的溫濕度數據
float displayTemperature = 0;
float displayHumidity = 0;

// NTP伺服器設定
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800;  // 台灣時區 UTC+8 (8*60*60)
const int   daylightOffset_sec = 0;

// 鬧鐘相關變數
bool alarmEnabled = false;         // 鬧鐘是否啟用
int alarmHour = 0;                 // 鬧鐘小時
int alarmMinute = 0;               // 鬧鐘分鐘
bool alarmTriggered = false;       // 鬧鐘是否已觸發
unsigned long alarmLastCheckTime = 0;  // 上次檢查鬧鐘的時間
const long alarmCheckInterval = 5000;  // 檢查鬧鐘間隔 (毫秒)
int alarmMelody[] = {               // 鬧鐘音樂
  NOTE_C6, NOTE_E6, NOTE_G6, NOTE_C7, NOTE_G6, NOTE_E6, NOTE_C6
};
int alarmMelodyLength = 7;          // 音樂長度
int alarmCurrentNote = 0;           // 當前播放的音符
unsigned long alarmLastNoteTime = 0; // 上次播放音符的時間
unsigned long alarmStartTime = 0;    // 鬧鐘開始時間
const long alarmDuration = 10000;    // 鬧鐘持續時間 (10秒)

// API相關設定
const char* nodeRedApiEndpoint = "http://172.20.10.5:1880/sitting-record";  // Node-RED API端點
unsigned long lastFailoverAttempt = 0;
const long failoverRetryInterval = 10000;  // 10秒重試一次

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
  
  // 處理燈光控制
  if (String(topic) == topic_light_control) {
    // 解析JSON格式的控制命令
    // 格式預期為 {"power": true/false}
    int powerPos = message.indexOf("power");
    if (powerPos > 0) {
      int valuePos = message.indexOf(":", powerPos);
      int endPos = message.indexOf("}", valuePos);
      if (valuePos > 0 && endPos > 0) {
        String powerStr = message.substring(valuePos + 1, endPos);
        powerStr.trim();
        lightEnabled = (powerStr == "true");
        
        // 如果關閉燈光，強制設定亮度為0
        if (!lightEnabled) {
          currentBrightness = 0;
        }
        
        Serial.print("燈光狀態設定為: ");
        Serial.println(lightEnabled ? "開啟" : "關閉");
        
        // 發送確認消息
        String confirmMsg = "{\"status\":\"" + String(lightEnabled ? "on" : "off") + "\"}";
        client.publish("smartlamp/light/status", confirmMsg.c_str());
      }
    }
  }
  
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
  // 處理鬧鐘設定
  else if (String(topic) == topic_alarm) {
    // 嘗試解析JSON格式的鬧鐘設定
    // 格式預期為 {"enabled": true/false, "hour": 小時, "minute": 分鐘}
    
    // 解析是否啟用
    int enabledPos = message.indexOf("enabled");
    if (enabledPos > 0) {
      int valuePos = message.indexOf(":", enabledPos);
      int commaPos = message.indexOf(",", valuePos);
      if (valuePos > 0) {
        String enabledStr = message.substring(valuePos + 1, commaPos > 0 ? commaPos : message.indexOf("}", valuePos));
        enabledStr.trim();
        alarmEnabled = (enabledStr == "true");
        
        Serial.print("鬧鐘狀態設定為: ");
        Serial.println(alarmEnabled ? "啟用" : "停用");
      }
    }
    
    // 只有啟用時才解析時間
    if (alarmEnabled) {
      // 解析小時
      int hourPos = message.indexOf("hour");
      if (hourPos > 0) {
        int valuePos = message.indexOf(":", hourPos);
        int commaPos = message.indexOf(",", valuePos);
        if (valuePos > 0) {
          String hourStr = message.substring(valuePos + 1, commaPos > 0 ? commaPos : message.indexOf("}", valuePos));
          hourStr.trim();
          int newHour = hourStr.toInt();
          
          // 檢查小時有效性 (0-23)
          if (newHour >= 0 && newHour <= 23) {
            alarmHour = newHour;
            Serial.print("鬧鐘小時設定為: ");
            Serial.println(alarmHour);
          }
        }
      }
      
      // 解析分鐘
      int minutePos = message.indexOf("minute");
      if (minutePos > 0) {
        int valuePos = message.indexOf(":", minutePos);
        int commaPos = message.indexOf("}", valuePos);
        if (valuePos > 0) {
          String minuteStr = message.substring(valuePos + 1, commaPos);
          minuteStr.trim();
          int newMinute = minuteStr.toInt();
          
          // 檢查分鐘有效性 (0-59)
          if (newMinute >= 0 && newMinute <= 59) {
            alarmMinute = newMinute;
            Serial.print("鬧鐘分鐘設定為: ");
            Serial.println(alarmMinute);
          }
        }
      }
      
      // 重置鬧鐘觸發狀態
      alarmTriggered = false;
      
      // 發送確認消息
      String confirmMsg = "{\"status\":\"success\",\"enabled\":" + String(alarmEnabled ? "true" : "false");
      confirmMsg += ",\"hour\":" + String(alarmHour) + ",\"minute\":" + String(alarmMinute) + "}";
      client.publish("smartlamp/alarm/status", confirmMsg.c_str());
    } else {
      // 禁用鬧鐘時發送確認
      String confirmMsg = "{\"status\":\"success\",\"enabled\":false}";
      client.publish("smartlamp/alarm/status", confirmMsg.c_str());
    }
  }
}

// 重新連接MQTT伺服器
void reconnect() {
  // 設定重連嘗試次數
  int attempts = 0;
  
  Serial.println("開始嘗試MQTT連接...");
  Serial.print("MQTT伺服器: ");
  Serial.println(mqtt_server);
  Serial.print("MQTT埠號: ");
  Serial.println(mqtt_port);
  
  // 嘗試連接，但限制最大重試次數，避免長時間阻塞
  while (!client.connected() && attempts < 3) {
    attempts++;
    Serial.print("MQTT連接嘗試 #");
    Serial.println(attempts);
    
    // 嘗試連接
    if (client.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("MQTT連接成功！");
      
      // 發送連接成功消息
      client.publish(topic_environment, "{\"status\":\"ESP32已連接\"}");
      
      // 訂閱設定主題
      client.subscribe(topic_settings);
      Serial.println("已訂閱主題: " + String(topic_settings));
      
      // 訂閱鬧鐘設定主題
      client.subscribe(topic_alarm);
      Serial.println("已訂閱主題: " + String(topic_alarm));
      
      // 訂閱燈光控制主題
      client.subscribe(topic_light_control);
      Serial.println("已訂閱主題: " + String(topic_light_control));
      
      return;
    } else {
      Serial.print("MQTT連接失敗, 錯誤代碼=");
      Serial.println(client.state());
      Serial.println("等待1秒後重試...");
      delay(1000);
    }
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
  delay(1000);  // 等待序列埠穩定
  
  Serial.println("\n=== ESP32智慧檯燈系統啟動 ===");
  Serial.println("版本: 1.0");
  Serial.println("正在初始化...");
  
  // 初始化WiFi
  Serial.println("\n[WiFi] 開始連接...");
  setup_wifi();
  
  // 設置MQTT
  Serial.println("\n[MQTT] 設置MQTT伺服器...");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  // 初始化時間
  Serial.println("\n[時間] 設置NTP伺服器...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // 初始化感測器
  Serial.println("\n[感測器] 初始化中...");
  dht1.begin();
  
  // 初始化LED矩陣
  Serial.println("\n[LED矩陣] 初始化中...");
  for (int i = 0; i < NBR_MTX; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 8);
    lc.clearDisplay(i);
  }
  
  // 設置GPIO
  Serial.println("\n[GPIO] 設置輸入輸出腳位...");
  pinMode(ledPin, OUTPUT);
  pinMode(lightSensorPin, INPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // 初始化完成
  Serial.println("\n=== 初始化完成 ===");
  Serial.println("WiFi IP: " + WiFi.localIP().toString());
  Serial.println("MQTT伺服器: " + String(mqtt_server));
  Serial.println("正在等待第一次感測器讀數...\n");
  
  // 初始化感測器讀數
  handleDHTSensor();
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
  // 如果燈光被關閉，直接返回
  if (!lightEnabled) {
    currentBrightness = 0;
    return;
  }

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
    // 發出兩聲警報，使用非阻塞方式
    tone(BUZZER_PIN, NOTE_C6, 300);
    hasBeeped = true;
    Serial.println("溫度過高警報！");
  } else if (temperature <=24.0) {
    // 溫度回到閾值以下，重置警報狀態
    hasBeeped = false;
  }
}

// 更新跑馬燈顯示文字
void updateScrollText() {
  struct tm timeinfo;
  static int displayMode = 0; // 0:時間, 1:溫度, 2:濕度
  char timeString[6]; // 移到函數作用域，避免局部變數問題
  
  if(!getLocalTime(&timeinfo)){
    Serial.println("無法獲取時間");
    // 如果獲取時間失敗，改為顯示溫度
    displayMode = 1;
  }
  
  switch(displayMode) {
    case 0: // 顯示時間
      sprintf(timeString, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
      scrollText = String(timeString);
      break;
    case 1: // 顯示溫度
      scrollText = String((int)displayTemperature) + "C";
      break;
    case 2: // 顯示濕度
      scrollText = String((int)displayHumidity) + "%";
      break;
  }
  
  // 重置文字位置
  textPosition = 0;
  
  Serial.print("更新跑馬燈文字: ");
  Serial.println(scrollText);
  
  // 更新顯示模式
  displayMode = (displayMode + 1) % 3; // 三種顯示模式循環
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
  } else if (c == 'A') {
    // 字母A - 自定義圖案
    byte aPattern[8] = {
      B00111000,
      B01000100,
      B01000100,
      B01111100,
      B01000100,
      B01000100,
      B01000100,
      B00000000
    };
    for (int i = 0; i < 8; i++) {
      charPattern[i] = aPattern[i];
    }
  } else if (c == 'L') {
    // 字母L - 自定義圖案
    byte lPattern[8] = {
      B01000000,
      B01000000,
      B01000000,
      B01000000,
      B01000000,
      B01000000,
      B01111100,
      B00000000
    };
    for (int i = 0; i < 8; i++) {
      charPattern[i] = lPattern[i];
    }
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
  if (scrollText.length() == 0) {
    // 如果文字為空，重新初始化
    updateScrollText();
    return;
  }
  
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
    
    // 每顯示完1次完整內容就更新顯示內容
    if (displayCycles >= 2) {  // 增加到2次，增強穩定性
      displayCycles = 0;
      updateScrollText();
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

// 透過API發送久坐記錄
void sendSittingRecordViaAPI() {
  // 只有當使用者從坐著變成站起來時才發送記錄
  static bool lastSittingState = false;
  
  if (lastSittingState && !isSitting) {  // 從坐著變成站起
    Serial.println("\n[API] 檢測到使用者站起，開始發送久坐記錄...");
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[API] 錯誤: WiFi未連接");
      return;
    }

    HTTPClient http;
    Serial.print("[API] 連接到端點: ");
    Serial.println(nodeRedApiEndpoint);
    
    // 取得目前時間作為結束時間
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) {
      Serial.println("[API] 錯誤: 無法獲取時間");
      return;
    }
    
    char endTimeStr[20];
    strftime(endTimeStr, sizeof(endTimeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    // 計算開始時間
    unsigned long sittingDuration = (millis() - sittingStartTime) / 1000; // 轉換為秒
    time_t startTimestamp = time(NULL) - sittingDuration;
    struct tm* startTime = localtime(&startTimestamp);
    
    char startTimeStr[20];
    strftime(startTimeStr, sizeof(startTimeStr), "%Y-%m-%d %H:%M:%S", startTime);
    
    // 建立JSON
    StaticJsonDocument<200> doc;
    doc["start_time"] = startTimeStr;
    doc["end_time"] = endTimeStr;
    doc["duration_seconds"] = sittingDuration;
    
    String jsonString;
    serializeJson(doc, jsonString);
    Serial.print("[API] 發送數據: ");
    Serial.println(jsonString);
    
    http.begin(nodeRedApiEndpoint);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST(jsonString);
    Serial.print("[API] 回應代碼: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("[API] 回應內容: ");
      Serial.println(response);
    } else {
      Serial.print("[API] 錯誤: ");
      Serial.println(http.errorToString(httpResponseCode));
    }
    
    http.end();
    Serial.println("[API] 請求完成\n");
  }
  
  // 更新上一次的坐姿狀態
  lastSittingState = isSitting;
}

// 發布久坐提醒狀態到MQTT
void publishReminderStatus() {
  // 檢查MQTT連接狀態
  if (client.connected()) {
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
    Serial.println("[MQTT] 已發布久坐提醒狀態");
    return; // 如果MQTT發送成功，直接返回，不執行API
  } 
  
  // 只有當MQTT未連接時，才使用API備用機制
  Serial.println("[MQTT] 連接已斷開，準備使用API備用機制");
  unsigned long currentTime = millis();
  if (currentTime - lastFailoverAttempt >= failoverRetryInterval) {
    lastFailoverAttempt = currentTime;
    Serial.println("[MQTT] 嘗試使用API發送數據");
    sendSittingRecordViaAPI();
  }
}

// 發布鬧鐘狀態到MQTT
void publishAlarmStatus() {
  String status = "{";
  status += "\"enabled\":" + String(alarmEnabled ? "true" : "false") + ",";
  status += "\"hour\":" + String(alarmHour) + ",";
  status += "\"minute\":" + String(alarmMinute);
  status += "}";
  
  client.publish("smartlamp/alarm/status", status.c_str());
  Serial.println("已發布鬧鐘狀態到MQTT");
}

// 檢查並處理鬧鐘
void handleAlarm() {
  struct tm timeinfo;
  unsigned long currentMillis = millis();
  
  // 如果鬧鐘沒有啟用，直接返回
  if (!alarmEnabled) {
    return;
  }
  
  // 檢查鬧鐘是否已觸發但需要結束
  if (alarmTriggered) {
    // 如果已經響了10秒，停止鬧鐘
    if (currentMillis - alarmStartTime >= alarmDuration) {
      Serial.println("鬧鐘已響10秒，停止鬧鐘");
      alarmTriggered = false;
      noTone(BUZZER_PIN);  // 停止蜂鳴器
      digitalWrite(redLedPin, LOW);  // 關閉紅燈
      return;
    }
    // 如果還在10秒內，繼續播放
    playAlarmMelody();
    return;
  }
  
  // 獲取當前時間
  if(!getLocalTime(&timeinfo)){
    Serial.println("無法獲取時間");
    return;
  }
  
  // 檢查是否到達鬧鐘時間
  if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute) {
    // 時間匹配且鬧鐘未觸發
    if (!alarmTriggered) {
      Serial.println("鬧鐘時間到！開始播放鬧鐘音樂");
      alarmTriggered = true;
      alarmCurrentNote = 0;
      alarmLastNoteTime = 0; // 重置為0，確保立即開始播放
      alarmStartTime = currentMillis; // 記錄鬧鐘開始時間
    }
  }
}

// 播放鬧鐘音樂
void playAlarmMelody() {
  // 如果鬧鐘沒有觸發，直接返回
  if (!alarmTriggered) {
    return;
  }
  
  unsigned long currentMillis = millis();
  
  // 檢查是否需要播放下一個音符
  if (currentMillis - alarmLastNoteTime > 300) { // 每個音符持續300毫秒
    alarmLastNoteTime = currentMillis;
    
    // 停止之前的音符
    noTone(BUZZER_PIN);
    
    // 播放當前音符
    tone(BUZZER_PIN, alarmMelody[alarmCurrentNote], 250);
    
    // 閃爍紅燈
    digitalWrite(redLedPin, !digitalRead(redLedPin));
    
    // 移至下一個音符
    alarmCurrentNote = (alarmCurrentNote + 1) % alarmMelodyLength;
  }
}

void loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastDebugTime = 0;
  
  // 檢查MQTT連接
  if (!client.connected()) {
    Serial.println("\n[MQTT] 連接已斷開，嘗試重新連接...");
    reconnect();
  }
  client.loop();
  
  // 每30秒輸出一次系統狀態
  if (currentMillis - lastDebugTime >= 30000) {
    lastDebugTime = currentMillis;
    Serial.println("\n=== 系統狀態 ===");
    Serial.print("運行時間: ");
    Serial.print(currentMillis / 1000);
    Serial.println(" 秒");
    Serial.print("WiFi狀態: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "已連接" : "未連接");
    Serial.print("MQTT狀態: ");
    Serial.println(client.connected() ? "已連接" : "未連接");
    Serial.print("目前溫度: ");
    Serial.print(temperature);
    Serial.println(" °C");
    Serial.print("目前濕度: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.println("===============\n");
  }
  
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
  
  // 檢查鬧鐘 (每5秒檢查一次)
  if (currentMillis - alarmLastCheckTime >= alarmCheckInterval) {
    alarmLastCheckTime = currentMillis;
    handleAlarm();
  }
  
  // 如果鬧鐘已觸發，持續播放音樂
  if (alarmTriggered) {
    playAlarmMelody();
  }
  
  // 發布MQTT數據 (每5秒發布一次)
  if (currentMillis - lastMqttPublishTime >= mqttPublishInterval) {
    lastMqttPublishTime = currentMillis;
    
    // 發布各種數據
    publishEnvironmentData();
    sendSittingRecordViaAPI();
    publishReminderStatus();
    publishAlarmStatus(); // 發布鬧鐘狀態
  }
  
  // 如果久坐警報啟動，持續執行閃爍
  if (alarmActive && !alarmTriggered) {  // 確保鬧鐘和久坐提醒不衝突
    blinkRedLed();
  }
  
  // 極短暫延遲，避免CPU過度佔用但不影響軟PWM精度
  delayMicroseconds(100);
}