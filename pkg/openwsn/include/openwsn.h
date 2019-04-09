/*
 * Copyright (C) 2018 Hamburg University of Applied Sciences
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    pkg_openwsn    OpenWSN
 * @ingroup     pkg
 * @brief       An IoT Network Stack Umplementing 6TiSCH
 * @see         https://github.com/openwsn-berkeley/openwsn-fw
 *
 *
 * @{
 *
 * @file
 * @brief   OpenWSN bootstrap definitions
 *
 * @author  Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 */
#ifndef OPENWSN_H
#define OPENWSN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OW_SCHED_MSG_TYPE_EVENT    0x667    // TODO ADAPT!!!
#define OW_NETDEV_MSG_TYPE_EVENT   0x666    // TODO ADAPT!!!

typedef enum {
    OW_CELL_ADV=0,
    OW_CELL_RX,
    OW_CELL_TX
} ow_cell_t;

typedef enum {
    OW_ROLE_PAN_COORDINATOR=0,
    OW_ROLE_COORDINATOR,
    OW_ROLE_LEAF
} ow_role_t;

/**
 * @brief Perform MLME-SET_LINK request.
 *
 * @param add True if it's requested to add a cell. False otherwise
 * @param slot_offset Slot offset.
 * @param type Cell type
 * @param shared True if the link is shared with other nodes. False otherwise
 * @param ch_offset Channel offset
 * @param addr The address of the neighbour. Set to NULL in case of ADV cell.
 *
 * @return 0 on success
 * @return -EINVAL on failure
 */
int openwsn_mlme_set_link_request(bool add, int slot_offset, ow_cell_t type, bool shared, int ch_offset,
        uint8_t *addr);

/**
 * @brief Set the role
 *
 * @param role the new role
 */
void openwsn_mlme_set_role(ow_role_t role);

/**
 * @brief Perform MLME-SET_SLOTFRAME request
 *
 * @param size length of the new slotframe
 *
 * @return 0 on success.
 * @return -EINVAL on failure
 */
int openwsn_mlme_set_slotframe_request(uint8_t size);

/**
 * @brief Perform MCPS-DATA request
 *
 * @param dest 64-bit destination address
 * @param data data to be sent
 * @param data_len length of the data
 */
void openwsn_mcps_data_request(char *dest, char *data, size_t data_len);

/**
 * @brief   Initializes the OpenWSN stack.
 */
void openwsn_bootstrap(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENWSN_H */
/** @} */
