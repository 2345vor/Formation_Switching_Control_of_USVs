// #include <Arduino.h>
// #include <WiFi.h>
// #include <WiFiUdp.h>
// #include <Arduino.h>
// #include <Ticker.h>

// Ticker timer_control;

// // TB6612FNG驱动模块控制信号 共6个
// // #define IN1 26 // 控制电机1的方向A，01为正转，10为反转
// // #define IN2 27 // 控制电机1的方向B，01为正转，10为反转
// #define PWMA 1 // 控制电机1 PWM控制引脚
// // #define IN3 12 // 控制电机2的方向A，01为正转，10为反转
// // #define IN4 13 // 控制电机2的方向B，01为正转，10为反转
// #define PWMB 2 // 控制电机2 PWM控制引脚



// // #define resolution 8              // 使用PWM占空比的分辨率，占空比最大可写2^8-1=255
// #define interrupt_time_control 15 // 定时器15ms中断控制时间

// // 1. 测试配置 WiFi网络配置
// // const char *ssid = "307";
// // const char *password = "dmu307307";
// // // UDP配置
// // WiFiUDP udp;
// // IPAddress serverIP(192, 168, 3, 88); // 服务器的IP地址，需修改为实际地址
// // 2. 实际配置
// const char* ssid = "test305";
// const char* password = "DMU305307";
// // UDP配置
// WiFiUDP udp;
// IPAddress serverIP(192, 168, 0, 102);  // 服务器的IP地址，需修改为实际地址
// unsigned int serverPort = 8000; // 服务器端口
// unsigned int localPort = 8889;  // 本地端口
// char replyBuffer[255];          // 接收缓冲区

// int pwmA1 = 0, pwmB1 = 0;
// void commend_parse(String incomingPacket)
// {
//   incomingPacket.trim();
//   // 解析控制指令
//   String command(incomingPacket);
//   int commaIndex1 = command.indexOf(',');
//   int commaIndex2 = command.indexOf(',', commaIndex1 + 1);
//   int commaIndex3 = command.indexOf(',', commaIndex2 + 1);

//   int speed1 = command.substring(0, commaIndex1).toInt();
//   int speed2 = command.substring(commaIndex1 + 1, commaIndex2).toInt();
//   int speed3 = command.substring(commaIndex2 + 1, commaIndex3).toInt();
//   int speed4 = command.substring(commaIndex3 + 1).toInt();
//   double moto1 = speed1 - speed2;
//   double moto2 = speed3 - speed4;
//   pwmA1 = int((moto1 / 250000) * 255);
//   pwmB1 = int((moto2 / 250000) * 255);
//   // Set_Pwm( moto1,  moto2);
// }
// void Set_Pwm(int moto1, int moto2)
// {
//   int Amplitude = 255; //===PWM满幅是256 限制在255
//   Serial.println(String(moto1) + ":" + String(moto2));
  
//   // 限制PWM
//   if (moto1 < -Amplitude)
//     moto1 = -Amplitude;
//   if (moto1 > Amplitude)
//     moto1 = Amplitude;
//   if (moto2 < -Amplitude)
//     moto2 = -Amplitude;
//   if (moto2 > Amplitude)
//     moto2 = Amplitude;

//   ledcWrite(0, abs(moto1));
//   ledcWrite(1, abs(moto1));
//   ledcWrite(2, abs(moto2));
//   ledcWrite(3, abs(moto2));
// }

// void control()
// {
//   Set_Pwm(pwmA1, pwmB1);
// }

// // 发送UDP消息
// void sendUdpMessage(String message)
// {
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
//         Set_Pwm(pwmA1, pwmB1);
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
//   Serial.println("ESP32 UDP客户端启动中...");

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
//   // 初始化电机
//   ledcSetup(0, 5000, 8);
//   ledcAttachPin(PWMA, 0);
  
//   ledcSetup(1, 5000, 8);
//   ledcAttachPin(PWMB, 1);


//   // timer_control.attach_ms(interrupt_time_control, control); // 定时器中断开启

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
//   String macLastFourDecimalStr = String(macLastFourDecimal);
//   sendUdpMessage(macLastFourDecimalStr);

//   // 创建任务来处理UDP接收数据
//   xTaskCreate(readUdpData, "ReadUdpData", 2048, NULL, 1, NULL);
// }

// void loop()
// {
//   // 检查串口是否有数据传入
//   if (Serial.available() > 0)
//   {
//     String receivedData = Serial.readStringUntil('\n'); // 读取一行数据直到换行符
//     sendUdpMessage(receivedData);
//   }
// }
