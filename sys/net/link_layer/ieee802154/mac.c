#include <errno.h>
#include "net/ieee802154/mac.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

void ieee802154_mac_rx_done_cb(ieee802154_submac_t *submac)
{
    ieee802154_mac_t *mac = (ieee802154_mac_t*) submac;
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

   ind.msdu = mhr + mhr_len;
   ind.msdu_len = res - mhr_len;
   ind.lqi = rx_info.lqi;
   ind.dsn = ieee802154_get_seq(mhr);

   mac->cb->indication(mac, MCPS_DATA, &ind);
}

void ieee802154_mac_tx_done_cb(ieee802154_submac_t *submac, int status, ieee802154_tx_info_t *info)
{
    (void) info;
    ieee802154_mac_t *mac = (ieee802154_mac_t*) submac;
    ieee802154_data_cnf_t cnf = {.status = status, .msdu = mac->tx_msdu};
   mac->cb->confirm(mac, MCPS_DATA, &cnf);
}

static const ieee802154_submac_cb_t _cb = {
    .rx_done = ieee802154_mac_rx_done_cb,
    .tx_done = ieee802154_mac_tx_done_cb,
};

int ieee802154_mac_init(ieee802154_mac_t *mac, eui64_t *eui, ieee802154_dev_t *dev)
{
    network_uint16_t short_addr = {.u16 = 0xFFFF};

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
