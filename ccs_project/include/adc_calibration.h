#ifndef ADC_CALIBRATION_H
#define ADC_CALIBRATION_H

/* 从STM32实测值迁移的初值，DSP上必须复核；REF仅观察。
 * 实测零点原始计数。人工测量后更新数值，不另设“已标定”标志。 */
/* 电流固定采PA0：这里必须填零电流时PA0完整码值，不是I-REF差值。
   2026-11-08仅MCU+辅助供电、无功率输入，64个均值读数再平均。 */
#define ADC_CURRENT_ZERO_RAW 2076.36098063f
#define ADC_CURRENT_A_PER_PIN_V 2.000f /* 原实物标定值保留，Boost反向增益需复核 */
/* Buck输出VBAT的电压标定保留；VBUS固定接PC1，单独保留标定入口。 */
#define ADC_VOLTAGE_ZERO_RAW 0.13525390625f
#define ADC_BUCK_VOLTAGE_RATIO 15.0f
#define ADC_BOOST_VOLTAGE_ZERO_RAW 0.0f /* VBUS未实测，先用名义零点 */
#define ADC_BOOST_VOLTAGE_RATIO 15.0f /* VBUS未实测，先用原理图名义倍率 */

#endif
