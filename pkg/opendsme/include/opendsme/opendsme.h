/*
 * Copyright (C) 2022 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     pkg_opendsme
 *
 * @{
 *
 * @file
 *
 * @author      José I. Álamos <jose.alamos@haw-hamburg.de>
 */
#ifndef PKG_OPENDSME_H
#define PKG_OPENDSME_H

#include <stdbool.h>
#include "byteorder.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/pktbuf.h"

/**
 * @brief default GNRC Pktsnip type for openDSME packets
 */
#ifndef CONFIG_OPENDSME_GNRC_PKTSNIP_TYPE
#define CONFIG_OPENDSME_GNRC_PKTSNIP_TYPE   GNRC_NETTYPE_SIXLOWPAN
#endif

/**
 * @brief maximum number of DSME neighbours
 */
#ifndef CONFIG_OPENDSME_MAX_NEIGHBOURS
#define CONFIG_OPENDSME_MAX_NEIGHBOURS      (20U)
#endif

/**
 * @brief maximum number of lost beacons before assuming the device desynchronized.
 */
#ifndef CONFIG_OPENDSME_MAX_LOST_BEACONS
#define CONFIG_OPENDSME_MAX_LOST_BEACONS    (8U)
#endif

/**
 * @brief DSME CAP queue size (for CSMA/CA transmissions)
 */
#ifndef CONFIG_OPENDSME_CAP_QUEUE_SIZE
#define CONFIG_OPENDSME_CAP_QUEUE_SIZE    (8U)
#endif

/**
 * @brief DSME CFP queue size (for GTS transmissions)
 */
#ifndef CONFIG_OPENDSME_CFP_QUEUE_SIZE
#define CONFIG_OPENDSME_CFP_QUEUE_SIZE    (22U)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DSME Allocation descriptor
 */
typedef struct {
    network_uint16_t addr;  /* neighbour address */
    uint8_t superframe_id;  /* superframe ID */
    uint8_t slot_id;        /* slot ID */
    uint8_t channel_id;     /* channel ID */
    bool tx;                /* whether the GTS is TX */
} ieee802154_dsme_alloc_t;

/**
 * @brief   Creates an openDSME network interface
 *
 * @param[out] netif    The interface. May not be `NULL`.
 * @param[in] stack     The stack for the network interface's thread.
 * @param[in] stacksize Size of @p stack.
 * @param[in] priority  Priority for the network interface's thread.
 * @param[in] name      Name for the network interface. May be NULL.
 * @param[in] dev       Device for the interface
 *
 * @see @ref gnrc_netif_create()
 *
 * @return  0 on success
 * @return  negative number on error
 */
int gnrc_netif_opendsme_create(gnrc_netif_t *netif, char *stack, int stacksize,
                                 char priority, const char *name, netdev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* PKG_OPENDSME_H */

/** @} */
