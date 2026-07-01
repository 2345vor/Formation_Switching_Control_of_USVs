# 室内水池无人船集群ESP32C3OTA下位机软件

## 项目简介

本系统是基于Platformio开发的室内水池无人船集群ESP32C3OTA下位机程序，用于实现无人船集群的控制、数据采集和分析。支持单桨和双桨两种船舶模型，通过UDP通信接收上位机控制指令，实现远程控制和OTA固件升级功能。

下位机Platformio环境搭建：
在vscode中安装Platformio插件，创建新的Platformio项目，选择ESP32C3作为开发板。

## 硬件配置

### 开发板

- **Seeed Xiao ESP32C3**：小巧的ESP32C3开发板，集成WiFi和蓝牙功能

### 单桨模式引脚定义

- **无刷电机电调**：引脚3 (ESC_CHANNEL: 0)
- **舵机**：引脚4 (SERVO_CHANNEL: 1)
- **PWM频率**：50Hz
- **PWM分辨率**：12位 (4096级)

### 双桨模式引脚定义

- **左电机A**：引脚1 (PWM1A_CHANNEL: 0)
- **左电机B**：引脚2 (PWM1B_CHANNEL: 1)
- **右电机A**：引脚3 (PWM2A_CHANNEL: 2)
- **右电机B**：引脚4 (PWM2B_CHANNEL: 3)
- **PWM频率**：5000Hz
- **PWM分辨率**：8位 (256级)

### 状态指示灯

- **LED**：引脚0 (用于系统状态指示)

## 软件架构

### 项目结构

```
esp32c3_single_servo_boat/
├── include/
│   └── main.hpp          # 头文件，包含宏定义、枚举和函数声明
├── src/
│   └── main.cpp          # 主程序文件，包含核心逻辑
├── platformio.ini        # PlatformIO配置文件
├── partition.csv         # 分区表配置
└── README.md             # 项目说明文档
```

### 核心功能

1. **船舶模型支持**：单桨和双桨两种模式
2. **UDP通信**：接收上位机控制指令
3. **OTA升级**：支持通过网页进行固件升级
4. **心跳机制**：定期向上位机发送心跳包
5. **自动校准**：系统启动时自动校准电机
6. **故障恢复**：支持控制参数重置和系统重启

## 船舶模型配置

### 配置位置

船舶模型配置位于 `include/main.hpp`文件的第10-11行：

```cpp
enum USV_type {Single_paddle, double_paddle };
enum USV_type USV = double_paddle;
```

### 配置说明

- `Single_paddle`：单桨模式，使用一个无刷电机和一个舵机控制
- `double_paddle`：双桨模式，使用两个直流电机控制（默认）

### 切换模型

只需修改 `USV`变量的值即可切换船舶模型：

```cpp
// 切换为单桨模式
enum USV_type USV = Single_paddle;

// 切换为双桨模式
enum USV_type USV = double_paddle;
```

切换模型后，系统会自动使用相应的引脚配置和控制逻辑。

## OTA升级过程

### 1. 进入OTA模式

#### 通过UDP指令

向上位机发送UDP指令 `OTA`即可进入OTA升级模式。

#### 通过串口指令

通过串口发送 `OTA`指令也可进入OTA升级模式。
![基本轨迹显示](fig\ESPOTA上传成功界面1.png)


### 2. 连接WiFi

设备默认连接到配置的WiFi网络（在 `src/main.cpp`中配置）：

```cpp
// WiFi配置
const char *ssid = "test305";
const char *password = "DMU305307";
```

### 3. 访问OTA页面

设备进入OTA模式后，会向上位机发送 `OTA_READY:<IP地址>`消息。使用该IP地址在浏览器中访问即可打开OTA升级页面。

### 4. 上传固件

在OTA页面中选择编译好的固件文件（位于 `.pio/build/seeed_xiao_esp32c3/firmware.bin`），点击"更新固件"按钮开始上传。

### 5. 等待更新完成

上传完成后，设备会自动重启并运行新固件。

### OTA升级页面特点

- 美观的界面设计
- 实时进度显示
- 状态提示
- 自动重启通知

## 项目配置

### PlatformIO配置

位于 `platformio.ini`：

```ini
[env:seeed_xiao_esp32c3]
platform = espressif32
board = seeed_xiao_esp32c3
framework = arduino
board_build.partitions = partition.csv
```

### 分区表配置

位于 `partition.csv`，支持OTA双分区：

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
app1,     app,  ota_1,   0x150000,0x140000,
spiffs,   data, spiffs,  0x290000,0x160000,
coredump, data, coredump,0x3F0000,0x10000,
```

## 通信协议

### UDP通信

- **服务器IP**：192.168.0.102
- **服务器端口**：8000
- **本地端口**：8080
- **心跳间隔**：2秒

### 指令格式

#### 单桨模式指令

```
<油门值>,<舵机值>
```

- 油门值范围：1000-2000 (us)
- 舵机值范围：1000-2000 (us)

#### 双桨模式指令

```
<左电机值>,<右电机值>
```

- 电机值范围：1000到2000（负值为反转）

#### 特殊指令

- `reset`：重置控制参数
- `restart`：重启系统
- `OTA`：进入OTA升级模式



### 设备标识

设备通过MAC地址的后四位（十进制）作为唯一标识，系统启动时会向上位机发送该标识。

## 代码说明

### 核心函数

#### 单桨模式控制

```cpp
void Set_Single_paddle_Pwm(int throttle_pwm, int steering_pwm);
```

将油门和舵机的PWM值转换为LEDC通道的占空比并输出。

#### 双桨模式控制

```cpp
void Set_Double_paddle_Pwm(int pwm_left, int pwm_right);
```

根据左右电机的PWM值控制电机的正反转和速度。

#### 指令解析

```cpp
void commend_parse(String incomingPacket);
```

解析接收到的UDP或串口指令，执行相应的操作。

#### UDP消息发送

```cpp
bool sendUdpMessage(String message);
```

向上位机发送UDP消息。

#### UDP数据读取

```cpp
void readUdpData(void *parameter);
```

FreeRTOS任务，负责读取UDP数据。

## 快速开始

### 1. 安装依赖

使用PlatformIO打开项目，自动安装所需依赖。

### 2. 配置船舶模型

在 `include/main.hpp`中修改 `USV`变量的值，选择单桨或双桨模式。

### 3. 配置WiFi

在 `src/main.cpp`中修改WiFi SSID和密码。

### 4. 编译上传

使用PlatformIO编译并上传固件到ESP32C3开发板。

### 5. 连接上位机

确保上位机运行在配置的IP地址和端口，等待设备发送心跳包。

## 注意事项

1. **电机校准**：系统启动时会自动校准电机，确保电机连接正确。
2. **PWM值范围**：确保发送的PWM值在安全范围内，避免损坏电机。
3. **WiFi连接**：确保设备能连接到配置的WiFi网络，否则无法进行OTA升级。
4. **上位机通信**：上位机需运行在配置的IP地址和端口，否则无法接收控制指令。
5. **OTA升级**：升级过程中请勿断电，否则可能导致设备无法启动。

## 故障排查

### 无法连接WiFi

- 检查WiFi SSID和密码是否正确
- 确保设备在WiFi信号覆盖范围内

### 无法接收指令

- 检查UDP服务器IP和端口是否正确配置
- 确保上位机和设备在同一网络

### 电机不工作

- 检查电机连接是否正确
- 确保PWM值在有效范围内
- 检查船舶模型是否正确配置

## 版本历史

- v1.0：初始版本，支持单桨和双桨模式，UDP通信，OTA升级
