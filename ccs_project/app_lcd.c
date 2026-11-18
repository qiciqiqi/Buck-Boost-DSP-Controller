/* SSD1306 128x64, I2CB, 7-bit 0x3C/0x3D. API name kept for main compatibility. */
#include "app_lcd.h"
#include "app_control.h"
#include "lcd_fonts.h"
#include "viewer.h"
#include <stdio.h>
#include <string.h>
volatile uint16_t oled_online;
/* Last failed I2C transfer: high word=1 bus, 2 data, 3 stop; low word=status. */
volatile uint32_t oled_last_error;
static uint16_t address=0x3cU, pixels[1024];
static uint32_t lastPoll;

static bool TimedOut(uint32_t start)
{
    Viewer_Poll();
    return (uint32_t)(start-CPUTimer_getTimerCount(CPUTIMER1_BASE))>DEVICE_SYSCLK_FREQ/50U;
}
static bool Transfer(uint16_t control,const uint16_t *data,uint16_t count)
{
    uint32_t start=CPUTimer_getTimerCount(CPUTIMER1_BASE);
    uint16_t i,phase=1U;
    SysCtl_serviceWatchdog();Viewer_Poll();
    while(I2C_isBusBusy(OLED_I2C_BASE) || I2C_getStopConditionStatus(OLED_I2C_BASE))
        if(TimedOut(start)) goto fail;
    I2C_clearStatus(OLED_I2C_BASE,I2C_STS_NO_ACK|I2C_STS_STOP_CONDITION);
    I2C_setTargetAddress(OLED_I2C_BASE,address);
    I2C_setConfig(OLED_I2C_BASE,I2C_CONTROLLER_SEND_MODE);
    I2C_setDataCount(OLED_I2C_BASE,count+1U);
    I2C_disableFIFO(OLED_I2C_BASE);
    I2C_enableFIFO(OLED_I2C_BASE);
    I2C_putData(OLED_I2C_BASE,control);
    for(i=0U;i<count && i<15U;i++) I2C_putData(OLED_I2C_BASE,data[i]&0xffU);
    I2C_sendStartCondition(OLED_I2C_BASE);
    phase=2U;
    for(;i<count;i++)
    {
        while(I2C_getTxFIFOStatus(OLED_I2C_BASE)==I2C_FIFO_TX16)
            if((I2C_getStatus(OLED_I2C_BASE)&I2C_STS_NO_ACK)||TimedOut(start)) goto fail;
        if(I2C_getStatus(OLED_I2C_BASE)&I2C_STS_NO_ACK) goto fail;
        I2C_putData(OLED_I2C_BASE,data[i]&0xffU);
    }
    phase=3U;
    I2C_sendStopCondition(OLED_I2C_BASE);
    while(I2C_getStopConditionStatus(OLED_I2C_BASE))
        if((I2C_getStatus(OLED_I2C_BASE)&I2C_STS_NO_ACK)||TimedOut(start)) goto fail;
    return !(I2C_getStatus(OLED_I2C_BASE)&I2C_STS_NO_ACK);
fail:
    oled_last_error=((uint32_t)phase<<16)|I2C_getStatus(OLED_I2C_BASE);
    I2C_disableModule(OLED_I2C_BASE);
    /* IRS alone preserves MDR.STP/STT after an aborted transaction. */
    HWREGH(OLED_I2C_BASE+I2C_O_MDR)&=~(I2C_MDR_STP|I2C_MDR_STT);
    I2C_clearStatus(OLED_I2C_BASE,I2C_STS_NO_ACK|I2C_STS_STOP_CONDITION|I2C_STS_ARB_LOST);
    DEVICE_DELAY_US(10);I2C_enableModule(OLED_I2C_BASE);
    return false;
}
static bool Setup(void)
{
    static const uint16_t init[]={0xae,0xd5,0x80,0xa8,0x3f,0xd3,0x00,0x40,
        0x8d,0x14,0x20,0x02,0xa1,0xc8,0xda,0x12,0x81,0x7f,0xd9,0xf1,
        0xdb,0x40,0xa4,0xa6,0x2e,0xaf};
    address=0x3cU;
    if(Transfer(0U,init,sizeof(init)/sizeof(init[0]))) return true;
    address=0x3dU;
    return Transfer(0U,init,sizeof(init)/sizeof(init[0]));
}
static void Text(uint16_t row,const char *s)
{
    uint16_t col=0U,ch,x,y,byte;
    const uint8_t *glyph;
    while(*s && col<16U)
    {
        ch=(uint16_t)*s++;if(ch<32U||ch>126U)ch='?';
        glyph=LCD_Font16.data+(ch-32U)*16U;
        for(x=0U;x<8U;x++)
        {
            byte=0U;for(y=0U;y<8U;y++)if(glyph[y]&(1U<<x))byte|=1U<<y;
            if(col*8U+x+2U<128U) pixels[row*256U+col*8U+x+2U]=byte;
            byte=0U;for(y=0U;y<8U;y++)if(glyph[y+8U]&(1U<<x))byte|=1U<<y;
            if(col*8U+x+2U<128U) pixels[row*256U+128U+col*8U+x+2U]=byte;
        }
        col++;
    }
}
static void Value(char *line,const char *prefix,float value,char unit)
{
    union{float f;uint32_t u;} n;
    int32_t q;uint32_t magnitude;n.f=value;
    if((n.u&0x7f800000UL)==0x7f800000UL || value>99.99f || value< -99.99f)
    {snprintf(line,17,"%s --.-- %c",prefix,unit);return;}
    q=(int32_t)(value*100.0f+((value<0)?-0.5f:0.5f));magnitude=(q<0)?-q:q;
    snprintf(line,17,"%s%c%2lu.%02lu%c",prefix,q<0?'-':' ',(unsigned long)(magnitude/100U),(unsigned long)(magnitude%100U),unit);
}
void AppLcd_Init(void)
{
    Platform_DelayMs(100U);
    oled_online=Setup();lastPoll=Platform_Millis()-200U;
}
void AppLcd_Update(void)
{
    uint16_t page,part,cmd[3];char line[17];float target;char unit;
    uint32_t now=Platform_Millis();
    if(now-lastPoll<(oled_online?200U:1000U))return;lastPoll=now;
    if(!oled_online){oled_online=Setup();if(!oled_online)return;}
    memset(pixels,0,sizeof(pixels));
    snprintf(line,17,"%s %s",active_power_mode==MODE_INACTIVE?"WAIT":(active_power_mode==POWER_BOOST?"BOOST":"BUCK"),
        active_control_mode==CONTROL_CURRENT?"CURRENT":active_control_mode==CONTROL_DUAL?"DUAL":active_control_mode==CONTROL_OPEN?"OPEN":"VOLTAGE");
    Text(0U,line);
    target=active_control_mode==CONTROL_OPEN?openloop_command:active_control_mode==CONTROL_CURRENT?current_reference:output_voltage_target;
    unit=active_control_mode==CONTROL_OPEN?'%':active_control_mode==CONTROL_CURRENT?'A':'V';
    Value(line,"SET",target,unit);Text(1U,line);
    Value(line,"I",feedback_current_avg,'A');Text(2U,line);
    snprintf(line,17,"RX:%lu %s",(unsigned long)(viewer_rx_bytes%100000UL),control_fault_latched?"FAULT":power_output_enabled?"RUN":"STOP");Text(3U,line);
    for(page=0U;page<8U;page++)
    {
        cmd[0]=0xb0U+page;cmd[1]=0U;cmd[2]=0x10U;
        if(!Transfer(0U,cmd,3U)){oled_online=0U;return;}
        for(part=0U;part<8U;part++)
        {
            if(AppKey_PollEvent())BuckControl_KeyPressed();
            if(!Transfer(0x40U,&pixels[page*128U+part*16U],16U)){oled_online=0U;return;}
        }
    }
}
