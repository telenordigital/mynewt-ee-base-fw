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

#include "i2c.h"

#include "console/console.h"
#include "hal/hal_i2c.h"
#include "mcu/nrf52_hal.h"
#include "os/os.h"

int init_i2c(uint8_t i2c_num) {
    struct nrf52_hal_i2c_cfg config = {
        .scl_pin = 12,
        .sda_pin = 17,
        .i2c_frequency = 100,
    };
    int err = hal_i2c_init(i2c_num, &config);
    if (err != OS_OK) {
        console_printf("hal_i2c_init error: %d\n", err);
    }
    return err;
}