/**
  ******************************************************************************
  * @file    app_control.h
  * @brief   Buck 变换器双闭环控制模块
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_CONTROL_H
#define __APP_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "pid.h"
#include "app_adc.h"

#include "control_settings.h"

/* 标志和全部人工参数见control_settings.h；保留原业务函数名。 */
/* 人工调参写这些独立参数组；运行PI的Kp/Ki仅作观察，每拍从参数组加载。 */
typedef struct {
    float current_kp, current_ki; /* 百分点/A、百分点/(A*s) */
    float voltage_kp, voltage_ki; /* 双环A/V、A/(V*s)；单电压环百分点/V、百分点/(V*s) */
} CONTROL_SETTINGS;
extern volatile CONTROL_SETTINGS buck_current_settings, buck_dual_settings;
extern volatile CONTROL_SETTINGS boost_current_settings, boost_dual_settings;
extern volatile CONTROL_SETTINGS buck_voltage_settings;

/* PI 控制器 */
extern volatile PI_CTRL_DATA_DEF voltage_pi;
extern volatile PI_CTRL_DATA_DEF current_pi;
/* 独立单电压环：Kp单位为占空比百分点/V，Ki为百分点/(V*s)。
   Out/ui仍为PWM比较计数；不复用双闭环外环参数。 */
extern volatile PI_CTRL_DATA_DEF voltage_only_pi;

/* 目标写0停机。双环内环给定只来自voltage_pi.Out。
   电压目标始终可观察；仅双环/单电压环使用。Buck为VBAT，Boost为VBUS。 */
extern volatile float output_voltage_target;

extern volatile float current_reference;
extern volatile float current_limit;
extern volatile float pwm_limit;

/* 采样值与控制量 */
extern volatile float pwm_duty_counts;          /* 主动开关导通计数：Buck高侧、Boost低侧；非TIM1_CCR1 */
extern volatile float openloop_command; /* 开环占空比百分数，0停机；首次建议5 */

/* Read-only indication of actual software PWM/driver enable. */
extern volatile uint8_t power_output_enabled;
extern volatile uint8_t control_fault_latched;

/* Exported functions prototypes ---------------------------------------------*/
void BuckControl_ParameterInit(void);
void BuckControl_KeyPressed(void);
void BuckControl_Start(void);
void BuckControl_EmergencyStop(uint8_t fault);
void BuckControl_Tick(uint16_t down);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CONTROL_H */
