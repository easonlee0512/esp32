# 智慧桌燈系統

基於ESP32和Node-RED的智慧桌燈系統，透過MQTT協議實現連接。

## 環境安裝步驟

### 1. 安裝Homebrew（MacOS）

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

安裝完成後，按照提示設定環境變數：

```bash
echo >> ~/.zprofile
echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
eval "$(/opt/homebrew/bin/brew shellenv)"
```

### 2. 安裝Node.js和Mosquitto MQTT伺服器

```bash
brew install node mosquitto
```

### 3. 安裝Node-RED

```bash
npm install -g --unsafe-perm node-red
```

### 4. 安裝專案依賴

```bash
# 在專案目錄中執行
cd /Users/eason/esp32/smart-desk-lamp
npm install
```

### 5. ESP32環境設定

在Arduino IDE中安裝以下庫：
- ESP32開發板
- WiFi庫
- PubSubClient（MQTT庫）
- DHT感測器庫
- LedControl庫
- Ultrasonic庫

## 啟動步驟

### 1. 啟動MQTT伺服器

```bash
# 使用以下命令開啟MQTT伺服器
mosquitto -c mosquitto.conf

# 或使用brew服務啟動
brew services start mosquitto
```

### 2. 啟動Node-RED

```bash
# 在專案目錄中執行
cd /Users/eason/esp32/smart-desk-lamp
npm start
```

然後在瀏覽器中訪問：http://localhost:1880

### 3. 上傳代碼到ESP32

1. 在Arduino IDE中打開 `Arduino_DHT/Arduino_DHT.ino`
2. 修改WiFi設定（SSID和密碼）
3. 修改MQTT伺服器設定（IP地址和埠號）
4. 上傳代碼到ESP32

### 4. 監控MQTT訊息

```bash
# 訂閱所有智慧桌燈主題
mosquitto_sub -h <您的IP地址> -t "smartlamp/#" -v
```

## MQTT主題結構

- `smartlamp/light` - 燈光控制和亮度資料
- `smartlamp/environment` - 環境數據（溫濕度、光敏）
- `smartlamp/timer` - 計時器狀態
- `smartlamp/alarm` - 鬧鐘設定
- `smartlamp/reminder` - 久坐提醒狀態

## MQTT消息示例

以下是系統運行時的MQTT消息示例：

```
smartlamp/light {"brightness":255}
smartlamp/environment {"temperature":29.00,"humidity":90.00,"light":128}
smartlamp/reminder {"sitting":true,"alarm":false,"duration":134}
smartlamp/test 測試訊息
smartlamp/environment {"temperature":27.00,"humidity":87.00,"light":159}
smartlamp/reminder {"sitting":true,"alarm":false,"duration":139}
smartlamp/environment {"temperature":27.00,"humidity":84.00,"light":139}
smartlamp/reminder {"sitting":true,"alarm":false,"duration":144}
smartlamp/environment {"temperature":27.00,"humidity":84.00,"light":141}
smartlamp/reminder {"sitting":true,"alarm":false,"duration":149}
smartlamp/environment {"temperature":27.00,"humidity":87.00,"light":163}
smartlamp/reminder {"sitting":true,"alarm":false,"duration":154}
```

這些消息可以通過以下命令監聽：
```bash
mosquitto_sub -h localhost -t "smartlamp/#" -v
```

## 故障排除

1. 如果MQTT連接失敗，請確保：
   - ESP32和電腦在同一網路中
   - MQTT伺服器設定允許外部連接
   - 防火牆沒有阻擋1883埠

2. 如果Node-RED無法啟動，請檢查：
   - node-red-data目錄是否存在
   - 1880埠是否被其他程式佔用
