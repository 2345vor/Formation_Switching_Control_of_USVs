# Formation_Switching_Control_of_USVs

A Practical Data-Driven Framework for Multi-USV Formation Switching Control: Design and Experimental Validation

演示视频（最新）：[无人船编队切换控制 — 室内水池实验](https://www.youtube.com/watch?v=DAyNJNGOOjk)

[English Version](README.md)

## 数据集介绍

本项目包含三艘无人水面艇(USV)的实验数据，存储在 `data/` 目录下：

- `usv1_data.xlsx` - USV1 的实验数据
- `usv2_data.xlsx` - USV2 的实验数据
- `usv3_data.xlsx` - USV3 的实验数据

### 数据格式

每个 Excel 文件包含以下列：

| 列名    | 说明            | 单位 |
| ------- | --------------- | ---- |
| x       | x 轴位置        | m    |
| y       | y 轴位置        | m    |
| Heading | 航向角          | 度   |
| u       | 纵向速度        | m/s  |
| v       | 横向速度        | m/s  |
| PWM_L   | 左推进器 PWM 值 | -    |
| PWM_R   | 右推进器 PWM 值 | -    |

### 数据采样参数

- 采样频率：10 Hz（dt = 0.1 s）
- 默认使用数据行数：450 行
- 数据来源：实际 USV 实验采集

## Multi-USV 模型识别

`multi_usv_identification.py` 用于通过多艇数据进行联合建模，采用 3 折交叉验证评估模型泛化能力。

### 识别流程

1. **单艇预处理**

   - 航向角转换：度 → 弧度，并解包裹
   - 计算角速度：r = d(psi)/dt
   - 平滑处理：使用 Savitzky-Golay 滤波器平滑 u、v、r
   - 计算加速度：du、dv、dr（有限差分）
   - PWM 中心化：PWM - 1500
2. **归一化**

   - 仅对控制输入 Tp、Ts 进行标准化
   - 状态变量（u、v、r）保持原始值
3. **构建回归矩阵**

   - 为每艘艇构建块对角特征矩阵（3N × 21）
   - 模型形式：
     ```
     du = a1*v*r + a2*u + a3*v + a4*r + a5*Tp + a6*Ts + a7
     dv = b1*u*r + b2*u + b3*v + b4*r + b5*Tp + b6*Ts + b7
     dr = c1*u*v + c2*u + c3*v + c4*r + c5*Tp + c6*Ts + c7
     ```
4. **3 折交叉验证**

   - Fold 1: 训练 [USV2+USV3] → 测试 [USV1]
   - Fold 2: 训练 [USV1+USV3] → 测试 [USV2]
   - Fold 3: 训练 [USV1+USV2] → 测试 [USV3]
5. **约束优化**

   - 使用 SLSQP 算法
   - 基于物理意义的参数边界约束

### 使用方法

直接运行脚本即可：

```bash
python multi_usv_identification.py
```

### 输出结果

1. **控制台输出**

   - 每艘艇的数据统计信息
   - 交叉验证过程和收敛状态
   - 详细的 MSE 结果表格
   - 识别出的模型参数方程
2. **可视化图表**

   | 图片                          | 说明                                                                               |
   | ----------------------------- | ---------------------------------------------------------------------------------- |
   | ![Figure 1](fig/Figure_1.png) | **轨迹跟踪对比** - 三艘 USV 的实测与仿真轨迹对比                             |
   | ![Figure 2](fig/Figure_2.png) | **状态变量对比** - 纵向速度 u、横向速度 v、角速度 r 和航向角 ψ 随时间的变化 |
   | ![Figure 3](fig/Figure_3.png) | **三艘 USV 实测轨迹**                                                        |
   | ![Figure 4](fig/Figure_4.png) | **三折交叉验证参数热力图** - 三个 Fold 识别出的模型参数                      |
   | ![Figure 5](fig/Figure_5.png) | **Fold 1 参数热力图** - USV2+USV3 训练、USV1 测试的模型参数                  |
   | ![Figure 6](fig/Figure_6.png) | **回归误差分析（5折）**                                                      |
   | ![Figure 7](fig/Figure_7.png) | **回归误差分析（3折）**                                                      |
3. **实验结果总结**

   - 最佳模型：USV2+USV3 训练 → USV1 测试，综合 Score = 0.614
   - 三折交叉验证均收敛，模型泛化能力良好

### 配置参数

可在脚本顶部修改以下配置：

```python
DATA_DIR   = 'data'              # 数据目录
BOAT_FILES = ['usv1_data.xlsx', 'usv2_data.xlsx', 'usv3_data.xlsx']
START_ROW  = 0                   # 起始行
N_ROWS     = 450                 # 使用数据行数
DT         = 0.1                 # 采样周期
SAVGOL_WIN = 5                   # 平滑窗口大小
PWM_CENTER = 1500.0              # PWM 中心值
```

## 依赖库

```bash
pip install numpy pandas matplotlib scipy scikit-learn seaborn openpyxl
```

## 源码结构

| 路径                            | 说明                                                                                                                                                                         |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `multi_usv_identification.py` | 核心识别脚本：带物理约束的 pooled SLSQP、留一艇交叉验证，以及消融实验（Individual OLS / Individual SLSQP / Pooled OLS / Pooled SLSQP）。                                     |
| `data/usv{1,2,3}_data.xlsx`   | 三艘双桨无人船的原始实验数据（位置、航向、速度、PWM 指令）。                                                                                                                 |
| `esp32c3_boat/`               | 下位机固件（Seeed Xiao ESP32-C3，PlatformIO/Arduino）。通过 UDP 接收上位机推进器指令，支持 OTA 升级，兼容单桨和双桨模式。引脚定义与通信协议详见 `esp32c3_boat/README.md`。 |
| `fig/`                        | 识别结果图（轨迹对比、状态变量、参数热力图、回归误差分析）。                                                                                                                 |

> **说明：** 共享模型 NMPC 控制器、PID/反步控制基线以及 Wi-Fi/UDP 实时控制循环运行于上位机，随论文实验数据一同发布于补充仓库。

## 实验平台

- **艇体：** 三艘 0.2\,m 3D 打印双桨电动无人船。
- **设施：** 室内拖曳水池，2\,m × 4\,m。
- **定位：** 光学动作捕捉系统（VRPN）100\,Hz，降采样至 10\,Hz 控制频率。
- **通信：** 上位机与各艇之间通过 Wi-Fi / UDP。
- **编队切换：** 三角形 → 一字形，线性插值过渡 $N_{\text{trans}}=50$ 步。

## 引用

如果使用本项目代码，请引用：

```
@ARTICLE{Fan2026muti,
  author={Fan, Yunsheng and Liu, Peng and Fugao, Duan and Sun, Xiaojie and Xiaoning, zhang and An, Quan},
  journal={Control Engineering Practice},
  title={A Practical Data-Driven Framework for Multi-USV Formation Switching Control: Design and Experimental Validation},
  year={2026},
}
```

## 许可证

详见 LICENSE 文件。
