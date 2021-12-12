/*
 * Copyright (C) 2021 HAW Hamburg
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
#ifndef OPENDSME_H
#define OPENDSME_H

#include <stdbool.h>
#include "byteorder.h"
#include "net/gnrc/pktbuf.h"

#ifndef CONFIG_OPENDSME_PAYLOAD_LENGTH
#define CONFIG_OPENDSME_PAYLOAD_LENGTH (16U)
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    network_uint16_t addr;
    uint8_t superframe_id;
    uint8_t slot_id;
    uint8_t channel_id;
    bool tx;
} dsme_alloc_t;

int opendsme_init(bool pan_coord);
int opendsme_is_associated(void);
void opendsme_get_short_addr(network_uint16_t *addr);
void opendsme_send_frame(void *addr, size_t addr_len, gnrc_pktsnip_t *pkt);
void opendsme_allocate_gts(uint8_t superframeID, uint8_t slotID,
                           uint8_t channelID, bool tx, uint16_t address);

#ifdef __cplusplus
}
#endif

#endif /* OPENDSME_H */
/** @} */
