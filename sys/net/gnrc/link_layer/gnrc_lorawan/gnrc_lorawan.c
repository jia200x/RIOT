#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/lora.h"
#include "net/gnrc/lorawan/lorawan.h"
#include "errno.h"
#include "net/gnrc/pktbuf.h"

#include "net/lorawan/hdr.h"
#include "net/gnrc/lorawan/region.h"

#define GNRC_LORAWAN_NUMOF_DATARATES 7

static uint8_t dr_sf[GNRC_LORAWAN_NUMOF_DATARATES] = {LORA_SF12, LORA_SF11, LORA_SF10, LORA_SF9, LORA_SF8, LORA_SF7, LORA_SF7};
static uint8_t dr_bw[GNRC_LORAWAN_NUMOF_DATARATES] = {LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_250_KHZ};

int gnrc_lorawan_set_dr(gnrc_netif_t *netif, uint8_t datarate)
{
    netdev_t *dev = netif->dev;
    if(datarate > GNRC_LORAWAN_NUMOF_DATARATES) {
        //TODO
        return -1;
    }
    uint8_t bw = dr_bw[datarate];
    uint8_t sf = dr_sf[datarate];

    dev->driver->set(dev, NETOPT_BANDWIDTH, &bw, sizeof(bw));
    dev->driver->set(dev, NETOPT_SPREADING_FACTOR, &sf, sizeof(sf));

    netif->lorawan.datarate = datarate;

    return 0;
}

int gnrc_lorawan_buffer_reset(lorawan_buffer_t *buf, uint8_t *data, size_t length)
{
    if(!buf || !data || !length) {
        return -EINVAL;
    }

    buf->data = data;
    buf->size = length;
    buf->index = 0;
    return 0;
}

uint8_t *gnrc_lorawan_read_bytes(lorawan_buffer_t *buf, size_t length)
{
    if(!length || buf->index + length > buf->size) {
        return NULL;
    }

    uint8_t *p = buf->data+ buf->index;
    buf->index += length;
    return p;
}

int gnrc_lorawan_write_bytes(lorawan_buffer_t *buf, uint8_t *bytes, size_t length)
{
    if(!length || buf->index + length > buf->size) {
        return 0;
    }

    memcpy(buf->data+buf->index, bytes, length);
    buf->index += length;
    return length;
}

int gnrc_lorawan_set_pending_fopt(gnrc_netif_t *netif, uint8_t cid, uint8_t value)
{
    if(!cid) {
        return -EINVAL;
    }

    uint8_t col = (cid-1) >> 3;
    uint8_t row = ((cid - 1) % 8) + 1;

    netif->lorawan.fopts[col] &= ~(1<<row);
    netif->lorawan.fopts[col] |= (!!value) << row;
    return 0;
}

int gnrc_lorawan_get_pending_fopt(gnrc_netif_t *netif, uint8_t cid)
{
    if(!cid) {
        return -EINVAL;
    }

    uint8_t col = (cid-1) >> 3;
    uint8_t row = ((cid - 1) % 8) + 1;

    return netif->lorawan.fopts[col] & (1<<row);
}

void gnrc_lorawan_reset(gnrc_netif_t *netif)
{
    uint8_t cr = LORA_CR_4_5;
    netif->dev->driver->set(netif->dev, NETOPT_CODING_RATE, &cr, sizeof(cr));

    uint8_t syncword = LORA_SYNCWORD_PUBLIC;
    netif->dev->driver->set(netif->dev, NETOPT_SYNCWORD, &syncword, sizeof(syncword));

    uint8_t confirmed_data = false;
    netif->dev->driver->set(netif->dev, NETOPT_ACK_REQ, &confirmed_data, sizeof(confirmed_data));

    netif->lorawan.joined = false;
    netif->lorawan.ack_requested = false;
    /* TODO: Default port */
    netif->lorawan.port = 1;
    memset(netif->lorawan.fopts, 0, sizeof(netif->lorawan.fopts));

    gnrc_lorawan_channels_init(netif);
}

/* TODO: Merge into one function */
static void _configure_rx_window(gnrc_netif_t *netif)
{
    netdev_t *netdev = netif->dev;
    netopt_enable_t iq_invert = true;
    netdev->driver->set(netdev, NETOPT_IQ_INVERT, &iq_invert, sizeof(iq_invert));

    /* TODO: Add DR1 offset */
    uint8_t dr_offset = (netif->lorawan.dl_settings >> 4) & 0x7;
    gnrc_lorawan_set_dr(netif, gnrc_lorawan_rx1_get_dr_offset(netif->lorawan.datarate, dr_offset));

    /* Switch to continuous listen mode */
    const netopt_enable_t single = true;
    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
    const uint32_t timeout = 25;
    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));
}

static void _configure_rx_window_2(gnrc_netif_t *netif)
{
    netdev_t *netdev = netif->dev;
    uint32_t channel_freq = 869525000;
    netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &channel_freq, sizeof(channel_freq));

    netopt_enable_t iq_invert = true;
    netdev->driver->set(netdev, NETOPT_IQ_INVERT, &iq_invert, sizeof(iq_invert));
    //sx127x_set_iq_invert(&sx127x, true);

    /* Get DR for RX 2 */
    /* TODO: Implement helper for this function */
    uint8_t dr_rx2 = netif->lorawan.dl_settings & 0xF;
    gnrc_lorawan_set_dr(netif, dr_rx2);

    /* Switch to continuous listen mode */
    const netopt_enable_t single = true;
    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
    const uint32_t timeout = 25;
    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));
}

void gnrc_lorawan_open_rx_window(gnrc_netif_t *netif)
{
    netdev_t *netdev = netif->dev;
    /* Switch to RX state */
    uint8_t state = NETOPT_STATE_RX;
    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));
}

void gnrc_lorawan_event_tx_complete(gnrc_netif_t *netif)
{
    netif->lorawan.msg.type = MSG_TYPE_TIMEOUT;
    netif->lorawan.msg_2.type = MSG_TYPE_TIMEOUT;
    netif->lorawan.state = LORAWAN_STATE_RX_1;

    /* TODO: This logic WILL change */
    if(!netif->lorawan.joined) {
        /* Join request */
        xtimer_set_msg(&netif->lorawan.rx_1, 4950000, &netif->lorawan.msg, netif->pid);
        xtimer_set_msg(&netif->lorawan.rx_2, 5950000, &netif->lorawan.msg_2, netif->pid);
        _configure_rx_window(netif);
    }
    else {
        puts("It's joined");
        xtimer_set_msg(&netif->lorawan.rx_1, netif->lorawan.rx_delay*1000000-50000, &netif->lorawan.msg, netif->pid);
        xtimer_set_msg(&netif->lorawan.rx_2, 1000000 + netif->lorawan.rx_delay*1000000-20000, &netif->lorawan.msg_2, netif->pid);
        _configure_rx_window(netif);
    }
    /* TODO: Sleep interface */
}

void gnrc_lorawan_event_timeout(gnrc_netif_t *netif)
{
    (void) netif;
    puts("gnrc_lorawan_event_timeout");
    if(netif->lorawan.state == LORAWAN_STATE_RX_1) {
        puts("Configuring RX2");
        _configure_rx_window_2(netif);
        netif->lorawan.state = LORAWAN_STATE_RX_2;
    }
}

