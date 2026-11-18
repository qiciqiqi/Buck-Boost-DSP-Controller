# 许可证范围与第三方组件

本项目自有软件采用根目录 LICENSE 中的 MIT 许可证。下列第三方文件保留原许可；根目录 MIT 不覆盖、替换或扩大其授权。整个文件集合不是统一的 MIT 代码库。

| 组件 | 位置 | 来源与许可 |
|---|---|---|
| TI C2000Ware 设备支持 | `ccs_project/device/` | Texas Instruments，来自 C2000Ware 26.02；文件头 BSD 三条款许可原样保留 |
| TI 链接布局 | `ccs_project/28p55x_generic_*_lnk.cmd` | 基于 TI F28P55X 示例布局调整；保留 TI 来源归属，许可文本见 `licenses/TI-BSD-3-Clause.txt` |
| MCUViewer SerialDriver v1.2 | `ccs_project/SerialDriver/` | © 2025 Piotr Wasilewski；限与 MCUViewer 软件相关使用、修改与分发，不是 MIT。完整声明保留在源文件头 |
| SysConfig 生成物 | 本机构建目录 `CPU1_RAM/syscfg/`、`CPU1_FLASH/syscfg/` | 由 TI 工具生成，保留各文件许可；不作为仓库源码提交 |

MCUViewer SerialDriver 在本项目增加了 C28x 内存访问适配、地址/长度检查及原子写入接入；原协议和许可声明保留。其许可不能解读成脱离 MCUViewer 的通用串口库授权。项目可分享源码，但第三方部分应按其各自许可使用。

OLED 字库已改为 `tools/generate_font.py` 中手工定义的简单 5×7 图形生成，采用本项目 MIT；小写字母显示为大写，未支持字符显示问号。未继续分发旧示例中来源许可不明确的字体表。

C2000Ware 的 DriverLib、编译器运行库、CCS、SysConfig、MATLAB 和 MCUViewer 应由使用者从原厂获取，不随本仓库打包。构建生成的固件可能链接第三方运行库；若另行分发二进制，请同时核对本机 SDK/工具链附带的相关分发要求。

嘉立创 EDA `.epro2` 硬件工程不自动适用软件 MIT，作者尚未指定硬件许可。第三方符号、封装、器件资料及商标权利仍归原权利人。当前整理完成的是本地待发布材料，并非已经完成硬件开放许可。

来源：[TI C2000Ware](https://www.ti.com/tool/C2000WARE)、[MCUViewer](https://mcuviewer.com/)、[MCUViewer C2000 接入说明](https://docs.mcuviewer.com/c2000/)。
