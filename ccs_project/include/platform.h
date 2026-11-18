#ifndef DCDC_PLATFORM_H
#define DCDC_PLATFORM_H
#include "byte_types.h"
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"
#include "board.h"
uint32_t Platform_Millis(void);
void Platform_DelayMs(uint16_t ms);

void PowerTiming_Start(void);
void AppKey_Init(void);
uint8_t AppKey_PollEvent(void);
extern volatile uint32_t control_isr_max_cycles;
#endif
