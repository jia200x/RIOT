#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/lora.h"
#include "net/gnrc/lorawan/lorawan.h"
#include "net/gnrc/lorawan/region.h"
#include "errno.h"
#include "net/gnrc/pktbuf.h"

#include "net/lorawan/hdr.h"

static int _compare_mic(uint32_t expected_mic, uint8_t *mic_buf)
{
   uint32_t mic = mic_buf[0] | (mic_buf[1] << 8) |
       (mic_buf[2] << 16) | (mic_buf[3] << 24);
   return expected_mic == mic;
}

static void _process_join_accept(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    /* TODO: Validate packet size */

    /* TODO: Proper handling */
    uint8_t out[32];
    uint8_t has_cflist = (pkt->size-1) >= 16;
    decrypt_join_accept(netif->lorawan.appkey, ((uint8_t*) pkt->data)+1,
            has_cflist, out);
    memcpy(((uint8_t*) pkt->data)+1, out, pkt->size-1);

    uint32_t mic = calculate_mic(pkt->data, pkt->size-MIC_SIZE, netif->lorawan.appkey);
    if(!_compare_mic(mic, ((uint8_t*) pkt->data)+pkt->size-MIC_SIZE)) {
        printf("BAD MIC!");
        return;
    }

    netif->lorawan.fcnt = 0;
    lorawan_join_accept_t *ja_hdr = (lorawan_join_accept_t*) pkt->data;
    generate_session_keys(ja_hdr->app_nonce, netif->lorawan.dev_nonce, netif->lorawan.appkey, netif->lorawan.nwkskey, netif->lorawan.appskey);

    /* Copy devaddr */
    memcpy(netif->lorawan.dev_addr, ja_hdr->dev_addr, 4);

    netif->lorawan.dl_settings = ja_hdr->dl_settings;

    /* delay 0 maps to 1 second */
    netif->lorawan.rx_delay = ja_hdr->rx_delay ? ja_hdr->rx_delay : 1;

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

    gnrc_lorawan_process_cflist(netif, out+sizeof(lorawan_join_accept_t)-1);
    netif->lorawan.joined = true;
}


void gnrc_lorawan_join_abp(gnrc_netif_t *netif)
{
    /* That's it! */
    puts("Joined with ABP");
    netif->lorawan.fcnt = 0;
    netif->lorawan.rx_delay = 1;
    netif->lorawan.joined = true;
}
void gnrc_lorawan_process_pkt(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    netif->lorawan.state = LORAWAN_STATE_IDLE;
    xtimer_remove(&netif->lorawan.rx_1);
    xtimer_remove(&netif->lorawan.rx_2);
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

    /* TODO: Port and fopts */
    gnrc_pktsnip_t *hdr = gnrc_pktbuf_add(payload, NULL, sizeof(lorawan_hdr_t) + 1, GNRC_NETTYPE_UNDEF);

    lorawan_hdr_t *lw_hdr = hdr->data;

    lorawan_hdr_set_mtype(lw_hdr, netif->lorawan.confirmed_data ? MTYPE_CNF_UPLINK : MTYPE_UNCNF_UPLINK);
    lorawan_hdr_set_maj(lw_hdr, MAJOR_LRWAN_R1);

    /* TODO: */
    /* De-hack!*/

    le_uint32_t dev_addr = *((le_uint32_t*) netif->lorawan.dev_addr); 
    lw_hdr->addr = dev_addr;

    /* No options */
    lw_hdr->fctrl = 0;

    le_uint16_t fcnt = *((le_uint16_t*) &netif->lorawan.fcnt);
    lw_hdr->fcnt = fcnt;

    /* TODO:Hardcoded port is 1 */
    *((uint8_t*) (lw_hdr+1)) = netif->lorawan.port;

    /* Encrypt payload (it's block encryption so we can use the same buffer!) */
    encrypt_payload(payload->data, payload->size, netif->lorawan.dev_addr, netif->lorawan.fcnt, 0, netif->lorawan.port ? netif->lorawan.appskey : netif->lorawan.nwkskey);

    /* Now calculate MIC */
    gnrc_pktsnip_t *mic = gnrc_pktbuf_add(NULL, NULL, 4, GNRC_NETTYPE_UNDEF);
    uint32_t u32_mic = calculate_pkt_mic(0, netif->lorawan.dev_addr, netif->lorawan.fcnt, hdr, netif->lorawan.nwkskey);

    *((le_uint32_t*) mic->data) = byteorder_btoll(byteorder_htonl(u32_mic));
    LL_APPEND(payload, mic);

    return hdr;
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

/*TODO: Move to gnrc_netif*/
void gnrc_lorawan_send_join_request(gnrc_netif_t *netif)
{
    netdev_t *dev = netif->dev;
    netif->lorawan.state = LORAWAN_STATE_TX;

    uint32_t channel_freq = gnrc_lorawan_pick_channel(netif);
    
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
