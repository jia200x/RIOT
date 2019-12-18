/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @ingroup sys_auto_init_gnrc_netif
 * @{
 *
 * @file
 * @brief   Auto initialization for ethernet-over-serial module
 *
 * @author  Kaspar Schleiser <kaspar@schleiser.de>
 */

#ifdef MODULE_ETHOS

#include "log.h"
#include "debug.h"
#include "ethos.h"
#include "periph/uart.h"
#include "net/gnrc/netif/ethernet.h"
#include "net/gnrc/netif/internal.h"

/**
 * @brief global ethos object, used by stdio_uart
 */
ethos_t ethos;
static gnrc_netif_t *netif;

/**
 * @brief   Define stack parameters for the MAC layer thread
 * @{
 */
#define ETHOS_MAC_STACKSIZE (THREAD_STACKSIZE_DEFAULT + DEBUG_EXTRA_STACKSIZE)
#ifndef ETHOS_MAC_PRIO
#define ETHOS_MAC_PRIO      (GNRC_NETIF_PRIO)
#endif

/**
 * @brief   Stacks for the MAC layer threads
 */
static char _netdev_eth_stack[ETHOS_MAC_STACKSIZE];
void ethos_task_handler(ethos_t *dev);

static uint8_t _inbuf[2048];

static void _th(void *arg)
{
    (void) arg;
    ethos_task_handler(&ethos);
}

static const gnrc_netif_task_handler_t ethos_th = {
    .th = _th,
};

void ethos_isr_cb(ethos_t *dev)
{
    (void) dev;
    puts("p");
    msg_t msg = { .type = NETDEV_MSG_TYPE_EVENT,
                  .content = { .ptr = (gnrc_netif_task_handler_t*) &ethos_th } };

    if (msg_send(&msg, netif->pid) <= 0) {
        puts("gnrc_netif: possibly lost interrupt.");
    }
}

void gnrc_netif_recv(gnrc_netif_t *netif);

void ethos_rx_complete(ethos_t *dev)
{
    (void) dev;
    gnrc_netif_recv(netif);
}

void ethos_hal_setup(ethernet_hal_t *dev);
void auto_init_ethos(void)
{
    LOG_DEBUG("[auto_init_netif] initializing ethos #0\n");

    /* setup netdev device */
    ethos_params_t p;
    p.uart      = ETHOS_UART;
    p.baudrate  = ETHOS_BAUDRATE;
    p.buf       = _inbuf;
    p.bufsize   = sizeof(_inbuf);
    ethos_setup(&ethos, &p);
    ethos_hal_setup((ethernet_hal_t*) &ethos);

    /* initialize netdev<->gnrc adapter state */
    netif = gnrc_netif_ethernet_create(_netdev_eth_stack, ETHOS_MAC_STACKSIZE,
                               ETHOS_MAC_PRIO, "ethos", (netdev_t *)&ethos);
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_ETHOS */
/** @} */
