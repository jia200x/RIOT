#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/lora.h"
#include "net/gnrc/lorawan/lorawan.h"
#include "errno.h"
#include "net/gnrc/pktbuf.h"

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

static void _process_join_accept(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    /* Decrypt packet */
    /* TODO: Proper handling */
    uint8_t out[32];
    decrypt_join_accept(netif->lorawan.appkey, ((uint8_t*) pkt->data)+1,
            (pkt->size-1) >= 16, out);
    memcpy(((uint8_t*) pkt->data)+1, out, pkt->size-1);

    /* Validate packet */

    uint32_t mic = calculate_mic(pkt->data, pkt->size-MIC_SIZE, netif->lorawan.appkey);
    if(!_compare_mic(mic, ((uint8_t*) pkt->data)+pkt->size-MIC_SIZE)) {
        printf("BAD MIC!");
        return;
    }

    netif->lorawan.fcnt = 0;
    generate_session_keys(((uint8_t*)pkt->data)+1, netif->lorawan.dev_nonce, netif->lorawan.appkey, netif->lorawan.nwkskey, netif->lorawan.appskey);

    /* Copy devaddr */
    memcpy(netif->lorawan.dev_addr, ((uint8_t*) pkt->data)+7, 4);

    netif->lorawan.dl_settings = *(((uint8_t*) pkt->data)+11);
    netif->lorawan.rx_delay = *(((uint8_t*) pkt->data)+12);

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

void gnrc_lorawan_process_pkt(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    /* TODO: Pass to proper struct */
    uint8_t *p = pkt->data;

    uint8_t mtype = (*p & MTYPE_MASK) >> 5;
    switch(mtype) {
        case MTYPE_JOIN_ACCEPT:
            _process_join_accept(netif, pkt);
            gnrc_pktbuf_release(pkt);
            break;
        default:
            break;
    }
}

/*TODO: REFACTOR */
gnrc_pktsnip_t *gnrc_lorawan_build_uplink(gnrc_netif_t *netif, gnrc_pktsnip_t *payload)
{
    /* TODO: Add error handling */
    gnrc_pktbuf_merge(payload);

    gnrc_pktsnip_t *enc_payload = gnrc_pktbuf_add(NULL, NULL, payload->size, GNRC_NETTYPE_UNDEF);
    gnrc_pktsnip_t *hdr = gnrc_pktbuf_add(enc_payload, NULL, sizeof(lorawan_hdr_t), GNRC_NETTYPE_UNDEF);

    lorawan_hdr_t *lw_hdr = hdr->data;

    lorawan_hdr_set_mtype(lw_hdr, MTYPE_UNCNF_UPLINK);
    lorawan_hdr_set_maj(lw_hdr, MAJOR_LRWAN_R1);

    /* TODO: */
    /* De-hack!*/

    le_uint32_t dev_addr = *((le_uint32_t*) netif->lorawan.dev_addr); 
    lw_hdr->addr = dev_addr;

    /* No options */
    lw_hdr->fctrl = 0;

    le_uint16_t fcnt = *((le_uint16_t*) &netif->lorawan.fcnt);
    lw_hdr->fcnt = fcnt;

    /* Hardcoded port is 1 */
    lw_hdr->port = 1;

    /* Encrypt payload */
    /*TODO*/
    encrypt_payload(payload->data, payload->size, netif->lorawan.dev_addr, netif->lorawan.fcnt, 0, netif->lorawan.appskey, enc_payload->data);

    gnrc_pktbuf_release(payload);

    /* Now calculate MIC */
    gnrc_pktsnip_t *mic = gnrc_pktbuf_add(NULL, NULL, 4, GNRC_NETTYPE_UNDEF);
    uint32_t u32_mic = calculate_pkt_mic(0, netif->lorawan.dev_addr, netif->lorawan.fcnt, hdr, netif->lorawan.nwkskey);

    *((le_uint32_t*) mic->data) = byteorder_btoll(byteorder_htonl(u32_mic));
    LL_APPEND(enc_payload, mic);

    return hdr;
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

static gnrc_pktsnip_t *_build_join_req_pkt(uint8_t *appeui, uint8_t *deveui, uint8_t *appkey, uint8_t *dev_nonce)
{
    gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, NULL, sizeof(lorawan_join_request_t), GNRC_NETTYPE_UNDEF);
    lorawan_join_request_t *hdr = (lorawan_join_request_t*) pkt->data;

    lorawan_hdr_set_mtype((lorawan_hdr_t*) hdr, MTYPE_JOIN_REQUEST);
    lorawan_hdr_set_maj((lorawan_hdr_t*) hdr, MAJOR_LRWAN_R1);

    le_uint64_t l_appeui = *((le_uint64_t*) appeui);
    le_uint64_t l_deveui = *((le_uint64_t*) deveui);

    hdr->app_eui = l_appeui;
    hdr->dev_eui = l_deveui;

    le_uint16_t l_dev_nonce = *((le_uint16_t*) dev_nonce);
    hdr->dev_nonce = l_dev_nonce;

    uint32_t mic = calculate_mic(pkt->data, JOIN_REQUEST_SIZE-MIC_SIZE, appkey);

    hdr->mic = byteorder_btoll(byteorder_htonl(mic));

    return pkt;
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
    gnrc_pktsnip_t *pkt = _build_join_req_pkt(netif->lorawan.appeui, netif->lorawan.deveui, netif->lorawan.appkey, netif->lorawan.dev_nonce);

    iolist_t iolist = {
        .iol_next = (iolist_t *)pkt->next,
        .iol_base = pkt->data,
        .iol_len = pkt->size
    };

    if (dev->driver->send(dev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    }
    gnrc_pktbuf_release(pkt);
    puts("Sent");
}
