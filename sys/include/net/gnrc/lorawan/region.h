#ifndef NET_GNRC_LORAWAN_REGION_H
#define NET_GNRC_LORAWAN_REGION_H

#include "net/gnrc/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

void gnrc_lorawan_process_cflist(gnrc_netif_t *netif, uint8_t *cflist);
uint8_t gnrc_lorawan_rx1_get_dr_offset(uint8_t dr_up, uint8_t dr_offset);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LORAWAN_REGION_H */
