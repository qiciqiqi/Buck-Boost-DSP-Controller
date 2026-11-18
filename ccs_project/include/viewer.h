#ifndef VIEWER_H
#define VIEWER_H
#include "platform.h"
extern volatile uint32_t viewer_heartbeat, viewer_rx_bytes, viewer_tx_bytes, viewer_errors;
void Viewer_Init(void);
void Viewer_Poll(void);
#endif
