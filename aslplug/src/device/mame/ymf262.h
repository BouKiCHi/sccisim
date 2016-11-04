#pragma once

#ifndef __YMF262_H__
#define __YMF262_H__

/* select number of output bits: 8 or 16 */
#define OPL3_SAMPLE_BITS 16

#include "mametype.h"

/*
#if (OPL3_SAMPLE_BITS==16)
typedef INT16 OPL3SAMPLE;
#endif
#if (OPL3_SAMPLE_BITS==8)
typedef INT8 OPL3SAMPLE;
#endif
*/

typedef void (*OPL3_TIMERHANDLER)(void *param,int timer);
typedef void (*OPL3_IRQHANDLER)(void *param,int irq);
typedef void (*OPL3_UPDATEHANDLER)(void *param,int min_interval_us);


void *ymf262_init(device_t *device, int clock, int rate);
void ymf262_shutdown(void *chip);
void ymf262_reset_chip(void *chip);
int  ymf262_write(void *chip, int a, int v);
unsigned char ymf262_read(void *chip, int a);
int  ymf262_timer_over(void *chip, int c);
void ymf262_update_one(void *chip, OPL3SAMPLE **buffers, int length);

void ymf262_set_timer_handler(void *chip, OPL3_TIMERHANDLER TimerHandler, void *param);
void ymf262_set_irq_handler(void *chip, OPL3_IRQHANDLER IRQHandler, void *param);
void ymf262_set_update_handler(void *chip, OPL3_UPDATEHANDLER UpdateHandler, void *param);


#endif /* __YMF262_H__ */
