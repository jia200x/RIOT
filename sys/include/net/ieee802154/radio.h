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

typedef struct ieee802154_dev ieee802154_dev_t;

typedef void (*ieee802154_cb_t)(ieee802154_dev_t *dev, int status);

struct ieee802154_dev {
    ieee802154_radio_ops_t *driver;
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
    int (*get_tx_status)(ieee802154_dev_t *dev);
    int (*set_hw_addr_filter)(ieee802154_dev_t *dev, uint8_t *short_addr, uint8_t *ext_addr, uint16_t pan_id);
    int (*set_frame_retries)(ieee802154_dev_t *dev, int retries);
    int (*set_csma_params)(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd, int8_t retries);
    int (*set_promiscuous)(ieee802154_dev_t *dev, bool enable);

    int (*start)(ieee802154_dev_t *dev);
};

#endif /* IEEE802154_RADIO_H */