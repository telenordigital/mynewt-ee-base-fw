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

#define SENSOR_BUFFER_SIZE 512

uint8_t lora_payload[SENSOR_PAYLOAD_SIZE];
uint8_t payloadIndex = 0;

static char rxBuffer[SENSOR_BUFFER_SIZE];
static char txBuffer[SENSOR_BUFFER_SIZE];
static uint8_t buffer_pos = 0;

const uint32_t spec_sensor_time_interval = 60 * OS_TICKS_PER_SEC;
static struct os_callout spec_sensor_callout;

static int charcount = 1;
static int TXChar() {
    if (charcount > 0) {
        charcount--;
        return '\r';    // Don't ask. This turned out to be the elusive "any character" key
                        // that was necessary to trigger a single measurement from the spec-sensor
        os_time_delay(OS_TICKS_PER_SEC/2);
    }
    return -1;
}

static void AddSampleToPayload(char * buffer) 
{
//    console_printf("Buffer: %s\n", buffer);

    console_printf("Payload index: %d\n", payloadIndex);
    const char separator[2] = ",";
    char * token = strtok(buffer, separator);

    int16_t ppb;
    uint8_t temp;
    uint8_t rh;

    if (token == 0)
        return;
    token = strtok(NULL, separator);
    if (token == 0)
        return;
    ppb = atoi(token);
    if (ppb < 0)
        ppb = 0;
    token = strtok(NULL, separator);
    if (token == 0)
        return;
    temp = atoi(token);
    token = strtok(NULL, separator);
    if (token == 0)
        return;
    rh = atoi(token);

    if (payloadIndex < SENSOR_PAYLOAD_SIZE-4) {
        // payloadIndex is reset by lora Tx
        lora_payload[payloadIndex++] = ppb & 0xFF00 >> 8;
        lora_payload[payloadIndex++] = (uint8_t)(ppb & 0xFF00);
        lora_payload[payloadIndex++] = temp;
        lora_payload[payloadIndex++] = rh;
    }

    console_printf("Sample: PPB:%d, Temp:%d, RH: %d\n", ppb, temp, rh);
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

        AddSampleToPayload(rxBuffer);
        memset(rxBuffer, 0, SENSOR_BUFFER_SIZE);

        hal_uart_close(0);
    }
    return 0;
}

static void InitUART()
{
    console_printf("InitUART\n");
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

static void reset_spec_sensor_callout() {
    console_printf("reset_spec_sensor_callout\n");
    int err = os_callout_reset(&spec_sensor_callout, spec_sensor_time_interval);
    if (err != OS_OK) {
        console_printf("spec_sensor os_callout_reset error: %d\n", err);
    }

    InitUART();
    charcount = 1;
    
    hal_uart_start_tx(0);
}

static void spec_sensor_event_callback(struct os_event* event) {
    console_printf("TX BUFFER: %s\n", (char*)txBuffer);

    reset_spec_sensor_callout();
}

void init_spec_sensor() {
    console_printf("--- Spec sensor init ---\n");

    os_callout_init(&spec_sensor_callout, os_eventq_dflt_get(), spec_sensor_event_callback, NULL);
    reset_spec_sensor_callout();
 }