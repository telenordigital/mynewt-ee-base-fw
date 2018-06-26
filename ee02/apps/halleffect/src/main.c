/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "i2c.h"
#include "hal/hal_i2c.h"
#include "lora.h"
#include "si7210.h"


#define I2CNUM 1

int
main(int argc, char **argv)
{
    int rc;

    sysinit();
    console_printf("Sysinit ok.\n");   

    console_printf("Initializing hall driver.\n");
    uint32_t err = SI7210_init();
    init_lora();
    if (err != SI7210_OK)
        console_printf("Driver initialization failed.\n");
    else
        console_printf("Driver initialization ok.\n");
    
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
        os_time_delay(OS_TICKS_PER_SEC);
    }
    assert(0);

    return rc;
}

