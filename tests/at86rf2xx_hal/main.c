/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for AT86RF2xx network device driver
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sys/uio.h"

#include "net/netdev.h"
#include "shell.h"
#include "shell_commands.h"
#include "thread.h"
#include "mutex.h"
#if IS_ACTIVE(MODULE_AT86RF2XX)
#include "at86rf2xx.h"
#include "at86rf2xx_params.h"
#endif
#if IS_ACTIVE(MODULE_NRF802154)
#include "nrf802154.h"
#endif
#include "net/ieee802154/submac.h"
#include "od.h"
#include "event/thread.h"
#include "event/callback.h"
#include "xtimer.h"
#define MAX_LINE    (80)

ieee802154_submac_t submac;
mutex_t lock;
void _ack_timeout(void *arg);
xtimer_t ack_timer = {.callback = _ack_timeout, .arg = &submac};

void ieee802154_submac_ack_timer_set(ieee802154_submac_t *submac, uint16_t us)
{
    (void) submac;
    xtimer_set(&ack_timer, us);
}

void ieee802154_submac_ack_timer_cancel(ieee802154_submac_t *submac)
{
    (void) submac;
    xtimer_remove(&ack_timer);
}

void _task_send(event_t *event)
{
    event_callback_t *ev = (event_callback_t*) event;
    ieee802154_submac_ack_timeout_fired(ev->arg);
}

event_callback_t _send = {.super.handler=_task_send, .arg=&submac};
void _ack_timeout(void *arg)
{
    (void) arg;
    event_post(EVENT_PRIO_HIGHEST, &_send.super);
}


uint8_t buffer[127];
            
static void radio_cb(ieee802154_dev_t *dev, ieee802154_tx_status_t status)
{
    switch(status) {
        case IEEE802154_RADIO_TX_DONE:
            ieee802154_submac_tx_done_cb(&submac);
            break;
        case IEEE802154_RADIO_RX_DONE: {
            ieee802154_rx_info_t info;
            struct iovec _iov = {
                .iov_base = buffer,
                .iov_len = ieee802154_radio_read(dev, buffer, 127, &info),
            };
            if (info.crc_ok) {
                ieee802154_submac_rx_done_cb(&submac, &_iov, &info);
            }
        }
        default:
           break;
    }
}

static void submac_tx_done(ieee802154_submac_t *submac, int status, ieee802154_tx_info_t *info)
{
    (void) status;
    (void) info;
    ieee802154_dev_t *dev = submac->dev;
    switch(status) {
        case IEEE802154_RF_EV_TX_DONE:
        case IEEE802154_RF_EV_TX_DONE_DATA_PENDING:
            puts("Done!");
            printf("Retrans: %i\n", info->retries);
            break;
        case IEEE802154_RF_EV_TX_MEDIUM_BUSY:
            puts("Medium busy!");
            break;
        case IEEE802154_RF_EV_TX_NO_ACK:
            puts("No ACK!");
            break;
        default:
            break;
    }

    ieee802154_radio_set_trx_state(dev, IEEE802154_TRX_STATE_RX_ON);
    mutex_unlock(&lock);
}

static void submac_rx_done(ieee802154_submac_t *submac, struct iovec *iov, ieee802154_rx_info_t *info)
{
    (void) submac;
    uint8_t *buffer = iov->iov_base;
    size_t size = iov->iov_len;
    puts("Terrible de recibi el paquete");
    for (unsigned i=0;i<size;i++) {
        printf("%02x ", buffer[i]);
    }
    printf("LQI: %i, RSSI: %i\n", (int) info->lqi, (int) info->rssi);
    puts("");
}


ieee802154_submac_cb_t _cb= {
    .rx_done = submac_rx_done,
    .tx_done = submac_tx_done,
};

#if IS_ACTIVE(MODULE_AT86RF2XX)
at86rf2xx_t dev;
ieee802154_submac_t submac = {.dev = (ieee802154_dev_t*) &dev, .cb = &_cb};
#endif
#if IS_ACTIVE(MODULE_NRF802154)
extern ieee802154_dev_t nrf802154_dev;
ieee802154_submac_t submac = {.dev = (ieee802154_dev_t*) &nrf802154_dev, .cb = &_cb};
#endif

void _ev_send(void *arg)
{
    ieee802154_send(&submac, arg);
}

static int send(uint8_t *dst, size_t dst_len,
                char *data)
{
    uint8_t flags;
    uint8_t mhr[IEEE802154_MAX_HDR_LEN];
    int mhr_len;

    le_uint16_t src_pan, dst_pan;
    iolist_t iol_data = {
        .iol_base = data,
        .iol_len = strlen(data),
        .iol_next = NULL,
    };

    flags = IEEE802154_FCF_TYPE_DATA | 0x20;
    src_pan = byteorder_btols(byteorder_htons(0x23));
    dst_pan = byteorder_btols(byteorder_htons(0x23));
    uint8_t src_len = 8;
    void *src = &submac.ext_addr;

    /* fill MAC header, seq should be set by device */
    if ((mhr_len = ieee802154_set_frame_hdr(mhr, src, src_len,
                                        dst, dst_len,
                                        src_pan, dst_pan,
                                        flags, submac.seq++)) < 0) {
        puts("txtsnd: Error preperaring frame");
        return 1;
    }

    iolist_t iol_hdr = {
        .iol_next = &iol_data,
        .iol_base = mhr,
        .iol_len = mhr_len,
    };

    event_callback_t _ev = EVENT_CALLBACK_INIT(_ev_send, &iol_hdr);
    event_post(EVENT_PRIO_HIGHEST, &_ev.super);
    mutex_lock(&lock);
    return 0;
}

static inline int _dehex(char c, int default_)
{
    if ('0' <= c && c <= '9') {
        return c - '0';
    }
    else if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    }
    else if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    }
    else {
        return default_;
    }
}

static size_t _parse_addr(uint8_t *out, size_t out_len, const char *in)
{
    const char *end_str = in;
    uint8_t *out_end = out;
    size_t count = 0;
    int assert_cell = 1;

    if (!in || !*in) {
        return 0;
    }
    while (end_str[1]) {
        ++end_str;
    }

    while (end_str >= in) {
        int a = 0, b = _dehex(*end_str--, -1);
        if (b < 0) {
            if (assert_cell) {
                return 0;
            }
            else {
                assert_cell = 1;
                continue;
            }
        }
        assert_cell = 0;

        if (end_str >= in) {
            a = _dehex(*end_str--, 0);
        }

        if (++count > out_len) {
            return 0;
        }
        *out_end++ = (a << 4) | b;
    }
    if (assert_cell) {
        return 0;
    }
    /* out is reversed */

    while (out < --out_end) {
        uint8_t tmp = *out_end;
        *out_end = *out;
        *out++ = tmp;
    }

    return count;
}

int txtsnd(int argc, char **argv)
{
    char *text;
    uint8_t addr[8];
    int iface, idx = 2;
    size_t res;
    le_uint16_t pan = { 0 };

    switch (argc) {
        case 4:
            break;
        case 5:
            res = _parse_addr((uint8_t *)&pan, sizeof(pan), argv[idx++]);
            if ((res == 0) || (res > sizeof(pan))) {
                return 1;
            }
            pan.u16 = byteorder_swaps(pan.u16);
            break;
        default:
            return 1;
    }

    iface = atoi(argv[1]);
    res = _parse_addr(addr, sizeof(addr), argv[idx++]);
    if (res == 0) {
        return 1;
    }
    text = argv[idx++];
    (void) iface;
    (void) pan;
    return send(addr, res, text);
}
static const shell_command_t shell_commands[] = {
    { "txtsnd", "Send IEEE 802.15.4 packet", txtsnd },
    { NULL, NULL, NULL }
};

#if IS_ACTIVE(MODULE_AT86RF2XX)
void at86rf2xx_init_int(at86rf2xx_t *dev);
int at86rf2xx_init(at86rf2xx_t *dev, const at86rf2xx_params_t *params, void (*isr)(void *arg));
#endif

#if IS_ACTIVE(MODULE_NRF802154)
void nrf802154_init_int(void (*isr)(void *arg));
int nrf802154_init(void);
#endif

void _irq_event_handler(event_t *event)
{
    (void) event;
    ieee802154_radio_irq_handler(submac.dev);
}

event_t _irq_event = {
    .handler = _irq_event_handler,
};

/* TODO: Add IRQ callbacks and timer functions */
static void _isr(void *arg)
{
    (void) arg;
    event_post(EVENT_PRIO_HIGHEST, &_irq_event);
}

int main(void)
{
    mutex_lock(&lock);
#if IS_ACTIVE(MODULE_AT86RF2XX)
    puts("AT86RF2xx device driver test");

    const at86rf2xx_params_t *p = &at86rf2xx_params[0];

    printf("Initializing AT86RF2xx radio at SPI_%d\n", p->spi);
    ieee802154_dev_t *d = (ieee802154_dev_t*) &dev;
    d->cb = radio_cb;
    at86rf2xx_init(&dev, p, _isr);
#endif
#if IS_ACTIVE(MODULE_NRF802154)
    ieee802154_dev_t *d = (ieee802154_dev_t*) &nrf802154_dev;
    d->cb = radio_cb;
    nrf802154_init(_isr);
#endif

    ieee802154_submac_init(&submac);
    uint8_t *_p = (uint8_t*) &submac.short_addr;
    for(int i=0;i<2;i++) {
        printf("%02x", *_p++);
    }
    printf("\n");
    _p = (uint8_t*) &submac.ext_addr;
    for(int i=0;i<8;i++) {
        printf("%02x", *_p++);
    }
    printf("\n");

    /* start the shell */
    puts("Initialization successful - starting the shell now");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
