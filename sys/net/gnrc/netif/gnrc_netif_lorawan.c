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
#include "net/gnrc/lorawan/lorawan.h"
#include "net/netdev.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt);
static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif);
static void _msg_handler(gnrc_netif_t *netif, msg_t *msg);
static int _get(gnrc_netif_t *netif, gnrc_netapi_opt_t *opt);
static int _set(gnrc_netif_t *netif, const gnrc_netapi_opt_t *opt);
static void _init(gnrc_netif_t *netif);

static const gnrc_netif_ops_t lorawan_ops = {
    .init = _init,
    .send = _send,
    .recv = _recv,
    .get = _get,
    .set = _set,
    .msg_handler = _msg_handler
};

static void _event_cb(netdev_t *dev, netdev_event_t event)
{
    gnrc_netif_t *netif = (gnrc_netif_t *) dev->context;

    if (event == NETDEV_EVENT_ISR) {
        msg_t msg = { .type = NETDEV_MSG_TYPE_EVENT,
                      .content = { .ptr = netif } };

        if (msg_send(&msg, netif->pid) <= 0) {
            puts("gnrc_netif: possibly lost interrupt.");
        }
    }
    else {
        DEBUG("gnrc_netif: event triggered -> %i\n", event);
        switch (event) {
            case NETDEV_EVENT_RX_COMPLETE: {
                    gnrc_pktsnip_t *pkt = netif->ops->recv(netif);

                    (void) pkt;
                    /*
                    if (pkt) {
                        _pass_on_packet(pkt);
                    }*/
                }
                break;
        case NETDEV_EVENT_TX_COMPLETE:
            /* we are the only ones supposed to touch this variable,
             * so no acquire necessary */
            puts("DONE");
            break;
#ifdef MODULE_NETSTATS_L2
            case NETDEV_EVENT_TX_MEDIUM_BUSY:
                /* we are the only ones supposed to touch this variable,
                 * so no acquire necessary */
                dev->stats.tx_failed++;
                break;
            case NETDEV_EVENT_TX_COMPLETE:
                /* we are the only ones supposed to touch this variable,
                 * so no acquire necessary */
                dev->stats.tx_success++;
                puts("DONE");
                break;
#endif
            default:
                DEBUG("gnrc_netif: warning: unhandled event %u.\n", event);
        }
    }
}
static void _init(gnrc_netif_t *netif)
{
    netif->dev->event_callback = _event_cb;
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
    (void) netif;
    return NULL;
}

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    (void) netif;
    (void) pkt;
    return 0;
}

static void _msg_handler(gnrc_netif_t *netif, msg_t *msg)
{
    (void) netif;
    (void) msg;
}

static int _get(gnrc_netif_t *netif, gnrc_netapi_opt_t *opt)
{
    (void) netif;
    (void) opt;
    return 0;
}

static int _set(gnrc_netif_t *netif, const gnrc_netapi_opt_t *opt)
{
    (void) netif;
    switch(opt->opt) {
        case NETOPT_JOIN:
            gnrc_lorawan_send_join_request(netif);
            puts("Good luck!");
            break;
        case NETOPT_ADDRESS_LONG:
            memcpy(&netif->lorawan.deveui, opt->data, opt->data_len);
            break;
        case NETOPT_APPEUI:
            memcpy(&netif->lorawan.appeui, opt->data, opt->data_len);
            break;
        case NETOPT_APPKEY:
            memcpy(&netif->lorawan.appkey, opt->data, opt->data_len);
            break;
        default:
            netif->dev->driver->get(netif->dev, opt->opt, opt->data, opt->data_len);
            break;
    }
    return 0;
}
/** @} */
