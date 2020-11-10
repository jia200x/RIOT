#include <errno.h>
#include "net/ieee802154/mac.h"
#include "ztimer.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static int _yield_channel(ieee802154_mac_t *mac, uint16_t *chan)
{
    int n;
    while((n = mac->scan.curr_scan++) < 27) {
        bool found = mac->scan.chan_bm & 0x1;
        mac->scan.chan_bm >>= 1;
        if (found) {
            *chan = n;
            return 0;
        }
    }

    return -ENOENT;
}

static int _build_beacon_request(ieee802154_mac_t *mac, uint8_t *psdu)
{
    le_uint16_t bcast = {.u16 = 0xFFFF};
    int res = ieee802154_set_frame_hdr(psdu, NULL, 0, bcast.u8, 2, bcast, bcast, 0x3, mac->dsn++);
    assert(res == 7);
    psdu[7] = 0x07;

    return 0;
}

static int _send_beacon_request(ieee802154_mac_t *mac)
{
   /* Find first channel */ 
    uint16_t chan;
    int res = _yield_channel(mac, &chan);

    if (res != 0) {
        mac->state = MAC_STATE_IDLE;
        ieee802154_scan_cnf_t cnf = {.status =
        mac->scan.num_channels == 0 ? -ENOENT : 0, .result_list = mac->scan.num_channels};
        mac->cb->confirm(mac, MLME_SCAN, &cnf);
        return res;
    }

    /* Send beacon request */
    uint8_t br_psdu[8];

    _build_beacon_request(mac, br_psdu);

    iolist_t iol = {.iol_base = br_psdu, .iol_len = 8};

    ieee802154_set_channel_number(&mac->submac, chan);
    printf("Scanning in channel: %i\n", chan);
    ieee802154_send(&mac->submac, &iol);
    return 0;
}

static int _build_beacon_mhr(ieee802154_mac_t *mac, uint8_t *mhr)
{ 
    le_uint16_t src_pan = byteorder_btols(byteorder_htons(mac->submac.panid));

    return ieee802154_set_frame_hdr(mhr, mac->submac.short_addr.u8, 2, NULL, 0,
                             src_pan, src_pan, 0, mac->bsn++);
}

void ieee802154_mac_send_beacon(ieee802154_mac_t *mac)
{
    iolist_t *psdu = ieee802154_get_framebuffer(mac);
    uint8_t *mhr = psdu->iol_base;
    _build_beacon_mhr(mac, mhr);
    size_t mhr_len = ieee802154_get_frame_hdr_len(mhr);
    uint8_t *msdu = mhr + mhr_len;
    msdu[0] = 0xFF;
    msdu[1] = 0xC0;
    /* GTS */
    msdu[2] = 0x0;
    /* Pending addresses */
    msdu[3] = 0x0;

    /* Beacon payload */
    msdu[4] = 'h';
    msdu[5] = 'o';
    msdu[6] = 'l';
    msdu[7] = 'i';
    iolist_t iol = {
        .iol_base = mhr,
        .iol_len = mhr_len + 8,
        .iol_next = NULL,
    };

    ieee802154_radio_set_csma_params(mac->submac.dev, NULL, -1);
    ieee802154_send(&mac->submac, &iol);
}

int ieee802154_mac_mlme_scan(ieee802154_mac_t *mac, ieee802154_scan_t scan_type,
                             uint32_t chan_bm, uint8_t duration, int page, ieee802154_sec_t *sec)
{
    (void) sec;
    if (!chan_bm) {
        return -EINVAL;
    }

    if (mac->state == MAC_STATE_SCANNING) {
        return -EBUSY;
    }
    
    /* Only active scan is supported so far (and page 0) */
    if (scan_type != IEEE802154_SCAN_ACTIVE || page != 0) {
        return -ENOTSUP;
    }

    ieee802154_set_state(&mac->submac, IEEE802154_STATE_IDLE);
    /* Temporarily set the PAN ID to 0xFFFF (as described in the IEEE 802.15.4 standard) */
    uint16_t pan_id = 0xFFFF;
    ieee802154_radio_set_hw_addr_filter(mac->submac.dev, NULL, NULL, &pan_id);

    mac->scan.chan_bm = chan_bm;
    mac->scan.curr_scan = 0;
    mac->scan.num_channels = 0;
    mac->scan.duration = duration;
    mac->state = MAC_STATE_SCANNING;

    ieee802154_set_channel_page(&mac->submac, page);

    _send_beacon_request(mac);
    return 0;
}

void ieee802154_mac_scan_timeout(ieee802154_mac_t *mac)
{
    ieee802154_rx_info_t rx_info;
    /* Check if a frame was received */
    if (mac->submac.state != IEEE802154_STATE_IDLE) {
        ieee802154_set_state(&mac->submac, IEEE802154_STATE_IDLE);
    }

    if (mac->scan.found) {
        iolist_t *psdu = ieee802154_get_framebuffer(mac);
        unsigned res = ieee802154_read_frame(&mac->submac, psdu->iol_base, psdu->iol_len, &rx_info);
        uint8_t *mhr = psdu->iol_base;

        size_t mhr_len = ieee802154_get_frame_hdr_len(mhr);
        ieee802154_beacon_ind_t ind;
        ind.bsn = ieee802154_get_seq(mhr);

        /* TODO */
        uint8_t len = res-(mhr_len+4);
        ind.sdu = &mhr[mhr_len + 4];
        ind.sdu_len = len;
        
        ind.pan.addr_len = ieee802154_get_src(mhr, ind.pan.addr, &ind.pan.panid);
        ind.pan.channel = mac->scan.curr_scan;

        mac->scan.num_channels += 1;
        /* TODO */
        ind.pan.page = 0;
        ind.pan.lqi = rx_info.lqi;

        /* TODO: Check if frame is a beacon */
        mac->cb->indication(mac, MLME_BEACON_NOTIFY, &ind);
    }

    _send_beacon_request(mac);
}

void ieee802154_mac_scan_tx_done(ieee802154_mac_t *mac)
{
    /* The transceiver is already off and channel set
     * accordingly */
    mac->scan.found = false;
    ieee802154_set_state(&mac->submac, IEEE802154_STATE_LISTEN);
    ztimer_set(ZTIMER_USEC, &mac->timer, SUPERFRAME_BASE_US);
}
