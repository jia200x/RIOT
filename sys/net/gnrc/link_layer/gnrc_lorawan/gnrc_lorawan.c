#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/lora.h"
#include "net/gnrc/lorawan/lorawan.h"
#include "errno.h"

#include "net/lorawan/hdr.h"

#define GNRC_LORAWAN_NUMOF_DATARATES 7

static uint8_t dr_sf[GNRC_LORAWAN_NUMOF_DATARATES] = {LORA_SF12, LORA_SF11, LORA_SF10, LORA_SF9, LORA_SF8, LORA_SF7, LORA_SF7};
static uint8_t dr_bw[GNRC_LORAWAN_NUMOF_DATARATES] = {LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_250_KHZ};

static int _compare_mic(uint32_t expected_mic, uint8_t *mic_buf)
{
   uint32_t mic = mic_buf[0] | (mic_buf[1] << 8) |
       (mic_buf[2] << 16) | (mic_buf[3] << 24);
   return expected_mic == mic;
}

static void _process_join_accept(gnrc_netif_t *netif, uint8_t *pkt, size_t size)
{
    /* TODO: PACKET < 33 */

    /* Decrypt packet */
    uint8_t out[32];
    decrypt_join_accept(netif->lorawan.appkey, pkt+1, (size-1) >= 16, out);
    memcpy(pkt+1, out, size-1);

    /* Validate packet */

    uint32_t mic = calculate_mic(pkt, size-MIC_SIZE, netif->lorawan.appkey);
    if(!_compare_mic(mic, pkt+size-MIC_SIZE)) {
        printf("BAD MIC!");
        return;
    }

    netif->lorawan.fcnt = 0;
    generate_session_keys(pkt+1, netif->lorawan.dev_nonce, netif->lorawan.appkey, netif->lorawan.nwkskey, netif->lorawan.appskey);

    for(int i=0;i<33;i++) {
        printf("%02x ", pkt[i]);
    }
    printf("\n");

    /* Copy devaddr */
    memcpy(netif->lorawan.dev_addr, pkt+7, 4);

    netif->lorawan.dl_settings = *(pkt+11);
    netif->lorawan.rx_delay = *(pkt+12);

    printf("dl_settings: %i\n", netif->lorawan.dl_settings);
    printf("rx_delay: %i\n", netif->lorawan.rx_delay);

    printf("NWKSKEY: ");
    for(int i=0;i<16;i++) {
        printf("%02x ", netif->lorawan.nwkskey[i]);
    }
    printf("\n");

    printf("APPSKEY: ");
    for(int i=0;i<16;i++) {
        printf("%02x ", netif->lorawan.appskey[i]);
    }
    printf("\n");
    netif->lorawan.joined = true;
}

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

    return 0;
}

void gnrc_lorawan_process_pkt(gnrc_netif_t *netif, uint8_t *pkt, size_t size)
{
    (void) size;
    uint8_t *p = pkt;

    uint8_t mtype = (*p & MTYPE_MASK) >> 5;
    switch(mtype) {
        case MTYPE_JOIN_ACCEPT:
            _process_join_accept(netif, pkt, size);
            break;
        default:
            break;
    }
}

size_t gnrc_lorawan_build_uplink(gnrc_netif_t *netif, uint8_t *pkt_buf)
{
    lorawan_hdr_t *hdr = (lorawan_hdr_t*) pkt_buf;

    lorawan_hdr_set_mtype(hdr, MTYPE_UNCNF_UPLINK);
    lorawan_hdr_set_maj(hdr, MAJOR_LRWAN_R1);

    /* TODO: */
    /* De-hack!*/

    le_uint32_t dev_addr = *((le_uint32_t*) netif->lorawan.dev_addr); 
    hdr->addr = dev_addr;

    /* No options */
    hdr->fctrl = 0;

    le_uint16_t fcnt = *((le_uint16_t*) &netif->lorawan.fcnt);
    hdr->fcnt = fcnt;
    hdr->port = 1;

    uint8_t *p=(pkt_buf+sizeof(lorawan_hdr_t));
    uint8_t payload[] = "RIOT";

    /* Encrypt payload */
    uint8_t enc_payload[4];
    encrypt_payload(payload, sizeof(payload), netif->lorawan.dev_addr, netif->lorawan.fcnt, 0, netif->lorawan.appskey, enc_payload);
    PKT_WRITE(p, enc_payload, sizeof(payload) - 1);

    /* Now calculate MIC */
    uint32_t mic = calculate_pkt_mic(0, netif->lorawan.dev_addr, netif->lorawan.fcnt, pkt_buf, p-pkt_buf, netif->lorawan.nwkskey);

    *((le_uint32_t*) p) = byteorder_btoll(byteorder_htonl(mic));
    p+=4;

    return p-pkt_buf;
}

void gnrc_lorawan_open_rx_window(gnrc_netif_t *netif)
{
    netdev_t *netdev = netif->dev;
    netopt_enable_t iq_invert = true;
    netdev->driver->set(netdev, NETOPT_IQ_INVERT, &iq_invert, sizeof(iq_invert));
    //sx127x_set_iq_invert(&sx127x, true);

    gnrc_lorawan_set_dr(netif, 5);

    /* Switch to continuous listen mode */
    const netopt_enable_t single = true;
    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
    const uint32_t timeout = 25;
    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));

    /* Switch to RX state */
    uint8_t state = NETOPT_STATE_RX;
    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));
}

static size_t _build_join_req_pkt(uint8_t *appeui, uint8_t *deveui, uint8_t *appkey, uint8_t *dev_nonce, uint8_t *packet)
{
    uint8_t *p = packet;

    uint8_t mhdr = 0;

    /* Message type */
    mhdr &= ~MTYPE_MASK;
    mhdr |= MTYPE_JOIN_REQUEST << 5;

    /* Major */
    mhdr &= ~MAJOR_MASK;
    mhdr |= MAJOR_LRWAN_R1;

    PKT_WRITE_BYTE(p, mhdr);
    PKT_WRITE(p, appeui, 8);
    PKT_WRITE(p, deveui, 8);

    PKT_WRITE(p, dev_nonce, 2);

    uint32_t mic = calculate_mic(packet, JOIN_REQUEST_SIZE-MIC_SIZE, appkey);

    PKT_WRITE_BYTE(p, mic & 0xFF);
    PKT_WRITE_BYTE(p, (mic >> 8) & 0xFF);
    PKT_WRITE_BYTE(p, (mic >> 16) & 0xFF);
    PKT_WRITE_BYTE(p, (mic >> 24) & 0xFF);

    return JOIN_REQUEST_SIZE;
}

void gnrc_lorawan_event_tx_complete(gnrc_netif_t *netif)
{
    netif->lorawan.msg.type = MSG_TYPE_TIMEOUT;
    /* This logic might change */
    if(!netif->lorawan.joined) {
        /* Join request */
        xtimer_set_msg(&netif->lorawan.rx_1, 4950000, &netif->lorawan.msg, netif->pid);
    }
    else {
        puts("It's joined");
        xtimer_set_msg(&netif->lorawan.rx_1, 950000, &netif->lorawan.msg, netif->pid);
    }


    /* TODO: Sleep interface */
}
void gnrc_lorawan_send_join_request(gnrc_netif_t *netif)
{
    netdev_t *dev = netif->dev;

    uint8_t buf[24];

    uint32_t channel_freq = 868300000;
    
    gnrc_lorawan_set_dr(netif, 5);

    dev->driver->set(dev, NETOPT_CHANNEL_FREQUENCY, &channel_freq, sizeof(channel_freq));

    /* Dev Nonce */
    uint32_t random_number;
    dev->driver->get(dev, NETOPT_RANDOM, &random_number, sizeof(random_number));
    printf("Random: %i\n", (unsigned) random_number);

    netif->lorawan.dev_nonce[0] = random_number & 0xFF;
    netif->lorawan.dev_nonce[1] = (random_number >> 8) & 0xFF;

    /* build join request */
    size_t pkt_size = _build_join_req_pkt(netif->lorawan.appeui, netif->lorawan.deveui, netif->lorawan.appkey, netif->lorawan.dev_nonce, buf);

    iolist_t iolist = {
        .iol_base = buf,
        .iol_len = pkt_size
    };

    for(unsigned int i=0;i<pkt_size;i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");
    uint8_t syncword = LORA_SYNCWORD_PUBLIC;

    dev->driver->set(dev, NETOPT_SYNCWORD, &syncword, sizeof(syncword));

    if (dev->driver->send(dev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    }
    puts("Sent");
}
