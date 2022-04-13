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
#define NODE_ID 0x3030
#endif

gnrc_pktsnip_t *pkt;

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
    dsme_alloc_t alloc;
    memset(&alloc, 0, sizeof(alloc));

    gnrc_netif_dsme_create(opendsme_get_netif(), dsme_stack, 2000, THREAD_PRIORITY_MAIN - 1,"dsme", NULL);

    bool pan_coord = COORD;

    gnrc_netapi_set(opendsme_get_netif()->pid, NETOPT_PAN_COORD, 0, &pan_coord, sizeof(pan_coord));
    gnrc_netapi_set(opendsme_get_netif()->pid, NETOPT_LINK, 0, NULL, 0);

    l2util_addr_from_str("30:31", (uint8_t*) &alloc.addr);
    alloc.tx = false;
    alloc.superframe_id = 0;
    alloc.slot_id = 2;
    alloc.channel_id = 0;
    gnrc_netapi_set(opendsme_get_netif()->pid, NETOPT_GTS_ALLOC, 0, &alloc, sizeof(alloc));

    return 0;
}

void sx127x_init_dsme(sx127x_t *dev, void *radio)
{
    sx127x_hal_setup(dev, radio);
    sx127x_setup(dev, sx127x_params, radio);
}
