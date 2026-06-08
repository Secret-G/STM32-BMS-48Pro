#ifndef __IO_CTRL_H
#define __IO_CTRL_H

#include "sys.h"

void IO_CTRL_Init(void);
void BQ_WAKE_High(void);
void BQ_WAKE_Low(void);
void BQ_WAKE_Pulse(void);

#endif

