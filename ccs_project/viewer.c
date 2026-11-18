#include "viewer.h"
#include "app_control.h"
#include "SerialDriver/serialDriver.h"
extern ____SerialDriver ____serialDriver;
volatile uint32_t viewer_heartbeat, viewer_rx_bytes, viewer_tx_bytes, viewer_errors;
static volatile uint16_t rx[256], rxHead, rxTail;
static uint16_t tx[256], txHead, txTail;
static uint32_t lastByte;

int viewerMemoryAllowed(uint32_t addr, uint16_t bytes, uint16_t write)
{
    uint32_t end;
    if(bytes!=2U && bytes!=4U) return 0;
    if(bytes==4U && (addr&1U))return 0;
    end=addr+bytes/2U;
    if(end<addr) return 0;
    if(!write)
        return (addr>=0x128UL && end<=0x7f8UL) ||
               (addr>=0x8000UL && end<=0x18000UL);
#define SCALAR(v) if(addr==(uint32_t)&v && bytes==sizeof(v)*2U) return 1
    SCALAR(power_mode); SCALAR(control_mode);
    SCALAR(current_reference); SCALAR(output_voltage_target); SCALAR(openloop_command);
    SCALAR(current_limit); SCALAR(pwm_limit);
#undef SCALAR
#define GROUP(v) if(addr>=(uint32_t)&v && end<=(uint32_t)&v+sizeof(v) && bytes==4U && ((addr-(uint32_t)&v)%2U)==0U) return 1
    GROUP(buck_current_settings); GROUP(buck_dual_settings);
    GROUP(boost_current_settings); GROUP(boost_dual_settings); GROUP(buck_voltage_settings);
#undef GROUP
    return 0;
}
void viewerMemoryRead(uint32_t addr, uint16_t *out, uint16_t bytes)
{
    uint32_t value;
    uint16_t i;
    bool masked=Interrupt_disableGlobal();
    value=(bytes==4U)?*(volatile uint32_t*)addr:*(volatile uint16_t*)addr;
    if(!masked)Interrupt_enableGlobal();
    for(i=0U;i<bytes;i++){out[i]=(uint16_t)(value&0xffU);value>>=8;}
}
void viewerMemoryWrite(uint32_t addr, uint32_t value, uint16_t bytes)
{
    bool masked=Interrupt_disableGlobal();
    if(bytes==4U) *(volatile uint32_t*)addr=value;
    else *(volatile uint16_t*)addr=(uint16_t)value;
    if(!masked) Interrupt_enableGlobal();
}
void serialDriverSendData(uint16_t *buf,uint16_t size)
{
    uint16_t i;
    if(size>((txTail-txHead-1U)&255U)) {viewer_errors++;return;}
    for(i=0;i<size;i++){tx[txHead]=buf[i]&0xffU;txHead=(txHead+1U)&255U;}
}
__interrupt void ViewerRxISR(void)
{
    uint16_t b,next;
    if(SCI_getOverflowStatus(VIEWER_SCI_BASE) || (SCI_getRxStatus(VIEWER_SCI_BASE)&SCI_RXSTATUS_ERROR))
    {
        viewer_errors++;
        SCI_resetRxFIFO(VIEWER_SCI_BASE);
        SCI_clearOverflowStatus(VIEWER_SCI_BASE);
        SCI_performSoftwareReset(VIEWER_SCI_BASE);
    }
    while(SCI_getRxFIFOStatus(VIEWER_SCI_BASE)!=SCI_FIFO_RX0)
    {
        b=SCI_readCharNonBlocking(VIEWER_SCI_BASE)&0xffU;
        viewer_rx_bytes++;
        next=(rxHead+1U)&255U;
        if(next!=rxTail){rx[rxHead]=b;rxHead=next;}else viewer_errors++;
    }
    /* Clearing SCI_INT_RXERR performs a software reset in TI DriverLib.
     * Do that only in the error branch above, never between valid bytes. */
    SCI_clearInterruptStatus(VIEWER_SCI_BASE,SCI_INT_RXFF);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}
void Viewer_Init(void)
{
    SCI_resetRxFIFO(VIEWER_SCI_BASE);
    SCI_resetTxFIFO(VIEWER_SCI_BASE);
    SCI_clearOverflowStatus(VIEWER_SCI_BASE);
    SCI_clearInterruptStatus(VIEWER_SCI_BASE,SCI_INT_RXFF|SCI_INT_RXERR);
    SCI_setFIFOInterruptLevel(VIEWER_SCI_BASE,SCI_FIFO_TX0,SCI_FIFO_RX1);
    Interrupt_register(INT_SCIB_RX,&ViewerRxISR);
    SCI_enableInterrupt(VIEWER_SCI_BASE,SCI_INT_RXFF|SCI_INT_RXERR);
    Interrupt_enable(INT_SCIB_RX);
}
void Viewer_Poll(void)
{
    uint16_t budget=32U,b;
    if(Platform_Millis()-lastByte>200U)
        ____serialDriver.state=____SERIAL_DRIVER_STATE_WAIT_SOF1;
    while(rxTail!=rxHead && budget--)
    {
        b=rx[rxTail];rxTail=(rxTail+1U)&255U;
        serialDriverReceiveByte(b);lastByte=Platform_Millis();
    }
    while(txTail!=txHead && SCI_getTxFIFOStatus(VIEWER_SCI_BASE)!=SCI_FIFO_TX16)
    {
        SCI_writeCharNonBlocking(VIEWER_SCI_BASE,tx[txTail]);
        txTail=(txTail+1U)&255U;viewer_tx_bytes++;
    }
}
