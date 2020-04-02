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
    bool crc_ok;
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

typedef enum {
    IEEE802154_CCA_MODE_ED_THRESHOLD,
    IEEE802154_CCA_MODE_CARRER_SENSING,
    IEEE802154_CCA_MODE_ED_THRESH_AND_CS,
    IEEE802154_CCA_MODE_ED_THRESH_OR_CS,
} ieee802154_cca_mode_t;

typedef struct {
    uint8_t channel;
    uint8_t page;
    int16_t pow;
} ieee802154_phy_conf_t;

struct ieee802154_radio_ops {
    /**
     * @brief Load packet in the framebuffer of a radio.
     *
     * This function shouldn't do any checks, so the packet MUST be valid.
     * If the radio is still transmitting, it should block until is safe to
     * write again in the frame buffer
     *
     * @pre the PHY state is @ref IEEE802154_TRX_STATE_TX_ON.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] pkt the packet to be sent with valid length
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*prepare)(ieee802154_dev_t *dev, iolist_t *pkt);

    /**
     * @brief Transmit a preloaded packet
     *
     * @pre the PHY state is @ref IEEE802154_TRX_STATE_TX_ON and the packet
     *      is already in the framebuffer.
     *
     * @post the PHY state is @ref IEEE802154_TRX_STATE_TX_ON.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*transmit)(ieee802154_dev_t *dev);

    /**
     * @brief Get the lenght of the received packet.
     *
     * This function can use SRAM, a reg value or similar to read the packet
     * length.
     *
     * @pre the radio already received a packet (e.g
     *      @ref ieee802154_dev_t::cb with @ref IEEE802154_RADIO_RX_DONE).
     * @pre the device is not sleeping
     *
     * @post the frame buffer is still protected against new packet arrivals.
     *      
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*len)(ieee802154_dev_t *dev);

    /**
     * @brief Read a packet from the internal framebuffer of the radio.
     *
     * This function should try to write the received packet into @p buf and
     * put the radio in a state where it can receive more packets.
     *
     * @pre the radio already received a packet (e.g
     *      @ref ieee802154_dev_t::cb with @ref IEEE802154_RADIO_RX_DONE).
     * @pre the device is not sleeping
     *
     * @post the PHY state is @ref IEEE802154_TRX_STATE_RX_ON and the radio is
     *       in a state where it can receive more packets, regardless of the
     *       return value.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[out] buf buffer to write the received packet into. If NULL, the
     *             packet is not copied.
     * @param[in] size size of @p buf
     * @param[in] info information of the received packet (LQI, RSSI). Can be
     *            NULL if this information is not needed.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*read)(ieee802154_dev_t *dev, void *buf, size_t size,
                ieee802154_rx_info_t *info);

    /**
     * @brief Perform Stand-Alone Clear Channel Assessment
     *
     * This function performs blocking CCA to check if the channel is clear.
     * @pre the PHY state is @ref IEEE802154_TRX_STATE_RX_ON.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @return true if channel is clear.
     * @return false if channel is busy.
     */
    bool (*cca)(ieee802154_dev_t *dev);

    /**
     * @brief Set the threshold for the Energy Detection (first mode of CCA)
     *
     * @pre the device is not sleeping
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] threshold the threshold in dBm.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_cca_threshold)(ieee802154_dev_t *dev, int8_t threshold);

    /**
     * @brief Set CCA mode
     *
     * All radios MUST at least implement the first CCA mode (ED Threshold).
     *
     * @pre the device is not sleeping
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] mode the CCA mode
     *
     * @return 0 on success
     * @return -ENOTSUP if the mode is not supported
     * @return negative errno on error
     */
    int (*set_cca_mode)(ieee802154_dev_t *dev, ieee802154_cca_mode_t mode);

    /**
     * @brief Set IEEE802.15.4 PHY configuration (channel, TX power)
     *
     * This function SHOULD NOT validate the PHY configurations unless
     * it's specific to the device. The upper layer is responsible of all kind
     * of validations.
     *
     * @pre the device is not sleeping
     * @pre the PHY state is @ref IEEE802154_TRX_STATE_TRX_OFF.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] conf the PHY configuration
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*config_phy)(ieee802154_dev_t *dev, ieee802154_phy_conf_t *conf);

    /**
     * @brief Set the transceiver PHY state
     *
     * @pre the device is not sleeping
     *
     * @note the implementation MUST block until the state change occurs.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] state the new state
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_trx_state)(ieee802154_dev_t *dev, ieee802154_trx_state_t state);

    /**
     * @brief Set the sleep state of the device (sleep or awake)
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] sleep whether the device should sleep or not.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_sleep)(ieee802154_dev_t *dev, bool sleep);

    /**
     * @brief Get a cap from the radio
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param cap cap to be checked
     *
     * @return true if the radio supports the cap
     * @return false otherwise
     */
    bool (*get_cap)(ieee802154_dev_t *dev, ieee802154_rf_caps_t cap);

    /**
     * @brief Process radio IRQ.
     *
     * This function calls the @ref ieee802154_cb_t::cb function with all
     * the corresponding events.
     *
     * @note if the device is sleeping, this function should do nothing
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     */
    void (*irq_handler)(ieee802154_dev_t *dev);

    /**
     * @brief Start the device
     *
     * @pre the init function of the radio succeeded.
     *
     * This function puts the radio in a state where it can be operated. It
     * should enable interrupts and set the transceiver state to
     * @ref IEEE802154_TRX_STATE_TRX_OFF
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*start)(ieee802154_dev_t *dev);

    /**
     * @brief Set IEEE802.15.4 promiscuous mode
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       hardware address filtering.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] enable whether the promiscuous mode should be enabled or not.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_promiscuous)(ieee802154_dev_t *dev, bool enable);

    /**
     * @brief Get the SubMAC TX information (number of retransmissions,
     *        pending bit, status, etc).
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       frame retransmissions
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[out] info the TX information
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*get_tx_status)(ieee802154_dev_t *dev, ieee802154_tx_info_t *info);

    /**
     * @brief Set IEEE802.15.4 addresses in hardware address filter
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       hardware address filtering.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] short_addr the IEEE802.15.4 short address
     * @param[in] ext_addr the IEEE802.15.4 extended address
     * @param[in] pan_id the IEEE802.15.4 PAN ID
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_hw_addr_filter)(ieee802154_dev_t *dev, uint8_t *short_addr,
                              uint8_t *ext_addr, uint16_t pan_id);

    /**
     * @brief Set number of frame retransmissions
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       frame retransmissions
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] retries the number of retransmissions
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_frame_retries)(ieee802154_dev_t *dev, int retries);

    /**
     * @brief 
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       frame retransmissions
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] bd parameters of the exponential backoff
     * @param[in] retries number of CSMA-CA retries
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_csma_params)(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd,
                           int8_t retries);
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

static inline int ieee802154_radio_config_phy(ieee802154_dev_t *dev, ieee802154_phy_conf_t *conf)
{
    return dev->driver->config_phy(dev, conf);
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
