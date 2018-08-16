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

#define MSG_TYPE_TIMEOUT            (0x3457)

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
                netif->lorawan.msg.type = MSG_TYPE_TIMEOUT;
                xtimer_set_msg(&netif->lorawan.rx_1, 4950000, &netif->lorawan.msg, netif->pid);
                //sx127x_set_sleep(&sx127x);
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
}

gnrc_netif_t *gnrc_netif_lorawan_create(char *stack, int stacksize,
                                    char priority, char *name,
                                    netdev_t *dev)
{
    return gnrc_netif_create(stack, stacksize, priority, name, dev,
                             &lorawan_ops);
}

void gnrc_lorawan_open_rx_window(gnrc_netif_t *netif)
{
    netdev_t *netdev = netif->dev;
    netopt_enable_t iq_invert = true;
    netdev->driver->set(netdev, NETOPT_IQ_INVERT, &iq_invert, sizeof(iq_invert));
    //sx127x_set_iq_invert(&sx127x, true);

    uint8_t bw = LORA_BW_125_KHZ;
    uint8_t sf = LORA_SF7;
    //uint8_t cr = LORA_CR_4_5;
    netdev->driver->set(netdev, NETOPT_BANDWIDTH, &bw, sizeof(bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR, &sf, sizeof(sf));
    //netdev->driver->set(netdev, NETOPT_CODING_RATE, &cr, sizeof(cr));
    //sx127x_set_channel(&sx127x, 869525000);

    /* Switch to continuous listen mode */
    const netopt_enable_t single = true;
    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
    const uint32_t timeout = 25;
    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));

    /* Switch to RX state */
    uint8_t state = NETOPT_STATE_RX;
    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));
}

uint8_t pkt_buf[50];

size_t build_uplink(gnrc_netif_t *netif)
{
    uint8_t *p = pkt_buf;
    uint8_t mhdr = 0;

    /* Message type */
    mhdr &= ~MTYPE_MASK;
    mhdr |= MTYPE_UNCNF_UPLINK << 5;

    /* Major */
    mhdr &= ~MAJOR_MASK;
    mhdr |= MAJOR_LRWAN_R1;

    PKT_WRITE_BYTE(p, mhdr);
    PKT_WRITE(p, netif->lorawan.dev_addr, 4);

    /* No options */
    PKT_WRITE_BYTE(p, 0);

    /* Frame counter */
    PKT_WRITE_BYTE(p, netif->lorawan.fcnt & 0xFF);
    PKT_WRITE_BYTE(p, (netif->lorawan.fcnt >> 8) & 0xFF);

    /* Port */
    PKT_WRITE_BYTE(p, 1);

    uint8_t payload[] = "RIOT";

    /* Encrypt payload */
    uint8_t enc_payload[4];
    encrypt_payload(payload, sizeof(payload), netif->lorawan.dev_addr, netif->lorawan.fcnt, 0, netif->lorawan.appskey, enc_payload);
    PKT_WRITE(p, enc_payload, sizeof(payload) - 1);

    /* Now calculate MIC */
    /* TODO: */
    uint32_t mic = calculate_pkt_mic(0, netif->lorawan.dev_addr, netif->lorawan.fcnt, pkt_buf, p-pkt_buf, netif->lorawan.nwkskey);

    PKT_WRITE_BYTE(p, mic & 0xFF);
    PKT_WRITE_BYTE(p, (mic >> 8) & 0xFF);
    PKT_WRITE_BYTE(p, (mic >> 16) & 0xFF);
    PKT_WRITE_BYTE(p, (mic >> 24) & 0xFF);

    return p-pkt_buf;
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

    //TODO: Remove sx127x info
    /* TODO: Time On Air from netdev! */

    printf("{Payload: \"%s\" (%d bytes), RSSI: %i, SNR: %i, TOA: %lu}\n",
           (char*) pkt->data, (int) pkt->size,
           rx_info.rssi, (int)rx_info.snr,
           (long int) 0);
    gnrc_lorawan_process_pkt(netif, pkt->data, pkt->size);
    return NULL;
}

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    netdev_t *netdev = netif->dev;
    (void) pkt;
    uint32_t chan = 868300000;
    uint8_t bw = LORA_BW_125_KHZ;
    uint8_t sf = LORA_SF7;
    netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan, sizeof(chan));
    netdev->driver->set(netdev, NETOPT_BANDWIDTH, &bw, sizeof(bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR, &sf, sizeof(sf));

    netopt_enable_t iq_invert = false;
    netdev->driver->set(netdev, NETOPT_IQ_INVERT, &iq_invert, sizeof(iq_invert));

    /* build join request */
    size_t pkt_size = build_uplink(netif);

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
    
    netif->lorawan.fcnt += 1;
    xtimer_sleep(3);
    puts("Let's pray!");
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
