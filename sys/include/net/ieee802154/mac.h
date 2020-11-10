#ifndef NET_IEEE802154_MAC_H
#define NET_IEEE802154_MAC_H

#include <string.h>
#include <assert.h>

#include "net/ieee802154.h"
#include "net/ieee802154/submac.h"
#include "net/ieee802154/radio.h"
#include "event.h"
#include "ztimer.h"

#define MAC_STATE_IDLE (0x0)
#define MAC_STATE_SCANNING (0x1)
#define MAC_STATE_SEND_BEACON (0x2)
#define MAC_STATE_ASSOCIATE (0x3)
#define MAC_STATE_SEND_ASSOCIATE_CMD (0x4)
#define MAC_STATE_POLL (0x5)
#define MAC_STATE_POLL_ASSOCIATE (0x6)

#define SUPERFRAME_BASE_US (60 * 60 * 16U)
#define MAC_RESPONSE_WAIT_TIME (32U * SUPERFRAME_BASE_US)

typedef enum {
    MCPS_DATA,
    MLME_BEACON_NOTIFY,
    MLME_SCAN,
    MLME_ASSOCIATE,
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

typedef enum {
    IEEE802154_SCAN_ACTIVE,
    IEEE802154_SCAN_PASSIVE,
    IEEE802154_SCAN_ED,
    IEEE802154_SCAN_ORPHAN,
} ieee802154_scan_t;

typedef struct {
    network_uint16_t short_addr;
    eui64_t ext_addr;
} ieee802154_child_t;

typedef struct {
    uint8_t src_mode;
    ieee802154_child_t *child;
    iolist_t *msdu;
    uint8_t flags;
    uint8_t dsn;
} ieee802154_mcps_it_t;

typedef struct {
    uint8_t addr[8];
    le_uint16_t panid;
    uint16_t channel;
    uint8_t addr_len;
    uint8_t page;
    uint8_t lqi;
} ieee802154_pan_t;

typedef struct {
    uint8_t bsn;
    ieee802154_pan_t pan;
    uint8_t *sdu;
    uint8_t sdu_len;
} ieee802154_beacon_ind_t;

typedef void ieee802154_sec_t;

typedef struct {
    uint32_t chan_bm;
    uint8_t curr_scan;
    uint8_t duration;
    uint8_t num_channels;
    bool found;
} ieee802154_scan_ctx_t;

typedef struct {
    int8_t status;
    uint8_t result_list;
} ieee802154_scan_cnf_t;

typedef struct {
    uint8_t addr[8];
    uint8_t addr_len;
    uint8_t caps;
} ieee802154_associate_ind_t;

struct ieee802154_mac {
    ieee802154_submac_t submac;
    ieee802154_scan_ctx_t scan;
    ztimer_t timer;
    event_queue_t *evq;
    event_t ev;
    const ieee802154_sap_cb_t *cb;
    iolist_t *tx_msdu;
    int state;
    uint8_t dsn;
    uint8_t bsn;
    bool coord;
    uint8_t coord_addr[8];
    uint8_t coord_addr_len;
    uint8_t queue[127];
    uint8_t pending_psdu_size;
};

iolist_t *ieee802154_get_framebuffer(ieee802154_mac_t *mac);

int ieee802154_mac_init(ieee802154_mac_t *mac, eui64_t *eui, ieee802154_dev_t *dev, event_queue_t *evq);

int ieee802154_mac_data_req(ieee802154_mac_t *mac, int src_mode, ieee802154_ep_t *dst, iolist_t *msdu, uint8_t tx_ops,
                   ieee802154_sec_t *sec);

int ieee802154_mac_mlme_start(ieee802154_mac_t *mac, le_uint16_t panid, uint8_t channel_num, uint8_t channel_page, bool pan_coord);
void ieee802154_mac_scan_timeout(ieee802154_mac_t *mac);
int ieee802154_mac_mlme_scan(ieee802154_mac_t *mac, ieee802154_scan_t scan_type,
                             uint32_t chan_bm, uint8_t duration, int page, ieee802154_sec_t *sec);
void ieee802154_mac_scan_tx_done(ieee802154_mac_t *mac);
void ieee802154_mac_send_beacon(ieee802154_mac_t *mac);
int ieee802154_mac_mlme_associate(ieee802154_mac_t *mac, uint16_t channel_num, uint8_t channel_page,
                                  void *coord_addr, size_t addr_len, le_uint16_t coord_panid,
                                  uint8_t caps, ieee802154_sec_t *sec);

#ifdef __cplusplus
}
#endif

#endif /* NET_IEEE802154_MAC_H */
