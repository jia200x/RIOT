#ifndef NET_GNRC_LORAWAN_LORAWAN_H
#define NET_GNRC_LORAWAN_LORAWAN_H

#include <stdio.h>
#include <string.h>
#include "net/lora.h"
#include "net/gnrc/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PKT_WRITE_BYTE(CURSOR, BYTE) *(CURSOR++) = BYTE

#define PKT_WRITE(CURSOR, SRC, LEN) do {\
    memcpy(CURSOR, SRC, LEN); \
    CURSOR += LEN; \
} while (0);

#define MSG_TYPE_TIMEOUT            (0x3457)

#define MTYPE_MASK           0b11100000
#define MTYPE_JOIN_REQUEST   0b000
#define MTYPE_JOIN_ACCEPT    0b001
#define MTYPE_UNCNF_UPLINK   0b010
#define MTYPE_UNCNF_DOWNLINK 0b011
#define MTYPE_CNF_UPLINK     0b100
#define MTYPE_CNF_DOWNLINK   0b101
#define MTYPE_REJOIN_REQ     0b110
#define MTYPE_PROPIETARY     0b111

#define MAJOR_MASK     0b11
#define MAJOR_LRWAN_R1 0b00

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


uint32_t calculate_mic(uint8_t *buf, size_t size, uint8_t *appkey);
uint32_t calculate_pkt_mic(uint8_t dir, uint8_t *dev_addr, uint16_t fcnt, uint8_t* msg, size_t size, uint8_t *nwkskey);
void encrypt_payload(uint8_t *payload, size_t size, uint8_t *dev_addr, uint16_t fcnt, uint8_t dir, uint8_t *appskey, uint8_t *out);
void decrypt_join_accept(uint8_t *key, uint8_t *pkt, int has_clist, uint8_t *out);
void generate_session_keys(uint8_t *app_nonce, uint8_t *dev_nonce, uint8_t *appkey, uint8_t *nwkskey, uint8_t *appskey);
void gnrc_lorawan_send_join_request(gnrc_netif_t *netif);
void gnrc_lorawan_open_rx_window(gnrc_netif_t *netif);
void gnrc_lorawan_process_pkt(gnrc_netif_t *netif, uint8_t *pkt, size_t size);
int gnrc_lorawan_set_dr(gnrc_netif_t *netif, uint8_t datarate);
gnrc_pktsnip_t *gnrc_lorawan_build_uplink(gnrc_netif_t *netif, gnrc_pktsnip_t *payload);
void gnrc_lorawan_event_tx_complete(gnrc_netif_t *netif);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LORAWAN_LORAWAN_H */
