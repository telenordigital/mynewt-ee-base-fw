#include <assert.h>
#include <os/os.h>
/* ADC */
#include "battery_status_task.h"
#include "nrf.h"
#include "app_util_platform.h"
#include "app_error.h"
#include <adc_nrf52/adc_nrf52.h>
#include "nrf_drv_saadc.h"
#include "console/console.h"
#include "bsp.h"
#include "scheduling.h"


#define ADC_NUMBER_SAMPLES (2)
#define ADC_NUMBER_CHANNELS (1)


const uint32_t adc_time_interval = ADC_TASK_DELAY * OS_TICKS_PER_SEC;
static struct os_callout adc_callout;
struct adc_dev *adc;

/* ADC Task settings */
//#define ADC_TASK_PRIO           5
//#define ADC_STACK_SIZE          (OS_STACK_ALIGN(336))
//struct os_eventq adc_evq;
//struct os_task adc_task;
//bssnz_t os_stack_t adc_stack[ADC_STACK_SIZE];

nrf_drv_saadc_config_t adc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;

struct adc_dev *adc;
uint8_t *sample_buffer1;
uint8_t *sample_buffer2;

int battery_voltage_mv = 0;

static struct adc_dev os_bsp_adc0;
static nrf_drv_saadc_config_t os_bsp_adc0_config = {
    .resolution         = MYNEWT_VAL(ADC_0_RESOLUTION),
    .oversample         = MYNEWT_VAL(ADC_0_OVERSAMPLE),
    .interrupt_priority = MYNEWT_VAL(ADC_0_INTERRUPT_PRIORITY),
};

int adc_read(void *buffer, int buffer_len)
{
    //console_printf("ADC Read...\n");

    int i;
    int adc_result;
    int rc;
    int voltage_mv;
    for (i = 0; i < ADC_NUMBER_SAMPLES; i++) {
        rc = adc_buf_read(adc, buffer, buffer_len, i, &adc_result);
        if (rc != 0) {
            goto err;
        }
        voltage_mv = adc_result_mv(adc, 0, adc_result);
        //console_printf("Sample: %d\n", i);
    }        
    adc_buf_release(adc, buffer, buffer_len);
    return voltage_mv;
err:
    console_printf("ADC error\n");
    return (rc);
}

int
adc_read_event(struct adc_dev *dev, void *arg, uint8_t etype, void *buffer, int buffer_len)
{
    battery_voltage_mv = 2 * adc_read(buffer, buffer_len); 
    if (battery_voltage_mv >= 0) {
        console_printf("Voltage %d mV\n", battery_voltage_mv);
    } else {
        console_printf("Error while reading: %d\n", battery_voltage_mv);
    }
    return (0);
}

void * adc_init(void)
{
    console_printf("ADC init...\n");
    int rc = 0;

    rc = os_dev_create((struct os_dev *) &os_bsp_adc0, "adc0",
            OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
            nrf52_adc_dev_init, &os_bsp_adc0_config);
    assert(rc == 0);
    nrf_saadc_channel_config_t cc = NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN1);
    cc.gain = NRF_SAADC_GAIN1_6;
    cc.reference = NRF_SAADC_REFERENCE_INTERNAL;
    adc = (struct adc_dev *) os_dev_open("adc0", 0, &adc_config);
    assert(adc != NULL);
    adc_chan_config(adc, 0, &cc);
    sample_buffer1 = malloc(adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    sample_buffer2 = malloc(adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    memset(sample_buffer1, 0, adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    memset(sample_buffer2, 0, adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    adc_buf_set(adc, sample_buffer1, sample_buffer2,
    adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));

    return adc;
}

static void reset_adc_callout() {
    int err = os_callout_reset(&adc_callout, adc_time_interval);
    if (err != OS_OK) {
        console_printf("adc0 os_callout_reset error: %d\n", err);
    }
    
}


static void adc_event_callback(struct os_event* event) 
{
    adc_sample(adc);

    reset_adc_callout();
}

void init_adc_task() {
    console_printf("--- ADC init ---\n");

    adc = adc_init();
    int rc;
    rc = adc_event_handler_set(adc, adc_read_event, (void *) NULL);
    assert(rc == 0);
    os_callout_init(&adc_callout, os_eventq_dflt_get(), adc_event_callback, NULL);
    reset_adc_callout();
 }



