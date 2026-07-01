// main.cpp
#include "main.hpp"
#define version "2026.6.4"
// ========== 性能调优参数 ==========
#define CHUNK_SIZE 1400          // 增大UDP包长，减少包数量 (MTU上限1500)
#define PACKET_DELAY_US 0        // 包间延迟0微秒，最大限度提速
#define FRAME_DELAY_MS 50   
// 定义摄像头引脚配置（根据ESP32-S3-USB-CAM模块调整）
#define PWDN_GPIO_NUM     (-1)
#define RESET_GPIO_NUM    (-1)
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15

#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

Ticker timer_control;

// WiFi配置
// const char *ssid = "test305";
// const char *password = "DMU305307";
// const char *ssid = "LSH_24G";
// const char *password = "LSH_2025";
const char *ssid = "ISSEC";
const char *password = "issec8888";

// UDP配置
WiFiUDP udp;
// IPAddress serverIP(192, 168, 0, 102); // 服务器IP地址
// IPAddress serverIP(192, 168, 170, 144);   // 电脑服务器IP地址
// IPAddress piServerIP(192, 168, 170, 191); // 树莓派服务器IP地址
IPAddress rkserverIP(192, 168, 6, 10);   // rk电脑服务器IP地址
IPAddress serverIP(192, 168, 6, 11); // 刘鹏服务器IP地址
IPAddress dfgserverIP(192, 168, 6, 12);   // 段富高电脑服务器IP地址
IPAddress piServerIP(192, 168, 6, 13); // 飞腾派服务器IP地址
unsigned int serverPort = 7020;       // 电脑服务器端port8000
unsigned int piServerPort = 7020;     // 树莓派服务器端port8000
unsigned int localPort = 7021;        // 本地端port8080
unsigned int cameraPort = 7022;       // 相机传输端port8888
unsigned long lastHeartbeatTime = 0;
unsigned long lastreconnectTime = 0;
const unsigned long heartbeatInterval = 2000; // 心跳间隔2秒
unsigned long currentTime = 0;
String macLastFourDecimalStr = "";

// // 相机配置
// bool cameraEnabled = false; // 相机是否启用
bool low_power = false;
// TaskHandle_t cameraTaskHandle = NULL; // 相机任务句柄

// 单桨模式
int throttle = 1000; // 油门值 (1000-2000us)
int steering = 1500; // 舵机转向值 (1000-2000us)
// 双桨模式
int pwm_left = 1500;
int pwm_right = 1500;
int led_left_state = 0;
int led_right_state = 0;
bool manual_led_mode = false;

bool otaMode = false; // OTA模式标志

// WebServer实例
WebServer server(80);

// 美化后的OTA HTML页面，包含进度条和状态显示
const char *upload_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>OTA更新</title>
  <meta charset="UTF-8">
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 600px;
      margin: 0 auto;
      padding: 20px;
      background-color: #f5f5f5;
    }
    .container {
      background-color: white;
      border-radius: 10px;
      padding: 30px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      text-align: center;
      margin-bottom: 30px;
    }
    .upload-form {
      text-align: center;
      margin: 30px 0;
    }
    input[type="file"] {
      margin-bottom: 20px;
      padding: 10px;
      border: 1px solid #ddd;
      border-radius: 5px;
    }
    input[type="submit"] {
      background-color: #4CAF50;
      color: white;
      padding: 12px 24px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-size: 16px;
    }
    input[type="submit"]:hover {
      background-color: #45a049;
    }
    .progress-container {
      display: none;
      margin-top: 20px;
    }
    .progress-bar {
      width: 100%;
      height: 25px;
      background-color: #f0f0f0;
      border-radius: 5px;
      overflow: hidden;
    }
    .progress-bar-inner {
      height: 100%;
      background-color: #4CAF50;
      width: 0%;
      transition: width 0.3s ease;
    }
    .status {
      margin-top: 15px;
      padding: 10px;
      border-radius: 5px;
      text-align: center;
      display: none;
    }
    .status.uploading {
      display: block;
      background-color: #e3f2fd;
      color: #1976d2;
    }
    .status.success {
      display: block;
      background-color: #e8f5e9;
      color: #388e3c;
    }
    .status.error {
      display: block;
      background-color: #ffebee;
      color: #d32f2f;
    }
    .reboot-message {
      display: none;
      margin-top: 20px;
      text-align: center;
      padding: 15px;
      background-color: #e8f5e9;
      border-radius: 5px;
      color: #388e3c;
    }
    .button-container {
      text-align: center;
      margin-top: 20px;
    }
    .home-button {
      background-color: #2196F3;
      color: white;
      padding: 10px 20px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      text-decoration: none;
      display: inline-block;
    }
    .home-button:hover {
      background-color: #0b7dda;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>鹏鹏的EPS32C3多船编队控制下位机 OTA Update</h1>
    <form class="upload-form" method='POST' action='/update' enctype='multipart/form-data' id='uploadForm'>
      <input type='file' name='update' id='fileInput' required>
      <br>
      <input type='submit' value='更新固件'>
    </form>
    
    <div class="progress-container" id="progressContainer">
      <div class="progress-bar">
        <div class="progress-bar-inner" id="progressBar"></div>
      </div>
      <div id="progressText">0%</div>
    </div>
    
    <div class="status" id="status"></div>
    
    <div class="reboot-message" id="rebootMessage">
      <h3>更新成功！设备重启中...</h3>
      <p>设备将自动重启，请稍候...</p>
    </div>
    
    <div class="button-container">
      <a href="/" class="home-button">返回首页</a>
    </div>
  </div>

  <script>
    document.getElementById('uploadForm').addEventListener('submit', function(e) {
      e.preventDefault();
      
      const fileInput = document.getElementById('fileInput');
      const file = fileInput.files[0];
      if (!file) {
        alert('请先选择一个文件!');
        return;
      }
      
      const formData = new FormData();
      formData.append('update', file);
      
      // 显示进度元素
      document.getElementById('progressContainer').style.display = 'block';
      document.getElementById('status').className = 'status uploading';
      document.getElementById('status').textContent = '正在上传...';
      document.getElementById('status').style.display = 'block';
      
      // 创建XMLHttpRequest来上传文件
      const xhr = new XMLHttpRequest();
      
      // 更新进度
      xhr.upload.addEventListener('progress', function(e) {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          document.getElementById('progressBar').style.width = percent + '%';
          document.getElementById('progressText').textContent = percent + '%';
        }
      });
      
      // 处理完成事件
      xhr.addEventListener('load', function() {
        const response = xhr.responseText;
        if (xhr.status === 200) {
          // 更新成功
          document.getElementById('status').className = 'status success';
          document.getElementById('status').textContent = '更新完成!';
          
          // 显示重启消息
          setTimeout(function() {
            document.getElementById('rebootMessage').style.display = 'block';
            
            // 一段时间后可以重新加载页面或重定向
            setTimeout(function() {
              // 可选：重启后重定向或重新加载页面
              // window.location.href = "/";
            }, 5000);
          }, 1000);
        } else {
          // 更新失败
          document.getElementById('status').className = 'status error';
          document.getElementById('status').textContent = '更新失败! 状态码: ' + xhr.status;
        }
      });
      
      // 处理错误
      xhr.addEventListener('error', function() {
        document.getElementById('status').className = 'status error';
        document.getElementById('status').textContent = '上传错误!';
      });
      
      // 发送请求
      xhr.open('POST', '/update');
      xhr.send(formData);
    });
  </script>
</body>
</html>
)rawliteral";

// 将PWM微秒值转换为LED duty cycle值1000~2000
// 20000 代表 PWM 周期时间（单位：微秒）
// 最大值为 2^12 - 1 = 4095 代表 LEDC 通道的 duty cycle 分辨率
int convertPWM_single(int pwmValue)
{
  return (pwmValue * 4095) / 20000;
}
// 将PWM微秒值转换为LED duty cycle值1000~2000
// 20000 代表 PWM 周期时间（单位：微秒）
// 最大值为 2^12 - 1 = 4095 代表 LEDC 通道的 duty cycle 分辨率
int convertPWM_double(int pwmValue)
{
  return (pwmValue * 4095) / 20000;
}
void Set_Single_paddle_Pwm(int throttle_pwm, int steering_pwm)
{

  int esc_duty = convertPWM_single(throttle_pwm);
  ledcWrite(ESC_CHANNEL, esc_duty);

  int servo_duty = convertPWM_single(steering_pwm);
  ledcWrite(SERVO_CHANNEL, servo_duty);
}
void Set_Double_paddle_Pwm(int pwm_left, int pwm_right)
{
  pwm_left = -(pwm_left - 1500) + 1500;
  int PWM_L_duty = convertPWM_double(pwm_left);
  ledcWrite(PWM_L_CHANNEL, PWM_L_duty);

  int PWM_R_duty = convertPWM_double(pwm_right);
  ledcWrite(PWM_R_CHANNEL, PWM_R_duty);
}

void setTurnLEDState(int left, int right)
{
  led_left_state = left ? 1 : 0;
  led_right_state = right ? 1 : 0;

  digitalWrite(LED_LEFT_PIN, led_left_state ? HIGH : LOW);
  digitalWrite(LED_RIGHT_PIN, led_right_state ? HIGH : LOW);
}

void updateTurnLEDs()
{
  if (USV == Single_paddle)
  {
    if (steering < 1500 - TURN_DEADBAND)
    {
      setTurnLEDState(1, 0);
    }
    else if (steering > 1500 + TURN_DEADBAND)
    {
      setTurnLEDState(0, 1);
    }
    else
    {
      setTurnLEDState(0, 0);
    }
  }
  else if (USV == double_paddle)
  {
    int diff = pwm_left - pwm_right;

    if (diff < -TURN_DEADBAND)
    {
      setTurnLEDState(1, 0);
    }
    else if (diff > TURN_DEADBAND)
    {
      setTurnLEDState(0, 1);
    }
    else
    {
      setTurnLEDState(0, 0);
    }
  }
}

bool handleLedCommand(String incomingPacket)
{
  if (incomingPacket.equalsIgnoreCase("LED_LEFT"))
  {
    manual_led_mode = true;
    setTurnLEDState(1, 0);
    Serial.println("LED test command: left");
    return true;
  }

  if (incomingPacket.equalsIgnoreCase("LED_RIGHT"))
  {
    manual_led_mode = true;
    setTurnLEDState(0, 1);
    Serial.println("LED test command: right");
    return true;
  }

  if (incomingPacket.equalsIgnoreCase("LED_OFF"))
  {
    manual_led_mode = true;
    setTurnLEDState(0, 0);
    Serial.println("LED test command: off");
    return true;
  }

  if (incomingPacket.equalsIgnoreCase("LED_BOTH"))
  {
    manual_led_mode = true;
    setTurnLEDState(1, 1);
    Serial.println("LED test command: both");
    return true;
  }

  return false;
}

void resetControls()
{
  if (USV == Single_paddle)
  {
    throttle = 1000;
    steering = 1500;
    Set_Single_paddle_Pwm(throttle, steering);
    if (!manual_led_mode)
    {
      updateTurnLEDs();
    }
    Serial.println("已重置控制参数 - 电机停止，舵机居中");
  }
  else if (USV == double_paddle)
  {
    pwm_left = 1500;
    pwm_right = 1500;
    Set_Double_paddle_Pwm(pwm_left, pwm_right);
    if (!manual_led_mode)
    {
      updateTurnLEDs();
    }
    Serial.println("已重置控制参数 - 电机停止");
  }
}

void restartSystem()
{
  Serial.println("系统重启中...");
  delay(100);
  ESP.restart();
}

// // 初始化摄像头
// bool initCamera()
// {
//   camera_config_t config;
//   config.ledc_channel = LEDC_CHANNEL_0;
//   config.ledc_timer = LEDC_TIMER_0;
//   config.pin_d0 = Y2_GPIO_NUM;
//   config.pin_d1 = Y3_GPIO_NUM;
//   config.pin_d2 = Y4_GPIO_NUM;
//   config.pin_d3 = Y5_GPIO_NUM;
//   config.pin_d4 = Y6_GPIO_NUM;
//   config.pin_d5 = Y7_GPIO_NUM;
//   config.pin_d6 = Y8_GPIO_NUM;
//   config.pin_d7 = Y9_GPIO_NUM;
//   config.pin_xclk = XCLK_GPIO_NUM;
//   config.pin_pclk = PCLK_GPIO_NUM;
//   config.pin_vsync = VSYNC_GPIO_NUM;
//   config.pin_href = HREF_GPIO_NUM;
//   config.pin_sscb_sda = SIOD_GPIO_NUM;
//   config.pin_sscb_scl = SIOC_GPIO_NUM;
//   config.pin_pwdn = PWDN_GPIO_NUM;
//   config.pin_reset = RESET_GPIO_NUM;
//   config.xclk_freq_hz = 20000000;
//   config.pixel_format = PIXFORMAT_JPEG;
//   config.frame_size = FRAMESIZE_UXGA; // UXGA 1600x1200
//   config.jpeg_quality = 12;
//   config.fb_count = 2;

//   // 初始化摄像头
//   esp_err_t err = esp_camera_init(&config);
//   if (err != ESP_OK) {
//     Serial.printf("摄像头初始化失败: %s\n", esp_err_to_name(err));
//     return false;
//   }

//   sensor_t * s = esp_camera_sensor_get();
//   s->set_framesize(s, FRAMESIZE_QVGA);     // 强制 QVGA
//   s->set_quality(s, 30);                   // 质量30

//   Serial.println("摄像头初始化成功");
//   return true;
// }

// // 关闭摄像头
// void deinitCamera()
// {
//   esp_camera_deinit();
//   Serial.println("摄像头已关闭");
// }


// // 相机传输任务
// void cameraTask(void *pvParameters)
// {
//   Serial.println("相机传输任务开始");
//   while (cameraEnabled) {
//     camera_fb_t *fb = esp_camera_fb_get();
//     if (!fb) {
//       Serial.println("Frame capture failed");
//       delay(10);
//       return;
//     }

//     // 分包发送
//     uint32_t frame_id = micros();  // 微秒级ID，更精确
//     uint16_t total_packets = (fb->len + CHUNK_SIZE - 1) / CHUNK_SIZE;

//     for (uint16_t i = 0; i < total_packets; i++) {
//       uint16_t packet_len = (i == total_packets - 1) ? (fb->len - i * CHUNK_SIZE) : CHUNK_SIZE;

//       udp.beginPacket(serverIP, cameraPort);
//       udp.write((uint8_t*)&frame_id, 4);
//       udp.write((uint8_t*)&total_packets, 2);
//       udp.write((uint8_t*)&i, 2);
//       udp.write((uint8_t*)&packet_len, 2);
//       udp.write(fb->buf + i * CHUNK_SIZE, packet_len);
//       udp.endPacket();

//       // 包间延迟设为0，但如果网络拥堵可以微调为2~5微秒
//       if (PACKET_DELAY_US > 0) delayMicroseconds(PACKET_DELAY_US);
//     }

//     esp_camera_fb_return(fb);

//     // 帧率控制：根据期望的帧率调整 delay
//     delay(FRAME_DELAY_MS);
//   }

//   Serial.println("相机传输任务结束");
//   vTaskDelete(NULL); // 删除自身任务
// }

// // 启动相机传输
// void startCamera()
// {
//   if (cameraEnabled) {
//     Serial.println("相机已经开启");
//     return;
//   }

//   if (!initCamera()) {
//     Serial.println("相机初始化失败");
//     return;
//   }

//   cameraEnabled = true;
//   // 将相机任务分配到核心 0（与主循环不同的核心）
//   xTaskCreatePinnedToCore(cameraTask, "CameraTask", 4096, NULL, 5, &cameraTaskHandle, 0);
//   Serial.println("相机已开启并开始传输（核心 0）");
// }

// // 停止相机传输
// void stopCamera()
// {
//   if (!cameraEnabled) {
//     Serial.println("相机已经关闭");
//     return;
//   }

//   cameraEnabled = false;
//   if (cameraTaskHandle != NULL) {
//     vTaskDelete(cameraTaskHandle);
//     cameraTaskHandle = NULL;
//   }
//   deinitCamera();
//   Serial.println("相机已关闭");
// }

// 启动OTA功能
void startOTA()
{
  if (otaMode)
    return;

  otaMode = true;

  // 配置HTTP服务器用于网页上传
  server.on("/", HTTP_GET, []()
            { server.send(200, "text/html", upload_html); });

  server.on("/update", HTTP_POST, []()
            {
        server.send(200, "text/html", upload_html); // 直接返回同一个页面
        delay(1000);
        restartSystem(); }, []()
            {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("HTTP更新: %s\n", upload.filename.c_str());
            if (!Update.begin()) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                Serial.println("HTTP更新完成");
            } else {
                Update.printError(Serial);
            }
        } });

  server.begin();
  Serial.println("HTTP OTA服务器已启动");

  // 发送OTA准备完成消息给上位机
  String otaInfo = "OTA_READY:" + WiFi.localIP().toString() + "\r\n" + "上传固件项目的.pio/build/<board_name>/firmware.bin";
  buildcommandMessage(otaInfo);
}

void commend_parse(String incomingPacket)
{
  incomingPacket.trim();

  if (handleLedCommand(incomingPacket))
  {
    return;
  }

  if (incomingPacket.equalsIgnoreCase("reset"))
  {
    manual_led_mode = false;
    resetControls();
    return;
  }

  if (incomingPacket.equalsIgnoreCase("restart"))
  {
    restartSystem();
    return;
  }

  if (incomingPacket.equalsIgnoreCase("OTA"))
  {
    startOTA();
    return;
  }

    // 添加相机控制指令
  // if (incomingPacket.equalsIgnoreCase("Camera_Toggle"))
  // {
  //   if (cameraEnabled)
  //   {
  //     stopCamera();
  //     sendUdpMessage("Camera:Closed");
  //   }
  //   else
  //   {
  //     startCamera();
  //     sendUdpMessage("Camera:Opened");
  //   }
  //   return;
  // }

  int commaIndex = incomingPacket.indexOf(',');
  if (commaIndex > 0)
  {
    if (USV == Single_paddle)
    {
      throttle = incomingPacket.substring(0, commaIndex).toInt();
      steering = incomingPacket.substring(commaIndex + 1).toInt();
            if (!low_power)
      {
        throttle = constrain(throttle, 1000, 2000);
        steering = constrain(steering, 1000, 2000);
        Set_Single_paddle_Pwm(throttle, steering);
      }
      manual_led_mode = false;
      Serial.println("解析到控制指令 - 油门: " + String(throttle) + ", 舵机: " + String(steering));
    }
    else if (USV == double_paddle)
    {
      pwm_left = incomingPacket.substring(0, commaIndex).toInt();
      pwm_right = incomingPacket.substring(commaIndex + 1).toInt();
      if (!low_power)
      {
        pwm_left = constrain(pwm_left, 1000, 2000);
        pwm_right = constrain(pwm_right, 1000, 2000);
        Set_Double_paddle_Pwm(pwm_left, pwm_right);
      }
      manual_led_mode = false;
      Serial.println("解析到控制指令 - 左电机: " + String(pwm_left) + ", 右电机: " + String(pwm_right));
    }
  }
  else
  {
    Serial.println("错误：无效的指令格式");
  }
}

void control()
{
  if (!otaMode)
  {
    if (!low_power)
    {
      if (USV == Single_paddle)
      {
        Set_Single_paddle_Pwm(throttle, steering); /* code */
      }
      else if (USV == double_paddle)
      {
        Set_Double_paddle_Pwm(pwm_left, pwm_right);
      }
      if (!manual_led_mode)
      {
        updateTurnLEDs();
      }
    }
    else{
      setTurnLEDState(1,1);
    }

    currentTime = millis();
    if (currentTime - lastreconnectTime >= 100 * heartbeatInterval)
    {
      sendUdpMessage(buildRegisterMessage());
      sendUdpMessage(macLastFourDecimalStr);
      lastreconnectTime = currentTime;
    }
    else
    {
      if (currentTime - lastHeartbeatTime >= heartbeatInterval)
      {
        // 获取ADC0数据
        int adcValue = analogRead(A0);
        float voltage = adcValue * 3.3 / 4095.0 * 3+0.5;
        if (voltage>7.2)
        {
          low_power = false;
        }
        else
        {
          low_power = true;
        }

        // 获取板载温度（使用ESP32内置温度传感器）
        float temperature = temperatureRead();


        // 发送状态上报数据
        sendUdpMessage(buildStatusMessage(adcValue, temperature));
        lastHeartbeatTime = currentTime;
      }
    }
  }
}

void calibrateESC()
{
  Serial.println("开始电调校准...");
  ledcWrite(ESC_CHANNEL, convertPWM_single(1000));
  delay(1000);
  ledcWrite(ESC_CHANNEL, convertPWM_single(1300));
  delay(2000);
  ledcWrite(ESC_CHANNEL, convertPWM_single(1000));
  delay(1000);

  ledcWrite(SERVO_CHANNEL, convertPWM_single(1500));
  delay(1000);
  ledcWrite(SERVO_CHANNEL, convertPWM_single(1500));
  delay(1000);

  Serial.println("单桨校准完成");
}

void calibratePWM()
{
  Serial.println("开始PWM校准...");
  ledcWrite(PWM_L_CHANNEL, convertPWM_double(1500));
  ledcWrite(PWM_R_CHANNEL, convertPWM_double(1500));
  delay(1000);
  // ledcWrite(PWM_L_CHANNEL, convertPWM_double(1100));
  // ledcWrite(PWM_R_CHANNEL, convertPWM_double(1100));
  // delay(2000);
  // ledcWrite(PWM_L_CHANNEL, convertPWM_double(1900));
  // ledcWrite(PWM_R_CHANNEL, convertPWM_double(1900));
  // delay(2000);
  // ledcWrite(PWM_L_CHANNEL, convertPWM_double(1500));
  // ledcWrite(PWM_R_CHANNEL, convertPWM_double(1500));
  // delay(1000);

  Serial.println("双桨校准完成");
}

String buildRegisterMessage()
{
  return "{\"type\":0,\"content\":{\"id\":\"" + macLastFourDecimalStr + "\"}}";
}

String buildStatusMessage(int adcValue, float temperature)
{
  String jsonData = "{\"type\":1,\"content\":{";
  jsonData += "\"adc0\":\"" + String(adcValue);
  jsonData += "\",\"temperature\":\"" + String(temperature, 1);
  jsonData += "\",\"led_left\":\"" + String(led_left_state);
  jsonData += "\",\"led_right\":\"" + String(led_right_state);
  jsonData += "\",\"id\":\"" + macLastFourDecimalStr;
  jsonData += "\",\"pwml\":\"" + String(pwm_left);
  jsonData += "\",\"pwmr\":\"" + String(pwm_right);
  // jsonData += "\",\"camera\":\"" + String(cameraEnabled);
  jsonData += "\",\"version\":\"" + String(version);
  jsonData += "\"}}";
  return jsonData;
}

String buildHeartbeatMessage()
{
  String jsonData = "{\"type\":2,\"content\":{";
  jsonData += "\"timestamp\":\"";
  jsonData += String(millis());
  jsonData += "\"}}";
  return jsonData;
}

String buildcommandMessage(String message)
{
  String jsonData = "{\"type\":3,\"content\":{";
  jsonData += "\"command\":\"";
  jsonData += message;
  jsonData += "\"}}";
  return jsonData;
}
bool sendUdpMessageTo(IPAddress ip, unsigned int port, String message)
{
  if (udp.beginPacket(ip, port))
  {
    udp.print(message);
    bool result = udp.endPacket();
    Serial.print("发送消息到 ");
    Serial.print(ip);
    Serial.print(":");
    Serial.print(port);
    Serial.print(" ");
    Serial.print(message);
    if (result)
    {
      Serial.println(" [成功]");
    }
    else
    {
      Serial.println(" [失败]");
    }
    return result;
  }
  else
  {
    Serial.print("发送消息到 ");
    Serial.print(ip);
    Serial.print(":");
    Serial.print(port);
    Serial.print(" ");
    Serial.print(message);
    Serial.println(" [失败 - 无法开始数据包]");
    return false;
  }
}

bool sendUdpMessage(String message)
{
message += "\n";
  bool pcResult = sendUdpMessageTo(serverIP, serverPort, message);
  bool piResult = sendUdpMessageTo(piServerIP, piServerPort, message);
  bool dfgResult = sendUdpMessageTo(dfgserverIP, serverPort, message);
  bool rkResult = sendUdpMessageTo(rkserverIP, piServerPort, message);
  return pcResult || piResult || dfgResult || rkResult;
}

void readUdpData(void *parameter)
{
  Serial.println("UDP数据读取任务已启动");

  while (true)
  {
    int packetSize = udp.parsePacket();
    if (packetSize)
    {
      char incomingPacket[255];
      int len = udp.read(incomingPacket, 255);
      if (len > 0)
      {
        incomingPacket[len] = '\0';
        Serial.print("接收到UDP数据: ");
        Serial.println(incomingPacket);
        commend_parse(incomingPacket);
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
  
  Serial.begin(115200);
  Serial.println("ESP32C3 UDP 无刷电机+灯控制启动中...");
  pinMode(LED_pin, OUTPUT);
  pinMode(LED_LEFT_PIN, OUTPUT);
  pinMode(LED_RIGHT_PIN, OUTPUT);
  digitalWrite(LED_LEFT_PIN, LOW);
  digitalWrite(LED_RIGHT_PIN, LOW);
  // digitalWrite(LED_pin, HIGH);
  analogWrite(LED_pin, 255);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi连接成功！");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
  udp.begin(localPort);
  Serial.println("UDP客户端已启动");
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("MAC地址: ");
  Serial.println(macStr);

  int macLastFourDecimal = (mac[4] << 8) | mac[5];
  if (macLastFourDecimal > 9999)
    macLastFourDecimal %= 10000;

  macLastFourDecimalStr = String(macLastFourDecimal);
  Serial.print("MAC地址的后四位（十进制）: ");
  Serial.println(macLastFourDecimal);
  sendUdpMessage(buildRegisterMessage());
  // sendUdpMessage(macLastFourDecimalStr);
  if (USV == Single_paddle)
  {
    ledcSetup(ESC_CHANNEL, PWM_FREQs, PWM_RESOLUTIONs);
    ledcAttachPin(ESC_PWM_PIN, ESC_CHANNEL);
    ledcSetup(SERVO_CHANNEL, PWM_FREQs, PWM_RESOLUTIONs);
    ledcAttachPin(SERVO_PWM_PIN, SERVO_CHANNEL);

    calibrateESC();
    Serial.println("单桨校准完成");
  }
  else if (USV == double_paddle)
  {
    ledcSetup(PWM_L_CHANNEL, PWM1_FREQs, PWM1_RESOLUTIONs);
    ledcAttachPin(ESC_PWM_L, PWM_L_CHANNEL);
    ledcSetup(PWM_R_CHANNEL, PWM1_FREQs, PWM1_RESOLUTIONs);
    ledcAttachPin(ESC_PWM_R, PWM_R_CHANNEL);

    calibratePWM();
    Serial.println("双桨校准完成");
  }

  timer_control.attach_ms(interrupt_time_control, control);
  // 将UDP读取任务分配到核心 1
  xTaskCreatePinnedToCore(readUdpData, "ReadUdpData", 4096, NULL, 1, NULL, 1);
  // sendUdpMessage(macLastFourDecimalStr);
  delay(2000);
  sendUdpMessage(buildRegisterMessage());
  // sendUdpMessage(macLastFourDecimalStr);
  // digitalWrite(LED_pin, LOW);
  analogWrite(LED_pin, 150);
}

void loop()
{
  // 处理HTTP服务器请求
  if (otaMode)
  {
    server.handleClient();
  }

  // 检查串口是否有数据传入
  if (Serial.available() > 0)
  {
    String receivedData = Serial.readStringUntil('\n');
    commend_parse(receivedData);
  }
}
