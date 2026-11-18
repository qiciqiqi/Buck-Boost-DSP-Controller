#ifndef CONTROL_SETTINGS_H
#define CONTROL_SETTINGS_H

/* 默认上电模式；运行中通过Viewer写power_mode/control_mode选择。换向前断开功率。 */
#define POWER_BUCK       0U /* power_mode=0：Buck降压 */
#define POWER_BOOST      1U /* power_mode=1：Boost升压 */
#define CONTROL_CURRENT  0U /* control_mode=0：单电流环 */
#define CONTROL_DUAL     1U /* control_mode=1：电压外环+电流内环 */
#define CONTROL_OPEN     2U /* control_mode=2：开环，占空比由openloop_command给定 */
#define CONTROL_VOLTAGE  3U /* control_mode=3：单电压环，仅Buck有效 */
#define DCDC_DEFAULT_POWER_MODE   POWER_BOOST  /* 修改此处选择上电默认Buck/Boost */
#define DCDC_DEFAULT_CONTROL_MODE CONTROL_DUAL /* 修改此处选择上电默认控制方式 */

/* Buck：沿用已验证的默认PI；电流为电池输出支路电流。 */
#define BUCK_CURRENT_KP         1.0f  /* 占空比百分点/A */
#define BUCK_CURRENT_KI        20.0f  /* 百分点/(A*s) */
#define BUCK_VOLTAGE_KP         0.05f /* A/V，双环外环 */
#define BUCK_VOLTAGE_KI         0.10f /* A/(V*s) */
#define BUCK_CURRENT_MIN_A      0.2f
#define BUCK_CURRENT_MAX_A      3.0f
#define BUCK_CURRENT_STEP_A     0.1f
#define BUCK_VOLTAGE_MIN_V      2.0f
#define BUCK_VOLTAGE_MAX_V     12.0f
#define BUCK_VOLTAGE_STEP_V     0.5f
#define BUCK_DUAL_CURRENT_MAX_A 3.0f
#define BUCK_OVERVOLTAGE_V     20.0f
#define BUCK_PWM_MAX_PERCENT   95.0f
#define BUCK_SINGLE_VOLTAGE_KP  1.0f /* 百分点/V，仅模式3 */
#define BUCK_SINGLE_VOLTAGE_KI  2.0f /* 百分点/(V*s) */

/* Boost首次验证：输入3V、输出足额5Ω。以下为待实物整定初值。 */
#define BOOST_CURRENT_KP         1.0f /* 低侧储能占空比百分点/A */
#define BOOST_CURRENT_KI        20.0f /* 百分点/(A*s) */
#define BOOST_VOLTAGE_KP         0.05f
#define BOOST_VOLTAGE_KI         0.10f
#define BOOST_CURRENT_MIN_A      0.8f /* 输入支路电流，不是输出电流 */
#define BOOST_CURRENT_MAX_A      2.0f
#define BOOST_CURRENT_STEP_A     0.1f
#define BOOST_VOLTAGE_MIN_V      4.0f
#define BOOST_VOLTAGE_MAX_V      5.0f
#define BOOST_VOLTAGE_STEP_V     0.5f
#define BOOST_DUAL_CURRENT_MAX_A 2.5f
#define BOOST_OVERVOLTAGE_V      7.0f
#define BOOST_PWM_MIN_PERCENT    5.0f /* 运行时保留低侧脉冲及自举刷新 */
#define BOOST_PWM_MAX_PERCENT   60.0f

#define DCDC_OVERCURRENT_A       3.2f /* 实测支路电流正/负方向均保护 */
#define DCDC_CONTROL_PERIOD_S    0.00005f /* 约20kHz，不改现有TIM/ADC */
#define DCDC_OPEN_MAX_PERCENT   30.0f /* 开环首次验证的额外限制 */

/* 双环内环独立初值：以后分别整定，不与单电流环联动。 */
#define BUCK_DUAL_CURRENT_KP  1.0f
#define BUCK_DUAL_CURRENT_KI 20.0f
#define BOOST_DUAL_CURRENT_KP  1.0f
#define BOOST_DUAL_CURRENT_KI 20.0f

/* 可写模式请求；active_*由控制中断发布，只读观察。255表示切换中/无效。 */
extern volatile uint32_t power_mode, control_mode;
extern volatile uint32_t active_power_mode, active_control_mode;
#define MODE_INACTIVE 255U
#define DCDC_POWER_MODE active_power_mode
#define DCDC_CONTROL_MODE active_control_mode
/* 以下都是运行时选择，不能用于预处理#if。 */
#define DCDC_SELECT(buck, boost) ((active_power_mode == POWER_BOOST) ? (boost) : (buck))
#define DCDC_CURRENT_MIN_A DCDC_SELECT(BUCK_CURRENT_MIN_A, BOOST_CURRENT_MIN_A)
#define DCDC_CURRENT_MAX_A DCDC_SELECT(BUCK_CURRENT_MAX_A, BOOST_CURRENT_MAX_A)
#define DCDC_CURRENT_STEP_A DCDC_SELECT(BUCK_CURRENT_STEP_A, BOOST_CURRENT_STEP_A)
#define DCDC_VOLTAGE_MIN_V DCDC_SELECT(BUCK_VOLTAGE_MIN_V, BOOST_VOLTAGE_MIN_V)
#define DCDC_VOLTAGE_MAX_V DCDC_SELECT(BUCK_VOLTAGE_MAX_V, BOOST_VOLTAGE_MAX_V)
#define DCDC_VOLTAGE_STEP_V DCDC_SELECT(BUCK_VOLTAGE_STEP_V, BOOST_VOLTAGE_STEP_V)
#define DCDC_DUAL_CURRENT_MAX_A DCDC_SELECT(BUCK_DUAL_CURRENT_MAX_A, BOOST_DUAL_CURRENT_MAX_A)
#define DCDC_OVERVOLTAGE_V DCDC_SELECT(BUCK_OVERVOLTAGE_V, BOOST_OVERVOLTAGE_V)
#define DCDC_PWM_MIN_PERCENT DCDC_SELECT(0.0f, BOOST_PWM_MIN_PERCENT)
#define DCDC_PWM_MAX_PERCENT DCDC_SELECT(BUCK_PWM_MAX_PERCENT, BOOST_PWM_MAX_PERCENT)
#define DCDC_CURRENT_SIGN DCDC_SELECT(1.0f, (-1.0f))

#endif
