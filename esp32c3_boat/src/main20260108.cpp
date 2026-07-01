// // main.cpp
// #include "main.hpp"

// Ticker timer_control;

// // WiFi配置
// const char *ssid = "test305";
// const char *password = "DMU305307";

// // UDP配置
// WiFiUDP udp;
// IPAddress serverIP(192, 168, 0, 102); // 服务器IP地址
// unsigned int serverPort = 8000;       // 服务器端口
// unsigned int localPort = 8080;        // 本地端口
// unsigned long lastHeartbeatTime = 0;
// unsigned long lastreconnectTime = 0;
// const unsigned long heartbeatInterval = 2000; // 心跳间隔2秒
// unsigned long currentTime = 0;
// String macLastFourDecimalStr = "";

// // 全局变量
// int throttle = 1000;  // 油门值 (1000-2000us)
// int steering = 1500;  // 舵机转向值 (1000-2000us)
// bool otaMode = false; // OTA模式标志

// // WebServer实例
// WebServer server(80);

// // 美化后的OTA HTML页面，包含进度条和状态显示
// const char *upload_html = R"rawliteral(
// <!DOCTYPE html>
// <html>
// <head>
//   <title>OTA更新</title>
//   <meta charset="UTF-8">
//   <style>
//     body {
//       font-family: Arial, sans-serif;
//       max-width: 600px;
//       margin: 0 auto;
//       padding: 20px;
//       background-color: #f5f5f5;
//     }
//     .container {
//       background-color: white;
//       border-radius: 10px;
//       padding: 30px;
//       box-shadow: 0 4px 8px rgba(0,0,0,0.1);
//     }
//     h1 {
//       color: #333;
//       text-align: center;
//       margin-bottom: 30px;
//     }
//     .upload-form {
//       text-align: center;
//       margin: 30px 0;
//     }
//     input[type="file"] {
//       margin-bottom: 20px;
//       padding: 10px;
//       border: 1px solid #ddd;
//       border-radius: 5px;
//     }
//     input[type="submit"] {
//       background-color: #4CAF50;
//       color: white;
//       padding: 12px 24px;
//       border: none;
//       border-radius: 5px;
//       cursor: pointer;
//       font-size: 16px;
//     }
//     input[type="submit"]:hover {
//       background-color: #45a049;
//     }
//     .progress-container {
//       display: none;
//       margin-top: 20px;
//     }
//     .progress-bar {
//       width: 100%;
//       height: 25px;
//       background-color: #f0f0f0;
//       border-radius: 5px;
//       overflow: hidden;
//     }
//     .progress-bar-inner {
//       height: 100%;
//       background-color: #4CAF50;
//       width: 0%;
//       transition: width 0.3s ease;
//     }
//     .status {
//       margin-top: 15px;
//       padding: 10px;
//       border-radius: 5px;
//       text-align: center;
//       display: none;
//     }
//     .status.uploading {
//       display: block;
//       background-color: #e3f2fd;
//       color: #1976d2;
//     }
//     .status.success {
//       display: block;
//       background-color: #e8f5e9;
//       color: #388e3c;
//     }
//     .status.error {
//       display: block;
//       background-color: #ffebee;
//       color: #d32f2f;
//     }
//     .reboot-message {
//       display: none;
//       margin-top: 20px;
//       text-align: center;
//       padding: 15px;
//       background-color: #e8f5e9;
//       border-radius: 5px;
//       color: #388e3c;
//     }
//     .button-container {
//       text-align: center;
//       margin-top: 20px;
//     }
//     .home-button {
//       background-color: #2196F3;
//       color: white;
//       padding: 10px 20px;
//       border: none;
//       border-radius: 5px;
//       cursor: pointer;
//       text-decoration: none;
//       display: inline-block;
//     }
//     .home-button:hover {
//       background-color: #0b7dda;
//     }
//   </style>
// </head>
// <body>
//   <div class="container">
//     <h1>鹏鹏的多船编队控制下位机 OTA Update</h1>
//     <form class="upload-form" method='POST' action='/update' enctype='multipart/form-data' id='uploadForm'>
//       <input type='file' name='update' id='fileInput' required>
//       <br>
//       <input type='submit' value='更新固件'>
//     </form>
    
//     <div class="progress-container" id="progressContainer">
//       <div class="progress-bar">
//         <div class="progress-bar-inner" id="progressBar"></div>
//       </div>
//       <div id="progressText">0%</div>
//     </div>
    
//     <div class="status" id="status"></div>
    
//     <div class="reboot-message" id="rebootMessage">
//       <h3>更新成功！设备重启中...</h3>
//       <p>设备将自动重启，请稍候...</p>
//     </div>
    
//     <div class="button-container">
//       <a href="/" class="home-button">返回首页</a>
//     </div>
//   </div>

//   <script>
//     document.getElementById('uploadForm').addEventListener('submit', function(e) {
//       e.preventDefault();
      
//       const fileInput = document.getElementById('fileInput');
//       const file = fileInput.files[0];
//       if (!file) {
//         alert('请先选择一个文件!');
//         return;
//       }
      
//       const formData = new FormData();
//       formData.append('update', file);
      
//       // 显示进度元素
//       document.getElementById('progressContainer').style.display = 'block';
//       document.getElementById('status').className = 'status uploading';
//       document.getElementById('status').textContent = '正在上传...';
//       document.getElementById('status').style.display = 'block';
      
//       // 创建XMLHttpRequest来上传文件
//       const xhr = new XMLHttpRequest();
      
//       // 更新进度
//       xhr.upload.addEventListener('progress', function(e) {
//         if (e.lengthComputable) {
//           const percent = Math.round((e.loaded / e.total) * 100);
//           document.getElementById('progressBar').style.width = percent + '%';
//           document.getElementById('progressText').textContent = percent + '%';
//         }
//       });
      
//       // 处理完成事件
//       xhr.addEventListener('load', function() {
//         const response = xhr.responseText;
//         if (xhr.status === 200) {
//           // 更新成功
//           document.getElementById('status').className = 'status success';
//           document.getElementById('status').textContent = '更新完成!';
          
//           // 显示重启消息
//           setTimeout(function() {
//             document.getElementById('rebootMessage').style.display = 'block';
            
//             // 一段时间后可以重新加载页面或重定向
//             setTimeout(function() {
//               // 可选：重启后重定向或重新加载页面
//               // window.location.href = "/";
//             }, 5000);
//           }, 1000);
//         } else {
//           // 更新失败
//           document.getElementById('status').className = 'status error';
//           document.getElementById('status').textContent = '更新失败! 状态码: ' + xhr.status;
//         }
//       });
      
//       // 处理错误
//       xhr.addEventListener('error', function() {
//         document.getElementById('status').className = 'status error';
//         document.getElementById('status').textContent = '上传错误!';
//       });
      
//       // 发送请求
//       xhr.open('POST', '/update');
//       xhr.send(formData);
//     });
//   </script>
// </body>
// </html>
// )rawliteral";

// // 将PWM微秒值转换为LED duty cycle值
// int convertPWMToDutyCycle(int pwmValue)
// {
//   return (pwmValue * 4095) / 20000;
// }

// void Set_Pwm(int throttle_pwm, int steering_pwm)
// {
//   int esc_duty = convertPWMToDutyCycle(throttle_pwm);
//   ledcWrite(ESC_CHANNEL, esc_duty);

//   int servo_duty = convertPWMToDutyCycle(steering_pwm);
//   ledcWrite(SERVO_CHANNEL, servo_duty);
// }

// void resetControls()
// {
//   throttle = 1000;
//   steering = 1500;
//   Set_Pwm(throttle, steering);
//   Serial.println("已重置控制参数 - 电机停止，舵机居中");
// }

// void restartSystem()
// {
//   Serial.println("系统重启中...");
//   delay(100);
//   ESP.restart();
// }

// // 启动OTA功能
// void startOTA()
// {
//   if (otaMode)
//     return;

//   otaMode = true;

//   // 配置HTTP服务器用于网页上传
//   server.on("/", HTTP_GET, []()
//             { server.send(200, "text/html", upload_html); });

//   server.on("/update", HTTP_POST, []()
//             {
//         server.send(200, "text/html", upload_html); // 直接返回同一个页面
//         delay(1000);
//         restartSystem(); }, []()
//             {
//         HTTPUpload& upload = server.upload();
//         if (upload.status == UPLOAD_FILE_START) {
//             Serial.printf("HTTP更新: %s\n", upload.filename.c_str());
//             if (!Update.begin()) {
//                 Update.printError(Serial);
//             }
//         } else if (upload.status == UPLOAD_FILE_WRITE) {
//             if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
//                 Update.printError(Serial);
//             }
//         } else if (upload.status == UPLOAD_FILE_END) {
//             if (Update.end(true)) {
//                 Serial.println("HTTP更新完成");
//             } else {
//                 Update.printError(Serial);
//             }
//         } });

//   server.begin();
//   Serial.println("HTTP OTA服务器已启动");

//   // 发送OTA准备完成消息给上位机
//   String otaInfo = "OTA_READY:" + WiFi.localIP().toString() + "\r\n" + "上传固件项目的.pio/build/<board_name>/firmware.bin";
//   sendUdpMessage(otaInfo);
// }

// void commend_parse(String incomingPacket)
// {
//   incomingPacket.trim();

//   if (incomingPacket.equalsIgnoreCase("reset"))
//   {
//     resetControls();
//     return;
//   }

//   if (incomingPacket.equalsIgnoreCase("restart"))
//   {
//     restartSystem();
//     return;
//   }

//   if (incomingPacket.equalsIgnoreCase("OTA"))
//   {
//     startOTA();
//     return;
//   }

//   int commaIndex = incomingPacket.indexOf(',');
//   if (commaIndex > 0)
//   {
//     throttle = incomingPacket.substring(0, commaIndex).toInt();
//     steering = incomingPacket.substring(commaIndex + 1).toInt();

//     throttle = constrain(throttle, 1000, 2000);
//     steering = constrain(steering, 1000, 2000);

//     Serial.println("解析到控制指令 - 油门: " + String(throttle) + ", 舵机: " + String(steering));
//     // sendUdpMessage(incomingPacket);
//   }
//   else
//   {
//     Serial.println("错误：无效的指令格式");
//   }
// }

// void control()
// {
//   if (!otaMode)
//   {
//     Set_Pwm(throttle, steering);

//     currentTime = millis();
//     // if (currentTime - lastreconnectTime >= 30 * heartbeatInterval)
//     // {
//     //   sendUdpMessage(macLastFourDecimalStr);
//     //   lastreconnectTime = currentTime;
//     // }

//     // else
//     // {
//       if (currentTime - lastHeartbeatTime >= heartbeatInterval)
//       {
//         sendUdpMessage("1");
//         lastHeartbeatTime = currentTime;
//       }
//     // }
//   }
// }

// void calibrateESC()
// {
//   Serial.println("开始电调校准...");
//   ledcWrite(ESC_CHANNEL, convertPWMToDutyCycle(1000));
//   delay(1000);
//   ledcWrite(ESC_CHANNEL, convertPWMToDutyCycle(1300));
//   delay(2000);
//   ledcWrite(ESC_CHANNEL, convertPWMToDutyCycle(1000));
//   delay(1000);

//   ledcWrite(SERVO_CHANNEL, convertPWMToDutyCycle(1500));
//   delay(1000);
//   ledcWrite(SERVO_CHANNEL, convertPWMToDutyCycle(1500));
//   delay(1000);

//   Serial.println("电调校准完成");
// }

// bool sendUdpMessage(String message)
// {
//     message += "\n";
//     if(udp.beginPacket(serverIP, serverPort)) {
//         udp.print(message);
//         bool result = udp.endPacket();
//         Serial.print("发送消息: ");
//         Serial.print(message);
//         if(result) {
//             Serial.println(" [成功]");
//         } else {
//             Serial.println(" [失败]");
//         }
//         return result;
//     } else {
//         Serial.print("发送消息: ");
//         Serial.print(message);
//         Serial.println(" [失败 - 无法开始数据包]");
//         return false;
//     }
// }

// void readUdpData(void *parameter)
// {
//   Serial.println("UDP数据读取任务已启动");

//   while (true)
//   {
//     int packetSize = udp.parsePacket();
//     if (packetSize)
//     {
//       char incomingPacket[255];
//       int len = udp.read(incomingPacket, 255);
//       if (len > 0)
//       {
//         incomingPacket[len] = '\0';
//         Serial.print("接收到UDP数据: ");
//         Serial.println(incomingPacket);
//         commend_parse(incomingPacket);
//       }
//     }
//     vTaskDelay(100 / portTICK_PERIOD_MS);
//   }
// }

// void setup()
// {
//   Serial.begin(115200);
//   Serial.println("ESP32 UDP 无刷电机+舵机控制启动中...");

//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);

//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("");
//   Serial.println("WiFi连接成功！");
//   Serial.print("IP地址: ");
//   Serial.println(WiFi.localIP());
//   udp.begin(localPort);
//   Serial.println("UDP客户端已启动");
//   uint8_t mac[6];
//   WiFi.macAddress(mac);
//   char macStr[18];
//   sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
//   Serial.print("MAC地址: ");
//   Serial.println(macStr);

//   int macLastFourDecimal = (mac[4] << 8) | mac[5];
//   if (macLastFourDecimal > 9999)
//     macLastFourDecimal %= 10000;

//   macLastFourDecimalStr = String(macLastFourDecimal);
//   Serial.print("MAC地址的后四位（十进制）: ");
//   Serial.println(macLastFourDecimal);
//   sendUdpMessage(macLastFourDecimalStr);
//   ledcSetup(ESC_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
//   ledcAttachPin(ESC_PWM_PIN, ESC_CHANNEL);

//   ledcSetup(SERVO_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
//   ledcAttachPin(SERVO_PWM_PIN, SERVO_CHANNEL);

//   calibrateESC();

//   timer_control.attach_ms(interrupt_time_control, control);
//   xTaskCreatePinnedToCore(readUdpData, "ReadUdpData", 4096, NULL, 1, NULL, 1);
//   // sendUdpMessage(macLastFourDecimalStr);
//   delay(1000);
//   sendUdpMessage(macLastFourDecimalStr);
// }

// void loop()
// {
//   // 处理HTTP服务器请求
//   if (otaMode)
//   {
//     server.handleClient();
//   }

//   // 检查串口是否有数据传入
//   if (Serial.available() > 0)
//   {
//     String receivedData = Serial.readStringUntil('\n');
//     receivedData.trim();

//     if (receivedData.equalsIgnoreCase("reset"))
//     {
//       resetControls();
//     }
//     else if (receivedData.equalsIgnoreCase("restart"))
//     {
//       restartSystem();
//     }
//     else if (receivedData.equalsIgnoreCase("OTA"))
//     {
//       startOTA();
//     }
//     else
//     {
//       sendUdpMessage(receivedData);
//     }
//   }
// }