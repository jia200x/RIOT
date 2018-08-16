#ifndef NET_GNRC_LORAWAN_LORAWAN_H
#define NET_GNRC_LORAWAN_LORAWAN_H

#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t calculate_mic(uint8_t *buf, size_t size, uint8_t *appkey);
uint32_t calculate_pkt_mic(uint8_t dir, uint8_t *dev_addr, uint16_t fcnt, uint8_t* msg, size_t size, uint8_t *nwkskey);
void encrypt_payload(uint8_t *payload, size_t size, uint8_t *dev_addr, uint16_t fcnt, uint8_t dir, uint8_t *appskey, uint8_t *out);
void decrypt_join_accept(uint8_t *key, uint8_t *pkt, int has_clist, uint8_t *out);
void generate_session_keys(uint8_t *app_nonce, uint8_t *dev_nonce, uint8_t *appkey, uint8_t *nwkskey, uint8_t *appskey);
void gnrc_lorawan_send_join_request(gnrc_netif_t *netif);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LORAWAN_LORAWAN_H */
