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

#include "xtimer.h"
#include "msg.h"

#define GNRC_LORAWAN_EUI_LEN 8
#define GNRC_LORAWAN_KEY_LEN 16

#ifdef __cplusplus
extern "C" {
#endif

/* TODO: Only store neessary keys! */
typedef struct {
    uint8_t deveui[GNRC_LORAWAN_EUI_LEN];
    uint8_t appeui[GNRC_LORAWAN_EUI_LEN];
    uint8_t appkey[GNRC_LORAWAN_KEY_LEN];
    uint8_t nwkskey[GNRC_LORAWAN_KEY_LEN];
    uint8_t appskey[GNRC_LORAWAN_KEY_LEN];
    uint8_t state;
    uint8_t dev_nonce[2];
    le_uint32_t dev_addr;
    uint8_t datarate;
    uint32_t fcnt;
    uint8_t dl_settings;
    uint8_t rx_delay;
    xtimer_t rx_1;
    xtimer_t rx_2;
    msg_t msg;
    msg_t msg_2;
    uint8_t joined;
    uint8_t confirmed_data;
    uint8_t port;
    uint8_t fopts[32];
    uint8_t last_margin;
    uint8_t last_num_gateways;
} gnrc_netif_lorawan_t;

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_NETIF_LORAWAN_H */
/** @} */
