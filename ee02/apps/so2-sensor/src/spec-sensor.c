/*
**   Copyright 2016 Telenor Digital AS
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**      http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
*/



#include <stdio.h>
#include "hal/hal_uart.h"
#include "mcu/nrf52_hal.h"
#include "console/console.h"
#include "lora.h"
#include "os/os.h"
#include "hal/hal_os_tick.h"
#include "spec-sensor.h"
#include <string.h>
#include "scheduling.h"

#define SENSOR_BUFFER_SIZE 512

int32_t sensor_ppb;
uint8_t sensor_temp;
uint8_t sensor_rh;

static char rxBuffer[SENSOR_BUFFER_SIZE];
static char txBuffer[SENSOR_BUFFER_SIZE];
static uint8_t buffer_pos = 0;

const uint32_t spec_sensor_time_interval = SENSOR_RUNNING_DELAY * OS_TICKS_PER_SEC;
static struct os_callout spec_sensor_callout;

char cmdChar;

static int charcount = 1;
static int TXChar() {
    if (charcount > 0) {
        console_printf("Transmitting : '%d'\n", cmdChar);
        charcount--;
        os_time_delay(OS_TICKS_PER_SEC);

        return cmdChar;    
    }
    return -1;
}

void DecodeBuffer(char * buffer) 
{
    console_printf("Buffer: %s\n", buffer);

    const char separator[2] = ",";
    char * token = strtok(buffer, separator);

    if (token == 0)
        return;
    token = strtok(NULL, separator);
    if (token == 0)
        return;
    sensor_ppb = atol(token);
//    if (ppb < 0)
//        ppb = 0;
    token = strtok(NULL, separator);
    if (token == 0)
        return;
    sensor_temp = atoi(token);
    token = strtok(NULL, separator);
    if (token == 0)
        return;
    sensor_rh = atoi(token);
}


static int rxData(void *arg, uint8_t data)
{
    rxBuffer[buffer_pos] = data;
    rxBuffer[buffer_pos+1] = 0;
    buffer_pos++;
    if (buffer_pos >= SENSOR_BUFFER_SIZE) {
        buffer_pos = 0;
        return -1;
    }

    if (data == '\n') {
        buffer_pos = 0;
        memcpy(txBuffer, rxBuffer, SENSOR_BUFFER_SIZE);
        DecodeBuffer(rxBuffer);
        memset(rxBuffer, 0, SENSOR_BUFFER_SIZE);

//        hal_uart_close(0);
    }
    return 0;
}

void InitUART()
{
    console_printf("InitUART\n");

    hal_uart_close(0);
    int rc;

    rc = hal_uart_init_cbs(0, TXChar, NULL, rxData, NULL);
    if (rc) {
        console_printf("Error: hal_uart_init_cbs returned %d\n", rc);
    }
  
    rc = hal_uart_config(0, 9600, 8, 1, HAL_UART_PARITY_NONE, HAL_UART_FLOW_CTL_NONE);
    if (rc) {
        console_printf("Error: hal_uart_config returned %d\n", rc);
    }
}

void reset_spec_sensor_callout() {
    console_printf("reset_spec_sensor_callout\n");
    int err = os_callout_reset(&spec_sensor_callout, spec_sensor_time_interval);
    if (err != OS_OK) {
        console_printf("spec_sensor os_callout_reset error: %d\n", err);
    }

    startSampleData();
    os_time_delay(OS_TICKS_PER_SEC);
    startSampleData();
    os_time_delay(OS_TICKS_PER_SEC);
    stopSampleData();
    
}


void startSampleData()
{
    InitUART();

    charcount = 1;
    console_printf("Sampling\n");
    cmdChar = '\r'; 
    hal_uart_start_tx(0);
}


void stopSampleData()
{
    InitUART();
    charcount = 1;
    console_printf("Stop sampling\n");
    os_time_delay(OS_TICKS_PER_SEC);
    cmdChar = 's';
    hal_uart_start_tx(0);
}





void spec_sensor_event_callback(struct os_event* event) {
    console_printf("TX BUFFER: %s\n", (char*)txBuffer);

    reset_spec_sensor_callout();
}

void init_spec_sensor() {
    console_printf("--- Spec sensor init ---\n");

    InitUART();

    os_callout_init(&spec_sensor_callout, os_eventq_dflt_get(), spec_sensor_event_callback, NULL);
    reset_spec_sensor_callout();
 }