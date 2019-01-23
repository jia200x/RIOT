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

    printf("Joined!\n");

    gnrc_lorawan_process_cflist(netif, out+sizeof(lorawan_join_accept_t)-1);
    netif->lorawan.joined = true;
    gnrc_pktbuf_release(pkt);
    return NULL;
}

gnrc_pktsnip_t *gnrc_lorawan_get_mac_payload(gnrc_pktsnip_t *pkt, uint8_t *nwkskey)
{
    gnrc_pktsnip_t *data; 

    if (!(data = gnrc_pktbuf_mark(pkt, (pkt->size-MIC_SIZE > 0) ? pkt->size-MIC_SIZE : 0, GNRC_NETTYPE_UNDEF))) {
        gnrc_pktbuf_release(pkt);
        pkt = NULL;
        goto end;
    }

    assert(pkt->size == MIC_SIZE);

    lorawan_hdr_t *lw_hdr = (lorawan_hdr_t*) data->data;

    uint32_t fcnt = byteorder_ntohs(byteorder_ltobs(lw_hdr->fcnt));

    le_uint32_t mic;
    le_uint32_t expected_mic;
    memcpy(&expected_mic, pkt->data, MIC_SIZE);

    gnrc_lorawan_calculate_mic(&lw_hdr->addr, fcnt, 1, data, nwkskey, &mic);

    if (expected_mic.u32 != mic.u32) {
        gnrc_pktbuf_release(pkt);
        pkt = NULL;
        goto end;
    }

    /* Remove MIC from packet buffer */
    pkt = gnrc_pktbuf_remove_snip(pkt, pkt);
    assert(data == pkt);

end:
    return pkt;
}

/* Assume pkt with NO MIC */
/* On error return NULL and free packet */
gnrc_pktsnip_t *gnrc_lorawan_mark_fhdr(gnrc_pktsnip_t *pkt, lorawan_hdr_t **lw_hdr, uint8_t **fopts)
{
    lorawan_buffer_t lw_buf;

    if(pkt->size < sizeof(lorawan_hdr_t)) {
        /* FAILURE */
        gnrc_pktbuf_release(pkt); 
        return NULL;
    }

    lorawan_hdr_t *_hdr = pkt->data;
    uint8_t fopts_length = lorawan_hdr_get_frame_opts_len(_hdr);
    size_t hdr_size = sizeof(lorawan_hdr_t) + fopts_length;

    gnrc_pktsnip_t *res = gnrc_pktbuf_mark(pkt, hdr_size, GNRC_NETTYPE_UNDEF);
    if(!res) {
        gnrc_pktbuf_release(pkt);
        return NULL;
    }

    _hdr = res->data;
    gnrc_lorawan_buffer_reset(&lw_buf, res->data, res->size);

    printf("%i %i %i\n", hdr_size, lw_buf.size, lw_buf.index);

    if(!(_hdr = (lorawan_hdr_t*) gnrc_lorawan_read_bytes(&lw_buf, sizeof(lorawan_hdr_t)))) {
        /* FAILURE */
        gnrc_pktbuf_release(pkt); 
        return NULL;
    }

    uint8_t *_fopts = NULL;

    if(fopts_length && !(_fopts = gnrc_lorawan_read_bytes(&lw_buf, fopts_length))) {
        gnrc_pktbuf_release(pkt); 
        return NULL;
    }

    *lw_hdr = _hdr;
    *fopts = _fopts;

    return pkt;
}

static gnrc_pktsnip_t *_handle_empty_payload(gnrc_netif_t *netif, lorawan_hdr_t *lw_hdr, gnrc_pktsnip_t *pkt, uint8_t* fopts)
{
    assert(pkt && pkt->data == NULL);
    if(fopts) {
        gnrc_lorawan_process_fopts(netif, fopts, lorawan_hdr_get_frame_opts_len(lw_hdr));
    }

    gnrc_pktbuf_release(pkt);
    return NULL;
}

static gnrc_pktsnip_t *_handle_payload(gnrc_netif_t *netif, lorawan_hdr_t *lw_hdr, gnrc_pktsnip_t *pkt, uint8_t* fopts)
{
    assert(pkt && pkt->data);

    if(pkt->size < 2) {
        gnrc_pktbuf_release(pkt);
        return NULL;
    }

    /* Decrypt pkt */
    uint8_t *port = ((uint8_t*) pkt->data);
    uint8_t *payload = port + 1;
    size_t payload_size = pkt->size-1;

    if(*port==0) {
        if(fopts) {
            gnrc_pktbuf_release(pkt);
            return NULL;
        }
        gnrc_lorawan_encrypt_payload(payload, payload_size, &lw_hdr->addr, byteorder_ntohs(byteorder_ltobs(lw_hdr->fcnt)), 1, netif->lorawan.nwkskey);
        gnrc_lorawan_process_fopts(netif, payload, payload_size);
        gnrc_pktbuf_release(pkt);
        return NULL;
    }
    else {
        gnrc_lorawan_encrypt_payload(payload, payload_size, &lw_hdr->addr, byteorder_ntohs(byteorder_ltobs(lw_hdr->fcnt)), 1, netif->lorawan.appskey);
        if(fopts) {
            gnrc_lorawan_process_fopts(netif, fopts, lorawan_hdr_get_frame_opts_len(lw_hdr));
        }
    }
    return pkt;
}

gnrc_pktsnip_t *gnrc_process_downlink(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    if(!(pkt = gnrc_lorawan_get_mac_payload(pkt, netif->lorawan.nwkskey))) {
        goto end;
    }

    uint8_t *fopts;
    lorawan_hdr_t *lw_hdr;
    
    if(!(pkt = gnrc_lorawan_mark_fhdr(pkt, &lw_hdr, &fopts))) {
        goto end;
    } 

    if(pkt->data) {
        pkt = _handle_payload(netif, lw_hdr, pkt, fopts);
    }
    else {
        pkt = _handle_empty_payload(netif, lw_hdr, pkt, fopts);
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
            pkt = gnrc_process_downlink(netif, pkt);
            if(pkt) {
                for(unsigned i=0;i<pkt->size;i++) {
                    printf("%02x ", ((uint8_t*) pkt->data)[i]);
                }
                printf("\n");
            }
            pkt->type = GNRC_NETTYPE_LORAWAN;
            break;
        default:
            gnrc_pktbuf_release(pkt);
            pkt = NULL;
            break;
    }
    return pkt;
}


size_t gnrc_lorawan_build_hdr(uint8_t mtype, le_uint32_t *dev_addr, uint32_t fcnt, uint8_t fctrl, uint8_t fopts_length, lorawan_buffer_t *buf)
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
