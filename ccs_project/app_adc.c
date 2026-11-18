/* 固定接线PA0=I、PC0=REF、PA1=VBAT、PC1=VBUS，换模式不换采样线。 */
#include "app_adc.h"
#include "platform.h"
#include "adc_calibration.h"
#include "control_settings.h"

#define ADC_REFERENCE_VOLTAGE 3.3f
#define ADC_FULL_SCALE 4095.0f
#define ADC_AVERAGE_ALPHA 0.05f
static float vbat_raw_avg, vbus_raw_avg;
volatile uint16_t adc_current_raw;
volatile uint16_t adc_reference_raw; /* PC0，仅观察REF，当前不参与电流零点补偿。 */
volatile uint16_t adc_vbat_raw;
volatile uint16_t adc_vbus_raw;
volatile float adc_current_raw_avg;
volatile float feedback_current;
volatile float feedback_current_avg;
volatile float vbat_voltage_avg; /* PA1，实际VBAT端口电压，单位V。 */
volatile float vbus_voltage_avg; /* PC1，实际VBUS端口电压，单位V。 */
volatile uint32_t adc_frame_count;
volatile uint32_t adc_error_count;

void BuckAdc_Start(void)
{
    adc_frame_count = adc_error_count = 0U;
    adc_current_raw = adc_reference_raw = adc_vbat_raw = adc_vbus_raw = 0U;
    adc_current_raw_avg = vbat_raw_avg = vbus_raw_avg = 0.0f;
    feedback_current = feedback_current_avg = 0.0f;
    vbat_voltage_avg = vbus_voltage_avg = 0.0f;

}

void BuckAdc_ConversionComplete(void)
{
    adc_current_raw = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
    adc_reference_raw = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);
    adc_vbat_raw = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER2);
    adc_vbus_raw = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER3);
    if (adc_frame_count == 0U)
    {
        adc_current_raw_avg = (float)adc_current_raw;
        vbat_raw_avg = (float)adc_vbat_raw;
        vbus_raw_avg = (float)adc_vbus_raw;
    }
    else
    {
        adc_current_raw_avg += ADC_AVERAGE_ALPHA * ((float)adc_current_raw - adc_current_raw_avg);
        vbat_raw_avg += ADC_AVERAGE_ALPHA * ((float)adc_vbat_raw - vbat_raw_avg);
        vbus_raw_avg += ADC_AVERAGE_ALPHA * ((float)adc_vbus_raw - vbus_raw_avg);
    }
    /* 保留已标定的完整零点，不能再直接减PC0，否则会重复扣除偏置。 */
    feedback_current = ((float)adc_current_raw - ADC_CURRENT_ZERO_RAW) *
                       (ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE) * ADC_CURRENT_A_PER_PIN_V * DCDC_CURRENT_SIGN;
    feedback_current_avg = (adc_current_raw_avg - ADC_CURRENT_ZERO_RAW) *
                           (ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE) * ADC_CURRENT_A_PER_PIN_V * DCDC_CURRENT_SIGN;
    vbat_voltage_avg = (vbat_raw_avg - ADC_VOLTAGE_ZERO_RAW) *
                       (ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE) * ADC_BUCK_VOLTAGE_RATIO;
    vbus_voltage_avg = (vbus_raw_avg - ADC_BOOST_VOLTAGE_ZERO_RAW) *
                       (ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE) * ADC_BOOST_VOLTAGE_RATIO;
    adc_frame_count++;
}

void BuckAdc_ReadFeedback(float *current, float *voltage, uint16_t *voltage_raw)
{
    bool saved_interrupt_state = Interrupt_disableGlobal();
    /* 模式切换后即使新ADC帧尚未到来，也按当前方向重算，避免旧符号。 */
    *current = ((float)adc_current_raw - ADC_CURRENT_ZERO_RAW) *
               (ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE) * ADC_CURRENT_A_PER_PIN_V * DCDC_CURRENT_SIGN;
    *voltage_raw = (active_power_mode == POWER_BOOST) ? adc_vbus_raw : adc_vbat_raw;
    if (!saved_interrupt_state) Interrupt_enableGlobal();
    /* 控制使用本帧原始采样，均值仅用于观察，避免额外滤波延迟。
       电压只在此函数局部选择，不另外定义会随模式改含义的全局别名。 */
    if (active_power_mode == POWER_BOOST)
        *voltage = ((float)*voltage_raw - ADC_BOOST_VOLTAGE_ZERO_RAW) *
               (ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE) * ADC_BOOST_VOLTAGE_RATIO;
    else
        *voltage = ((float)*voltage_raw - ADC_VOLTAGE_ZERO_RAW) *
               (ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE) * ADC_BUCK_VOLTAGE_RATIO;
}
