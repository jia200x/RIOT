/*
 * Copyright (C) 2021 Inria
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
 * @brief       Auto initialization for SX1261/2 LoRa interfaces
 *
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 */

#include <assert.h>

#include "log.h"
#include "board.h"
#include "net/gnrc/netif/lorawan_base.h"
#include "net/gnrc/netif/raw.h"
#include "net/gnrc/netif/ieee802154.h"
#if !IS_USED(MODULE_OPENDSME)
#include "net/netdev/ieee802154_submac.h"
#endif
#include "net/gnrc.h"
#include "include/init_devs.h"

#include "include/init_devs.h"

#include "sx126x.h"
#include "sx126x_params.h"

#if IS_USED(MODULE_OPENDSME)
#include "opendsme/opendsme.h"
#endif

/**
 * @brief   Calculate the number of configured SX126X devices
 */
#define SX126X_NUMOF                ARRAY_SIZE(sx126x_params)

/**
 * @brief   Define stack parameters for the MAC layer thread
 */
#define SX126X_STACKSIZE            (THREAD_STACKSIZE_LARGE)
#ifndef SX126X_PRIO
#define SX126X_PRIO                 (GNRC_NETIF_PRIO)
#endif

/**
 * @brief   Allocate memory for device descriptors, stacks, and GNRC adaption
 */
#if IS_USED(MODULE_SX126X_HAL)
static ieee802154_dev_t sx126x_hal[SX126X_NUMOF];
#endif
static sx126x_t sx126x_devs[SX126X_NUMOF];
static char sx126x_stacks[SX126X_NUMOF][SX126X_STACKSIZE];
static gnrc_netif_t _netif[SX126X_NUMOF];

void auto_init_sx126x(void)
{
#if IS_USED(MODULE_SX126X_HAL)
    for (unsigned i = 0; i < SX126X_NUMOF; ++i) {
        LOG_DEBUG("[auto_init_netif] initializing sx126x #%u\n", i);
        

        sx126x_hal_setup(&sx126x_devs[i], &sx126x_hal[i]);
        sx126x_init(&sx126x_devs[i], &sx126x_params[i], &_netif->evq[GNRC_NETIF_EVQ_INDEX_PRIO_LOW]);
        sx126x_setup(&sx126x_devs[i], i);
        gnrc_netif_opendsme_create(&_netif[i], sx126x_stacks[i],
                                 SX126X_STACKSIZE,
                                 SX126X_PRIO, "sx126x",
                                 (netdev_t*) &sx126x_hal[i]);
    }
#else
    for (unsigned i = 0; i < SX126X_NUMOF; ++i) {
        LOG_DEBUG("[auto_init_netif] initializing sx126x #%u\n", i);
        sx126x_setup(&sx126x_devs[i], &sx126x_params[i], i);
        if (IS_USED(MODULE_GNRC_NETIF_LORAWAN)) {
            /* Currently only one lora device is supported */
            assert(SX126X_NUMOF == 1);

            gnrc_netif_lorawan_create(&_netif[i], sx126x_stacks[i],
                                      SX126X_STACKSIZE, SX126X_PRIO,
                                      "sx126x", &sx126x_devs[i].netdev);
        }
        else {
            gnrc_netif_raw_create(&_netif[i], sx126x_stacks[i],
                                  SX126X_STACKSIZE, SX126X_PRIO,
                                  "sx126x", &sx126x_devs[i].netdev);
        }
    }
#endif
}
/** @} */
