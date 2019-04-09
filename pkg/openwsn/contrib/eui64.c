/*
 * Copyright (C) 2017 Hamburg University of Applied Sciences
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     pkg_openwsn
 * @{
 *
 * @file
 * @brief       RIOT adaption definition of the "eui64" bsp module
 *
 * @author      Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 *
 * @}
 */

#include "string.h"
#include "eui64.h"

#include "luid.h"
#include "net/ieee802154.h"

void eui64_get(uint8_t *addressToWrite)
{
    uint8_t addr[IEEE802154_LONG_ADDRESS_LEN];
    luid_get((void *)addr, IEEE802154_LONG_ADDRESS_LEN);

    for(unsigned i=0;i<IEEE802154_LONG_ADDRESS_LEN;i++) {
        addressToWrite[i] = addr[IEEE802154_LONG_ADDRESS_LEN-i-1];
    }
}
