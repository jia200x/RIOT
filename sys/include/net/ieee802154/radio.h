#ifndef IEEE802154_RADIO_H
#define IEEE802154_RADIO_H
#include <stdbool.h>
#include "iolist.h"

typedef struct ieee802154_radio_ops ieee802154_radio_ops_t;
typedef enum {
    IEEE802154_RF_EV_TX_DONE,
    IEEE802154_RF_EV_TX_DONE_DATA_PENDING,
    IEEE802154_RF_EV_TX_NO_ACK,
    IEEE802154_RF_EV_TX_MEDIUM_BUSY,
} ieee802154_tx_status_t;

#define IEEE802154_RF_FLAG_RX_DONE (0x1)
#define IEEE802154_RF_FLAG_RX_START (0x2)
#define IEEE802154_RF_FLAG_TX_DONE (0x4)

typedef enum {
    IEEE802154_FLAG_SLEEP,
    IEEE802154_FLAG_RX_CONTINUOUS,
    IEEE802154_FLAG_HAS_HW_ADDR_FILTER,
    IEEE802154_FLAG_HAS_FRAME_RETRIES,
    IEEE802154_FLAG_HAS_CSMA_BACKOFF,
    IEEE802154_FLAG_HAS_ACK_TIMEOUT,
    IEEE802154_FLAG_HAS_AUTO_ACK,
} ieee802154_rf_flags_t;

typedef enum {
    IEEE802154_TRX_STATE_TRX_OFF,
    IEEE802154_TRX_STATE_RX_ON,
    IEEE802154_TRX_STATE_TX_ON,
} ieee802154_trx_state_t;

typedef struct {
    uint8_t min;
    uint8_t max;
} ieee802154_csma_be_t;

typedef struct {
    int16_t rssi;
    uint8_t lqi;
} ieee802154_rx_info_t;

typedef struct ieee802154_dev ieee802154_dev_t;

struct ieee802154_dev {
    uint8_t flags;
    ieee802154_radio_ops_t *driver;
};

struct ieee802154_radio_ops {
    int (*prepare)(ieee802154_dev_t *dev, iolist_t *pkt);
    int (*transmit)(ieee802154_dev_t *dev);
    int (*read)(ieee802154_dev_t *dev, void *buf, size_t max_size, ieee802154_rx_info_t *info);
    bool (*cca)(ieee802154_dev_t *dev);
    int (*set_cca_threshold)(ieee802154_dev_t *dev, int8_t threshold);
    //int set_cca_mode(ieee802154_dev_t *dev, ieee802154_cca_mode_t mode);
    int (*set_channel)(ieee802154_dev_t *dev, uint8_t channel, uint8_t page);
    int (*set_tx_power)(ieee802154_dev_t *dev, int16_t pow);
    int (*set_trx_state)(ieee802154_dev_t *dev, ieee802154_trx_state_t state);
    int (*set_flag)(ieee802154_dev_t *dev, ieee802154_rf_flags_t flag, bool value);
    bool (*get_flag)(ieee802154_dev_t *dev, ieee802154_rf_flags_t flag);
    uint8_t (*poll_events)(ieee802154_dev_t *dev);
    int (*get_tx_status)(ieee802154_dev_t *dev);
    int (*set_hw_addr_filter)(ieee802154_dev_t *dev, uint8_t *short_addr, uint8_t *ext_addr, uint16_t pan_id);
    int (*set_frame_retries)(ieee802154_dev_t *dev, int retries);
    int (*set_csma_params)(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd, int8_t retries);
    int (*set_promiscuous)(ieee802154_dev_t *dev, bool enable);

    //int (*start)(ieee802154_dev_t *dev);
    //int (*stop)(ieee802154_dev_t *dev);
};
#endif /* IEEE802154_RADIO_H */
