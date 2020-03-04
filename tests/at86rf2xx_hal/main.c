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

#include "net/netdev.h"
#include "shell.h"
#include "shell_commands.h"
#include "thread.h"
#include "xtimer.h"
#if IS_ACTIVE(MODULE_AT86RF2XX)
#include "at86rf2xx.h"
#include "at86rf2xx_params.h"
#endif
#if IS_ACTIVE(MODULE_NRF802154)
#include "nrf802154.h"
#endif
#include "net/ieee802154/radio.h"
#include "luid.h"
#include "od.h"
#include "net/csma_sender.h"
#define MAX_LINE    (80)

#define _STACKSIZE      (THREAD_STACKSIZE_DEFAULT + THREAD_EXTRA_STACKSIZE_PRINTF)
#define MSG_TYPE_ISR    (0x3456)
#define MSG_TYPE_ACK_TIMEOUT    (0x3457)

typedef enum {
    SUBMAC_RX,
    SUBMAC_WAIT_FOR_ACK,
    SUBMAC_WAIT_TX_DONE,
    SUBMAC_ACK_SENT,
    SUBMAC_ACK_RECV,
    SUBMAC_ACK_FAILED,
} ieee802154_submac_state_t;

ieee802154_submac_t submac;

static char stack[_STACKSIZE];
static kernel_pid_t _recv_pid;

static void radio_cb(ieee802154_dev_t *dev, int status, void *ctx)
{
    switch(status) {
        case IEEE802154_RADIO_TX_DONE:
            switch(submac.state) {
                case SUBMAC_ACK_SENT:
                    dev->driver->set_trx_state(dev, IEEE802154_TRX_STATE_RX_ON);
                    submac.state = SUBMAC_RX;
                    break;
                case SUBMAC_WAIT_TX_DONE:
                    submac.state = SUBMAC_WAIT_FOR_ACK;
                    dev->driver->set_trx_state(dev, IEEE802154_TRX_STATE_RX_ON);
                    break;
            }
            break;
        case IEEE802154_RADIO_RX_DONE: {
            ieee802154_rx_data_t *data = ctx;
            uint8_t buffer[127];
            if (!data->buf) {
                dev->driver->read(dev, buffer, 127, data);
            }
            switch(submac.state) {
                case SUBMAC_WAIT_FOR_ACK:
                    if(data->len <= 5 && data->buf[0] == 0x2) {
                        submac.state = SUBMAC_ACK_RECV;
                    }
                    else {
                        submac.state = SUBMAC_ACK_FAILED;
                    }
                    break;
                default:
                    if (!dev->driver->get_flag(dev, IEEE802154_FLAG_HAS_AUTO_ACK)) {
                        if (((uint8_t*) data->buf)[0] & 0x2) {
                            return;
                        }
                        /* Send ACK packet */
                        uint8_t ack_pkt[3];
                        ack_pkt[0] = 0x2;
                        ack_pkt[1] = 0;
                        ack_pkt[2] = ((uint8_t*) data->buf)[2];

                        iolist_t ack = {
                            .iol_base = ack_pkt,
                            .iol_len = 3,
                            .iol_next = NULL,
                        };

                        submac.state = SUBMAC_ACK_SENT;
                        dev->driver->set_trx_state(dev, IEEE802154_TRX_STATE_TX_ON);
                        dev->driver->prepare(dev, &ack);
                        dev->driver->transmit(dev);
                    }
                    submac.cb->rx_done(dev, data->buf, data->len);
                    break;
            }
       }
    }
}

static void submac_tx_done(ieee802154_dev_t *dev, int status, bool frame_pending,
        int retrans)
{
    (void) status;
    (void) frame_pending;
    (void) retrans;
    switch(status) {
        case IEEE802154_RF_EV_TX_DONE:
        case IEEE802154_RF_EV_TX_DONE_DATA_PENDING:
            puts("Done!");
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

    dev->driver->set_trx_state(dev, IEEE802154_TRX_STATE_RX_ON);
}

static void submac_rx_done(ieee802154_dev_t *dev, uint8_t *buffer, size_t size)
{
    (void) dev;
    puts("Terrible de recibi el paquete");
    for (unsigned i=0;i<size;i++) {
        printf("%02x ", buffer[i]);
    }
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

/* SubMAC variables */
#define IEEE802154_SUBMAC_MAX_RETRANSMISSIONS (2)

void _perform_csma_with_retrans(ieee802154_dev_t *dev, iolist_t *psdu)
{
    int res;
    int submac_retransmissions = 0;
    while (submac_retransmissions++ < IEEE802154_SUBMAC_MAX_RETRANSMISSIONS) {
        submac.state = SUBMAC_WAIT_TX_DONE;
        res = csma_sender_csma_ca_send(dev, psdu, NULL);
        if (res < 0) {
            submac.cb->tx_done(dev, IEEE802154_RF_EV_TX_MEDIUM_BUSY, 0, 0);
            break;
        }

        /* Wait for TX done */
        while(submac.state == SUBMAC_WAIT_TX_DONE ) {
            dev->driver->irq_handler(dev);
        }
        xtimer_usleep(1000);
        dev->driver->irq_handler(dev);
        res = -ENOMSG;
        if (submac.state == SUBMAC_ACK_RECV) {
            res = 0;
            break;
        }
    }
    if (res < 0) {
        submac.cb->tx_done(dev, IEEE802154_RF_EV_TX_NO_ACK, 0, 0);
    }
    else {
        submac.cb->tx_done(dev, IEEE802154_RF_EV_TX_DONE, 0, 0);
    }
    /* TODO: */
    submac.state = SUBMAC_RX;
}

int ieee802154_send(ieee802154_dev_t *dev, iolist_t *iolist)
{
    int res;
    if (dev->driver->get_flag(dev, IEEE802154_FLAG_HAS_FRAME_RETRIES) || 
        dev->driver->get_flag(dev, IEEE802154_FLAG_HAS_CSMA_BACKOFF))
    {
        dev->driver->set_trx_state(dev, IEEE802154_TRX_STATE_TX_ON);
        res = dev->driver->prepare(dev, iolist);

        if (res < 0) {
            return 1;
        }
        else {
        }
        res = dev->driver->transmit(dev);
    }
    else {
        _perform_csma_with_retrans(dev, iolist);
        res = 0;
    }

    return res;
}

int ieee802154_set_addresses(ieee802154_dev_t *dev, network_uint16_t *short_addr,
        eui64_t *ext_addr, uint16_t panid)
{
    memcpy(&submac.short_addr, short_addr, 2);
    memcpy(&submac.ext_addr, ext_addr, 8);
    submac.panid = panid;

    if (dev->driver->set_hw_addr_filter) {
        dev->driver->set_hw_addr_filter(dev, (void*) short_addr, (void*) ext_addr, panid);
    }
    return 0;
}


static inline int ieee802154_set_short_addr(ieee802154_dev_t *dev, network_uint16_t *short_addr)
{
    return ieee802154_set_addresses(dev, short_addr, &submac.ext_addr, submac.panid);
}

static inline int ieee802154_set_ext_addr(ieee802154_dev_t *dev, eui64_t *ext_addr)
{
    return ieee802154_set_addresses(dev, &submac.short_addr, ext_addr, submac.panid);
}

static inline int ieee802154_set_panid(ieee802154_dev_t *dev, uint16_t panid)
{
    return ieee802154_set_addresses(dev, &submac.short_addr, &submac.ext_addr, panid);
}

int ieee802154_set_channel(ieee802154_dev_t *dev, uint8_t channel_num, uint8_t channel_page)
{
    return dev->driver->set_channel(dev, channel_num, channel_page);
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

    ieee802154_send(submac.dev, &iol_hdr);
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

int txpow(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    submac.dev->driver->set_tx_power(submac.dev, 0);
    return 0;
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
    { "txpow", "Send IEEE 802.15.4 packet", txpow },
    { NULL, NULL, NULL }
};

#if IS_ACTIVE(MODULE_AT86RF2XX)
void at86rf2xx_init_int(at86rf2xx_t *dev, void (*isr)(void *arg));
int at86rf2xx_init(at86rf2xx_t *dev, const at86rf2xx_params_t *params);
#endif

#if IS_ACTIVE(MODULE_NRF802154)
void nrf802154_init_int(void (*isr)(void *arg));
int nrf802154_init(void);
#endif

void *_recv_thread(void *arg)
{
    (void)arg;
    msg_t msg, queue[8];
    msg_init_queue(queue, 8);
    while (1) {
        msg_receive(&msg);
        if (msg.type == MSG_TYPE_ISR) {
            submac.dev->driver->irq_handler(submac.dev);
            //submac.cb->tx_done(submac.dev, 0, 0, 0);
        }
        else {
            puts("unexpected message type");
        }
    }
    return NULL;
}

static void _isr(void *arg)
{
    switch(submac.state) {
        case SUBMAC_WAIT_FOR_ACK:
        case SUBMAC_WAIT_TX_DONE:
            return;
        default:
            break;
    }
    msg_t msg;

    msg.type = MSG_TYPE_ISR;
    msg.content.ptr = arg;

    if (msg_send(&msg, _recv_pid) <= 0) {
        puts("gnrc_netdev: possibly lost interrupt.");
    }
}

int main(void)
{
#if IS_ACTIVE(MODULE_AT86RF2XX)
    puts("AT86RF2xx device driver test");

    const at86rf2xx_params_t *p = &at86rf2xx_params[0];

    printf("Initializing AT86RF2xx radio at SPI_%d\n", p->spi);
    ieee802154_dev_t *d = (ieee802154_dev_t*) &dev;
    d->cb = radio_cb;
    at86rf2xx_init(&dev, p);
    at86rf2xx_init_int(&dev, _isr);
#endif
#if IS_ACTIVE(MODULE_NRF802154)
    ieee802154_dev_t *d = (ieee802154_dev_t*) &nrf802154_dev;
    d->cb = radio_cb;
    nrf802154_init();
    nrf802154_init_int(_isr);
#endif
    /* generate EUI-64 and short address */
    luid_get_eui64(&submac.ext_addr);
    luid_get_short(&submac.short_addr);
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
#if IS_ACTIVE(MODULE_AT86RF2XX)
    d->driver->set_hw_addr_filter(d, (uint8_t*) &submac.short_addr, (uint8_t*) &submac.ext_addr, 0x23);
#endif
    d->driver->set_channel(submac.dev, 26, 0);

    _recv_pid = thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
                              THREAD_CREATE_STACKTEST, _recv_thread, NULL,
                              "recv_thread");

    if (_recv_pid <= KERNEL_PID_UNDEF) {
        puts("Creation of receiver thread failed");
        return 1;
    }

    /* start the shell */
    puts("Initialization successful - starting the shell now");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
