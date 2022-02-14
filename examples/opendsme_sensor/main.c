/*
 * Copyright (C) 2021 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief       OpenDSME example
 *
 * @author      José I. Álamos <jose.alamos@haw-hamburg.de>
 */

#include <stdio.h>
#include <string.h>
#include "opendsme/opendsme.h"
#include "event/thread.h"
#include "luid.h"
#include "pm_layered.h"

#if IS_USED(MODULE_SX127X)
#include "sx127x.h"
#include "sx127x_params.h"
#endif

#include "net/gnrc.h"
#include "net/gnrc/netreg.h"

#include "shell.h"
#include "shell_commands.h"
#include "net/l2util.h"
#include "fmt.h"

#ifndef NODE_ID
#define NODE_ID 0x3031
#endif

gnrc_pktsnip_t *pkt;
static uint16_t counter = 0;

gnrc_netif_t *opendsme_get_netif(void);
int gnrc_netif_dsme_create(gnrc_netif_t *netif, char *stack, int stacksize,
                                 char priority, const char *name, netdev_t *dev);


uint16_t get_node_id(void)
{
    return NODE_ID;
}

static char dsme_stack[2000];
int main(void)
{
    pm_unblock(1);
    LED0_OFF;
    LED1_OFF;
    LED2_OFF;
    LED3_OFF;
    uint8_t addr[2];
    dsme_alloc_t alloc;
    memset(&alloc, 0, sizeof(alloc));

    gnrc_netif_dsme_create(opendsme_get_netif(), dsme_stack, 2000, THREAD_PRIORITY_MAIN - 1,"dsme", NULL);

    bool pan_coord = false;

    gnrc_netapi_set(opendsme_get_netif()->pid, NETOPT_PAN_COORD, 0, &pan_coord, sizeof(pan_coord));
    gnrc_netapi_set(opendsme_get_netif()->pid, NETOPT_LINK, 0, NULL, 0);

    l2util_addr_from_str("30:30", (uint8_t*) &alloc.addr);
    alloc.tx = true;
    alloc.superframe_id = 0;
    alloc.slot_id = 0;
    alloc.channel_id = 0;
    gnrc_netapi_set(opendsme_get_netif()->pid, NETOPT_GTS_ALLOC, 0, &alloc, sizeof(alloc));

    l2util_addr_from_str("30:30", (uint8_t*) &addr);
    int count = 0;
    while(true) {
        if (opendsme_is_associated()) {
            if (count > 1)  {
                pkt = gnrc_pktbuf_add(NULL, NULL, CONFIG_OPENDSME_PAYLOAD_LENGTH, GNRC_NETTYPE_UNDEF);
                memset(pkt->data, 0, pkt->size);
                uint8_t *p = pkt->data;
                p[0] = counter >> 8;
                p[1] = counter & 0xFF;
                counter++;
                gnrc_pktsnip_t *netif_hdr = gnrc_netif_hdr_build(NULL, 0, addr, 2);
                pkt = gnrc_pkt_prepend(pkt, netif_hdr);
                gnrc_netapi_send(opendsme_get_netif()->pid, pkt);
            }
            else {
                count++;
            }
        }
        ztimer_sleep(ZTIMER_MSEC, 10000);
    }

    return 0;
}

void sx127x_init_dsme(sx127x_t *dev, void *radio)
{
    sx127x_hal_setup(dev, radio);
    sx127x_setup(dev, sx127x_params, radio);
}
