/*
 * Copyright (C) 2016 Fundación Inria Chile
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_lorawan_hdr    LoRaWAN header
 * @ingroup     net_lorawan
 * @brief       LoRaWAN header types and helper functions
 * @{
 *
 * @file
 * @brief   LoRaWAN header type and helper function definitions
 *
 * @author  José Ignacio Alamos <jose.alamos@inria.cl>
 */

#ifndef LW_HDR_H_
#define LW_HDR_H_

#include <stdint.h>

#include "byteorder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Data type to represent a LoRaWAN packet header
 *
 * @details This definition includes MHDR and FHDR in the same structure. The structure of the header is as follows:
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.unparsed}
 *                      1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 * |Mtype| RFU |Maj|           LoRaWAN Address                   ...
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * ...             | Frame Control |         Frame Counter         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * @see <a href="https://www.lora-alliance.org/portals/0/specs/LoRaWAN%20Specification%201R0.pdf">
 *          LoRaWAN spcefication, section 4
 *      </a>
 */
typedef struct __attribute__((packed)) {
    /**
     * @brief message type and major version
     *
     * @details the message type are the 3 most significant bytes and the
     * major version are the 2 less significant bytes. There are 3 bytes
     * in the middle reserved for future usage.
     * This module provides helper functions to set and get:
     * * lorawan_hdr_set_mtype()
     * * lorawan_hdr_get_mtype()
     * * lorawan_hdr_set_maj()
     * * lorawan_hdr_get_maj()
     */
    uint8_t mt_maj;
    le_uint32_t addr; /**< 32 bit LoRaWAN address */
    uint8_t fctrl;    /**< frame control */
    le_uint16_t fcnt; /**< frame counter */
    uint8_t port;     /**< port */
} lorawan_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t mt_maj;
    le_uint64_t app_eui;
    le_uint64_t dev_eui;
    le_uint16_t dev_nonce;
    le_uint32_t mic;
} lorawan_join_request_t;

typedef struct __attribute__((packed)) {
    uint8_t mt_maj;
    uint8_t app_nonce[3];
    uint8_t net_id[3];
    uint32_t dev_addr[4];
    uint8_t dl_settings;
    uint8_t rx_delay;
} lorawan_join_accept_t;

#include <stdio.h>
static inline void lorawan_hdr_set_mtype(lorawan_hdr_t *hdr, uint8_t mtype)
{
    hdr->mt_maj &= 0x1f;
    hdr->mt_maj |= mtype << 5;
}

static inline uint8_t lorawan_hdr_get_mtype(lorawan_hdr_t *hdr)
{
    return ((hdr->mt_maj & 0xe0) >> 5);
}

static inline void lorawan_hdr_set_maj(lorawan_hdr_t *hdr, uint8_t maj)
{
    hdr->mt_maj &= 0xfc;
    hdr->mt_maj |= maj & 0x03;
}

static inline uint8_t lorawan_hdr_get_maj(lorawan_hdr_t *hdr)
{
    return (hdr->mt_maj & 0x03);
}

static inline void lorawan_hdr_set_adr(lorawan_hdr_t *hdr, bool adr)
{
    hdr->fctrl &= 0x7F;
    hdr->fctrl |= (adr << 7);
}

static inline bool lorawan_hdr_get_adr(lorawan_hdr_t *hdr)
{
    return (hdr->fctrl & 0x80);
}

static inline void lorawan_hdr_set_adr_ack_req(lorawan_hdr_t *hdr, bool adr_ack_req)
{
    hdr->fctrl &= 0xBF;
    hdr->fctrl |= (adr_ack_req << 6);
}

static inline bool lorawan_hdr_get_adr_ack_req(lorawan_hdr_t *hdr)
{
    return (hdr->fctrl & 0x40);
}

static inline void lorawan_hdr_set_ack(lorawan_hdr_t *hdr, bool ack)
{
    hdr->fctrl &= 0xDF;
    hdr->fctrl |= (ack << 5);
}

static inline bool lorawan_hdr_get_ack(lorawan_hdr_t *hdr)
{
    return (hdr->fctrl & 0x20);
}

static inline void lorawan_hdr_set_frame_pending(lorawan_hdr_t *hdr, bool frame_pending)
{
    hdr->fctrl &= 0xEF;
    hdr->fctrl |= (frame_pending << 4);
}

static inline bool lorawan_hdr_get_frame_pending(lorawan_hdr_t *hdr)
{
    return (hdr->fctrl & 0x10);
}

static inline void lorawan_hdr_set_frame_opts_len(lorawan_hdr_t *hdr, uint8_t len)
{
    hdr->fctrl &= 0xF0;
    hdr->fctrl |= (len & 0x0F);
}

static inline uint8_t lorawan_hdr_get_frame_opts_len(lorawan_hdr_t *hdr)
{
    return (hdr->fctrl & 0x0F);
}
#endif
