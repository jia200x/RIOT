#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/lora.h"
#include "net/gnrc/lorawan.h"
#include "net/gnrc/lorawan/region.h"
#include "errno.h"
#include "net/gnrc/pktbuf.h"

#include "net/lorawan/hdr.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

void gnrc_lorawan_radio_sleep(gnrc_lorawan_t *mac)
{
    netopt_state_t state = NETOPT_STATE_SLEEP;
    netdev_set_pass((netdev_t *) mac, NETOPT_STATE, &state, sizeof(state));
}

void gnrc_lorawan_radio_set_cr(gnrc_lorawan_t *mac, uint8_t cr)
{
    netdev_set_pass(&mac->netdev, NETOPT_CODING_RATE, &cr, sizeof(cr));
}

void gnrc_lorawan_radio_set_syncword(gnrc_lorawan_t *mac, uint8_t syncword)
{
    netdev_set_pass(&mac->netdev, NETOPT_SYNCWORD, &syncword, sizeof(syncword));
}

void gnrc_lorawan_radio_set_frequency(gnrc_lorawan_t *mac, uint32_t channel)
{
    netdev_set_pass(&mac->netdev, NETOPT_CHANNEL_FREQUENCY, &channel, sizeof(channel));
}

void gnrc_lorawan_radio_set_iq_invert(gnrc_lorawan_t *mac, int invert)
{
    netopt_enable_t iq_invert = invert;
    netdev_set_pass(&mac->netdev, NETOPT_IQ_INVERT, &iq_invert, sizeof(iq_invert));
}

void gnrc_lorawan_radio_set_rx_symbol_timeout(gnrc_lorawan_t *mac, uint16_t timeout)
{
    /* Switch to single listen mode */
    const netopt_enable_t single = true;
    netdev_set_pass(&mac->netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));

    netdev_set_pass(&mac->netdev, NETOPT_RX_SYMBOL_TIMEOUT, &timeout, sizeof(timeout));
}

void gnrc_lorawan_radio_rx_on(gnrc_lorawan_t *mac)
{
    uint8_t state = NETOPT_STATE_RX;
    netdev_set_pass(&mac->netdev, NETOPT_STATE, &state, sizeof(state));
}

void gnrc_lorawan_radio_set_sf(gnrc_lorawan_t *mac, uint8_t sf)
{
    netdev_t *dev = mac->netdev.lower;
    dev->driver->set(dev, NETOPT_SPREADING_FACTOR, &sf, sizeof(sf));
}

void gnrc_lorawan_radio_set_bw(gnrc_lorawan_t *mac, uint8_t bw)
{
    netdev_t *dev = mac->netdev.lower;
    dev->driver->set(dev, NETOPT_BANDWIDTH, &bw, sizeof(bw));
}

void gnrc_lorawan_radio_send(gnrc_lorawan_t *mac, iolist_t *io)
{
    if (netdev_send_pass(&mac->netdev, io) == -ENOTSUP) {
        DEBUG("gnrc_lorawan: Cannot send: radio is still transmitting");
    }
}
