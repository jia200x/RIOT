/*
 * Copyright (C) 2019 HAW Hamburg
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
 * @brief  LoRaWAN adaption for @ref net_gnrc_netif
 *
 * @author  Jose Ignacio Alamos <jose.alamos@haw-hamburg.de>
 */
#ifndef NET_GNRC_NETIF_LORAWAN_H
#define NET_GNRC_NETIF_LORAWAN_H

#include "gnrc_lorawan/lorawan.h"
#include "net/loramac.h"
#include "xtimer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   GNRC LoRaWAN interface descriptor
 */
typedef struct {
    gnrc_lorawan_t mac;                     /**< gnrc lorawan mac descriptor */
    xtimer_t rx;                                    /**< RX timer */
    msg_t msg;                                      /**< MAC layer message descriptor */
    xtimer_t backoff_timer;                 /**< timer used for backoff expiration */
    msg_t backoff_msg;                      /**< msg for backoff expiration */
    gnrc_pktsnip_t *rx_pkt;
    gnrc_pktsnip_t *outgoing_pkt;
    uint8_t nwkskey[LORAMAC_NWKSKEY_LEN];   /**< network SKey buffer */
    uint8_t appskey[LORAMAC_APPSKEY_LEN];   /**< App SKey buffer */
    uint8_t demod_margin;                   /**< value of last demodulation margin */
    uint8_t num_gateways;                   /**< number of gateways of last link check */
    uint8_t datarate;                       /**< LoRaWAN datarate for the next transmission */
    uint8_t port;                           /**< LoRaWAN port for the next transmission */
    uint8_t deveui[LORAMAC_DEVEUI_LEN];     /**< Device EUI buffer */
    uint8_t appeui[LORAMAC_APPEUI_LEN];     /**< App EUI buffer */
    uint8_t appkey[LORAMAC_APPKEY_LEN];     /**< App Key buffer */
    int ack_req;                            /**< Request ACK in the next transmission */
    int otaa;                               /**< wether the next transmission is OTAA or not */
} gnrc_netif_lorawan_t;

#define MSG_TYPE_TIMEOUT             (0x3457)           /**< Timeout message type */
#define MSG_TYPE_MLME_BACKOFF_EXPIRE (0x3459)           /**< Backoff timer expiration message type */


#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_NETIF_LORAWAN_H */
/** @} */
