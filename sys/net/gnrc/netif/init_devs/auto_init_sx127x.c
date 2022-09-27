/*
 * Copyright (C) 2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @ingroup     sys_auto_init_gnrc_netif
 * @{
 *
 * @file
 * @brief       Auto initialization for SX1272/SX1276 LoRa interfaces
 *
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 */

#include <assert.h>

#include "log.h"
#include "board.h"
#include "net/gnrc/netif/lorawan_base.h"
#include "net/gnrc/netif/raw.h"
#include "net/gnrc.h"
#include "include/init_devs.h"

#include "sx127x.h"
#include "sx127x_params.h"

#if IS_USED(MODULE_OPENDSME)
#include "opendsme/opendsme.h"
#endif

/**
 * @brief   Calculate the number of configured SX127x devices
 */
#define SX127X_NUMOF        ARRAY_SIZE(sx127x_params)

/**
 * @brief   Define stack parameters for the MAC layer thread
 */
<<<<<<< HEAD
#define SX127X_STACKSIZE           (GNRC_NETIF_STACKSIZE_DEFAULT)
=======
#define SX127X_STACKSIZE           (THREAD_STACKSIZE_LARGE)
>>>>>>> ba612aadb4 (IPv6 over DSME LoRa)
#ifndef SX127X_PRIO
#define SX127X_PRIO                (GNRC_NETIF_PRIO)
#endif

/**
 * @brief   Allocate memory for device descriptors, stacks, and GNRC adaption
 */
static sx127x_t sx127x_devs[SX127X_NUMOF];
static char sx127x_stacks[SX127X_NUMOF][SX127X_STACKSIZE];
static gnrc_netif_t _netif[SX127X_NUMOF];
static ieee802154_dev_t sx127x_hal[SX127X_NUMOF];

void auto_init_sx127x(void)
{
    for (unsigned i = 0; i < SX127X_NUMOF; ++i) {
        sx127x_setup(&sx127x_devs[i], &sx127x_params[i], &sx127x_hal[i], &_netif[i].evq);
        /* TODO: HAAAACK */
        gnrc_netif_opendsme_create(&_netif[i], sx127x_stacks[i],
                                 SX127X_STACKSIZE,
                                 SX127X_PRIO, "sx127x",
                                 (netdev_t*) &sx127x_hal[i]);
    }
}
/** @} */
