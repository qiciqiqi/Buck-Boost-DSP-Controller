# 编译、下载与人工调参

阅读顺序：[根目录README](../README.md)的模式/按键范围 → [接线说明](wiring.md) → 本文 → [MATLAB仿真说明](../simulation/README.md)。模式标志、引脚和目标范围以README及 `ccs_project/include/control_settings.h` 为准。

## 1. 工具与工程导入

下列命令均在仓库根目录执行；双击启动器位于 `mcuviewer/Open_MCUViewer.cmd`。

本机已验证：CCS21.0.1、C2000Ware26.02、内置SysConfig1.28.1、C2000编译器25.11.1.LTS、MCUViewer1.4.0。Python需要pyserial：

```powershell
python -m pip install -r requirements.txt
```

在CCS导入 `ccs_project`，安装并关联C2000Ware，检查 `.syscfg`。当前工程的SDK链接使用CCS产品变量，不要求把完整SDK复制进仓库。

本机自动构建：

```powershell
python tools/check_new_ccs.py
```

其他电脑可设置 `CCS_ROOT`（含tools/eclipse/utils的ccs目录）、`C2000WARE_ROOT`（SDK目录）；脚本默认路径是本机D盘。自动脚本按已验证的SysConfig版本检查日志，换工具版本后须自行检查兼容性，或直接在CCS中构建。

固件调试信息采用DWARF3，链接时不压缩DWARF，用于MCUViewer的C2000类型解析。不要为了缩小文件随意改回不兼容的调试信息。

## 2. 烧录与独立运行

1. 确认接线、ADC输入范围和供电。首次保持功率主回路断电，所有目标为0。
2. 选择 **CPU1_FLASH** 构建，使用板载XDS110 USB下载 `ccs_project/CPU1_FLASH/BUCK_F28P55X.out`。CPU1_RAM用于在线调试，掉电不保留。
3. 下载完成若暂停在main，按继续运行。S2置0/0，S3为Flash启动1/1。
4. 确认零目标、`power_output_enabled=0`、心跳递增，再做ADC标定和限流上电。
5. 无线DAP只做UART透传，CLK/DIO不接DSP；无法代替XDS110烧录。

构建产物和变量地址必须对应实际板上固件；改代码重新构建后，先下载对应镜像再使用新地址。构建目录中的 `.elf` 不是另一个固件，而是构建脚本从同一 `.out` 复制出的解析副本。

本轮开源整理替换了许可来源不明确的旧字库，保留8×16/16×32接口，控制逻辑未改。新字库已经编译验证，**本轮未烧录实板，未进行新版字体的实物目视验证**。之前屏幕和通信验收属于此前固件。

## 3. 打开MCUViewer

本机电脑端DAP为COM14，其他电脑以实际串口为准。从DAP TX→DSP43/IO.15，RX←DSP44/IO.56，公共地，115200 8N1。

```powershell
.\mcuviewer\Start_MCUViewer.ps1 -Port COM14
```

脚本生成配置、启动后台串口桥接并打开MCUViewer。双击 `mcuviewer/Open_MCUViewer.cmd` 相当于使用COM14。其他电脑设置 `MCUVIEWER_EXE` 或通过 `-ViewerExe` 指定MCUViewer.exe；配置生成器可通过 `OFD2000_EXE` 或 `--ofd2000` 指定TI解析工具路径。

MCUViewer连接选 **GDBSERVER、127.0.0.1:3334**，使用C2000解析器与当前FLASH ELF。这里是本项目的UART内存桥接，不是真正的CPU调试服务器，不提供暂停、复位或下载。

默认打开Boost双环单页。选择其他页面前停止采集，打开 `mcuviewer/profiles` 相应文件，再启动采集；每页≤20个变量，保持本机已验证的使用方式。打开文件只改变观测页，不自动切换模式。

- 五份 `*_settings.mcuvproj`：各自PI和模式、目标、反馈、输出。
- `Modes_and_Feedback.mcuvproj`：总体观察、开环、current_limit/pwm_limit。
- `ADC_Calibration.mcuvproj`：原始码、零点标定观测。
- `Communication.mcuvproj`：心跳、错误、OLED状态。

人工保存的 `.mcuv` 是个人布局，默认忽略提交；不要与生成模板混作不同固件。重新构建后运行下列命令刷新模板，先另存需要保留的个人布局：

```powershell
python tools/prepare_mcuviewer.py --port COM14
```

配置地址来自本次FLASH map。C28x地址单位是16位字，串口长度单位是8位字节，不能套用STM32地址换算。

## 4. 先验收采样，再调控制

零目标时看 `viewer_heartbeat`（约1000/s）、`adc_frame_count`（约40000/s）是否增加，错误计数是否持续增加，输出是否关闭。`oled_online=1`表示I²C应答，实际显示仍要目视看。

电流通道在 `include/adc_calibration.h` 设定完整I通道零电流平均码；默认2076.36098063及2A/V沿用参考版本，**不等于DSP已经标定**。零电流时读取I通道本身，不把I-REF差值填作零点。REF当前只观察，电流计算中不能再次扣REF。

用可靠电流表在至少两个工作点核对增益与方向；Buck方向系数+1、Boost-1。VBAT/VBUS分别有ZERO_RAW和RATIO。控制使用原始反馈，0.05一阶均值仅用于观察。

带功率前先验证目标0确实关断，按输入方向配置电源与5Ω负载，限流启动。5Ω不能说明输入电压、电感额定电流或负载功率额定值，这些都须独立确认。

## 5. 人工调整的顺序与变量

1. 停机：当前模式目标写0，确认输出关闭。
2. 写 `power_mode`、`control_mode`，等待 `active_*` 一致；255表示切换等待或无效请求。Buck/Boost换向前断开功率主回路并重新确认接线。
3. 单电流环先用小电流目标，调整该方向 `*_current_settings.current_kp/current_ki`。先在Ki=0或较小时观察比例作用，再逐步增加Ki消除静差；每次改动记录输入、负载、目标、峰值与响应。
4. 双环先确认该方向 `*_dual_settings.current_kp/current_ki` 对电流参考有可控响应，再调同组 `voltage_kp/voltage_ki`。外环给出电流目标，限幅由 `current_limit` 决定。外环不应比内环更快；频响/裕度需要另行建模或测量，当前没有给出通用最优值。
5. Buck单电压环使用 `buck_voltage_settings.voltage_kp/voltage_ki`；开环只写 `openloop_command`，不使用PI。
6. 同时观察 `power_output_enabled`、`control_fault_latched`、`current_pi.Out`、`voltage_pi.Out` 和 `pwm_duty_counts`。输出持续顶限时先检查输入、负载、反馈方向与限值，不靠继续放大Ki解决。

单电流环与双环的内环是独立参数组，改一组不会自动改另一组。Ki输入的是每秒增益，代码内部乘50µs，不要人工再乘一次；电流PI单位为占空比百分点/A、百分点/(A·s)，双环电压PI为A/V、A/(V·s)。Buck单电压PI为百分点/V、百分点/(V·s)。

`pwm_duty_counts`为主动开关指令计数；Boost IN比较值取补数。非零目标时Buck PI降为零仍保留同步零高侧占空比行为；**目标写0**才是明确停机操作。带功率暂停CCS前先停机。

保护后先写目标0，故障消除且反馈安全才解锁；ADC停更/溢出须排查后复位。反馈/保护/active状态为只读，桥接只允许目标、模式、限值和PI组写入。

运行时调好的值断电不保存。确认后手动填回 `control_settings.h`，记录硬件条件，重跑仿真并重新编译下载。

## 6. 验证证据与可复现脚本

- `docs/validation/新版CCS_CPU1_RAM.txt`、FLASH：本轮最终完整编译记录。
- `python tools/verify_git_logic.py`：七种合法模式、非法组合、目标清零、互补PWM、故障锁定/解锁和零输出边界。使用主机外设替身，不是硬件在环。
- `python tools/verify_serial_protocol.py`：C28x读写、CRC及非法长度/地址检查。
- 上述主机测试默认使用MATLAB附带LCC，可设 `MATLAB_LCC_ROOT`；不依赖同级STM32工程。
- `python tools/test_viewer_link.py --port COMxx`：零目标只读无线自检。需要先停止桥接释放串口，仅停止MCUViewer采集不会释放后台串口。`test_oled_link.py`为OLED应答诊断。
- `docs/validation/20261116_OLED_MCUViewer/`：此前实板下载、无线通信、OLED、独立供电和七模式零目标切换证据。日期目录属于历史实际测试，不代表本轮新版已重新烧录。

作者已反馈带功率效果与STM32版接近；没有全工况、统一仪器的实测性能曲线。历史独立供电验收是拔掉XDS110 USB、保持外部供电的运行测试，未完全断电，不能算冷启动验收。OLED历史错误码与当前是否持续新增错误分开判断。
