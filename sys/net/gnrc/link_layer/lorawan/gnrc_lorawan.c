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
 * @}
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "errno.h"
#include "kernel_defines.h"

#include "net/lora.h"
#include "net/gnrc/lorawan.h"
#include "net/gnrc/pktbuf.h"
#include "net/lorawan/hdr.h"
#include "net/loramac.h"
#include "net/gnrc/lorawan/region.h"
#include "timex.h"

#if IS_USED(MODULE_GNRC_LORAWAN_1_1)
#include "periph/flashpage.h"
#endif

#define ENABLE_DEBUG 0
#include "debug.h"

#define GNRC_LORAWAN_DL_RX2_DR_MASK       (0x0F)    /**< DL Settings DR Offset mask */
#define GNRC_LORAWAN_DL_RX2_DR_POS        (0)       /**< DL Settings DR Offset pos */
#define GNRC_LORAWAN_DL_DR_OFFSET_MASK    (0x70)    /**< DL Settings RX2 DR mask */
#define GNRC_LORAWAN_DL_DR_OFFSET_POS     (4)       /**< DL Settings RX2 DR pos */

static gnrc_lorawan_fsm_status_t _state_idle(gnrc_lorawan_t *mac, gnrc_lorawan_event_t ev);
static gnrc_lorawan_fsm_status_t _state_wait_rx_window(gnrc_lorawan_t *mac, gnrc_lorawan_event_t ev);
static gnrc_lorawan_fsm_status_t _state_rx_window(gnrc_lorawan_t *mac, gnrc_lorawan_event_t ev);
static gnrc_lorawan_fsm_status_t _state_tx(gnrc_lorawan_t *mac, gnrc_lorawan_event_t ev);
static void _ev_timer(event_t *ev);
static void _ev_aloha(event_t *ev);
static void _ev_toa(event_t *ev);

static inline void gnrc_lorawan_mlme_reset(gnrc_lorawan_t *mac)
{
    mac->mlme.activation = MLME_ACTIVATION_NONE;
    mac->mlme.pending_mlme_opts = 0;
    mac->rx_delay = (CONFIG_LORAMAC_DEFAULT_RX1_DELAY / MS_PER_SEC);
    mac->mlme.nid = CONFIG_LORAMAC_DEFAULT_NETID;
    memset(mac->mlme.dev_nonce, 0x00, sizeof(mac->mlme.dev_nonce));
}

static inline void gnrc_lorawan_mlme_backoff_init(gnrc_lorawan_t *mac)
{
    mac->mlme.backoff_state = 0;

    gnrc_lorawan_mlme_backoff_expire_cb(mac);
}

static inline void gnrc_lorawan_mcps_reset(gnrc_lorawan_t *mac)
{
    mac->mcps.ack_requested = false;
    mac->mcps.waiting_for_ack = false;
    mac->mcps.fcnt = 0;
    mac->mcps.fcnt_down = 0;
    gnrc_lorawan_set_uncnf_redundancy(mac, CONFIG_LORAMAC_DEFAULT_REDUNDANCY);
}

void gnrc_lorawan_set_rx2_dr(gnrc_lorawan_t *mac, uint8_t rx2_dr)
{
    mac->dl_settings &= ~GNRC_LORAWAN_DL_RX2_DR_MASK;
    mac->dl_settings |= (rx2_dr << GNRC_LORAWAN_DL_RX2_DR_POS) &
                        GNRC_LORAWAN_DL_RX2_DR_MASK;
}

static void _sleep_radio(gnrc_lorawan_t *mac)
{
    netdev_t *dev = gnrc_lorawan_get_netdev(mac);
    netopt_state_t state = NETOPT_STATE_SLEEP;

    dev->driver->set(dev, NETOPT_STATE, &state, sizeof(state));
}

static void _load_persistent_state(gnrc_lorawan_t *mac)
{
    (void) mac;
#if IS_USED(MODULE_GNRC_LORAWAN_1_1)
    void *addr = flashpage_addr(GNRC_LORAWAN_STATE_FLASHPAGE_NUM);
    gnrc_lorawan_persistent_state_t state = { 0 };

    memcpy(&state, addr, sizeof(state));

    if (state.initialized_marker != GNRC_LORAWAN_INITIALIZED_MARKER) {
        memset(&state, 0x0, sizeof(state));
        state.initialized_marker = GNRC_LORAWAN_INITIALIZED_MARKER;

        flashpage_erase(GNRC_LORAWAN_STATE_FLASHPAGE_NUM);
        /* ensure written length is multiple of FLASHPAGE_WRITE_BLOCK_SIZE */
        flashpage_write(addr, &state, (sizeof(state) /
                                           FLASHPAGE_WRITE_BLOCK_SIZE + 0x1) *
                            FLASHPAGE_WRITE_BLOCK_SIZE);
    }
    memcpy(mac->mlme.dev_nonce, state.dev_nonce, sizeof(mac->mlme.dev_nonce));
#endif
}

void gnrc_lorawan_init(gnrc_lorawan_t *mac, uint8_t *joineui, const gnrc_lorawan_key_ctx_t *ctx, event_queue_t *evq)
{
    DEBUG("Lorawan init !\n");
    mac->joineui = joineui;

    memcpy(&mac->ctx, ctx, sizeof(gnrc_lorawan_key_ctx_t));

    mac->busy = false;
    mac->ev_timer.handler = _ev_timer;
    mac->ev_aloha.handler = _ev_aloha;
    mac->ev_toa.handler = _ev_toa;

    event_timeout_ztimer_init(&mac->evt, ZTIMER_MSEC, evq, &mac->ev_timer);
    event_timeout_ztimer_init(&mac->evt_aloha, ZTIMER_MSEC, evq, &mac->ev_aloha);
    event_timeout_ztimer_init(&mac->evt_toa, ZTIMER_MSEC, evq, &mac->ev_toa);

    gnrc_lorawan_mlme_backoff_init(mac);
    gnrc_lorawan_reset(mac);

    if (IS_USED(MODULE_GNRC_LORAWAN_1_1)) {
        _load_persistent_state(mac);
    }

    event_timeout_set(&mac->evt_toa, GNRC_LORAWAN_BACKOFF_WINDOW_TICK);
}

void gnrc_lorawan_reset(gnrc_lorawan_t *mac)
{
    netdev_t *dev = gnrc_lorawan_get_netdev(mac);
    uint8_t cr = LORA_CR_4_5;

    dev->driver->set(dev, NETOPT_CODING_RATE, &cr, sizeof(cr));

    uint8_t syncword = IS_ACTIVE(CONFIG_LORAMAC_DEFAULT_PRIVATE_NETWORK) ? LORA_SYNCWORD_PRIVATE
                                                                         : LORA_SYNCWORD_PUBLIC;

    dev->driver->set(dev, NETOPT_SYNCWORD, &syncword, sizeof(syncword));

    /* Continuous reception */
    uint32_t rx_timeout = 0;

    dev->driver->set(dev, NETOPT_RX_TIMEOUT, &rx_timeout, sizeof(rx_timeout));

    gnrc_lorawan_set_rx2_dr(mac, CONFIG_LORAMAC_DEFAULT_RX2_DR);

    mac->toa = 0;
    gnrc_lorawan_mcps_reset(mac);
    gnrc_lorawan_mlme_reset(mac);
    gnrc_lorawan_channels_init(mac);
    mac->curr_state = _state_idle;
}

void gnrc_lorawan_store_dev_nonce(uint8_t *dev_nonce)
{
    (void) dev_nonce;
#if IS_USED(MODULE_GNRC_LORAWAN_1_1)
    void *addr = flashpage_addr(GNRC_LORAWAN_STATE_FLASHPAGE_NUM);
    gnrc_lorawan_persistent_state_t state = { 0 };
    memcpy(&state, addr, sizeof(state));

    memcpy(state.dev_nonce, dev_nonce, sizeof(state.dev_nonce));

    flashpage_erase(GNRC_LORAWAN_STATE_FLASHPAGE_NUM);
    /* ensure written length is multiple of FLASHPAGE_WRITE_BLOCK_SIZE */
    flashpage_write(addr, &state, (sizeof(state) /
                    FLASHPAGE_WRITE_BLOCK_SIZE + 0x1) *
                    FLASHPAGE_WRITE_BLOCK_SIZE);
#endif
}

static void _config_radio(gnrc_lorawan_t *mac, uint32_t channel_freq,
                          uint8_t dr, int rx)
{
    netdev_t *dev = gnrc_lorawan_get_netdev(mac);

    if (channel_freq != 0) {
        dev->driver->set(dev, NETOPT_CHANNEL_FREQUENCY, &channel_freq,
                         sizeof(channel_freq));
    }

    netopt_enable_t iq_invert = rx;

    dev->driver->set(dev, NETOPT_IQ_INVERT, &iq_invert, sizeof(iq_invert));

    gnrc_lorawan_set_dr(mac, dr);

    if (rx) {
        /* Switch to single listen mode */
        const netopt_enable_t single = true;
        dev->driver->set(dev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
        const uint16_t timeout = CONFIG_GNRC_LORAWAN_MIN_SYMBOLS_TIMEOUT;
        dev->driver->set(dev, NETOPT_RX_SYMBOL_TIMEOUT, &timeout,
                         sizeof(timeout));
    }
}

static void _configure_rx_window(gnrc_lorawan_t *mac, uint32_t channel_freq,
                                 uint8_t dr)
{
    _config_radio(mac, channel_freq, dr, true);
}

void gnrc_lorawan_dispatch_event(gnrc_lorawan_t *mac, gnrc_lorawan_event_t event)
{
    gnrc_lorawan_state_t last_state = mac->curr_state;
    int last_event = event;
    int res;

    /* This line may update the state if there's a state transition */
    while ((res = mac->curr_state(mac, last_event)) == GNRC_LORAWAN_FSM_TRANSITION) {
        res = last_state(mac, GNRC_LORAWAN_EV_EXIT);
        assert(res != GNRC_LORAWAN_FSM_TRANSITION);
        last_event = GNRC_LORAWAN_EV_ENTRY;
    }
}

static void _ev_toa(event_t *ev)
{
    gnrc_lorawan_t *mac = container_of(ev, gnrc_lorawan_t, ev_toa);
    gnrc_lorawan_mlme_backoff_expire_cb(mac);
    event_timeout_set(&mac->evt_toa, GNRC_LORAWAN_BACKOFF_WINDOW_TICK);
}

static void _ev_aloha(event_t *ev)
{
    gnrc_lorawan_t *mac = container_of(ev, gnrc_lorawan_t, ev_aloha);
    if (mac->mlme.activation == MLME_ACTIVATION_NONE) {
        /* Send join request */
        gnrc_lorawan_trigger_join(mac);
    }
    else {
        gnrc_lorawan_event_retrans_timeout(mac);
    }
}

static void _ev_timer(event_t *ev)
{
    gnrc_lorawan_t *mac = container_of(ev, gnrc_lorawan_t, ev_timer);
    gnrc_lorawan_dispatch_event(mac, GNRC_LORAWAN_EV_TO);
}

void gnrc_lorawan_send_pkt(gnrc_lorawan_t *mac, iolist_t *psdu, uint8_t dr,
                           uint32_t chan_idx)
{
    assert(psdu);
    mac->last_dr = dr;
    mac->last_chan_idx = chan_idx;
    mac->psdu = psdu;
    gnrc_lorawan_dispatch_event(mac, GNRC_LORAWAN_EV_REQUEST_TX);
}

static gnrc_lorawan_fsm_status_t _state_transition(gnrc_lorawan_t *mac, gnrc_lorawan_state_t next_state)
{
    mac->curr_state = next_state;
    return GNRC_LORAWAN_FSM_TRANSITION;
}

static gnrc_lorawan_fsm_status_t _state_rx_window(gnrc_lorawan_t *mac, gnrc_lorawan_event_t ev)
{
    netdev_t *dev = gnrc_lorawan_get_netdev(mac);
    netopt_state_t state = NETOPT_STATE_RX;
    switch(ev) {
        case GNRC_LORAWAN_EV_ENTRY:
            if (mac->rx_window == 0) {
                event_timeout_set(&mac->evt, MS_PER_SEC);
                uint8_t dr_offset = (mac->dl_settings & GNRC_LORAWAN_DL_DR_OFFSET_MASK) >>
                                    GNRC_LORAWAN_DL_DR_OFFSET_POS;

                _configure_rx_window(mac, mac->channel[mac->last_chan_idx],
                                     gnrc_lorawan_rx1_get_dr_offset(mac->last_dr,
                                                                    dr_offset));
            }
            else {
                _configure_rx_window(mac, CONFIG_LORAMAC_DEFAULT_RX2_FREQ,
                                 mac->dl_settings &
                                 GNRC_LORAWAN_DL_RX2_DR_MASK);
            }
            /* Open RX Window */
            dev->driver->set(dev, NETOPT_STATE, &state, sizeof(state));
            return GNRC_LORAWAN_FSM_HANDLED;
        case GNRC_LORAWAN_EV_EXIT:
            mac->rx_window++;
            return GNRC_LORAWAN_FSM_HANDLED;
        case GNRC_LORAWAN_EV_RX_TO:
            _sleep_radio(mac);
            if (mac->rx_window == 0) {
                return _state_transition(mac, _state_wait_rx_window);
            }
            else {
                gnrc_lorawan_event_no_rx(mac);
                return _state_transition(mac, _state_idle);
            }
            break;
        case GNRC_LORAWAN_EV_RX_DONE:
            event_timeout_clear(&mac->evt);
            _sleep_radio(mac);
            uint8_t *psdu = mac->psdu->iol_base;
            uint8_t size = mac->psdu->iol_len;
            uint8_t mtype = (*(psdu) & MTYPE_MASK) >> 5;

            switch (mtype) {
            case MTYPE_JOIN_ACCEPT:
                gnrc_lorawan_mlme_process_join(mac, psdu, size);
                break;
            case MTYPE_CNF_DOWNLINK:
            case MTYPE_UNCNF_DOWNLINK:
                gnrc_lorawan_mcps_process_downlink(mac, psdu, size);
                break;
            default:
                break;
            }
            return _state_transition(mac, _state_idle);
        default:
            assert(false);
            break;
    }

    return GNRC_LORAWAN_FSM_IGNORED;
}

static gnrc_lorawan_fsm_status_t _state_wait_rx_window(gnrc_lorawan_t *mac, gnrc_lorawan_event_t ev)
{
    switch(ev) {
        case GNRC_LORAWAN_EV_ENTRY:
            if (mac->rx_window == 0) {
                /* Configure timeout */
                /* if the MAC is not activated, then this is a Join Request */
                int rx_1 = mac->mlme.activation == MLME_ACTIVATION_NONE ?
                       CONFIG_LORAMAC_DEFAULT_JOIN_DELAY1 : mac->rx_delay;

                event_timeout_set(&mac->evt, rx_1 * MS_PER_SEC);
            }

            _sleep_radio(mac);
            return GNRC_LORAWAN_FSM_HANDLED;
        case GNRC_LORAWAN_EV_EXIT:
            return GNRC_LORAWAN_FSM_IGNORED;
        case GNRC_LORAWAN_EV_TO:
            return _state_transition(mac, _state_rx_window);
        default:
            assert(false);
            break;
    }
    return GNRC_LORAWAN_FSM_IGNORED;
}

static gnrc_lorawan_fsm_status_t _state_tx(gnrc_lorawan_t *mac, gnrc_lorawan_event_t ev)
{
    netdev_t *dev = gnrc_lorawan_get_netdev(mac);
    switch(ev) {
        case GNRC_LORAWAN_EV_ENTRY:
            /* Send packet */
            _config_radio(mac, mac->channel[mac->last_chan_idx], mac->last_dr, false);
            uint8_t cr;

            dev->driver->get(dev, NETOPT_CODING_RATE, &cr, sizeof(cr));
            mac->toa = lora_time_on_air(iolist_size(mac->psdu), mac->last_dr, cr);
            if (dev->driver->send(dev, mac->psdu) == -ENOTSUP) {
                DEBUG("gnrc_lorawan: Cannot send: radio is still transmitting");
            }
            return GNRC_LORAWAN_FSM_HANDLED;
        case GNRC_LORAWAN_EV_TX_DONE: {
            return _state_transition(mac, _state_wait_rx_window);
        }
        case GNRC_LORAWAN_EV_EXIT:
            mac->rx_window = 0;
            return GNRC_LORAWAN_FSM_HANDLED;
        default:
            assert(false);
            break;
    }
    return GNRC_LORAWAN_FSM_IGNORED;
}

static gnrc_lorawan_fsm_status_t _state_idle(gnrc_lorawan_t *mac, gnrc_lorawan_event_t ev)
{
    switch(ev) {
        case GNRC_LORAWAN_EV_ENTRY:
        case GNRC_LORAWAN_EV_EXIT:
            return GNRC_LORAWAN_FSM_IGNORED;
        case GNRC_LORAWAN_EV_REQUEST_TX: {
            return _state_transition(mac, _state_tx);
        }
        break;
        default:
            assert(false);
            break;
    }
    return GNRC_LORAWAN_FSM_IGNORED;
}
