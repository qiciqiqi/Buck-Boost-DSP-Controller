#include "platform.h"
#include "app_control.h"
#include "app_adc.h"
#include "control_settings.h"
#include "viewer.h"
static volatile uint32_t milliseconds;
volatile uint32_t control_isr_max_cycles;
static volatile uint32_t last_control_tick;
static volatile uint32_t last_adc_tick;

uint32_t Platform_Millis(void) { return milliseconds; }
void Platform_DelayMs(uint16_t ms)
{
    while(ms--) DEVICE_DELAY_US(1000);
}

__interrupt void AdcaISR(void)
{
    if(ADC_getInterruptOverflowStatus(POWER_ADC_BASE, ADC_INT_NUMBER1))
    {
        adc_error_count++;
        BuckControl_EmergencyStop(5U);
        ADC_clearInterruptOverflowStatus(POWER_ADC_BASE, ADC_INT_NUMBER1);
    }
    BuckAdc_ConversionComplete();
    last_adc_tick=milliseconds;
    ADC_clearInterruptStatus(POWER_ADC_BASE, ADC_INT_NUMBER1);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

__interrupt void PwmISR(void)
{
    uint32_t start=CPUTimer_getTimerCount(CPUTIMER1_BASE), elapsed;
    bool down=EPWM_getTimeBaseCounterDirection(POWER_PWM_BASE)==EPWM_TIME_BASE_STATUS_COUNT_DOWN;
    /* Zero event uses the completed previous peak ADC frame, matching STM32.
     * New CMPA loads at period, then SD# opens there with IN low. */
    BuckControl_Tick(down);
    last_control_tick=milliseconds;
    elapsed=start-CPUTimer_getTimerCount(CPUTIMER1_BASE);
    if(elapsed>control_isr_max_cycles) control_isr_max_cycles=elapsed;
    if(elapsed >= 3750U) BuckControl_EmergencyStop(7U);
    EPWM_clearEventTriggerInterruptFlag(POWER_PWM_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

__interrupt void TickISR(void)
{
    milliseconds++;
    viewer_heartbeat++;
    CPUTimer_clearOverflowFlag(CPUTIMER0_BASE);
    /* Independent scheduler detects stalled PWM interrupts even during LCD IO. */
    if(milliseconds-last_control_tick>2U)
        BuckControl_EmergencyStop(7U);
    if(milliseconds-last_adc_tick>2U && adc_error_count==0U)
    { adc_error_count++; BuckControl_EmergencyStop(5U); }
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

__interrupt void FatalISR(void)
{
    GPIO_writePin(POWER_EN,0U);
    openloop_command=current_reference=output_voltage_target=0.0f;
    control_fault_latched=1U;
    for(;;) { /* Watchdog reset, no automatic restart. */ }
}

void PowerTiming_Start(void)
{
    Interrupt_register(INT_ADCA1,&AdcaISR);
    Interrupt_register(INT_EPWM1,&PwmISR);
    Interrupt_register(INT_TIMER0,&TickISR);
    Interrupt_register(INT_ILLEGAL,&FatalISR);
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_setPreScaler(CPUTIMER0_BASE,0U);
    CPUTimer_setPeriod(CPUTIMER0_BASE,DEVICE_SYSCLK_FREQ/1000U-1U);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_clearOverflowFlag(CPUTIMER0_BASE);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    ADC_clearInterruptOverflowStatus(POWER_ADC_BASE,ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(POWER_ADC_BASE,ADC_INT_NUMBER1);
    EPWM_clearEventTriggerInterruptFlag(POWER_PWM_BASE);
    EPWM_setTimeBaseCounter(POWER_PWM_BASE,0U);
    Interrupt_enable(INT_ADCA1);
    Interrupt_enable(INT_EPWM1);
    Interrupt_enable(INT_TIMER0);
    CPUTimer_startTimer(CPUTIMER0_BASE);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    EINT;
}
