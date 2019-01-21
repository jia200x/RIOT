#ifndef NET_GNRC_LORAWAN_LORAWAN_H
#define NET_GNRC_LORAWAN_LORAWAN_H

#include <stdio.h>
#include <string.h>
#include "net/lora.h"
#include "net/gnrc/netif.h"
#include "net/lorawan/hdr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PKT_WRITE_BYTE(CURSOR, BYTE) *(CURSOR++) = BYTE

#define PKT_WRITE(CURSOR, SRC, LEN) do {\
    memcpy(CURSOR, SRC, LEN); \
    CURSOR += LEN; \
} while (0);

#define MSG_TYPE_TIMEOUT            (0x3457)

#define MTYPE_MASK           0xE0
#define MTYPE_JOIN_REQUEST   0x0
#define MTYPE_JOIN_ACCEPT    0x1
#define MTYPE_UNCNF_UPLINK   0x2
#define MTYPE_UNCNF_DOWNLINK 0x3
#define MTYPE_CNF_UPLINK     0x4
#define MTYPE_CNF_DOWNLINK   0x5
#define MTYPE_REJOIN_REQ     0x6
#define MTYPE_PROPIETARY     0x7

#define MAJOR_MASK     0x3
#define MAJOR_LRWAN_R1 0x0

#define DEV_ADDR_LEN 4
#define FCTRL_LEN 1
#define FCNT_LEN 2
#define FOPTS_MAX_LEN 16


#define ADR_MASK 0x80
#define ADR_ACK_REQ_MASK 0x40
#define ACK_MASK 0x20
#define FPENDING_MASK 0x10
#define FOPTS_MASK 0x0F


#define JOIN_REQUEST_SIZE 23
#define MIC_SIZE 4

#define LORAWAN_STATE_IDLE (0)
#define LORAWAN_STATE_RX_1 (1)
#define LORAWAN_STATE_RX_2 (2)
#define LORAWAN_STATE_TX (2)

typedef struct {
    uint8_t *data;
    uint8_t size;
    uint8_t index;
} lorawan_buffer_t;

uint32_t calculate_mic(uint8_t *buf, size_t size, uint8_t *appkey);
uint32_t calculate_pkt_mic(uint8_t dir, uint8_t *dev_addr, uint16_t fcnt, gnrc_pktsnip_t *pkt, uint8_t *nwkskey);
void gnrc_lorawan_encrypt_payload(uint8_t *payload, size_t size, le_uint32_t *dev_addr, uint16_t fcnt, uint8_t dir, uint8_t *appskey);
void decrypt_join_accept(uint8_t *key, uint8_t *pkt, int has_clist, uint8_t *out);
void generate_session_keys(uint8_t *app_nonce, uint8_t *dev_nonce, uint8_t *appkey, uint8_t *nwkskey, uint8_t *appskey);
void gnrc_lorawan_send_join_request(gnrc_netif_t *netif);
void gnrc_lorawan_join_abp(gnrc_netif_t *netif);
void gnrc_lorawan_open_rx_window(gnrc_netif_t *netif);
void gnrc_lorawan_event_timeout(gnrc_netif_t *netif);
gnrc_pktsnip_t *gnrc_lorawan_process_pkt(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt);
int gnrc_lorawan_set_dr(gnrc_netif_t *netif, uint8_t datarate);
gnrc_pktsnip_t *gnrc_lorawan_build_uplink(gnrc_netif_t *netif, gnrc_pktsnip_t *payload);
void gnrc_lorawan_event_tx_complete(gnrc_netif_t *netif);
uint32_t gnrc_lorawan_pick_channel(gnrc_netif_t *netif);
int gnrc_lorawan_set_pending_fopt(gnrc_netif_t *netif, uint8_t cid, uint8_t value);
int gnrc_lorawan_get_pending_fopt(gnrc_netif_t *netif, uint8_t cid);
uint8_t gnrc_lorawan_build_options(gnrc_netif_t *netif, lorawan_buffer_t *buf);
void gnrc_lorawan_process_fopts(gnrc_netif_t *netif, gnrc_pktsnip_t *fopts);
void gnrc_lorawan_calculate_mic(le_uint32_t *dev_addr, uint16_t fcnt,
        uint8_t dir, gnrc_pktsnip_t *pkt, uint8_t *nwkskey, le_uint32_t *out);
size_t gnrc_lorawan_build_hdr(uint8_t mtype, le_uint32_t *dev_addr, uint16_t fcnt, uint8_t fctrl, uint8_t fopts_length, lorawan_buffer_t *buf);
int gnrc_lorawan_fopts_mlme_link_check_req(gnrc_netif_t *netif, lorawan_buffer_t *buf);
int gnrc_lorawan_fopt_read_cid(lorawan_buffer_t *fopt, uint8_t *cid);
int gnrc_lorawan_perform_fopt(uint8_t cid, gnrc_netif_t *netif, lorawan_buffer_t *fopt);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LORAWAN_LORAWAN_H */
