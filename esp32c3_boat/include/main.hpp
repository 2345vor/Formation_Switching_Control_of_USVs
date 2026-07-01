#ifndef MAIN_HPP
#define MAIN_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <WebServer.h>
#include <Update.h>
// #include "esp_camera.h" // 添加摄像头支持头文件
// #include "soc/soc.h" // Disable brownout problems
// #include "soc/rtc_cntl_reg.h" // Disable brownout problems
enum USV_type {Single_paddle,double_paddle };
enum USV_type USV  = double_paddle;

// 无刷电机电调和舵机引脚定义
#define ESC_PWM_PIN 3    // 无刷电机电调控制引脚
#define SERVO_PWM_PIN 4  // 舵机控制引脚
// LEDC通道定义
#define ESC_CHANNEL 0    // 电调LED通道
#define SERVO_CHANNEL 1  // 舵机LED通道
#define PWM_FREQs 50      // PWM频率
#define PWM_RESOLUTIONs 12 // PWM分辨率

// 双桨机控制引脚定义
#define ESC_PWM_L 4    // 无刷电机电调左
#define ESC_PWM_R 3    // 无刷电机电调右
// LEDC通道定义
#define PWM_L_CHANNEL 0    // 电调LED通道
#define PWM_R_CHANNEL 1  // 舵机LED通道
#define PWM1_FREQs 50      // PWM频率
#define PWM1_RESOLUTIONs 12 // PWM分辨率

#define LED_pin 0
#define LED_LEFT_PIN 5     // 左侧转向灯
#define LED_RIGHT_PIN 6    // 右侧转向灯
#define TURN_DEADBAND 50   // 转向灯死区，避免轻微偏差误触发


#define interrupt_time_control 100 // 定时器中断时间

// WiFi配置
extern const char* ssid;
extern const char* password;

// UDP配置
extern WiFiUDP udp;
extern IPAddress serverIP;
extern IPAddress piServerIP;
extern IPAddress rkserverIP;
extern IPAddress dfgServerIP;
extern unsigned int serverPort;
extern unsigned int piServerPort;
extern unsigned int localPort;
extern unsigned int cameraPort;

// 相机配置
// extern bool cameraEnabled;
// extern TaskHandle_t cameraTaskHandle;

// 全局变量
extern int throttle;
extern int steering;
extern int pwm_left;
extern int pwm_right;
extern bool otaMode;

// 函数声明
int convertPWM_single(int pwmValue);
int convertPWM_double(int pwmValue);
void Set_Single_paddle_Pwm(int throttle_pwm, int steering_pwm);
void Set_Double_paddle_Pwm(int throttle_pwm, int steering_pwm);
void resetControls();
void restartSystem();
// void startCamera();
// void stopCamera();
// bool initCamera();
// void deinitCamera();
// void sendImageUDP(camera_fb_t * fb);
// void cameraTask(void *pvParameters);
void startOTA();
void commend_parse(String incomingPacket);
void setTurnLEDState(int left, int right);
void updateTurnLEDs();
bool handleLedCommand(String incomingPacket);
void control();
void calibrateESC();
void calibratePWM();
bool sendUdpMessageTo(IPAddress ip, unsigned int port, String message);
bool sendUdpMessage(String message);
void readUdpData(void *parameter);
String buildRegisterMessage();
String buildStatusMessage(int adcValue, float temperature);
String buildHeartbeatMessage();
String buildcommandMessage(String message);
#endif // MAIN_HPP
