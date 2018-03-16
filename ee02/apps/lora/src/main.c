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
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#include "lora.h"

char lora_payload[12] = "Hello there!";

/**
 * main
 *
 * The main task for the project. This function initializes packages,
 * and starts the event loop.
 *
 * @return int NOTE: this function should never return!
 *
 * Device OTAA parameters are defined in "provisioning.h"
 * 
 */

int main(int argc, char **argv)
{
#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();
    console_printf("Transmitting a LoRa packet every 5 minutes...\n");
    init_lora();
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
}

