#include <errno.h>
#include "net/ieee802154/mac.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

void ieee802154_mac_rx_done_cb(ieee802154_submac_t *submac)
{
    ieee802154_mac_t *mac = (ieee802154_mac_t*) submac;
    /* TODO: Fix */
    if (mac->state == MAC_STATE_SCANNING) {
        mac->scan.found = true;
        ieee802154_set_state(&mac->submac, IEEE802154_STATE_IDLE);
        return;
    }
    iolist_t *psdu = ieee802154_get_framebuffer(mac);
    ieee802154_rx_info_t rx_info;
    int res;
    ieee802154_data_ind_t ind;

    if (!psdu || (res = ieee802154_read_frame(submac, psdu->iol_base, psdu->iol_len, &rx_info)) < 0) {
        return;
    }

    uint8_t *mhr = psdu->iol_base;
    size_t mhr_len = ieee802154_get_frame_hdr_len(mhr);

    ind.src.addr_len = ieee802154_get_src(psdu->iol_base, ind.src.addr, &ind.src.panid);
    ind.dst.addr_len = ieee802154_get_dst(psdu->iol_base, ind.dst.addr, &ind.dst.panid);

    uint8_t *msdu = mhr + mhr_len;
    ind.msdu = mhr + mhr_len;
    ind.msdu_len = res - mhr_len;
    ind.lqi = rx_info.lqi;
    ind.dsn = ieee802154_get_seq(mhr);

    int frame_type = mhr[0] & 0x3;
    if (frame_type == 3) {
        /* Only reply with beacons if device is a coordinator */
        if (mac->coord && msdu[0] == 0x07) {
            mac->state = MAC_STATE_SEND_BEACON;
            event_post(mac->evq, &mac->ev);
        }
        else if (msdu[0] == 0x1) {
            mac->state = MAC_STATE_SEND_ASSOCIATE_CMD;
            event_post(mac->evq, &mac->ev);
        }
    }
    else {
        mac->cb->indication(mac, MCPS_DATA, &ind);
    }
}

void ieee802154_mac_associate_ind(ieee802154_mac_t *mac)
{
    ieee802154_associate_ind_t ind;
    le_uint16_t panid;
    iolist_t *io = ieee802154_get_framebuffer(mac);
    uint8_t *psdu = io->iol_base;
    ind.addr_len = ieee802154_get_src(psdu, ind.addr, &panid);
    uint8_t *msdu = psdu + ieee802154_get_frame_hdr_len(psdu);
    ind.caps = msdu[1];
    mac->cb->indication(mac, MLME_ASSOCIATE, &ind);
}

static int _build_poll(ieee802154_mac_t *mac, uint8_t *psdu, void *coord_addr, size_t coord_addr_len, le_uint16_t coord_panid, ieee802154_sec_t *sec)
{
    (void) sec;
    le_uint16_t bcast;
    (void) coord_panid;
    memcpy(&bcast, &mac->submac.panid, 2);
    /* This command requires extended address */
    /* TODO: Use also LONG address */
    int res = ieee802154_set_frame_hdr(psdu, (void*) &mac->submac.ext_addr, 8, coord_addr, coord_addr_len, bcast, bcast, 0x3 | IEEE802154_FCF_ACK_REQ, mac->dsn++);

    assert(res == 15);
    psdu[15] = 0x04;

    return 16;
}

void ieee802154_mac_finish_associate(ieee802154_mac_t *mac)
{
    /* Send Data Request command */
    uint8_t psdu[22];
    le_uint16_t dest;
    memcpy(&dest, &mac->submac.panid, 2);
    _build_poll(mac, psdu, mac->coord_addr, mac->coord_addr_len, dest, NULL);

    iolist_t iol = {.iol_base = psdu, .iol_len = 22};

    ieee802154_send(&mac->submac, &iol);
}

void ieee802154_mac_tx_done_cb(ieee802154_submac_t *submac, int status, ieee802154_tx_info_t *info)
{
    (void) info;
    ieee802154_mac_t *mac = (ieee802154_mac_t*) submac;
    switch(mac->state) {
        case MAC_STATE_IDLE: {
            ieee802154_data_cnf_t cnf = {.status = status, .msdu = mac->tx_msdu};
            mac->cb->confirm(mac, MCPS_DATA, &cnf);
             }
            break;
        case MAC_STATE_SCANNING:
            ieee802154_mac_scan_tx_done(mac);
            break;
        case MAC_STATE_ASSOCIATE:
            /* TODO: */
            assert(status == TX_STATUS_SUCCESS);
            mac->state = MAC_STATE_POLL_ASSOCIATE;
            ztimer_set(ZTIMER_USEC, &mac->timer, MAC_RESPONSE_WAIT_TIME);
            break;
        case MAC_STATE_SEND_BEACON:
            mac->state = MAC_STATE_IDLE;
            break;
    }
}

static const ieee802154_submac_cb_t _cb = {
    .rx_done = ieee802154_mac_rx_done_cb,
    .tx_done = ieee802154_mac_tx_done_cb,
};

static void timer_cb(void *arg)
{
    ieee802154_mac_t *mac = arg; 
    event_post(mac->evq, &mac->ev);
}

static void _mac_ev_handler(event_t *ev)
{
    ieee802154_mac_t *mac = container_of(ev, ieee802154_mac_t, ev);
    if (mac->state == MAC_STATE_SCANNING) {
        ieee802154_mac_scan_timeout(mac);
    }
    else if (mac->state == MAC_STATE_SEND_BEACON) {
        ieee802154_mac_send_beacon(mac);
    }
    else if (mac->state == MAC_STATE_SEND_ASSOCIATE_CMD) {
        ieee802154_mac_associate_ind(mac);
    }
    else if (mac->state == MAC_STATE_POLL_ASSOCIATE) {
        ieee802154_mac_finish_associate(mac);
    }
}


int ieee802154_mac_init(ieee802154_mac_t *mac, eui64_t *eui, ieee802154_dev_t *dev, event_queue_t *evq)
{
    network_uint16_t short_addr = {.u16 = 0xFFFF};

    mac->evq = evq;
    mac->ev.handler = _mac_ev_handler;
    mac->timer.callback = timer_cb;
    mac->timer.arg = mac;
    dev->ctx = mac;
    mac->submac.cb = &_cb;
    mac->submac.dev = dev;

    /* TODO: This should be random */
    mac->dsn = 0;

    return ieee802154_submac_init(&mac->submac, &short_addr, eui);
}

int ieee802154_mac_data_req(ieee802154_mac_t *mac, int src_mode, ieee802154_ep_t *dst, iolist_t *msdu, uint8_t tx_ops,
                            ieee802154_sec_t *sec)
{
    (void) sec;

    uint8_t mhr[IEEE802154_MAX_HDR_LEN];
    void *src;
    int src_len;
    int res;

    if (src_mode == 0x02) {
        src = &mac->submac.short_addr;
        src_len = 2;
    }
    else if (src_mode == 0x03) {
        src = &mac->submac.ext_addr;
        src_len = 8;
    }
    else {
        src = NULL;
        src_len = 0;
    }

    le_uint16_t src_pan = byteorder_btols(byteorder_htons(mac->submac.panid));

    if ((res = ieee802154_set_frame_hdr(mhr, src, src_len,
                                        dst->addr, dst->addr_len, src_pan,
                                        dst->panid, tx_ops | 0x1, mac->dsn++)) == 0) {
        DEBUG("_send_ieee802154: Error preperaring frame\n");

        return -EINVAL;
    }

    iolist_t iolist = {
        .iol_next = (iolist_t *)msdu,
        .iol_base = mhr,
        .iol_len = (size_t)res
    };

    mac->tx_msdu = msdu;

    return ieee802154_send(&mac->submac, &iolist);
}

int ieee802154_mac_mlme_start(ieee802154_mac_t *mac, le_uint16_t panid, uint8_t channel_num, uint8_t channel_page, bool pan_coord)
{
    if (mac->submac.short_addr.u16 == 0xFFFF) {
        /* TODO: Check error message */
        return -EFAULT;
    }
    
    if (ieee802154_set_phy_conf(&mac->submac, channel_num, channel_page, mac->submac.tx_pow) < 0) {
        return -EINVAL;
    }

    ieee802154_set_panid(&mac->submac, &panid.u16);

    /* TODO: Add support for regular coordinator */
    assert(pan_coord);

    mac->coord = true;
    return 0;
}

static int _build_associate_request(ieee802154_mac_t *mac, uint8_t *psdu, uint8_t caps)
{
    le_uint16_t bcast = {.u16 = 0xFFFF};
    le_uint16_t dest;

    memcpy(&dest, &mac->submac.panid, 2);

    int res = ieee802154_set_frame_hdr(psdu, (void*) &mac->submac.ext_addr, 8, (void*) mac->coord_addr, mac->coord_addr_len, bcast, dest, 0x3 | IEEE802154_FCF_ACK_REQ, mac->dsn++);
    assert(res == 17);

    /* Association request */
    psdu[17] = 0x01;

    psdu[18] = caps;

    return 0;
}

static int _build_associate_response(ieee802154_mac_t *mac, uint8_t *psdu, uint8_t *dev_addr,
        le_uint16_t *short_addr, uint8_t status)
{
    /* Use PAN ID compression */
    le_uint16_t bcast;
    memcpy(&bcast, &mac->submac.panid, 2);

    int res = ieee802154_set_frame_hdr(psdu, (void*) &mac->submac.ext_addr, 8, dev_addr, 8, bcast, bcast, 0x3 | IEEE802154_FCF_ACK_REQ, mac->dsn++);
    assert(res == 21);

    (void) short_addr;
    /* Association request */
    psdu[21] = 0x02;

    psdu[22] = 0xFE;
    psdu[23] = 0xFF;
    psdu[24] = status;

    return 25;
}

int ieee802154_mac_mlme_associate(ieee802154_mac_t *mac, uint16_t channel_num, uint8_t channel_page,
                                  void *coord_addr, size_t addr_len, le_uint16_t coord_panid,
                                  uint8_t caps, ieee802154_sec_t *sec)
{
    (void) sec;
    /* TODO: MLME-RESET... */
    ieee802154_set_phy_conf(&mac->submac, channel_num, channel_page, mac->submac.tx_pow);
    ieee802154_set_panid(&mac->submac, &coord_panid.u16);

    mac->state = MAC_STATE_ASSOCIATE;
    assert(addr_len <= 8);
    memcpy(mac->coord_addr, coord_addr, addr_len);
    mac->coord_addr_len = addr_len;

    uint8_t psdu[19];
    _build_associate_request(mac, psdu, caps);

    iolist_t iol = {.iol_base = psdu, .iol_len = 19};

    return ieee802154_send(&mac->submac, &iol);
}

void ieee802154_mac_mlme_associate_response(ieee802154_mac_t* mac, uint8_t *addr,
        le_uint16_t *short_addr, int status, ieee802154_sec_t *sec)
{
    (void) sec;
    if (mac->pending_psdu_size) {
        /* Pending transaction queue full */
        /* TODO: Generate MLME indication */
        assert(false);
        return;
    }

    mac->pending_psdu_size = _build_associate_response(mac, mac->queue, addr, short_addr, status);
}

#if 0
int ieee802154_mac_mlme_poll(ieee802154_mac_t *mac, void *coord_addr, size_t coord_addr_size, le_uint16_t *coord_panid, ieee802154_sec_t *sec)
{
    mac->state = MAC_STATE_POLL;
    /* TODO: Check if PAN coordinator */
    uint8_t psdu[22];
    return 0;
}
#endif
