#include <assert.h>
#include <os/os.h>
/* ADC */
#include "adc.h"
#include "nrf.h"
#include "app_util_platform.h"
#include "app_error.h"
#include <adc.h>
#include <adc_nrf52/adc_nrf52.h>
#include "nrf_drv_saadc.h"
#include "console/console.h"
#include "bsp.h"


#define ADC_NUMBER_SAMPLES (2)
#define ADC_NUMBER_CHANNELS (1)

/* ADC Task settings */
#define ADC_TASK_PRIO           5
#define ADC_STACK_SIZE          (OS_STACK_ALIGN(336))
struct os_eventq adc_evq;
struct os_task adc_task;
bssnz_t os_stack_t adc_stack[ADC_STACK_SIZE];

nrf_drv_saadc_config_t adc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;

struct adc_dev *adc;
uint8_t *sample_buffer1;
uint8_t *sample_buffer2;

static struct adc_dev os_bsp_adc0;
static nrf_drv_saadc_config_t os_bsp_adc0_config = {
    .resolution         = MYNEWT_VAL(ADC_0_RESOLUTION),
    .oversample         = MYNEWT_VAL(ADC_0_OVERSAMPLE),
    .interrupt_priority = MYNEWT_VAL(ADC_0_INTERRUPT_PRIORITY),
};


int
adc_read_event(struct adc_dev *dev, void *arg, uint8_t etype, void *buffer, int buffer_len)
{
    int value;
    value = adc_read(buffer, buffer_len);
    if (value >= 0) {
        console_printf("Got %d\n", value);
    } else {
        console_printf("Error while reading: %d\n", value);
    }
    return (0);
}

static void adc_task_handler(void *unused)
{
    console_printf("ADC task...\n");
    struct adc_dev *adc;
    int rc;
    /* ADC init */
    adc = adc_init();
    rc = adc_event_handler_set(adc, adc_read_event, (void *) NULL);
    assert(rc == 0);
    while (1) {
        adc_sample(adc);
        /* Wait 2 second */
       os_time_delay(OS_TICKS_PER_SEC * 2);
    }
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

void adc_task_init()
{
           /* Initialize adc sensor task eventq */
    os_eventq_init(&adc_evq);

    /* Create the ADC reader task.  
     * All sensor operations are performed in this task.
     */
    os_task_init(&adc_task, "sensor", adc_task_handler,
            NULL, ADC_TASK_PRIO, OS_WAIT_FOREVER,
            adc_stack, ADC_STACK_SIZE);
}


int
adc_read(void *buffer, int buffer_len)
{
    console_printf("ADC Read...\n");

    int i;
    int adc_result;
    int my_result_mv = 0;
    int rc;
    for (i = 0; i < ADC_NUMBER_SAMPLES; i++) {
        rc = adc_buf_read(adc, buffer, buffer_len, i, &adc_result);
        if (rc != 0) {
            goto err;
        }
        my_result_mv = adc_result_mv(adc, 0, adc_result);
        console_printf("Sample: %d\n", i);
    }        
    adc_buf_release(adc, buffer, buffer_len);
    return my_result_mv;
err:
    console_printf("ADC error\n");
    return (rc);
}



