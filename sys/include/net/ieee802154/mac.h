#ifndef NET_IEEE802154_MAC_H
#define NET_IEEE802154_MAC_H

#include <string.h>
#include <assert.h>

#include "net/ieee802154.h"
#include "net/ieee802154/submac.h"
#include "net/ieee802154/radio.h"

#define TASKLET_MCPS_DATA_CNF (0x1)
typedef enum {
    MCPS_DATA,
} sap_type_t;

typedef struct ieee802154_mac ieee802154_mac_t;

typedef struct {
    void (*confirm)(ieee802154_mac_t *mac, sap_type_t type, void *data);
    void (*indication)(ieee802154_mac_t *mac, sap_type_t type, void *data);
} ieee802154_sap_cb_t;

typedef struct {
   int status; 
   iolist_t *msdu;
} ieee802154_data_cnf_t;

typedef struct {
    uint8_t addr[8];
    uint8_t addr_len;
    le_uint16_t panid;
} ieee802154_ep_t;

/* TODO: Add timestamp and sec */
typedef struct {
    ieee802154_ep_t src;
    ieee802154_ep_t dst;
    void *msdu;
    uint8_t msdu_len;
    uint8_t lqi;
    uint8_t dsn;
} ieee802154_data_ind_t;

typedef void ieee802154_sec_t;

struct ieee802154_mac {
    ieee802154_submac_t submac;
    const ieee802154_sap_cb_t *cb;
    iolist_t *tx_msdu;
    uint8_t dsn;
};

iolist_t *ieee802154_get_framebuffer(ieee802154_mac_t *mac);


int ieee802154_mac_init(ieee802154_mac_t *mac, eui64_t *eui, ieee802154_dev_t *dev);

int ieee802154_mac_data_req(ieee802154_mac_t *mac, int src_mode, ieee802154_ep_t *dst, iolist_t *msdu, uint8_t tx_ops,
                   ieee802154_sec_t *sec);

#ifdef __cplusplus
}
#endif

#endif /* NET_IEEE802154_MAC_H */
