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
#include "os/os.h"
#include "hal/hal_os_tick.h"
#include "rd200m_task.h"
#include <string.h>
#include "payload.h"
#include "load_switch.h"
#include <hal/hal_watchdog.h>
#include "scheduling.h"

#define SENSOR_BUFFER_SIZE 512

uint8_t rdm200_status;
uint8_t rdm200_minutes;
uint8_t rdm200_integer;
uint8_t rdm200_decimal;
uint8_t rdm200_checksum;


static char rxBuffer[SENSOR_BUFFER_SIZE];
//static char txBuffer[SENSOR_BUFFER_SIZE];
static uint8_t rxPos = 0;

const uint32_t rd200m_sensor_time_interval = RADON_SENSOR_RUNNING_DELAY * OS_TICKS_PER_SEC;
static struct os_callout rd200m_sensor_callout;

static uint8_t REQUEST[4] = { 0x02, 0x01, 0x00, 0xFE};
static uint8_t RESET[4] = { 0x02, 0xA0, 0x00, 0xFF-0xA0 };
static uint8_t PERIOD[4] = { 0x02, 0xA1, 0x01, 0xFF-0xA1-0x01 };

static uint8_t * cmdPtr  = RESET;

static int txByte = 0;
static int TXChar() {
    if (txByte < 4) {
        int retVal = *cmdPtr; 
        console_printf("%02X ", retVal);
        txByte++;
        cmdPtr++;
        return retVal;    
    }
    console_printf("\n");
    cmdPtr = 0;
    return -1;
}

void resetRDM()
{
    cmdPtr = RESET;
    txByte = 0;
    rxPos = 0;
    hal_uart_start_tx(0);
    os_time_delay(OS_TICKS_PER_SEC*2);
}

void requestDataRDM()
{
    cmdPtr = REQUEST;
    txByte = 0;
    rxPos = 0;
    hal_uart_start_tx(0);
}

void setDataTransferPeriodRDM()
{
    cmdPtr = PERIOD;
    txByte = 0;
    rxPos = 0;
    hal_uart_start_tx(0);
}

static int rxData(void *arg, uint8_t data)
{
    //console_printf("\n");
    rxBuffer[rxPos] = data;
    rxPos++;

    //console_printf("RX: %02X (Rxpos:%d)\n", data, rxPos);
    if (rxPos > 7)
    {
        console_printf("Received frame.\n");
        if (rxBuffer[0] != 0x02)
            return -1;
        if (rxBuffer[1] != 0x10)
            return -1;
        if (rxBuffer[2] != 0x04)
            return -1;
        rdm200_status = rxBuffer[3];
        if (rxBuffer[3] == 0xE0) {
            console_printf("HANDS OFF!\n");
            return -1;
        }
        rdm200_minutes = rxBuffer[4];
        rdm200_integer = rxBuffer[5];
        rdm200_decimal = rxBuffer[6];
        rdm200_checksum = rxBuffer[7];

        if (rdm200_checksum != 0xFF- (0x10+0x04+rdm200_status + rdm200_minutes + rdm200_integer + rdm200_decimal))
        {
            console_printf("Incorrect checksum. Discarding frame.\n");
            return -1;
        }

        console_printf("Status: %d, Minutes: %d, Value: %d.%d\n", rdm200_status, rdm200_minutes, rdm200_integer, rdm200_decimal);
        rxPos = 0;
    }
    if (rxPos >= SENSOR_BUFFER_SIZE) {
        rxPos = 0;
        return -1;
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
  
    rc = hal_uart_config(0, 19200, 8, 1, HAL_UART_PARITY_NONE, HAL_UART_FLOW_CTL_NONE);
    if (rc) {
        console_printf("Error: hal_uart_config returned %d\n", rc);
    }
}

static void reset_rd200m_sensor_callout() {
    console_printf("Reset callout...\n");

    int err = os_callout_reset(&rd200m_sensor_callout, rd200m_sensor_time_interval);
    if (err != OS_OK) {
        console_printf("rd200m_sensor os_callout_reset error: %d\n", err);
    }
    
}


static void rd200m_sensor_event_callback(struct os_event* event) 
{
    console_printf("rd200m_sensor_event_callback.\n");
   
    console_printf("Powering on...\n");
    powerOn();
    os_time_delay(OS_TICKS_PER_SEC*10);
    setDataTransferPeriodRDM();
    os_time_delay(OS_TICKS_PER_SEC*1);
    resetRDM();
    os_time_delay(OS_TICKS_PER_SEC*1);
    for (int i=0; i<60; i++) {
        for (int j=0; j<10; j++) {
            os_time_delay(OS_TICKS_PER_SEC*6);
            requestDataRDM();    
            hal_watchdog_tickle();
        }
    }
    console_printf("Powering off...\n");
    powerOff();

    reset_rd200m_sensor_callout();
}

void init_rd200m_sensor_task() {
    console_printf("--- RD200M sensor init ---\n");

    InitUART();

    // setDataTransferPeriodRDM();
    // resetRDM();

    os_callout_init(&rd200m_sensor_callout, os_eventq_dflt_get(), rd200m_sensor_event_callback, NULL);
    reset_rd200m_sensor_callout();
 }