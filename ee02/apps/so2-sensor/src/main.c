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

#include <assert.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_uart.h"
#include <os/os_cputime.h>
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif
#include "lora.h"
#include "spec-sensor.h"
#include "hal/hal_os_tick.h"
#include "console/console.h"


int main(int argc, char **argv)
{
    int rc;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();

    console_printf("SO2 application\n");

    init_lora();
    init_spec_sensor();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
        os_time_delay(OS_TICKS_PER_SEC*5);
    }
    assert(0);

    return rc;
}
