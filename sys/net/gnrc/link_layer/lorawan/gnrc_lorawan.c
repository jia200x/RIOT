/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  José Ignacio Alamos <jose.alamos@haw-hamburg.de>
 */
#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/lora.h"
#include "net/gnrc/lorawan.h"
#include "errno.h"
#include "net/gnrc/pktbuf.h"

#include "net/lorawan/hdr.h"
#include "net/loramac.h"
#include "net/gnrc/lorawan/region.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define GNRC_LORAWAN_DL_RX2_DR_MASK       (0x0F)  /**< DL Settings DR Offset mask */
#define GNRC_LORAWAN_DL_RX2_DR_POS        (0)     /**< DL Settings DR Offset pos */
#define GNRC_LORAWAN_DL_DR_OFFSET_MASK    (0x70)  /**< DL Settings RX2 DR mask */
#define GNRC_LORAWAN_DL_DR_OFFSET_POS     (4)     /**< DL Settings RX2 DR pos */

static inline void gnrc_lorawan_mlme_reset(gnrc_lorawan_t *mac)
{
    mac->mlme.activation = MLME_ACTIVATION_NONE;
    mac->mlme.pending_mlme_opts = 0;
    mac->rx_delay = (LORAMAC_DEFAULT_RX1_DELAY/MS_PER_SEC);
    mac->mlme.nid = LORAMAC_DEFAULT_NETID;
}

static inline void gnrc_lorawan_mlme_backoff_init(gnrc_lorawan_t *mac)
{
    mac->mlme.backoff_state = 0;
}

static inline void gnrc_lorawan_mcps_reset(gnrc_lorawan_t *mac)
{
    mac->mcps.ack_requested = false;
    mac->mcps.waiting_for_ack = false;
    mac->mcps.fcnt = 0;
    mac->mcps.fcnt_down = 0;
}

void gnrc_lorawan_init(gnrc_lorawan_t *mac, uint8_t *nwkskey, uint8_t *appskey,
        uint8_t *tx_buf)
{
    mac->nwkskey = nwkskey;
    mac->appskey = appskey;
    mac->tx_buf = tx_buf;
    mac->busy = false;
    gnrc_lorawan_mlme_backoff_init(mac);
    gnrc_lorawan_reset(mac);
}

void gnrc_lorawan_set_rx2_dr(gnrc_lorawan_t *mac, uint8_t rx2_dr)
{
    mac->dl_settings &= ~GNRC_LORAWAN_DL_RX2_DR_MASK;
    mac->dl_settings |= (rx2_dr << GNRC_LORAWAN_DL_RX2_DR_POS) &
        GNRC_LORAWAN_DL_RX2_DR_MASK;
}


void gnrc_lorawan_reset(gnrc_lorawan_t *mac)
{
    gnrc_lorawan_radio_set_cr(mac, LORA_CR_4_5);
    gnrc_lorawan_radio_set_syncword(mac, LORAMAC_DEFAULT_PUBLIC_NETWORK ? LORA_SYNCWORD_PUBLIC
                                                      : LORA_SYNCWORD_PRIVATE);
    gnrc_lorawan_set_rx2_dr(mac, LORAMAC_DEFAULT_RX2_DR);

    mac->toa = 0;
    mac->tx_len = 0;
    gnrc_lorawan_mcps_reset(mac);
    gnrc_lorawan_mlme_reset(mac);
    gnrc_lorawan_channels_init(mac);
}

static void _config_radio(gnrc_lorawan_t *mac, uint32_t channel_freq, uint8_t dr, int rx)
{
    if (channel_freq != 0) {
        gnrc_lorawan_radio_set_frequency(mac, channel_freq); 
    }

    gnrc_lorawan_radio_set_iq_invert(mac, rx);

    gnrc_lorawan_set_dr(mac, dr);

    if (rx) {
        /* Switch to single listen mode */
        gnrc_lorawan_radio_set_rx_symbol_timeout(mac, CONFIG_GNRC_LORAWAN_MIN_SYMBOLS_TIMEOUT);
    }
}

static void _configure_rx_window(gnrc_lorawan_t *mac, uint32_t channel_freq, uint8_t dr)
{
    _config_radio(mac, channel_freq, dr, true);
}

void gnrc_lorawan_open_rx_window(gnrc_lorawan_t *mac)
{
    /* Switch to RX state */
    if (mac->state == LORAWAN_STATE_RX_1) {
        gnrc_lorawan_timer_set(mac, 1000);
    }
    gnrc_lorawan_radio_rx_on(mac);
}

void gnrc_lorawan_event_tx_complete(gnrc_lorawan_t *mac)
{
    mac->state = LORAWAN_STATE_RX_1;

    int rx_1;
    /* if the MAC is not activated, then this is a Join Request */
    rx_1 = mac->mlme.activation == MLME_ACTIVATION_NONE ?
           LORAMAC_DEFAULT_JOIN_DELAY1 : mac->rx_delay;

    gnrc_lorawan_timer_set(mac, rx_1*1000);

    uint8_t dr_offset = (mac->dl_settings & GNRC_LORAWAN_DL_DR_OFFSET_MASK) >>
        GNRC_LORAWAN_DL_DR_OFFSET_POS;
    _configure_rx_window(mac, 0, gnrc_lorawan_rx1_get_dr_offset(mac->last_dr, dr_offset));

    gnrc_lorawan_radio_sleep(mac);
}

void gnrc_lorawan_event_timeout(gnrc_lorawan_t *mac)
{
    (void) mac;
    switch (mac->state) {
        case LORAWAN_STATE_RX_1:
            _configure_rx_window(mac, LORAMAC_DEFAULT_RX2_FREQ, mac->dl_settings & GNRC_LORAWAN_DL_RX2_DR_MASK);
            mac->state = LORAWAN_STATE_RX_2;
            break;
        case LORAWAN_STATE_RX_2:
            gnrc_lorawan_mlme_no_rx(mac);
            gnrc_lorawan_mcps_event(mac, MCPS_EVENT_NO_RX, 0);
            mac->state = LORAWAN_STATE_IDLE;
            gnrc_lorawan_mac_release(mac);
            break;
        default:
            assert(false);
    }
    gnrc_lorawan_radio_sleep(mac);
}

/* This function uses a precomputed table to calculate time on air without
 * using floating point arithmetics */
static uint32_t lora_time_on_air(size_t payload_size, uint8_t dr, uint8_t cr)
{
    assert(dr <= LORAMAC_DR_6);
    uint8_t _K[6][4] = {    { 0, 1, 5, 5 },
                            { 0, 1, 4, 5 },
                            { 1, 5, 5, 5 },
                            { 1, 4, 5, 4 },
                            { 1, 3, 4, 4 },
                            { 1, 2, 4, 3 } };

    uint32_t t_sym = 1 << (15 - dr);
    uint32_t t_preamble = (t_sym << 3) + (t_sym << 2) + (t_sym >> 2);

    int index = dr < LORAMAC_DR_6 ? dr : LORAMAC_DR_5;
    uint8_t n0 = _K[index][0];
    int nb_symbols;

    uint8_t offset = _K[index][1];
    if (payload_size < offset) {
        nb_symbols = 8 + n0 * cr;
    }
    else {
        uint8_t c1 = _K[index][2];
        uint8_t c2 = _K[index][3];
        uint8_t pos = (payload_size - offset) % (c1 + c2);
        uint8_t cycle = (payload_size - offset) / (c1 + c2);
        nb_symbols = 8 + (n0 + 2 * cycle + 1 + (pos > (c1 - 1))) * cr;
    }

    uint32_t t_payload = t_sym * nb_symbols;
    return t_preamble + t_payload;
}

void gnrc_lorawan_send_pkt(gnrc_lorawan_t *mac, iolist_t *io, uint8_t dr)
{
    mac->state = LORAWAN_STATE_TX;

    uint32_t chan = gnrc_lorawan_pick_channel(mac);
    _config_radio(mac, chan, dr, false);

    mac->last_dr = dr;
    mac->toa = lora_time_on_air(iolist_size(io), dr, LORA_CR_4_5 + 4);

    gnrc_lorawan_radio_send(mac, io);
}

void gnrc_lorawan_process_pkt(gnrc_lorawan_t *mac, uint8_t *data, size_t size)
{
    gnrc_lorawan_radio_sleep(mac);
    mac->state = LORAWAN_STATE_IDLE;
    gnrc_lorawan_timer_stop(mac);

    uint8_t mtype = (*data & MTYPE_MASK) >> 5;
    switch (mtype) {
        case MTYPE_JOIN_ACCEPT:
            gnrc_lorawan_mlme_process_join(mac, data, size);
            break;
        case MTYPE_CNF_DOWNLINK:
        case MTYPE_UNCNF_DOWNLINK:
            gnrc_lorawan_mcps_process_downlink(mac, data, size);
            break;
        default:
            break;
    }

    gnrc_lorawan_mac_release(mac);
}

const netdev_driver_t gnrc_lorawan_driver = {
    .init = netdev_init_pass,
    .send = netdev_send_pass,
    .recv = netdev_recv_pass,
    .get = netdev_get_pass,
    .set = netdev_set_pass,
    .isr = netdev_isr_pass,
};

void gnrc_lorawan_setup(gnrc_lorawan_t *mac, netdev_t *lower)
{
    mac->netdev.driver = &gnrc_lorawan_driver;
    mac->netdev.lower = lower;
    lower->context = mac;
}

void gnrc_lorawan_timer_fired(gnrc_lorawan_t *mac)
{
    if(mac->state == LORAWAN_STATE_IDLE)
    {
        iolist_t pkt = {
            .iol_base = mac->tx_buf,
            .iol_len = mac->tx_len,
            .iol_next = NULL
        };
        gnrc_lorawan_send_pkt(mac, &pkt, mac->last_dr);
    }
    else {
        gnrc_lorawan_open_rx_window(mac);
    }
}

/** @} */
