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
#ifndef PKG_OPENDSME_SETTINGS_H
#define PKG_OPENDSME_SETTINGS_H

#include <stdint.h>

#include <stdio.h>
#include "kernel_defines.h"
#include "net/ieee802154.h"
#include "opendsme.h"

#define DSME_MAX_LOST_BEACONS CONFIG_OPENDSME_MAX_LOST_BEACONS

namespace dsme {

namespace const_redefines {
constexpr uint8_t aNumSuperframeSlots = 16; /* fixed value, see Table 8-80 of IEEE 802.15.4-2015, this includes beacon, CAP and GTS */
constexpr uint8_t macSIFSPeriod = 12; /* fixed value, see 8.1.3 of IEEE 802.15.4-2011 (assuming no UWB PHY) */
constexpr uint8_t macLIFSPeriod = 40; /* fixed value, see 8.1.3 of IEEE 802.15.4-2011 (assuming no UWB PHY) */
}

constexpr uint8_t PRE_EVENT_SHIFT = const_redefines::macLIFSPeriod;
constexpr uint8_t MIN_CSMA_SLOTS = 0; /* 0 for CAP reduction */
constexpr uint8_t MAX_GTSLOTS = const_redefines::aNumSuperframeSlots - MIN_CSMA_SLOTS - 1;
constexpr uint8_t MAX_CHANNELS = 16;
constexpr uint8_t MIN_CHANNEL = 11;
constexpr uint8_t MAC_DEFAULT_CHANNEL = CONFIG_IEEE802154_DEFAULT_CHANNEL;
constexpr uint8_t MAX_NEIGHBORS = CONFIG_OPENDSME_MAX_NEIGHBOURS;

constexpr uint16_t MAC_DEFAULT_NWK_ID = CONFIG_IEEE802154_DEFAULT_PANID;

constexpr uint8_t MIN_SO = CONFIG_IEEE802154_DSME_SUPERFRAME_ORDER;
constexpr uint8_t MAX_BO = CONFIG_IEEE802154_DSME_MULTISUPERFRAME_ORDER;
constexpr uint8_t MAX_MO = CONFIG_IEEE802154_DSME_BEACON_ORDER;
constexpr uint16_t MAX_SLOTS_PER_SUPERFRAMES = 1 << (uint16_t)(MAX_BO - MIN_SO);
constexpr uint16_t MAX_TOTAL_SUPERFRAMES = 1 << (uint16_t)(MAX_BO - MIN_SO);
constexpr uint16_t MAX_SUPERFRAMES_PER_MULTI_SUPERFRAME = 1 << (uint16_t)(MAX_MO - MIN_SO);
constexpr uint16_t MAX_OCCUPIED_SLOTS = MAX_SUPERFRAMES_PER_MULTI_SUPERFRAME*MAX_GTSLOTS*MAX_CHANNELS;

constexpr uint8_t MAX_SAB_UNITS = 1;

constexpr uint16_t CAP_QUEUE_SIZE = CONFIG_OPENDSME_CAP_QUEUE_SIZE;
constexpr uint16_t TOTAL_GTS_QUEUE_SIZE = CONFIG_OPENDSME_CFP_QUEUE_SIZE;

/* Take openDSME default value */
constexpr uint16_t UPPER_LAYER_QUEUE_SIZE = 4;

constexpr uint16_t DSME_BROADCAST_PAN_ID = 0xffff;

/* Defaults from openDSME */
constexpr uint8_t ADDITIONAL_ACK_WAIT_DURATION = 63;
}

#endif /* PKG_OPENDSME_SETTINGS_H */

/** @} */
