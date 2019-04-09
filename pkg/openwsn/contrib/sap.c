/*
 * Copyright (C) 2019 Hamburg University of Applied Sciences
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
 *
 * @author  José I. Alamos <jose.alamos@haw-hamburg.de>
 * @}
 */

#ifdef OW_MAC_ONLY
#include <stdio.h>

#include "errno.h"
#include "02b-MAChigh/schedule.h"
#include "cross-layers/idmanager.h"
#include "openwsn.h"

extern void ow_mcps_data_confirm(int status);
extern void ow_mcps_data_indication(char *data, size_t data_len);

void upper_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   (void) msg;
   (void) error;
    ow_mcps_data_confirm(error == E_SUCCESS ? 0 : -EINVAL);
    openqueue_freePacketBuffer(msg);
}

void upper_receive(OpenQueueEntry_t* msg) {
    ow_mcps_data_indication(msg->payload, msg->length);
    openqueue_freePacketBuffer(msg);
}

int openwsn_mlme_set_link_request(bool add, int slot_offset, ow_cell_t type, bool shared, int ch_offset,
        uint8_t *addr)
{
    int res;
    open_addr_t temp_addr;
    memset(&temp_addr,0,sizeof(temp_addr));

    temp_addr.type = ADDR_ANYCAST;

    if(addr != NULL) {
        temp_addr.type = ADDR_64B;
        memcpy(temp_addr.addr_64b, addr, 8);
    }

    int cell_type;
    switch(type) {
        case OW_CELL_ADV:
            cell_type = CELLTYPE_TXRX;
            break;
        case OW_CELL_TX:
            cell_type = CELLTYPE_TX;
            break;
        case OW_CELL_RX:
            cell_type = CELLTYPE_RX;
            break;
        default:
            break;
    }

    if(add) { 
        res = schedule_addActiveSlot(slot_offset, cell_type, shared,
                    ch_offset, &temp_addr);
    }
    else {
        res = schedule_removeActiveSlot(slot_offset, &temp_addr);
    }
    
    return res == E_SUCCESS ? 0 : -EINVAL;
}

void openwsn_mlme_set_role(ow_role_t role)
{
    if(role == OW_ROLE_PAN_COORDINATOR) {
        idmanager_setRole(ROLE_PAN_COORDINATOR);
    }
    else {
        idmanager_setRole(ROLE_COORDINATOR);
    }
}

int openwsn_mlme_set_slotframe_request(uint8_t size)
{
    if(schedule_getFrameLength() == 0) {
        schedule_setFrameLength(size);
        schedule_setFrameHandle(SCHEDULE_MINIMAL_6TISCH_DEFAULT_SLOTFRAME_HANDLE);
        schedule_setFrameNumber(SCHEDULE_MINIMAL_6TISCH_DEFAULT_SLOTFRAME_NUMBER);
        return 0;
    }
    return -EINVAL;
}

void openwsn_mcps_data_request(char *dest, char *data, size_t data_len)
{
    OpenQueueEntry_t *test_pkt;

    /* get a free packet buffer */
    test_pkt = openqueue_getFreePacketBuffer(COMPONENT_SIXTOP);
    if (test_pkt == NULL) {
        ow_mcps_data_confirm(-ENOBUFS);
        return;
    }

    test_pkt->owner = COMPONENT_NULL;
    test_pkt->creator = COMPONENT_NULL;

    /* add payload */
    packetfunctions_reserveHeaderSize(test_pkt, data_len);
    memcpy(&test_pkt->payload[0], data, data_len);

    if(dest != NULL) {
        test_pkt->l2_nextORpreviousHop.type        = ADDR_64B;
        memcpy(test_pkt->l2_nextORpreviousHop.addr_64b, dest, 8);
    }
    else {
        test_pkt->l2_nextORpreviousHop.type        = ADDR_16B;
        test_pkt->l2_nextORpreviousHop.addr_16b[0] = 0xff;
        test_pkt->l2_nextORpreviousHop.addr_16b[1] = 0xff;
    }

    sixtop_send(test_pkt);
}
#else
typedef int dont_be_pedantic;
#endif
