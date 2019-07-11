/*
 * Copyright (C) 2018 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  Jose Ignacio Alamos <jose.alamos@haw-hamburg.de>
 */

#include "net/gnrc/pktbuf.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netif/lorawan.h"
#include "net/gnrc/netif/internal.h"
#include "net/gnrc/lorawan.h"
#include "net/netdev.h"
#include "net/lora.h"
#include "net/loramac.h"
#include "net/gnrc/netreg.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static uint8_t _nwkskey[LORAMAC_NWKSKEY_LEN] = LORAMAC_NWK_SKEY_DEFAULT;
static uint8_t _appskey[LORAMAC_APPSKEY_LEN] = LORAMAC_APP_SKEY_DEFAULT;
static uint8_t _appkey[LORAMAC_APPKEY_LEN] = LORAMAC_APP_KEY_DEFAULT;
static uint8_t _deveui[LORAMAC_DEVEUI_LEN] = LORAMAC_DEV_EUI_DEFAULT;
static uint8_t _appeui[LORAMAC_APPEUI_LEN] = LORAMAC_APP_EUI_DEFAULT;
static uint8_t _devaddr[LORAMAC_DEVADDR_LEN] = LORAMAC_DEV_ADDR_DEFAULT;

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt);
static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif);
static void _msg_handler(gnrc_netif_t *netif, msg_t *msg);
static int _get(gnrc_netif_t *netif, gnrc_netapi_opt_t *opt);
static int _set(gnrc_netif_t *netif, const gnrc_netapi_opt_t *opt);
static void _init(gnrc_netif_t *netif);

uint8_t tx_buf[250];

static const gnrc_netif_ops_t lorawan_ops = {
    .init = _init,
    .send = _send,
    .recv = _recv,
    .get = _get,
    .set = _set,
    .msg_handler = _msg_handler
};

void gnrc_lorawan_mcps_indication(gnrc_lorawan_t *mac, mcps_indication_t *ind)
{
    gnrc_netif_lorawan_t *netif = (gnrc_netif_lorawan_t*) mac;
    if (gnrc_netapi_dispatch_receive(GNRC_NETTYPE_LORAWAN, ind->data.port, netif->rx_pkt)) {
       netif->rx_pkt = NULL; 
    }
}

void gnrc_lorawan_mlme_indication(gnrc_lorawan_t *mac, mlme_indication_t *ind)
{
    (void) mac;
    (void) ind;
}

void gnrc_lorawan_mcps_confirm(gnrc_lorawan_t *mac, mcps_confirm_t *confirm)
{
    gnrc_netif_lorawan_t *netif = (gnrc_netif_lorawan_t*) mac;
    if (confirm->status == 0) {
        gnrc_pktbuf_release(netif->outgoing_pkt);
    }
    else {
        gnrc_pktbuf_release_error(netif->outgoing_pkt, 1);
    }
    netif->outgoing_pkt = NULL;
}

void gnrc_lorawan_mlme_confirm(gnrc_lorawan_t *mac, mlme_confirm_t *confirm)
{
    gnrc_netif_t *netif = (gnrc_netif_t *) mac->netdev.context;

    if (confirm->type == MLME_JOIN) {
        if (confirm->status == 0) {
            DEBUG("gnrc_lorawan: join succeeded\n");
        }
        else {
            DEBUG("gnrc_lorawan: join failed\n");
        }
    }
    else if (confirm->type == MLME_LINK_CHECK) {
        netif->flags &= ~GNRC_NETIF_FLAGS_LINK_CHECK;
        netif->lorawan.demod_margin = confirm->link_req.margin;
        netif->lorawan.num_gateways = confirm->link_req.num_gateways;
    }
}

static void _mac_cb(netdev_t *dev, netdev_event_t event)
{
    assert(false);
    netdev_event_cb_pass(dev, event);
}

static void _driver_cb(netdev_t *dev, netdev_event_t event)
{
    gnrc_lorawan_t *mac = (gnrc_lorawan_t *) dev->context;
    gnrc_netif_t *netif = (gnrc_netif_t *) mac->netdev.context;

    if (event == NETDEV_EVENT_ISR) {
        msg_t msg = { .type = NETDEV_MSG_TYPE_EVENT,
                      .content = { .ptr = netif } };

        if (msg_send(&msg, netif->pid) <= 0) {
            DEBUG("gnrc_netif: possibly lost interrupt.\n");
        }
    }
    else {
        DEBUG("gnrc_netif: event triggered -> %i\n", event);
        switch (event) {
            case NETDEV_EVENT_RX_COMPLETE:
                netif->ops->recv(netif);
                break;
            case NETDEV_EVENT_TX_COMPLETE:
                gnrc_lorawan_event_tx_complete(mac);
                break;
            case NETDEV_EVENT_RX_TIMEOUT:
                gnrc_lorawan_event_timeout(mac);
                break;
            default:
                DEBUG("gnrc_netif: warning: unhandled event %u.\n", event);
        }
    }
}

static void _reset(gnrc_netif_t *netif)
{
    netif->lorawan.otaa = LORAMAC_DEFAULT_JOIN_PROCEDURE == LORAMAC_JOIN_OTAA ? NETOPT_ENABLE : NETOPT_DISABLE;
    netif->lorawan.datarate = LORAMAC_DEFAULT_DR;
    netif->lorawan.demod_margin = 0;
    netif->lorawan.num_gateways = 0;
    netif->lorawan.port = LORAMAC_DEFAULT_TX_PORT;
    netif->lorawan.ack_req = LORAMAC_DEFAULT_TX_MODE == LORAMAC_TX_CNF;
    netif->flags = 0;
}

static void _memcpy_reversed(uint8_t *dst, uint8_t *src, size_t size)
{
    for(size_t i=0;i<size;i++) {
        dst[size-i-1] = src[i];
    }
}

static void _init(gnrc_netif_t *netif)
{
    netif->dev->event_callback = _driver_cb;
    netif->lorawan.mac.netdev.event_callback = _mac_cb;
    netif->lorawan.mac.netdev.context = netif;
    _reset(netif);

    /* Initialize default keys, address and EUIs */
    memcpy(netif->lorawan.nwkskey, _nwkskey, sizeof(_nwkskey));
    memcpy(netif->lorawan.appskey, _appskey, sizeof(_appskey));
    _memcpy_reversed(netif->lorawan.deveui, _deveui, sizeof(_deveui));
    memcpy(netif->lorawan.appkey, _appkey, sizeof(_appkey));
    _memcpy_reversed(netif->lorawan.appeui, _appeui, sizeof(_appeui));

    gnrc_lorawan_setup(&netif->lorawan.mac, netif->dev);
    netif->lorawan.mac.netdev.driver->set(&netif->lorawan.mac.netdev, NETOPT_ADDRESS, _devaddr, sizeof(_devaddr));

    netif->lorawan.backoff_msg.type = MSG_TYPE_MLME_BACKOFF_EXPIRE;
    gnrc_lorawan_mlme_backoff_expire(&netif->lorawan.mac);
    xtimer_set_msg(&netif->lorawan.backoff_timer,
                   GNRC_LORAWAN_BACKOFF_WINDOW_TICK,
                   &netif->lorawan.backoff_msg, thread_getpid());
    netif->lorawan.outgoing_pkt = NULL;
    gnrc_lorawan_init(&netif->lorawan.mac, netif->lorawan.nwkskey, netif->lorawan.appskey,
            tx_buf);
}

gnrc_netif_t *gnrc_netif_lorawan_create(char *stack, int stacksize,
                                        char priority, char *name,
                                        netdev_t *dev)
{
    return gnrc_netif_create(stack, stacksize, priority, name, dev,
                             &lorawan_ops);
}

static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif)
{
    netdev_t *mac = &netif->lorawan.mac.netdev;
    int bytes_expected = netdev_recv_pass(mac, NULL, 0, 0);
    int nread;
    struct netdev_radio_rx_info rx_info;
    gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, NULL, bytes_expected, GNRC_NETTYPE_UNDEF);
    if (pkt == NULL) {
        DEBUG("_recv_ieee802154: cannot allocate pktsnip.\n");
        /* Discard packet on netdev device */
        netdev_recv_pass(mac, NULL, bytes_expected, NULL);
        return NULL;
    }

    nread = netdev_recv_pass(mac, pkt->data, bytes_expected, &rx_info);
    if (nread <= 0) {
        gnrc_pktbuf_release(pkt);
        return NULL;
    }

    netif->lorawan.rx_pkt = pkt;
    gnrc_lorawan_process_pkt((gnrc_lorawan_t*) mac, pkt->data, pkt->size);

    if(netif->lorawan.rx_pkt) {
        gnrc_pktbuf_release(pkt);
        netif->lorawan.rx_pkt = NULL;
    }

    return NULL;
}

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *payload)
{
    mlme_request_t mlme_request;
    mlme_confirm_t mlme_confirm;

    netif->lorawan.outgoing_pkt = payload;

    if (netif->flags & GNRC_NETIF_FLAGS_LINK_CHECK) {
        mlme_request.type = MLME_LINK_CHECK;
        gnrc_lorawan_mlme_request(&netif->lorawan.mac, &mlme_request, &mlme_confirm);
    }
    mcps_request_t req = { .type = netif->lorawan.ack_req ? MCPS_CONFIRMED : MCPS_UNCONFIRMED, .data.pkt = (iolist_t*) payload, .data.port = netif->lorawan.port,
                           .data.dr = netif->lorawan.datarate };
    mcps_confirm_t conf;
    gnrc_lorawan_mcps_request(&netif->lorawan.mac, &req, &conf);
    if(conf.status != GNRC_LORAWAN_REQ_STATUS_DEFERRED) {
        gnrc_pktbuf_release_error(payload, 1);
        netif->lorawan.outgoing_pkt = NULL;
    }
    return conf.status;
}

static void _msg_handler(gnrc_netif_t *netif, msg_t *msg)
{
    (void) netif;
    (void) msg;
    switch (msg->type) {
        case MSG_TYPE_TIMEOUT:
            gnrc_lorawan_timer_fired(&netif->lorawan.mac);
            break;
        case MSG_TYPE_MLME_BACKOFF_EXPIRE:
            gnrc_lorawan_mlme_backoff_expire(&netif->lorawan.mac);
            xtimer_set_msg(&netif->lorawan.backoff_timer,
                           GNRC_LORAWAN_BACKOFF_WINDOW_TICK,
                           &netif->lorawan.backoff_msg, thread_getpid());
        default:
            break;
    }
}

static int _get(gnrc_netif_t *netif, gnrc_netapi_opt_t *opt)
{
    int res = 0;

    mlme_confirm_t mlme_confirm;
    mlme_request_t mlme_request;
    switch (opt->opt) {
        case NETOPT_OTAA:
            assert(opt->data_len >= sizeof(netopt_enable_t));
            *((netopt_enable_t *) opt->data) = netif->lorawan.otaa;
            break;
        case NETOPT_LINK_CONNECTED:
            mlme_request.type = MLME_GET;
            mlme_request.mib.type = MIB_ACTIVATION_METHOD;
            gnrc_lorawan_mlme_request(&netif->lorawan.mac, &mlme_request, &mlme_confirm);
            *((netopt_enable_t *) opt->data) = mlme_confirm.mib.activation != MLME_ACTIVATION_NONE;
            break;
        case NETOPT_LINK_CHECK:
            assert(opt->data_len == sizeof(netopt_enable_t));
            *((netopt_enable_t *) opt->data) = (netif->flags & GNRC_NETIF_FLAGS_LINK_CHECK) ?
                                               NETOPT_ENABLE : NETOPT_DISABLE;
            break;
        case NETOPT_NUM_GATEWAYS:
            assert(opt->data_len == sizeof(uint8_t));
            *((uint8_t *) opt->data) = netif->lorawan.num_gateways;
            break;
        case NETOPT_DEMOD_MARGIN:
            assert(opt->data_len == sizeof(uint8_t));
            *((uint8_t *) opt->data) = netif->lorawan.demod_margin;
            break;
        default:
            res = netif->lorawan.mac.netdev.driver->get(&netif->lorawan.mac.netdev, opt->opt, opt->data, opt->data_len);
    }
    return res;
}

static int _set(gnrc_netif_t *netif, const gnrc_netapi_opt_t *opt)
{
    int res = 0;
    mlme_confirm_t mlme_confirm;
    mlme_request_t mlme_request;

    gnrc_netif_acquire(netif);
    switch (opt->opt) {
        case NETOPT_LORAWAN_DR:
            assert(opt->data_len == sizeof(uint8_t));
            netif->lorawan.datarate = *((uint8_t *) opt->data);
            break;
        case NETOPT_LORAWAN_TX_PORT:
            assert(opt->data_len == sizeof(uint8_t));
            netif->lorawan.port = *((uint8_t *) opt->data);
            break;
        case NETOPT_ACK_REQ:
            assert(opt->data_len == sizeof(netopt_enable_t));
            netif->lorawan.ack_req = *((netopt_enable_t *) opt->data);
            break;
        case NETOPT_LORAWAN_APPKEY:
            assert(opt->data_len == LORAMAC_APPKEY_LEN);
            memcpy(netif->lorawan.appkey, opt->data, LORAMAC_APPKEY_LEN);
            break;
        case NETOPT_ADDRESS_LONG:
            assert(opt->data_len == LORAMAC_DEVEUI_LEN);
            _memcpy_reversed(netif->lorawan.deveui, opt->data, LORAMAC_DEVEUI_LEN);
            break;
        case NETOPT_LORAWAN_APPEUI:
            assert(opt->data_len == LORAMAC_APPEUI_LEN);
            _memcpy_reversed(netif->lorawan.appeui, opt->data, LORAMAC_APPEUI_LEN);
            break;
        case NETOPT_OTAA:
            assert(opt->data_len == sizeof(netopt_enable_t));
            netif->lorawan.otaa = *((netopt_enable_t *) opt->data);
            break;
        case NETOPT_LORAWAN_APPSKEY:
            assert(opt->data_len >= LORAMAC_APPSKEY_LEN);
            memcpy(netif->lorawan.appskey, opt->data, LORAMAC_APPSKEY_LEN);
            break;
        case NETOPT_LORAWAN_NWKSKEY:
            assert(opt->data_len >= LORAMAC_NWKSKEY_LEN);
            memcpy(netif->lorawan.nwkskey, opt->data, LORAMAC_NWKSKEY_LEN);
            break;
        case NETOPT_LINK_CONNECTED:
        {
            netopt_enable_t en = *((netopt_enable_t *) opt->data);
            if (en) {
                if(netif->lorawan.otaa) {
                    mlme_request.type = MLME_JOIN;
                    mlme_request.join.deveui = netif->lorawan.deveui;
                    mlme_request.join.appeui = netif->lorawan.appeui;
                    mlme_request.join.appkey = netif->lorawan.appkey;
                    mlme_request.join.dr = netif->lorawan.datarate;
                    gnrc_lorawan_mlme_request(&netif->lorawan.mac, &mlme_request, &mlme_confirm);
                }
                else {
                    mlme_request.type = MLME_SET;
                    mlme_request.mib.activation = MLME_ACTIVATION_ABP;
                    gnrc_lorawan_mlme_request(&netif->lorawan.mac, &mlme_request, &mlme_confirm);
                }
            }
            else {
                mlme_request.type = MLME_RESET;
                gnrc_lorawan_mlme_request(&netif->lorawan.mac, &mlme_request, &mlme_confirm);
                res = mlme_confirm.status;
                if (mlme_confirm.status == 0) {
                    /* reset netif as well */
                    _reset(netif);
                }
            }
            break;
        }
        case NETOPT_LINK_CHECK:
            netif->flags |= GNRC_NETIF_FLAGS_LINK_CHECK;
            break;
        default:
            res = netif->lorawan.mac.netdev.driver->set(&netif->lorawan.mac.netdev, opt->opt, opt->data, opt->data_len);
    }
    gnrc_netif_release(netif);
    return res;
}
/** @} */
