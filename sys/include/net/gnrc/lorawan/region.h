#ifndef NET_GNRC_LORAWAN_REGION_H
#define NET_GNRC_LORAWAN_REGION_H

#include "net/gnrc/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GNRC_LORAWAN_LC_1 (868100000)
#define GNRC_LORAWAN_LC_2 (868300000)
#define GNRC_LORAWAN_LC_3 (868500000)

#define GNRC_LORAWAN_DEFAULT_CHANNELS 3

void gnrc_lorawan_process_cflist(gnrc_netif_t *netif, uint8_t *cflist);
uint8_t gnrc_lorawan_rx1_get_dr_offset(uint8_t dr_up, uint8_t dr_offset);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LORAWAN_REGION_H */
