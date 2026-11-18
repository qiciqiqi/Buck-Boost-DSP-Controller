#include "platform.h"
#include "app_control.h"
#include "app_lcd.h"
#include "viewer.h"
#include <limits.h>

typedef char CheckFloat32[(sizeof(float)*CHAR_BIT==32)?1:-1];
typedef char CheckDouble64[(sizeof(double)*CHAR_BIT==64)?1:-1];
typedef char CheckUint32[(sizeof(uint32_t)*CHAR_BIT==32)?1:-1];
void main(void)
{
    Device_init();
    Device_initGPIO();
    DINT;
    Interrupt_initModule();
    Interrupt_initVectorTable();
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    GPIO_setPinConfig(GPIO_1_GPIO1);
    GPIO_writePin(1U,0U);
    GPIO_setDirectionMode(1U,GPIO_DIR_MODE_OUT);
    Board_init();
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    GPIO_writePin(POWER_EN,0U);
    CPUTimer_stopTimer(CPUTIMER1_BASE);
    CPUTimer_setPreScaler(CPUTIMER1_BASE,0U);
    CPUTimer_setPeriod(CPUTIMER1_BASE,0xffffffffUL);
    CPUTimer_reloadTimerCounter(CPUTIMER1_BASE);
    CPUTimer_startTimer(CPUTIMER1_BASE);
    BuckControl_ParameterInit();
    Viewer_Init();
    AppLcd_Init();
    AppKey_Init();
    BuckControl_Start();
    /* Main must continue servicing; a hung ISR or SPI timeout never enables PWM.
     * 512*8*256/10MHz ~=105ms. Debug halt must be preceded by manual STOP. */
    SysCtl_setWatchdogPredivider(SYSCTL_WD_PREDIV_512);
    SysCtl_setWatchdogPrescaler(SYSCTL_WD_PRESCALE_8);
    SysCtl_setWatchdogMode(SYSCTL_WD_MODE_RESET);
    SysCtl_serviceWatchdog();
    SysCtl_enableWatchdog();
    for(;;)
    {
        if(AppKey_PollEvent()) BuckControl_KeyPressed();
        Viewer_Poll();
        AppLcd_Update();
        SysCtl_serviceWatchdog();
    }
}
