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
#include "net/gnrc/lorawan/region.h"
#include "errno.h"
#include "net/gnrc/pktbuf.h"

#include "net/lorawan/hdr.h"

#include "random.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define _16_UPPER_BITMASK 0xFFFF0000
#define _16_LOWER_BITMASK 0xFFFF

static inline void *_mcps_allocate(gnrc_lorawan_t *mac)
{
    mac->netdev.event_callback((netdev_t *) mac, NETDEV_EVENT_MCPS_GET_BUFFER);
    return mac->mcps_buf;
}

int gnrc_lorawan_mic_is_valid(uint8_t *buf, size_t len, uint8_t *nwkskey)
{
    le_uint32_t calc_mic;

    lorawan_hdr_t *lw_hdr = (lorawan_hdr_t *) buf;

    uint32_t fcnt = byteorder_ntohs(byteorder_ltobs(lw_hdr->fcnt));
    iolist_t data = {
        .iol_base = buf,
        .iol_len = len-MIC_SIZE
    };
    gnrc_lorawan_calculate_mic(&lw_hdr->addr, fcnt, GNRC_LORAWAN_DIR_DOWNLINK, (iolist_t *) &data, nwkskey, &calc_mic);
    return calc_mic.u32 == ((le_uint32_t *) (buf+data.iol_len))->u32;
}

uint32_t gnrc_lorawan_fcnt_stol(uint32_t fcnt_down, uint16_t s_fcnt)
{
    uint32_t u32_fcnt = (fcnt_down & _16_UPPER_BITMASK) | s_fcnt;

    if (fcnt_down + LORAMAC_DEFAULT_MAX_FCNT_GAP >= _16_LOWER_BITMASK
        && s_fcnt < (fcnt_down & _16_LOWER_BITMASK)) {
        u32_fcnt += _16_LOWER_BITMASK;
    }
    return u32_fcnt;
}

struct parsed_packet {
    uint32_t fcnt_down;
    lorawan_hdr_t *hdr;
    int ack_req;
    iolist_t fopts;
    iolist_t enc_payload;
    uint8_t port;
    uint8_t ack;
    uint8_t frame_pending;
};

int gnrc_lorawan_parse_dl(gnrc_lorawan_t *mac, uint8_t *buf, size_t len,
        struct parsed_packet *pkt)
{
    memset(pkt, 0, sizeof(struct parsed_packet));

    lorawan_hdr_t *_hdr = (lorawan_hdr_t*) buf;
    uint8_t *p_mic = buf + len - MIC_SIZE;

    pkt->hdr = _hdr;
    buf += sizeof(lorawan_hdr_t);

    /* Validate header */
    if (_hdr->addr.u32 != mac->dev_addr.u32) {
        DEBUG("gnrc_lorawan: received packet with wrong dev addr. Drop\n");
        return -1;
    }

    uint32_t _fcnt = gnrc_lorawan_fcnt_stol(mac->mcps.fcnt_down, _hdr->fcnt.u16);
    if (mac->mcps.fcnt_down > _fcnt || mac->mcps.fcnt_down +
        LORAMAC_DEFAULT_MAX_FCNT_GAP < _fcnt) {
        DEBUG("gnrc_lorawan: wrong frame counter\n");
        return -1;
    }

    pkt->fcnt_down = _fcnt;

    int fopts_length = lorawan_hdr_get_frame_opts_len(_hdr);
    if(fopts_length) {
        pkt->fopts.iol_base = buf;
        pkt->fopts.iol_len = fopts_length;
        buf += fopts_length;
    }

    if(buf < p_mic) {
        pkt->port = *(buf++);
        if (buf < p_mic) {
            pkt->enc_payload.iol_base = buf;
            pkt->enc_payload.iol_len = p_mic - buf;
            if(!pkt->port && fopts_length) {
                DEBUG("gnrc_lorawan: packet with fopts and port == 0. Drop\n");
                return -1;
            }
        }
    }

    pkt->ack_req = lorawan_hdr_get_mtype(_hdr) == MTYPE_CNF_DOWNLINK;
    pkt->ack = lorawan_hdr_get_ack(_hdr);
    pkt->frame_pending = lorawan_hdr_get_frame_pending(_hdr);

    return 0;
}

void gnrc_lorawan_mcps_process_downlink(gnrc_lorawan_t *mac, uint8_t *buf,
        size_t len)
{
    struct parsed_packet _pkt;

    /* NOTE: MIC is in pkt */
    if (!gnrc_lorawan_mic_is_valid(buf, len, mac->nwkskey)) {
        DEBUG("gnrc_lorawan: invalid MIC\n");
        gnrc_lorawan_mcps_event(mac, MCPS_EVENT_NO_RX, 0);
        return;
    }

    if (gnrc_lorawan_parse_dl(mac, buf, len, &_pkt) < 0) {
        DEBUG("gnrc_lorawan: couldn't parse packet\n");
        gnrc_lorawan_mcps_event(mac, MCPS_EVENT_NO_RX, 0);
        return;
    }

    iolist_t *fopts = NULL;
    if(_pkt.fopts.iol_base) {
        fopts = &_pkt.fopts;
    }

    if(_pkt.enc_payload.iol_base) {
        uint8_t *key;
        if(_pkt.port) {
            key = mac->appskey;
        }
        else {
            key = mac->nwkskey;
            fopts = &_pkt.enc_payload;
        }
        gnrc_lorawan_encrypt_payload(&_pkt.enc_payload, &_pkt.hdr->addr, byteorder_ntohs(byteorder_ltobs(_pkt.hdr->fcnt)), GNRC_LORAWAN_DIR_DOWNLINK, key);
    }

    mac->mcps.fcnt_down = _pkt.fcnt_down;

    if (_pkt.ack_req) {
        mac->mcps.ack_requested = true;
    }

    /* if there are fopts, it's either an empty packet or application payload */
    if (fopts) {
        DEBUG("gnrc_lorawan: processing fopts\n");
        gnrc_lorawan_process_fopts(mac, fopts->iol_base, fopts->iol_len);
    }

    gnrc_lorawan_mcps_event(mac, MCPS_EVENT_RX, _pkt.ack);

    if (_pkt.frame_pending) {
        mcps_indication_t *mcps_indication = _mcps_allocate(mac);
        mcps_indication->type = MLME_SCHEDULE_UPLINK;
        mac->netdev.event_callback((netdev_t *) mac, NETDEV_EVENT_MLME_INDICATION);
    }

    if (_pkt.port) {
        mcps_indication_t *mcps_indication = _mcps_allocate(mac);
        mcps_indication->type = _pkt.ack_req;
        mcps_indication->data.pkt = &_pkt.enc_payload;;
        mcps_indication->data.port = _pkt.port;
        mac->netdev.event_callback((netdev_t *) mac, NETDEV_EVENT_MCPS_INDICATION);
    }
}

void gnrc_lorawan_build_uplink(gnrc_lorawan_t *mac, gnrc_pktsnip_t *enc_payload, int confirmed_data, uint8_t port,
        gnrc_pktsnip_t *mac_hdr, gnrc_pktsnip_t *mic)
{

    lorawan_buffer_t buf = {
        .data = (uint8_t *) mac_hdr->data,
        .size = mac_hdr->size,
        .index = 0
    };

    lorawan_hdr_t *lw_hdr = (lorawan_hdr_t *) buf.data;

    lw_hdr->mt_maj = 0;
    lorawan_hdr_set_mtype(lw_hdr, confirmed_data ? MTYPE_CNF_UPLINK : MTYPE_UNCNF_UPLINK);
    lorawan_hdr_set_maj(lw_hdr, MAJOR_LRWAN_R1);

    lw_hdr->addr = mac->dev_addr;
    lw_hdr->fctrl = 0;

    lorawan_hdr_set_ack(lw_hdr, mac->mcps.ack_requested);

    lw_hdr->fcnt = byteorder_btols(byteorder_htons(mac->mcps.fcnt));

    buf.index += sizeof(lorawan_hdr_t);

    int fopts_length = gnrc_lorawan_build_options(mac, &buf);
    assert(fopts_length < 16);
    lorawan_hdr_set_frame_opts_len(lw_hdr, fopts_length);

    assert(buf.index == mac_hdr->size - 1);

    buf.data[buf.index++] = port;

    gnrc_lorawan_calculate_mic(&mac->dev_addr, mac->mcps.fcnt, GNRC_LORAWAN_DIR_UPLINK,
                               (iolist_t *) mac_hdr, mac->nwkskey, mic->data);

    LL_APPEND(enc_payload, mic);
}

static void _end_of_tx(gnrc_lorawan_t *mac, int type, int status)
{
    mac->mcps.waiting_for_ack = false;

    mcps_confirm_t *mcps_confirm = _mcps_allocate(mac);

    mcps_confirm->type = type;
    mcps_confirm->status = status;
    mac->netdev.event_callback((netdev_t *) mac, NETDEV_EVENT_MCPS_CONFIRM);

    mac->mcps.fcnt += 1;
}

void gnrc_lorawan_mcps_event(gnrc_lorawan_t *mac, int event, int data)
{
    if (mac->mlme.activation == MLME_ACTIVATION_NONE) {
        return;
    }

    if (event == MCPS_EVENT_ACK_TIMEOUT) {
        gnrc_lorawan_send_pkt(mac, (iolist_t*) mac->mcps.outgoing_pkt, mac->last_dr);
    }
    else {
        int state = mac->mcps.waiting_for_ack ? MCPS_CONFIRMED : MCPS_UNCONFIRMED;
        if (state == MCPS_CONFIRMED && ((event == MCPS_EVENT_RX && !data) ||
                                        event == MCPS_EVENT_NO_RX)) {
            if (mac->mcps.nb_trials-- == 0) {
                _end_of_tx(mac, MCPS_CONFIRMED, -ETIMEDOUT);
            }
        }
        else {
            _end_of_tx(mac, state, GNRC_LORAWAN_REQ_STATUS_SUCCESS);
        }

        if (mac->mcps.outgoing_pkt) {
            gnrc_lorawan_timer_set(mac, 1000 + random_uint32_range(0, 2000));
        }
    }
}

void gnrc_lorawan_mcps_request(gnrc_lorawan_t *mac, const mcps_request_t *mcps_request, mcps_confirm_t *mcps_confirm)
{
    int release = true;
    gnrc_pktsnip_t *payload = (gnrc_pktsnip_t*) mcps_request->data.pkt;
    gnrc_pktsnip_t *pkt = payload;

    if (mac->mlme.activation == MLME_ACTIVATION_NONE) {
        DEBUG("gnrc_lorawan_mcps: LoRaWAN not activated\n");
        mcps_confirm->status = -ENOTCONN;
        goto out;
    }

    if (!gnrc_lorawan_mac_acquire(mac)) {
        mcps_confirm->status = -EBUSY;
        goto out;
    }

    if (mcps_request->data.port < LORAMAC_PORT_MIN ||
        mcps_request->data.port > LORAMAC_PORT_MAX) {
        mcps_confirm->status = -EBADMSG;
        goto out;
    }

    if (!gnrc_lorawan_validate_dr(mcps_request->data.dr)) {
        mcps_confirm->status = -EINVAL;
        goto out;
    }

    int waiting_for_ack = mcps_request->type == MCPS_CONFIRMED;

    /* Encrypt payload (it's block encryption so we can use the same buffer!) */
    gnrc_lorawan_encrypt_payload((iolist_t*) payload, &mac->dev_addr, mac->mcps.fcnt, GNRC_LORAWAN_DIR_UPLINK, mcps_request->data.port ? mac->appskey : mac->nwkskey);

    /* We try to allocate the whole header with fopts at once */
    uint8_t fopts_length = gnrc_lorawan_build_options(mac, NULL);

    gnrc_pktsnip_t *mac_hdr = gnrc_pktbuf_add(payload, NULL, sizeof(lorawan_hdr_t) + fopts_length + 1, GNRC_NETTYPE_UNDEF);

    if (!mac_hdr) {
        gnrc_pktbuf_release_error(payload, -ENOBUFS);
        mcps_confirm->status = -ENOBUFS;
        goto out;
    }

    gnrc_pktsnip_t *mic = gnrc_pktbuf_add(NULL, NULL, MIC_SIZE, GNRC_NETTYPE_UNDEF);
    if (!mic) {
        gnrc_pktbuf_release_error(mac_hdr, -ENOBUFS);
        mcps_confirm->status = -ENOBUFS;
        goto out;
    }

    gnrc_lorawan_build_uplink(mac, payload, waiting_for_ack, mcps_request->data.port, mac_hdr, mic);

    pkt = mac_hdr;
    if ((gnrc_pkt_len(pkt) - MIC_SIZE - 1) > gnrc_lorawan_region_mac_payload_max(mcps_request->data.dr)) {
        mcps_confirm->status = -EMSGSIZE;
        goto out;
    }

    release = false;
    mac->mcps.waiting_for_ack = waiting_for_ack;
    mac->mcps.ack_requested = false;

    mac->mcps.nb_trials = LORAMAC_DEFAULT_RETX;

    assert(mac->mcps.outgoing_pkt == NULL);
    mac->mcps.outgoing_pkt = pkt;

    gnrc_lorawan_send_pkt(mac, (iolist_t*) pkt, mcps_request->data.dr);
    mcps_confirm->status = GNRC_LORAWAN_REQ_STATUS_DEFERRED;
out:

    if (mcps_confirm->status != GNRC_LORAWAN_REQ_STATUS_DEFERRED) {
        gnrc_lorawan_mac_release(mac);
    }

    if (release) {
        gnrc_pktbuf_release_error(pkt, mcps_confirm->status);
    }
}

/** @} */
