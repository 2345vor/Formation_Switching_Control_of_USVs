// #include "main.hpp"

// Ticker timer_control;

// // WiFi配置
// const char *ssid = "test305";
// const char *password = "DMU305307";

// // UDP配置
// WiFiUDP udp;
// IPAddress serverIP(192, 168, 0, 102);         // 服务器的IP地址，需修改为实际地址
// unsigned int serverPort = 8000;               // 服务器端口
// unsigned int localPort = 8080;                // 本地端口
// unsigned long lastHeartbeatTime = 0;          // 记录上次发送心跳的时间
// unsigned long lastreconnectTime = 0;          // 记录上次尝试重新连接的时间
// const unsigned long heartbeatInterval = 2000; // 心跳间隔2秒（2000毫秒）
// unsigned long currentTime = 0;
// String macLastFourDecimalStr = ""; // 用于存储MAC地址的后四位
// // 全局变量
// int throttle = 1000; // 油门值 (1000-2000us)
// int steering = 1500; // 舵机转向值 (1000-2000us)

// // 将PWM微秒值转换为LED duty cycle值
// int convertPWMToDutyCycle(int pwmValue)
// {
//   // 对于12位分辨率，duty cycle = (pwmValue * 4095) / 20000
//   // 因为ESP32的LED控制器期望的是duty cycle值
//   // 简化计算: 50Hz对应20ms周期，即20000us，所以 duty = (pwmValue * 4095) / 20000
//   return (pwmValue * 4095) / 20000;
// }

// void Set_Pwm(int throttle_pwm, int steering_pwm)
// {
//   // 控制无刷电机
//   int esc_duty = convertPWMToDutyCycle(throttle_pwm);
//   ledcWrite(ESC_CHANNEL, esc_duty);

//   // 控制舵机
//   int servo_duty = convertPWMToDutyCycle(steering_pwm);
//   ledcWrite(SERVO_CHANNEL, servo_duty);

//   // Serial.println("设置 - 电机: " + String(throttle_pwm) + "us, 舵机: " + String(steering_pwm) + "us");
// }

// void resetControls()
// {
//   throttle = 1000; // 电机停止
//   steering = 1500; // 舵机居中
//   Set_Pwm(throttle, steering);
//   Serial.println("已重置控制参数 - 电机停止，舵机居中");
// }

// void restartSystem()
// {
//   Serial.println("系统重启中...");
//   delay(100);    // 确保串口消息发送完成
//   ESP.restart(); // 重启ESP32系统
// }

// void commend_parse(String incomingPacket)
// {
//   incomingPacket.trim();

//   // 检查是否为reset指令
//   if (incomingPacket.equalsIgnoreCase("reset"))
//   {
//     resetControls();
//     return;
//   }

//   // 检查是否为restart指令
//   if (incomingPacket.equalsIgnoreCase("restart"))
//   {
//     restartSystem();
//     return;
//   }

//   // 解析控制指令，格式: "throttle,steering"
//   String command(incomingPacket);
//   int commaIndex = command.indexOf(',');

//   if (commaIndex > 0)
//   {
//     throttle = command.substring(0, commaIndex).toInt();
//     steering = command.substring(commaIndex + 1).toInt();

//     // 限制范围
//     throttle = constrain(throttle, 1000, 2000);
//     steering = constrain(steering, 1000, 2000);

//     Serial.println("解析到控制指令 - 油门: " + String(throttle) + ", 舵机: " + String(steering));
//     sendUdpMessage(incomingPacket);
//   }
//   else
//   {
//     Serial.println("错误：无效的指令格式");
//   }
// }

// void control()
// {
//   Set_Pwm(throttle, steering);
//   // 检查是否需要发送心跳包
//   currentTime = millis();
//   if (currentTime - lastHeartbeatTime >= heartbeatInterval)
//   {
//     sendUdpMessage("1");             // 发送心跳包
//     lastHeartbeatTime = currentTime; // 更新上次发送时间
//   }
//   if (currentTime - lastreconnectTime >= 30 * heartbeatInterval)
//   {
//     sendUdpMessage(macLastFourDecimalStr);
//     lastreconnectTime = currentTime;
//   }
// }

// // 电调校准函数
// void calibrateESC()
// {
//   Serial.println("开始电调校准...");
//   // 发送最小油门信号 (1000us)
//   ledcWrite(ESC_CHANNEL, convertPWMToDutyCycle(1000));
//   delay(1000); // 保持2秒

//   // 发送最大油门信号 (1300us)
//   ledcWrite(ESC_CHANNEL, convertPWMToDutyCycle(1300));
//   delay(2000); // 保持2秒

//   // 发送最小油门信号，完成校准
//   ledcWrite(ESC_CHANNEL, convertPWMToDutyCycle(1000));
//   delay(1000);

//   ledcWrite(SERVO_CHANNEL, convertPWMToDutyCycle(1500));
//   delay(1000);
//   ledcWrite(SERVO_CHANNEL, convertPWMToDutyCycle(1500));
//   delay(1000);

//   Serial.println("电调校准完成");
// }

// // 发送UDP消息
// void sendUdpMessage(String message)
// {
//   message = message + String("\n");
//   udp.beginPacket(serverIP, serverPort);
//   udp.print(message);
//   udp.endPacket();
//   Serial.print("发送消息: ");
//   Serial.println(message);
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
//         // Set_Pwm(throttle, steering);
//       }
//     }
//     // 延迟一段时间以避免CPU占用过高
//     vTaskDelay(100 / portTICK_PERIOD_MS);
//   }
// }

// void setup()
// {
//   // 初始化串口
//   Serial.begin(115200);
//   Serial.println("ESP32 UDP 无刷电机+舵机控制启动中...");

//   // 连接WiFi
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

//   // 初始化电调和舵机PWM
//   ledcSetup(ESC_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
//   ledcAttachPin(ESC_PWM_PIN, ESC_CHANNEL);

//   ledcSetup(SERVO_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
//   ledcAttachPin(SERVO_PWM_PIN, SERVO_CHANNEL);

//   // 初始化电调（校准过程，通常需要发送1000us脉冲）
//   calibrateESC();

//   timer_control.attach_ms(interrupt_time_control, control); // 定时器中断开启

//   // 获取MAC地址
//   uint8_t mac[6];
//   WiFi.macAddress(mac);
//   char macStr[18];
//   sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
//   Serial.print("MAC地址: ");
//   Serial.println(macStr);

//   // 提取MAC地址的后四位并转换为十进制
//   int macLastFourDecimal = (mac[4] << 8) | mac[5];
//   Serial.print("MAC地址的后四位（十进制）: ");
//   Serial.println(macLastFourDecimal);

//   // 如果转换后的十进制数是五位数，则截取后四位
//   if (macLastFourDecimal > 9999)
//   {
//     macLastFourDecimal = macLastFourDecimal % 10000;
//   }

//   // 启动UDP
//   udp.begin(localPort);
//   Serial.println("UDP客户端已启动");

//   // 向服务器发送MAC地址的后四位（十进制）
//   macLastFourDecimalStr = String(macLastFourDecimal);
//   sendUdpMessage(macLastFourDecimalStr);

//   // 创建任务来处理UDP接收数据
//   xTaskCreatePinnedToCore(readUdpData, "ReadUdpData", 4096, NULL, 1, NULL, 1);
// }

// void loop()
// {
//   // 检查串口是否有数据传入
//   if (Serial.available() > 0)
//   {
//     String receivedData = Serial.readStringUntil('\n'); // 读取一行数据直到换行符
//     receivedData.trim();                                // 去除首尾空白字符

//     // 检查是否为reset指令
//     if (receivedData.equalsIgnoreCase("reset"))
//     {
//       resetControls();
//     }
//     // 检查是否为restart指令
//     else if (receivedData.equalsIgnoreCase("restart"))
//     {
//       restartSystem();
//     }
//     else
//     {
//       sendUdpMessage(receivedData);
//     }
//   }
// }