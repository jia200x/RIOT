#ifndef IEEE802154_SUBMAC_H
#define IEEE802154_SUBMAC_H

#endif /* IEEE802154_SUBMAC_H */

#include "net/ieee802154/radio.h"

#define IEEE802154_SUBMAC_MAX_RETRANSMISSIONS (4)

typedef struct ieee802154_submac ieee802154_submac_t;

typedef struct {
    void (*rx_done)(ieee802154_submac_t *submac, uint8_t *buffer, size_t size);
    void (*tx_done)(ieee802154_submac_t *submac, int status, bool frame_pending, int retrans);
} ieee802154_submac_cb_t;

struct ieee802154_submac {
    eui64_t ext_addr;
    network_uint16_t short_addr;
    ieee802154_dev_t *dev;
    ieee802154_submac_cb_t *cb;
    void *ctx;
    bool wait_for_ack;
    uint16_t panid;
    uint8_t seq;
    uint8_t retrans;
};

extern ieee802154_submac_t submac;

static inline bool ieee802154_submac_expects_ack(ieee802154_submac_t *submac)
{
    return submac->wait_for_ack;
}

int ieee802154_send(ieee802154_submac_t *submac, iolist_t *iolist);
int ieee802154_set_addresses(ieee802154_submac_t *submac, network_uint16_t *short_addr,
        eui64_t *ext_addr, uint16_t panid);
static inline int ieee802154_set_short_addr(ieee802154_submac_t *submac, network_uint16_t *short_addr)
{
    return ieee802154_set_addresses(submac, short_addr, &submac->ext_addr, submac->panid);
}
static inline int ieee802154_set_ext_addr(ieee802154_submac_t *submac, eui64_t *ext_addr)
{
    return ieee802154_set_addresses(submac, &submac->short_addr, ext_addr, submac->panid);
}
static inline int ieee802154_set_panid(ieee802154_submac_t *submac, uint16_t panid)
{
    return ieee802154_set_addresses(submac, &submac->short_addr, &submac->ext_addr, panid);
}
int ieee802154_set_channel(ieee802154_submac_t *submac, uint8_t channel_num, uint8_t channel_page);
int ieee802154_submac_init(ieee802154_submac_t *submac);

/* To be implemented by the user */
void ieee802154_submac_ack_timer_cancel(ieee802154_submac_t *submac);
void ieee802154_submac_ack_timer_set(ieee802154_submac_t *submac, uint16_t us);

/* To be called by the user */
void ieee802154_submac_ack_timeout_fired(ieee802154_submac_t *submac);
void ieee802154_submac_rx_done_cb(ieee802154_submac_t *submac, struct iovec *iov);
void ieee802154_submac_tx_done_cb(ieee802154_submac_t *submac);
