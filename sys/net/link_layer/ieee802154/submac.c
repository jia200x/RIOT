#include <stdio.h>
#include <string.h>
#include "net/ieee802154/submac.h"
#include "net/ieee802154.h"
#include "net/csma_sender.h"
#include "luid.h"
#include "kernel_defines.h"
#include <assert.h>

static void _perform_retrans(ieee802154_submac_t *submac)
{
    iolist_t *psdu = submac->ctx;
    ieee802154_dev_t *dev = submac->dev;
    int res;

    if (submac->retrans++ < IEEE802154_SUBMAC_MAX_RETRANSMISSIONS) {
        res = csma_sender_csma_ca_send(dev, psdu, NULL);
        if (res < 0) {
            submac->cb->tx_done(submac, IEEE802154_RF_EV_TX_MEDIUM_BUSY, NULL);
        }
    }
    else {
        submac->cb->tx_done(submac, IEEE802154_RF_EV_TX_NO_ACK, NULL);
    }
}

void ieee802154_submac_ack_timeout_fired(ieee802154_submac_t *submac)
{
    /* This is required to avoid race conditions */
    if(submac->wait_for_ack) {
        _perform_retrans(submac);
    }
}

static void _send_ack(ieee802154_submac_t *submac, uint8_t *mhr)
{
    ieee802154_dev_t *dev = submac->dev;
    /* Send ACK packet */
    uint8_t ack_pkt[3];
    ack_pkt[0] = 0x2;
    ack_pkt[1] = 0;
    ack_pkt[2] = mhr[2];

    iolist_t ack = {
        .iol_base = ack_pkt,
        .iol_len = 3,
        .iol_next = NULL,
    };

    ieee802154_radio_set_trx_state(dev, IEEE802154_TRX_STATE_TX_ON);

    ieee802154_radio_prepare(dev, &ack);
    ieee802154_radio_transmit(dev, IEEE802154_TX_MODE_DIRECT);
}

/* All callbacks run in the same context */
int ieee802154_submac_rx_done_cb(ieee802154_submac_t *submac, struct iovec *iov, ieee802154_rx_info_t *info)
{
    uint8_t *buf = iov->iov_base;
    ieee802154_dev_t *dev = submac->dev;
    if (submac->wait_for_ack) {
        ieee802154_submac_ack_timer_cancel(submac);
        if(iov->iov_len <= 5 && buf[0] == 0x2) {
            ieee802154_tx_info_t tx_info;
            tx_info.retries = submac->retrans-1;
            tx_info.frame_pending = buf[0] & IEEE802154_FCF_FRAME_PEND;
            submac->wait_for_ack = false;
            submac->cb->tx_done(submac, IEEE802154_RF_EV_TX_DONE, &tx_info);
        }
        else {
            _perform_retrans(submac);
        }
    }
    else {
        if (!ieee802154_radio_has_auto_ack(dev)) {
            if (buf[0] & 0x2) {
                return -EINVAL;
            }
            if (buf[0] & IEEE802154_FCF_ACK_REQ) {
                submac->tx = false;
                _send_ack(submac, buf);
            }
        }
        submac->cb->rx_done(submac, iov, info);
    }
    return 0;
}

void ieee802154_submac_tx_done_cb(ieee802154_submac_t *submac)
{
    ieee802154_dev_t *dev = submac->dev;
    ieee802154_tx_info_t info;
    if (ieee802154_radio_has_frame_retries(dev)) {
        int status = ieee802154_radio_get_tx_status(dev, &info);
        submac->cb->tx_done(submac, status, &info);
    }
    else if (submac->tx) {
        if (submac->wait_for_ack) {
            ieee802154_submac_ack_timer_set(submac, 2000);
        }
        else {
            submac->cb->tx_done(submac, IEEE802154_RF_EV_TX_DONE, &info);
        }
    }
    /* TODO: Check state changes */
    ieee802154_radio_set_trx_state(dev, IEEE802154_TRX_STATE_RX_ON);
}

int ieee802154_send(ieee802154_submac_t *submac, iolist_t *iolist)
{
    ieee802154_dev_t *dev = submac->dev;
    uint8_t *buf = iolist->iol_base;
    bool cnf = buf[0] & IEEE802154_FCF_ACK_REQ;
    int res;
    /* TODO */
    if (ieee802154_radio_has_frame_retries(dev) || !cnf)
    {
        ieee802154_radio_set_trx_state(dev, IEEE802154_TRX_STATE_TX_ON);
        res = ieee802154_radio_prepare(dev, iolist);

        if (res < 0) {
            return res;
        }
        else {
        }
        ieee802154_radio_transmit(dev, IEEE802154_TX_MODE_CSMA_CA);
    }
    else {
        submac->wait_for_ack = true;
        submac->retrans = 0;
        submac->ctx = iolist;
        /* This function could be called from the same context of the callbacks! */
        _perform_retrans(submac);
    }
    submac->tx = true;
    return 0;
}

int ieee802154_submac_init(ieee802154_submac_t *submac)
{
    ieee802154_dev_t *dev = submac->dev;
    submac->tx = false;
    /* generate EUI-64 and short address */
    luid_get_eui64(&submac->ext_addr);
    luid_get_short(&submac->short_addr);

    /* TODO */
#if IS_ACTIVE(MODULE_AT86RF2XX)
    ieee802154_radio_set_hw_addr_filter(dev, (uint8_t*) &submac->short_addr, (uint8_t*) &submac->ext_addr, 0x23);
#endif
    ieee802154_phy_conf_t conf = {.channel=21, .page=0, .pow=1000};
    ieee802154_radio_config_phy(dev, &conf);
    /* TODO: remove */
    /*
    if(!dev->driver->start) {
        assert(false);
    }*/

    ieee802154_radio_start(submac->dev);
    return 0;
}

int ieee802154_set_addresses(ieee802154_submac_t *submac, network_uint16_t *short_addr,
        eui64_t *ext_addr, uint16_t panid)
{
    ieee802154_dev_t *dev = submac->dev;
    memcpy(&submac->short_addr, short_addr, 2);
    memcpy(&submac->ext_addr, ext_addr, 8);
    submac->panid = panid;

    if (ieee802154_radio_has_addr_filter(dev)) {
        /*TODO: Change signature */
        ieee802154_radio_set_hw_addr_filter(dev, (void*) short_addr, (void*) ext_addr, panid);
    }
    return 0;
}


int ieee802154_set_channel(ieee802154_submac_t *submac, uint8_t channel_num, uint8_t channel_page)
{
    ieee802154_dev_t *dev = submac->dev;
    ieee802154_phy_conf_t conf = {.channel=channel_num, .page=channel_page, .pow=1000};
    return ieee802154_radio_config_phy(dev, &conf);
}

