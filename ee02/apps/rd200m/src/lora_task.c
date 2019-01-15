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

#include "lora_task.h"

#include <assert.h>

#include "node/lora.h"
#include "provisioning.h"
#include "console/console.h"
#include "node/mac/LoRaMacTest.h"
#include "os/os.h"
#include "payload.h"
#include "scheduling.h"

typedef enum {
    DISCONNECTED = 0,
    CONNECTED,
    SENDING,
} state_t;

static state_t state = DISCONNECTED;

static uint8_t DevEui[] = LORAWAN_DEVICE_EUI;
static uint8_t AppEui[] = LORAWAN_APP_EUI;
static uint8_t AppKey[] = LORAWAN_APP_KEY;

uint32_t transmit_counter = 0;
uint32_t lora_time_interval = LORA_STARTUP_DELAY * OS_TICKS_PER_SEC;
uint32_t SET_UP_DIAGNOSTIC_TRANSMISSIONS = 10;

static struct os_callout lora_callout;

static void reset_lora_callout() {
    int err = os_callout_reset(&lora_callout, lora_time_interval);
    if (err != OS_OK) {
        console_printf("lora os_callout_reset error: %d\n", err);
    }
}

extern uint8_t lora_payload[SENSOR_PAYLOAD_SIZE];
extern uint8_t payloadIndex;
extern uint8_t rdm200_status;
extern uint8_t rdm200_minutes;
extern uint8_t rdm200_integer;
extern uint8_t rdm200_decimal;
extern uint8_t rdm200_checksum;
extern int battery_voltage_mv;

static void lora_event_callback(struct os_event* event) {
    console_printf("Sending payload\n");

    lora_payload[payloadIndex++] = rdm200_status;
    lora_payload[payloadIndex++] = rdm200_minutes;
    lora_payload[payloadIndex++] = rdm200_integer;
    lora_payload[payloadIndex++] = rdm200_decimal;
    lora_payload[payloadIndex++] = rdm200_checksum;
    lora_payload[payloadIndex++] = (battery_voltage_mv & 0xFF000000) >> 24;
    lora_payload[payloadIndex++] = (battery_voltage_mv & 0x00FF0000) >> 16;
    lora_payload[payloadIndex++] = (battery_voltage_mv & 0x0000FF00) >> 8;
    lora_payload[payloadIndex++] = (battery_voltage_mv & 0x000000FF);
    
    console_printf("PACKING PAYLOAD: Status: %d, Minutes: %d, Value: %d.%d. Battery: %d\n", rdm200_status, rdm200_minutes, rdm200_integer, rdm200_decimal, battery_voltage_mv);
    console_printf("ENCODED PAYLOAD: %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", lora_payload[0], lora_payload[1], lora_payload[2], lora_payload[3], lora_payload[4], lora_payload[5], lora_payload[6], lora_payload[7], lora_payload[8]);

    lora_send(&lora_payload[0], payloadIndex);
    payloadIndex = 0;

    // To make life easier for us when deploying, we'll transmit a bit more frequently just after poweron
    // After a while, we will fall back to RADON_SENSOR_RUNNING_DELAY * OS_TICKS_PER_SEC seconds between transmissions
    transmit_counter++; // Nope. This will not wrap around before the battery runs out.
    if (transmit_counter > SET_UP_DIAGNOSTIC_TRANSMISSIONS)
    {
        lora_time_interval = LORA_RUNNING_DELAY * OS_TICKS_PER_SEC; 
    }

    reset_lora_callout();
}


static void transmit_done(uint8_t port, LoRaMacEventInfoStatus_t status, Mcps_t pkt_type, struct os_mbuf* om) {
    assert(om != NULL);
    console_printf("Transmitted on port %u type=%s status=%d len=%u\n",
        port, pkt_type == MCPS_CONFIRMED ? "conf" : "unconf",
        status, OS_MBUF_PKTLEN(om));

    struct lora_pkt_info* lpkt = LORA_PKT_INFO_PTR(om);
    console_printf("\tdr:%u\n", lpkt->txdinfo.datarate);
    console_printf("\ttxpower:%d\n", lpkt->txdinfo.txpower);
    console_printf("\ttries:%u\n", lpkt->txdinfo.retries);
    console_printf("\tack_rxd:%u\n", lpkt->txdinfo.ack_rxd);
    console_printf("\ttx_time_on_air:%lu\n", lpkt->txdinfo.tx_time_on_air);
    console_printf("\tuplink_cntr:%lu\n", lpkt->txdinfo.uplink_cntr);
    console_printf("\tuplink_freq:%lu\n", lpkt->txdinfo.uplink_freq);

    int rc = os_mbuf_free_chain(om);
    assert(rc == 0);

    state = CONNECTED;

}

static void receive_done(uint8_t port, LoRaMacEventInfoStatus_t status, Mcps_t pkt_type, struct os_mbuf* om) {
    assert(om != NULL);
    console_printf("Received on port %u type=%s status=%d len=%u\n",
        port, pkt_type == MCPS_CONFIRMED ? "conf" : "unconf",
        status, OS_MBUF_PKTLEN(om));

    struct lora_pkt_info* lpkt = LORA_PKT_INFO_PTR(om);
    console_printf("\trxdr:%u\n", lpkt->rxdinfo.rxdatarate);
    console_printf("\tsnr:%u\n", lpkt->rxdinfo.snr);
    console_printf("\trssi:%d\n", lpkt->rxdinfo.rssi);
    console_printf("\trxslot:%u\n", lpkt->rxdinfo.rxslot);
    console_printf("\tack_rxd:%u\n", lpkt->rxdinfo.ack_rxd);
    console_printf("\trxdata:%u\n", lpkt->rxdinfo.rxdata);
    console_printf("\tmulticast:%u\n", lpkt->rxdinfo.multicast);
    console_printf("\tfp:%u\n", lpkt->rxdinfo.frame_pending);
    console_printf("\tdownlink_cntr:%lu\n", lpkt->rxdinfo.downlink_cntr);

    console_printf("-------------------------");
    console_printf("Received data: ");
    for (uint16_t i = 0; i < OS_MBUF_PKTLEN(om); ) {
        uint16_t len = OS_MBUF_PKTLEN(om) - i;
        if (len > 16) {
            len = 16;
        }
        uint8_t tmp[16];
        int rc = os_mbuf_copydata(om, i, len, tmp);
        assert(rc == 0);
        for (int i = 0; i < len; ++i) {
            console_printf("%02x ", tmp[i]);
        }
        i += len;
    }
    console_printf("\n");
    console_printf("-------------------------");

    int rc = os_mbuf_free_chain(om);
    assert(rc == 0);
}

static const uint8_t app_port = 1;

static void join_cb(LoRaMacEventInfoStatus_t status, uint8_t attempts) {
    console_printf("Join cb. status=%d attempts=%u\n", status, attempts);
    if (status != 0) {
        return;
    }

    int rc = lora_app_port_open(app_port, transmit_done, receive_done);
    if (rc != LORA_APP_STATUS_OK) {
        console_printf("Failed to open app port %u err=%d\n", app_port, rc);
        return;
    }
    state = CONNECTED;
    console_printf("Opened app port %u\n", app_port);
}

void lora_send(const void* data, int len) {
    switch (state) {
    case DISCONNECTED:
        console_printf("LoRa disconnected, dropping data.\n");
        return;
    case SENDING:
        console_printf("LoRa sending in progress, dropping data.\n");
        return;
    case CONNECTED:
        break;
    }

    struct os_mbuf* om = lora_pkt_alloc();
    if (!om) {
        console_printf("Unable to allocate LoRa packet.\n");
        return;
    }

    int rc = os_mbuf_copyinto(om, 0, data, len);
    assert(rc == 0);

    rc = lora_app_port_send(app_port, MCPS_UNCONFIRMED, om);
    if (rc) {
        console_printf("Failed to send: %d\n", rc);
        return;
    }

    console_printf("Sending LoRa packet...\n");

    // const uint8_t * p = data;
    // console_printf("Sent payload: %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", *p, *(p+1), *(p+2), *(p+3), *(p+4), *(p+5), *(p+6), *(p+7), *(p+8));

    state = SENDING;
    os_callout_init(&lora_callout, os_eventq_dflt_get(), lora_event_callback, NULL);
    reset_lora_callout();
}

static void link_chk_cb(LoRaMacEventInfoStatus_t status, uint8_t num_gw, uint8_t demod_margin) {
    console_printf("Link check cb. status=%d num_gw=%u demod_margin=%u\n", status, num_gw, demod_margin);
}

void init_lora_task() {
    srand((unsigned) DevEui[7]);
    os_time_delay(rand() % OS_TICKS_PER_SEC);

    LoRaMacTestSetDutyCycleOn(false);

    console_printf("INIT LORA\n");
    lora_app_set_join_cb(join_cb);
    lora_app_set_link_check_cb(link_chk_cb);
    int rc = lora_app_join(DevEui, AppEui, AppKey, 255);
    if (rc != 0) {
        console_printf("Join failed: %d\n", rc);
        return;
    }
    console_printf("Joining LoRa...\n");
    os_callout_init(&lora_callout, os_eventq_dflt_get(), lora_event_callback, NULL);
    reset_lora_callout();
}
