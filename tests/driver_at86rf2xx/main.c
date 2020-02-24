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

#include "net/netdev.h"
#include "shell.h"
#include "shell_commands.h"
#include "thread.h"
#include "xtimer.h"
#include "net/ieee802154/radio.h"
#include "luid.h"

#include "common.h"

#define _STACKSIZE      (THREAD_STACKSIZE_DEFAULT + THREAD_EXTRA_STACKSIZE_PRINTF)
#define MSG_TYPE_ISR    (0x3456)

static char stack[_STACKSIZE];
static kernel_pid_t _recv_pid;

at86rf2xx_t devs[AT86RF2XX_NUM];

static const shell_command_t shell_commands[] = {
    { "ifconfig", "Configure netdev", ifconfig },
    { "txtsnd", "Send IEEE 802.15.4 packet", txtsnd },
    { NULL, NULL, NULL }
};

#if 0
static void _event_cb(netdev_t *dev, netdev_event_t event)
{
    if (event == NETDEV_EVENT_ISR) {
        msg_t msg;

        msg.type = MSG_TYPE_ISR;
        msg.content.ptr = dev;

        if (msg_send(&msg, _recv_pid) <= 0) {
            puts("gnrc_netdev: possibly lost interrupt.");
        }
    }
    else {
        switch (event) {
            case NETDEV_EVENT_RX_COMPLETE:
            {
                recv(dev);

                break;
            }
            default:
                puts("Unexpected event received");
                break;
        }
    }
}
#endif

void at86rf2xx_init_int(at86rf2xx_t *dev, void (*isr)(void *arg));
int at86rf2xx_init(at86rf2xx_t *dev, const at86rf2xx_params_t *params, const ieee802154_rf_cb_t cb);
void at86rf2xx_task_handler(at86rf2xx_t *dev);

void *_recv_thread(void *arg)
{
    (void)arg;
    while (1) {
        msg_t msg;
        msg_receive(&msg);
        if (msg.type == MSG_TYPE_ISR) {
            at86rf2xx_task_handler(msg.content.ptr);
        }
        else {
            puts("unexpected message type");
        }
    }
}

static void at86rf2xx_cbs(void *dev, ieee802154_rf_event_t event, void *ctx)
{
    printf("CB!");
    (void) dev;
    (void) event;
    (void) ctx;
}

static void _isr(void *arg)
{
    msg_t msg;

    msg.type = MSG_TYPE_ISR;
    msg.content.ptr = arg;

    if (msg_send(&msg, _recv_pid) <= 0) {
        puts("gnrc_netdev: possibly lost interrupt.");
    }
}

int main(void)
{
    puts("AT86RF2xx device driver test");

    unsigned dev_success = 0;
    for (unsigned i = 0; i < AT86RF2XX_NUM; i++) {
        const at86rf2xx_params_t *p = &at86rf2xx_params[i];

        printf("Initializing AT86RF2xx radio at SPI_%d\n", p->spi);
        at86rf2xx_init(&devs[i], p, at86rf2xx_cbs);
        at86rf2xx_init_int(&devs[i], _isr);
        /* generate EUI-64 and short address */
        eui64_t addr_long;
        network_uint16_t addr_short;
        luid_get_eui64(&addr_long);
        luid_get_short(&addr_short);
        ieee802154_dev_t *d = (ieee802154_dev_t*) &devs[i];
        d->driver->set_hw_addr_filter(d, (uint8_t*) &addr_short, (uint8_t*) &addr_long, 0x23);
        dev_success++;
    }

    if (!dev_success) {
        puts("No device could be initialized");
        return 1;
    }

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
