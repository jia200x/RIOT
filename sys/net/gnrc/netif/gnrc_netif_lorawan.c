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
#include "sx127x_netdev.h"
#include "net/lora.h"

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
                gnrc_lorawan_event_tx_complete(netif);
                puts("Transmission completed");
                break;
            default:
                DEBUG("gnrc_netif: warning: unhandled event %u.\n", event);
        }
    }
}
static void _init(gnrc_netif_t *netif)
{
    netif->dev->event_callback = _event_cb;
    uint8_t cr = LORA_CR_4_5;
    netif->dev->driver->set(netif->dev, NETOPT_CODING_RATE, &cr, sizeof(cr));

    uint8_t syncword = LORA_SYNCWORD_PUBLIC;
    netif->dev->driver->set(netif->dev, NETOPT_SYNCWORD, &syncword, sizeof(syncword));


    netif->lorawan.joined = false;
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
    netdev_t *dev = netif->dev;
    int bytes_expected = dev->driver->recv(dev, NULL, 0, 0);
    int nread;
    netdev_sx127x_lora_packet_info_t rx_info;
    gnrc_pktsnip_t *pkt = NULL;

    pkt = gnrc_pktbuf_add(NULL, NULL, bytes_expected, GNRC_NETTYPE_UNDEF);
    if (pkt == NULL) {
        DEBUG("_recv_ieee802154: cannot allocate pktsnip.\n");
        /* Discard packet on netdev device */
        dev->driver->recv(dev, NULL, bytes_expected, NULL);
        return NULL;
    }
    nread = dev->driver->recv(dev, pkt->data, bytes_expected, &rx_info);
    if (nread <= 0) {
        gnrc_pktbuf_release(pkt);
        return NULL;
    }

    /* TODO: Time On Air from netdev! */

    printf("{Payload: \"%s\" (%d bytes), RSSI: %i, SNR: %i, TOA: %lu}\n",
           (char*) pkt->data, (int) pkt->size,
           rx_info.rssi, (int)rx_info.snr,
           (long int) 0);
    gnrc_lorawan_process_pkt(netif, pkt->data, pkt->size);
    return NULL;
}

uint8_t pkt_buf[50];

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    if(!netif->lorawan.joined) {
        puts("LoRaWAN not joined!");
        return 0;
    }
    netdev_t *netdev = netif->dev;
    (void) pkt;
    uint32_t chan = 868300000;

    netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan, sizeof(chan));

    netopt_enable_t iq_invert = false;
    netdev->driver->set(netdev, NETOPT_IQ_INVERT, &iq_invert, sizeof(iq_invert));

    /* build join request */
    size_t pkt_size = gnrc_lorawan_build_uplink(netif, pkt_buf);

    iolist_t iolist = {
        .iol_base = pkt_buf,
        .iol_len = pkt_size
    };

    for(unsigned int i=0;i<pkt_size;i++) {
        printf("%02x ", pkt_buf[i]);
    }
    printf("\n");

    uint8_t syncword = LORA_SYNCWORD_PUBLIC;
    netdev->driver->set(netdev, NETOPT_SYNCWORD, &syncword, sizeof(syncword));

    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    }
    
    /* TODO:! */
    netif->lorawan.fcnt += 1;
    return 0;
}

static void _msg_handler(gnrc_netif_t *netif, msg_t *msg)
{
    (void) netif;
    (void) msg;
    puts(":)");
    switch(msg->type) {
        case MSG_TYPE_TIMEOUT:
            gnrc_lorawan_open_rx_window(netif);
            break;
        default:
            break;
    }
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
        case NETOPT_DATARATE:
            assert(opt->data_len == sizeof(uint8_t));
            gnrc_lorawan_set_dr(netif, *(uint8_t*) opt->data);  
            break;
        default:
            netif->dev->driver->get(netif->dev, opt->opt, opt->data, opt->data_len);
            break;
    }
    return 0;
}
/** @} */
