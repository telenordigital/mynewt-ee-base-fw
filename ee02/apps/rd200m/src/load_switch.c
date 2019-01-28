#include "load_switch.h"
#include "hal/hal_gpio.h"
#include "console/console.h"

void init_load_switch()
{
    console_printf("--------------- ENABLE/DISABLE power to RD200M -----------\n");
    hal_gpio_init_out(RDM_BOOST_ENABLE_PIN, 0);
    hal_gpio_init_out(HIGH_SIDE_SWITCH_ENABLE_PIN, 1);
    powerOff();
}

void powerOn()
{
    console_printf("Powering on...\n");
    hal_gpio_write(RDM_BOOST_ENABLE_PIN, 1);
    hal_gpio_write(HIGH_SIDE_SWITCH_ENABLE_PIN, 0);
}

void powerOff()
{
    console_printf("Powering off...\n");
    hal_gpio_write(RDM_BOOST_ENABLE_PIN, 0);
    hal_gpio_write(HIGH_SIDE_SWITCH_ENABLE_PIN, 1);
}
    