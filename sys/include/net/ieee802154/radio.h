#ifndef IEEE802154_RADIO_H
#define IEEE802154_RADIO_H
#include <stdbool.h>
#include "iolist.h"
#include "sys/uio.h"
#include "byteorder.h"
#include "net/eui64.h"

typedef struct ieee802154_radio_ops ieee802154_radio_ops_t;
typedef enum {
    IEEE802154_RF_EV_TX_DONE,
    IEEE802154_RF_EV_TX_DONE_DATA_PENDING,
    IEEE802154_RF_EV_TX_NO_ACK,
    IEEE802154_RF_EV_TX_MEDIUM_BUSY,
} ieee802154_tx_status_t;

typedef enum {
    IEEE802154_CAP_HW_ADDR_FILTER,
    IEEE802154_CAP_FRAME_RETRIES,
    IEEE802154_CAP_CSMA_BACKOFF,
    IEEE802154_CAP_ACK_TIMEOUT,
    IEEE802154_CAP_AUTO_ACK,
} ieee802154_rf_caps_t;

typedef enum {
    IEEE802154_TRX_STATE_TRX_OFF,
    IEEE802154_TRX_STATE_RX_ON,
    IEEE802154_TRX_STATE_TX_ON,
} ieee802154_trx_state_t;

typedef enum {
    IEEE802154_RADIO_RX_START,
    IEEE802154_RADIO_RX_DONE,
    IEEE802154_RADIO_TX_DONE,
} ieee802154_trx_ev_t;

typedef struct {
    uint8_t min;
    uint8_t max;
} ieee802154_csma_be_t;

typedef struct {
    int16_t rssi;
    uint8_t lqi;
} ieee802154_rx_info_t;

typedef struct {
    int8_t retries;
    bool frame_pending;
} ieee802154_tx_info_t;

typedef struct ieee802154_dev ieee802154_dev_t;

typedef void (*ieee802154_cb_t)(ieee802154_dev_t *dev, int status);

struct ieee802154_dev {
    const ieee802154_radio_ops_t *driver;
    ieee802154_cb_t cb;
};

struct ieee802154_radio_ops {
    int (*prepare)(ieee802154_dev_t *dev, iolist_t *pkt);
    int (*transmit)(ieee802154_dev_t *dev);
    int (*read)(ieee802154_dev_t *dev, void *buf, size_t size, ieee802154_rx_info_t *info);
    bool (*cca)(ieee802154_dev_t *dev);
    int (*set_cca_threshold)(ieee802154_dev_t *dev, int8_t threshold);
    //int set_cca_mode(ieee802154_dev_t *dev, ieee802154_cca_mode_t mode);
    int (*set_channel)(ieee802154_dev_t *dev, uint8_t channel, uint8_t page);
    int (*set_tx_power)(ieee802154_dev_t *dev, int16_t pow);
    int (*set_trx_state)(ieee802154_dev_t *dev, ieee802154_trx_state_t state);
    int (*set_sleep)(ieee802154_dev_t *dev, bool sleep);
    bool (*get_cap)(ieee802154_dev_t *dev, ieee802154_rf_caps_t cap);
    void (*irq_handler)(ieee802154_dev_t *dev);
    int (*get_tx_status)(ieee802154_dev_t *dev, ieee802154_tx_info_t *info);
    int (*set_hw_addr_filter)(ieee802154_dev_t *dev, uint8_t *short_addr, uint8_t *ext_addr, uint16_t pan_id);
    int (*set_frame_retries)(ieee802154_dev_t *dev, int retries);
    int (*set_csma_params)(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd, int8_t retries);
    int (*set_promiscuous)(ieee802154_dev_t *dev, bool enable);

    int (*start)(ieee802154_dev_t *dev);
};

static inline int ieee802154_radio_prepare(ieee802154_dev_t *dev, iolist_t *pkt)
{
    return dev->driver->prepare(dev, pkt);
}

static inline int ieee802154_radio_transmit(ieee802154_dev_t *dev)
{
    return dev->driver->transmit(dev);
}

static inline int ieee802154_radio_read(ieee802154_dev_t *dev, void *buf, size_t size, ieee802154_rx_info_t *info)
{
    return dev->driver->read(dev, buf, size, info);
}

static inline bool ieee802154_radio_cca(ieee802154_dev_t *dev)
{
    return dev->driver->cca(dev);
}

static inline int ieee802154_radio_set_cca_threshold(ieee802154_dev_t *dev, int8_t threshold)
{
    return dev->driver->set_cca_threshold(dev, threshold);
}

static inline int ieee802154_radio_set_channel(ieee802154_dev_t *dev, uint8_t channel, uint8_t page)
{
    return dev->driver->set_channel(dev, channel, page);
}

static inline int ieee802154_radio_set_tx_power(ieee802154_dev_t *dev, int16_t pow)
{
    return dev->driver->set_tx_power(dev, pow);
}

static inline int ieee802154_radio_set_trx_state(ieee802154_dev_t *dev, ieee802154_trx_state_t state)
{
    return dev->driver->set_trx_state(dev, state);
}

static inline int ieee802154_radio_set_sleep(ieee802154_dev_t *dev, bool sleep)
{
    return dev->driver->set_sleep(dev, sleep);
}

static inline bool ieee802154_radio_get_cap(ieee802154_dev_t *dev, ieee802154_rf_caps_t cap)
{
    return dev->driver->get_cap(dev, cap);
}

static inline void ieee802154_radio_irq_handler(ieee802154_dev_t *dev)
{
    dev->driver->irq_handler(dev);
}

static inline int ieee802154_radio_get_tx_status(ieee802154_dev_t *dev, ieee802154_tx_info_t *info)
{
    return dev->driver->get_tx_status(dev, info);
}

static inline int ieee802154_radio_set_hw_addr_filter(ieee802154_dev_t *dev, uint8_t *short_addr, uint8_t *ext_addr, uint16_t pan_id)
{
    return dev->driver->set_hw_addr_filter(dev, short_addr, ext_addr, pan_id);
}

static inline int ieee802154_radio_set_frame_retries(ieee802154_dev_t *dev, int retries)
{
    return dev->driver->set_frame_retries(dev, retries);
}

static inline int ieee802154_radio_set_csma_params(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd, int8_t retries)
{
    return dev->driver->set_csma_params(dev, bd, retries);
}

static inline int ieee802154_radio_set_promiscuous(ieee802154_dev_t *dev, bool enable)
{
    return dev->driver->set_promiscuous(dev, enable);
}

static inline int ieee802154_radio_start(ieee802154_dev_t *dev)
{
    return dev->driver->start(dev);
}

static inline bool ieee802154_radio_has_ack_timeout(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_AUTO_ACK);
}

static inline bool ieee802154_radio_has_frame_retries(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_FRAME_RETRIES);
}

static inline bool ieee802154_radio_has_addr_filter(ieee802154_dev_t *dev)
{
    return dev->driver->set_hw_addr_filter != NULL;
}

#endif /* IEEE802154_RADIO_H */
