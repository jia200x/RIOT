/*
 * Copyright (C) 2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup         pkg_loramac-semtech
 * @brief           Default definitions for Semtech LoRaMAC package
 * @{
 *
 * @file
 * @brief           Default definitions for Semtech LoRaMAC package
 *
 * @author          Alexandre Abadie <alexandre.abadie@inria.fr>
 */

#ifndef LORAMAC_PARAMS_H
#define LORAMAC_PARAMS_H

/**
 * @brief   Default device EUI
 *
 *          8 bytes key, required for join procedure
 */
#define LORAMAC_DEV_EUI_DEFAULT                     { 0x00, 0x94, 0xAF, 0x00, 0x4D, 0x98, 0xCA, 0xC7 }

/**
 * @brief   Default application EUI
 *
 *          8 bytes key, required for join procedure
 */
#define LORAMAC_APP_EUI_DEFAULT                     { 0x70, 0xB3, 0xD5, 0x7E, 0xF0, 0x00, 0x64, 0x05 }

/**
 * @brief   Default application key
 *
 *          16 bytes key, required for join procedure
 */
#define LORAMAC_APP_KEY_DEFAULT                     { 0x19, 0xAC, 0xC3, 0xDB, 0xA6, 0xC9, 0xB1, 0x35, 0x05, 0xB4, 0xAA, 0xFA, 0x63, 0x81, 0xEA, 0x0F }

/**
 * @brief   Default device address
 */
#define LORAMAC_DEV_ADDR_DEFAULT                    (uint32_t)0x2601174B

/**
 * @brief   Default network session key
 *
 *          16 bytes key, only required for ABP join procedure type.
 */
#define LORAMAC_NET_SKEY_DEFAULT                    { 0x7F, 0x20, 0x50, 0x32, 0x38, 0x0A, 0xD0, 0x54, 0x8F, 0x09, 0x8F, 0x96, 0x4C, 0x78, 0x8A, 0x1E }

/**
 * @brief   Default application session key
 *
 *          16 bytes key, only required for ABP join procedure type
 */
#define LORAMAC_APP_SKEY_DEFAULT                    { 0xA3, 0xBF, 0xEC, 0xC8, 0xD6, 0x99, 0xCC, 0x79, 0x8A, 0x0D, 0xE4, 0x04, 0xA1, 0xE6, 0xAF, 0x73 }

#endif /* LORAMAC_PARAMS_H */