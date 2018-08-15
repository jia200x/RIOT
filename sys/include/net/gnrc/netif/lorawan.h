/*
 * Copyright (C) 2017 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup net_gnrc_netif
 * @{
 *
 * @file
 * @brief  LoRaWAN adaption
 *
 * @author  Jose Ignacio Alamos <jose.alamos@haw-hamburg.de>
 */
#ifndef NET_GNRC_NETIF_LORAWAN_H
#define NET_GNRC_NETIF_LORAWAN_H

#define GNRC_LORAWAN_EUI_LEN 8
#define GNRC_LORAWAN_KEY_LEN 16

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *deveui[GNRC_LORAWAN_EUI_LEN];
    uint8_t *appeui[GNRC_LORAWAN_EUI_LEN];
    uint8_t *appkey[GNRC_LORAWAN_KEY_LEN];
} gnrc_netif_lorawan_t;

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_NETIF_LORAWAN_H */
/** @} */
