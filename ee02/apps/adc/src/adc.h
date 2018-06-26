#ifndef _ADC_H_
#define _ADC_H_

#include "os/os.h"


void adc_task_init();
void * adc_init(void);
int adc_read(void *buffer, int buffer_len);

#endif