#ifndef APP_ADC_H
#define APP_ADC_H

#include "platform.h"

/* 原始ADC计数；REF仅观察，电流换算仍使用adc_calibration.h的标定零点。 */
extern volatile uint16_t adc_current_raw;   /* PA0 */
extern volatile uint16_t adc_reference_raw; /* PC0 */
extern volatile uint16_t adc_vbat_raw;      /* PA1 */
extern volatile uint16_t adc_vbus_raw;      /* PC1 */
extern volatile float adc_current_raw_avg;
extern volatile float feedback_current;     /* 正向：Buck输出支路 / Boost输入支路。 */
extern volatile float feedback_current_avg; /* A，平滑观察值。 */

/* 实际功率端电压仅保留这两个变量，含义不随模式改变，单位V。 */
extern volatile float vbat_voltage_avg; /* PA1 */
extern volatile float vbus_voltage_avg; /* PC1 */

extern volatile uint32_t adc_frame_count;
extern volatile uint32_t adc_error_count;

void BuckAdc_Start(void);
void BuckAdc_ConversionComplete(void);

void BuckAdc_ReadFeedback(float *current, float *voltage, uint16_t *voltage_raw);

#endif
