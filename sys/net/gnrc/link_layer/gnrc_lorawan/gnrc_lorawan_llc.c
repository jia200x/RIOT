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

static gnrc_pktsnip_t *_process_join_accept(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
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
        printf("BAD MIC!\n");
        gnrc_pktbuf_release(pkt);
        return NULL;
    }

    netif->lorawan.fcnt = 0;
    lorawan_join_accept_t *ja_hdr = (lorawan_join_accept_t*) pkt->data;
    generate_session_keys(ja_hdr->app_nonce, netif->lorawan.dev_nonce, netif->lorawan.appkey, netif->lorawan.nwkskey, netif->lorawan.appskey);

    /* Copy devaddr */
    memcpy(&netif->lorawan.dev_addr, ja_hdr->dev_addr, 4);

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
    gnrc_pktbuf_release(pkt);
    return NULL;
}

/* PRECONDITION: pkt is valid and doesn't contain MIC
 *
 * If fopts != NULL, there are options in the FOPTS field
 * It pkt->data == NULL, there's no payload
 *
 * If pkt == NULL, there was an error (and packet was freed)
 */
int gnrc_lorawan_parse_downlink(gnrc_pktsnip_t *pkt, 
        gnrc_pktsnip_t **hdr, gnrc_pktsnip_t **fopts, gnrc_pktsnip_t **port)
{
    int err = -EINVAL;
    *fopts = *hdr = *port = NULL;

    if (!(*hdr = gnrc_pktbuf_mark(pkt, sizeof(lorawan_hdr_t), GNRC_NETTYPE_UNDEF))) {
        puts("Couldn't allocate header");
        goto end;
    }

    lorawan_hdr_t *lw_hdr = (lorawan_hdr_t*) (*hdr)->data;

    uint8_t n_fopts = lorawan_hdr_get_frame_opts_len(lw_hdr);
    *fopts = gnrc_pktbuf_mark(pkt, n_fopts, GNRC_NETTYPE_UNDEF);

    /* Failed to allocate buffer */
    if(n_fopts && !fopts) {
        puts("Couldn't allocate fopts");
        goto end;
    }

    /* Port not present. Stop reading frame and process options (if any) */
    if(pkt->size == 0) {
        err = 0;
        goto end;
    }

    *port = gnrc_pktbuf_mark(pkt, 1, GNRC_NETTYPE_UNDEF);
    if(*port == NULL || pkt->size == 0)
    {
        puts("Couldn't allocate port or invalid packet");
        goto end;
    }
    err = 0;

end:
    return err;

}

int gnrc_lorawan_downlink_is_valid(gnrc_netif_t *netif, gnrc_pktsnip_t *hdr,
        gnrc_pktsnip_t *fopts, gnrc_pktsnip_t *port)
{
    /* TODO: Validate dev_addr */
    /* TODO: Validate fcnt */
    int err = -EINVAL;

    (void) hdr;
    (void) netif;

    if(fopts && port && *((uint8_t*) port->data) == 0) {
        goto end;
    }

    /* All OK */
    err = 0;

end:
    return err;
}
static gnrc_pktsnip_t *_process_downlink(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    uint8_t is_fopt_payload;
    gnrc_pktsnip_t *data, *hdr, *fopts, *port;

    if (!(data = gnrc_pktbuf_mark(pkt, (pkt->size-MIC_SIZE > 0) ? pkt->size-MIC_SIZE : 0, GNRC_NETTYPE_UNDEF))) {
        gnrc_pktbuf_release(pkt);
        pkt = NULL;
        goto end;
    }

    le_uint32_t expected_mic = *((le_uint32_t*) pkt->data);
    lorawan_hdr_t *lw_hdr = data->data;
    uint16_t fcnt = byteorder_ntohs(byteorder_ltobs(lw_hdr->fcnt));

    le_uint32_t mic;
    gnrc_lorawan_calculate_mic(&lw_hdr->addr, fcnt, 1, data, netif->lorawan.nwkskey, &mic);

    if (expected_mic.u32 != mic.u32) {
        puts("Invalid MIC");
        gnrc_pktbuf_release(pkt);
        pkt = NULL;
        goto end;
    }

    /* Remove MIC from packet buffer */
    pkt = gnrc_pktbuf_remove_snip(pkt, pkt);
    assert(data == pkt);

    if(gnrc_lorawan_parse_downlink(pkt, &hdr, &fopts, &port) < 0) {
        gnrc_pktbuf_release(pkt);
        pkt = NULL;
        goto end;
    }

    if(gnrc_lorawan_downlink_is_valid(netif, hdr, fopts, port) < 0) {
        gnrc_pktbuf_release(pkt);
        pkt = NULL;
        goto end;
    }

    lw_hdr = hdr->data;

    if (port) {
        is_fopt_payload = *((uint8_t*) port->data) == 0;
    }
    else {
        is_fopt_payload = 0;
    }

    /* If port is 0, NwkSKey is used to decrypt FRMPayload fopts */
    gnrc_lorawan_encrypt_payload(pkt->data, pkt->size, &lw_hdr->addr, fcnt, 1, is_fopt_payload ? netif->lorawan.nwkskey : netif->lorawan.appskey);

    assert((is_fopt_payload && fopts) == false);

    if(is_fopt_payload) {
        gnrc_lorawan_process_fopts(netif, pkt);
    }
    else {
        gnrc_lorawan_process_fopts(netif, fopts);
    }

    if(port == NULL || is_fopt_payload) {
        /* Everything we needed was already consumed. Drop packet */
        gnrc_pktbuf_release(pkt);
        pkt = NULL;
    }

end:
    return pkt;
}

void gnrc_lorawan_join_abp(gnrc_netif_t *netif)
{
    /* That's it! */
    puts("Joined with ABP");
    netif->lorawan.fcnt = 0;
    netif->lorawan.rx_delay = 1;
    netif->lorawan.joined = true;
}

gnrc_pktsnip_t *gnrc_lorawan_process_pkt(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    netif->lorawan.state = LORAWAN_STATE_IDLE;
    xtimer_remove(&netif->lorawan.rx_1);
    xtimer_remove(&netif->lorawan.rx_2);

    /* TODO: Pass to proper struct */
    uint8_t *p = pkt->data;

    uint8_t mtype = (*p & MTYPE_MASK) >> 5;
    switch(mtype) {
        case MTYPE_JOIN_ACCEPT:
            pkt = _process_join_accept(netif, pkt);
            break;
        case MTYPE_CNF_DOWNLINK:
        case MTYPE_UNCNF_DOWNLINK:
            pkt = _process_downlink(netif, pkt);
            if(pkt) {
                for(unsigned i=0;i<pkt->size;i++) {
                    printf("%02x ", ((uint8_t*) pkt->data)[i]);
                }
                printf("\n");
            }
            break;
        default:
            gnrc_pktbuf_release(pkt);
            pkt = NULL;
            break;
    }
    return pkt;
}


size_t gnrc_lorawan_build_hdr(uint8_t mtype, le_uint32_t *dev_addr, uint16_t fcnt, uint8_t fctrl, uint8_t fopts_length, lorawan_buffer_t *buf)
{
    assert(fopts_length < 16);
    lorawan_hdr_t *lw_hdr = (lorawan_hdr_t*) buf->data;

    lorawan_hdr_set_mtype(lw_hdr, mtype);
    lorawan_hdr_set_maj(lw_hdr, MAJOR_LRWAN_R1);

    lw_hdr->addr = *dev_addr;
    lw_hdr->fctrl = fctrl;

    lorawan_hdr_set_frame_opts_len(lw_hdr, fopts_length);

    lw_hdr->fcnt = byteorder_btols(byteorder_htons(fcnt));

    buf->index += sizeof(lorawan_hdr_t);

    return sizeof(lorawan_hdr_t);
}

/* TODO: REFACTOR */
/* TODO: Add error handling */
/* TODO: Check if it's possible to send in fopts!*/
/* TODO: No options so far */
gnrc_pktsnip_t *gnrc_lorawan_build_uplink(gnrc_netif_t *netif, gnrc_pktsnip_t *payload)
{
    gnrc_pktbuf_merge(payload);

    /* Encrypt payload (it's block encryption so we can use the same buffer!) */
    gnrc_lorawan_encrypt_payload(payload->data, payload->size, &netif->lorawan.dev_addr, netif->lorawan.fcnt, 0, netif->lorawan.port ? netif->lorawan.appskey : netif->lorawan.nwkskey);

    /* We try to allocate the whole header with fopts at once */
    uint8_t fopts_length = gnrc_lorawan_build_options(netif, NULL);
    
    gnrc_pktsnip_t *mac_hdr = gnrc_pktbuf_add(payload, NULL, sizeof(lorawan_hdr_t) + fopts_length + 1, GNRC_NETTYPE_UNDEF);

    lorawan_buffer_t buf = {
        .data = (uint8_t*) mac_hdr->data,
        .size = mac_hdr->size,
        .index = 0
    };

    gnrc_lorawan_build_hdr(netif->lorawan.confirmed_data ? MTYPE_CNF_UPLINK : MTYPE_UNCNF_UPLINK,
           &netif->lorawan.dev_addr, netif->lorawan.fcnt, 0, fopts_length, &buf);

    gnrc_lorawan_build_options(netif, &buf);

    assert(buf.index == mac_hdr->size-1);

    buf.data[buf.index++] = netif->lorawan.port;

    /* Now calculate MIC */
    gnrc_pktsnip_t *mic = gnrc_pktbuf_add(NULL, NULL, 4, GNRC_NETTYPE_UNDEF);
    uint32_t u32_mic = calculate_pkt_mic(0, (uint8_t*) &netif->lorawan.dev_addr, netif->lorawan.fcnt, mac_hdr, netif->lorawan.nwkskey);

    *((le_uint32_t*) mic->data) = byteorder_btoll(byteorder_htonl(u32_mic));
    LL_APPEND(payload, mic);

    return mac_hdr;
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
