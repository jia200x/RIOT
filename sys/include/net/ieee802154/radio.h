#ifndef IEEE802154_RADIO_H
#define IEEE802154_RADIO_H
#include <stdbool.h>
#include "iolist.h"
#include "sys/uio.h"
#include "byteorder.h"
#include "net/eui64.h"

typedef struct ieee802154_radio_ops ieee802154_radio_ops_t;
typedef enum {
    /**
     * @brief either the transceiver finished sending a packet without ACK
     *        request or received a valid ACK
     */
    IEEE802154_RF_EV_TX_DONE,
    /**
     * @brief the transceiver received a valid ACK with the frame pending bit
     */
    IEEE802154_RF_EV_TX_DONE_DATA_PENDING,
    /**
     * @brief the transceiver ran out of retransmission retries
     */
    IEEE802154_RF_EV_TX_NO_ACK,
    /**
     * @brief the CSMA-CA algorithm failed to measure a clear channel
     */
    IEEE802154_RF_EV_TX_MEDIUM_BUSY,
} ieee802154_tx_status_t;

typedef enum {
    /**
     * @brief the device supports hardware address filter
     */
    IEEE802154_CAP_HW_ADDR_FILTER,
    /**
     * @brief the device support frame retransmissions with CSMA-CA
     */
    IEEE802154_CAP_FRAME_RETRIES,
    /**
     * @brief the device support ACK timeout interrupt
     */
    IEEE802154_CAP_ACK_TIMEOUT,
    /**
     * @brief the device supports Auto ACK
     */
    IEEE802154_CAP_AUTO_ACK,
    /**
     * @brief the device performs CCA when sending
     */
    IEEE802154_CAP_24_GHZ,
    /**
     * @brief the device support the IEEE802.15.4 Sub GHz band
     */
    IEEE802154_CAP_SUB_GHZ,
} ieee802154_rf_caps_t;

typedef enum {
    IEEE802154_TRX_STATE_TRX_OFF, /**< the transceiver state if off */
    IEEE802154_TRX_STATE_RX_ON, /**< the transceiver is ready to receive packets */
    IEEE802154_TRX_STATE_TX_ON, /**< the transceiver is ready to send packets */
} ieee802154_trx_state_t;

typedef enum {
    IEEE802154_TX_MODE_DIRECT,      /**< direct transmissions */
    IEEE802154_TX_MODE_CCA,         /**< transmit using CCA */
    IEEE802154_TX_MODE_CSMA_CA,     /**< transmit using CSMA-CA */
} ieee802154_tx_mode_t;

typedef enum {
    /**
     * @brief the transceiver detected a valid SFD
     */
    IEEE802154_RADIO_RX_START,

    /**
     * @brief the transceiver received a packet and there's a packet in the
     *        internal framebuffer.
     *
     * The transceiver is in @ref IEEE802154_TRX_STATE_RX_ON state when
     * this funcion is called, but with framebuffer protection (either
     * dynamic framebuffer protection or disabled RX). Thus, the packet
     * won't be overwritten before calling the @ref ieee802154_radio_read
     * function. However, @ref ieee802154_radio_read MUST be called in
     * order to receive new packets. If there's no interest in the
     * packet, the function can be called with a NULL buffer to drop
     * the packet.
     */
    IEEE802154_RADIO_RX_DONE,

    /**
     * @brief the transceiver finished sending a packet or the
     *        retransmission procedure
     *
     * If the radio supports frame retransmissions the 
     * @ref ieee802154_radio_get_tx_status MAY be called to retrieve useful
     * information (number of retries, frame pending bit, etc). The
     * transceiver is in @ref IEEE802154_TRX_STATE_TX_ON state when this function
     * is called.
     */
    IEEE802154_RADIO_TX_DONE,
    /**
     * @brief the transceiver reports that the ACK timeout expired
     *
     * This event is present only if the radio support ACK timeout.
     */
    IEEE802154_RADIO_ACK_TIMEOUT,
    /**
     * @brief the transceiver received a packet but the CRC check failed
     */
    IEEE802154_RADIO_CRC_FAIL,
} ieee802154_trx_ev_t;

typedef struct {
    uint8_t min; /**< minimum value of the exponential backoff */
    uint8_t max; /**< maximum value of the exponential backoff */
} ieee802154_csma_be_t;

typedef struct {
    int16_t rssi; /**< RSSI of the received packet */
    uint8_t lqi;  /**< LQI of the received packet */
    bool crc_ok;  /**< whether the CRC in the packet is OK or not */
} ieee802154_rx_info_t;

typedef struct {
    int8_t retries; /**< number of frame retransmissions of the last TX */
    bool frame_pending; /**< the pending bit of the ACK frame */
} ieee802154_tx_info_t;

/**
 * @brief Forwared declaration of the IEEE802.15.4 device descriptor
 */
typedef struct ieee802154_dev ieee802154_dev_t;

/**
 * @brief Prototype of the IEEE802.15.4 device event callback
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] status the status 
 */
typedef void (*ieee802154_cb_t)(ieee802154_dev_t *dev,
                                ieee802154_tx_status_t status);

/**
 * @brief the IEEE802.15.4 device descriptor
 */
struct ieee802154_dev {
    /**
     * @brief pointer to the operations of the device
     */
    const ieee802154_radio_ops_t *driver;
    /**
     * @brief the event callback of the device
     */
    ieee802154_cb_t cb;
};

typedef enum {
    IEEE802154_CCA_MODE_ED_THRESHOLD,     /**< CCA mode 1 */
    IEEE802154_CCA_MODE_CARRER_SENSING,   /**< CCA mode 2 */
    IEEE802154_CCA_MODE_ED_THRESH_AND_CS, /**< CCA mode 3 (AND) */
    IEEE802154_CCA_MODE_ED_THRESH_OR_CS,  /**< CCA mode 3 (OR) */
} ieee802154_cca_mode_t;

/**
 * @brief Holder of the PHY configuration
 */
typedef struct {
    uint8_t channel; /**< IEEE802.15.4 channel number */
    uint8_t page;    /**< IEEE802.15.4 channel page */
    int16_t pow;     /**< TX power in dBm */
} ieee802154_phy_conf_t;

struct ieee802154_radio_ops {
    /**
     * @brief Load packet in the framebuffer of a radio.
     *
     * This function shouldn't do any checks, so the packet MUST be valid.
     * If the radio is still transmitting, it should block until is safe to
     * write again in the frame buffer
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
     * @param[in] mode transmissions mode
     *
     * @return 0 on success
     * @return -ENOTSUP if a transmission mode is not supported
     * @return -EBUSY if the medium is busy
     * @return negative errno on error
     */
    int (*transmit)(ieee802154_dev_t *dev, ieee802154_tx_mode_t mode);

    /**
     * @brief Get the lenght of the received packet.
     *
     * This function can use SRAM, a reg value or similar to read the packet
     * length.
     *
     * @pre the radio already received a packet (e.g
     *      @ref ieee802154_dev_t::cb with @ref IEEE802154_RADIO_RX_DONE).
     * @pre the device is on
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
     * @pre the device is on
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
     * @return number of bytes written in @p buffer (0 if @p buf == NULL)
     * @return -ENOBUFS if the packet doesn't fit in @p
     */
    int (*read)(ieee802154_dev_t *dev, void *buf, size_t size,
                ieee802154_rx_info_t *info);

    /**
     * @brief Perform Stand-Alone Clear Channel Assessment
     *
     * This function performs blocking CCA to check if the channel is clear.
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
     * @pre the device is on
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
     * @pre the device is on
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
     * @pre the device is on
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
     * @pre the device is on
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
     * @brief Turn on the device
     *
     * @pre the init function of the radio succeeded.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @post the transceiver state ins @ref IEEE802154_TRX_STATE_TRX_OFF
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*on)(ieee802154_dev_t *dev);

    /**
     * @brief Turn off the device
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @post the transceiver state is @ref IEEE802154_TRX_STATE_TRX_OFF
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*off)(ieee802154_dev_t *dev);

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
     * @note if the device is off, this function should do nothing
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     */
    void (*irq_handler)(ieee802154_dev_t *dev);

    /**
     * @brief Set IEEE802.15.4 promiscuous mode
     *
     * @pre the device is on
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
     * @pre the device is on
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
     * @pre the device is on
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
     * @pre the device is on
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
     * @pre the device is on
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       frame retransmissions
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] bd parameters of the exponential backoff
     * @param[in] retries number of CSMA-CA retries. If @p retries < 0, 
     *                    retransmissions with CSMA-CA MUST be disabled.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_csma_params)(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd,
                           int8_t retries);
};


/**
 * @brief Load a packet in the internal framebuffer of the device.
 *
 * @pre the device is on
 *
 * This function assumes @p pkt is valid and doesn't exceed the maximum PHY
 * length. Also, it should block until is safe to write a packet.
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] pkt the packet to be loaded
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_prepare(ieee802154_dev_t *dev, iolist_t *pkt)
{
    return dev->driver->prepare(dev, pkt);
}


/**
 * @brief Transmit a preloaded packet in the framebuffer.
 *
 * @pre the PHY state is @ref IEEE802154_TRX_STATE_TX_ON.
 * @pre the device is on
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_transmit(ieee802154_dev_t *dev, ieee802154_tx_mode_t mode)
{
    return dev->driver->transmit(dev, mode);
}

/**
 * @brief Read the length of the received packet
 *
 * This function can be used to allocate buffer space for a packet.
 * It doesn't release the framebuffer and MUST be called
 * before the @ref ieee802154_radio_read function, otherwise the behavior is
 * undefined.
 *
 * @pre the device is on
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_len(ieee802154_dev_t *dev)
{
    return dev->driver->len(dev);
}

/**
 * @brief Read a packet into a buffer of given size
 * 
 * @pre the radio already received a packet (e.g
 *      @ref ieee802154_dev_t::cb with @ref IEEE802154_RADIO_RX_DONE).
 * @pre the device is on
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[out] buf buffer to write the received packet into. If NULL, the
 *             packet is not copied.
 * @param[in] size size of @p buf
 * @param[in] info information of the received packet (LQI, RSSI). Can be
 *            NULL if this information is not needed.
 *
 * @post the PHY state is @ref IEEE802154_TRX_STATE_RX_ON
 * @post the transceiver can receive more packets (raise frame buffer protection)
 *
 * @return number of bytes written in @p buffer
 * @return 0 if @p buf == NULL
 * @return -ENOBUFS if the packet doesn't fit in @p buffer
 */
static inline int ieee802154_radio_read(ieee802154_dev_t *dev, void *buf,
                                        size_t size, ieee802154_rx_info_t *info)
{
    return dev->driver->read(dev, buf, size, info);
}

/**
 * @brief Perform CCA to check if the channel is clear
 *
 * @pre the device is on
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if channel is clear.
 * @return false if channel is busy.
 */
static inline bool ieee802154_radio_cca(ieee802154_dev_t *dev)
{
    return dev->driver->cca(dev);
}

/**
 * @brief Set the threshold for the CCA (first mode)
 *
 * @pre the device is on
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] threshold the threshold in dBm
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_cca_threshold(ieee802154_dev_t *dev,
                                                     int8_t threshold)
{
    return dev->driver->set_cca_threshold(dev, threshold);
}

/**
 * @brief Set CCA mode
 *
 * @note It's guaranteed that CCA with ED Threshold is supported in all radios.
 *
 * @pre the device is on
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] mode the CCA mode
 *
 * @return 0 on success
 * @return -ENOTSUP if the CCA mode is not supported
 * @return negative errno on error
 *
 */
static inline int ieee802154_radio_set_cca_mode(ieee802154_dev_t *dev,
                                                ieee802154_cca_mode_t mode)
{
    return dev->driver->set_cca_mode(dev, mode);
}

/**
 * @brief Set IEEE802.15.4 PHY configuration (channel, TX power)
 *
 * @note This functio DOES NOT validate the PHY configurations unless
 * it's specific to the device. The upper layer is responsible of all kind
 * of validations.
 *
 * @pre the device is on
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] conf the PHY configuration
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_config_phy(ieee802154_dev_t *dev,
                                              ieee802154_phy_conf_t *conf)
{
    return dev->driver->config_phy(dev, conf);
}

/**
 * @brief Sets the transceiver PHY state.
 *
 * This function blocks until the state change occurs.
 *
 * @pre the device is on
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] state the transceiver state to change to
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_trx_state(ieee802154_dev_t *dev,
                                                 ieee802154_trx_state_t state)
{
    return dev->driver->set_trx_state(dev, state);
}

/**
 * @brief Turn off the transceiver
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @post the transceiver state is @ref IEEE802154_TRX_STATE_TRX_OFF
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_off(ieee802154_dev_t *dev)
{
    return dev->driver->off(dev);
}

/**
 * @brief Process the transceiver IRQ
 *
 * @note It's safe to call this function when the transceiver is off. In
 *       that case it does nothing.
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 */
static inline void ieee802154_radio_irq_handler(ieee802154_dev_t *dev)
{
    dev->driver->irq_handler(dev);
}

/**
 * @brief Get the SubMAC TX information
 *
 * @pre the device is on
 * @pre the device finished the TX procedure
 * @pre the device supports frame retransmissions
 *      (@ref ieee802154_radio_has_frame_retries() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] info the TX info
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_get_tx_status(ieee802154_dev_t *dev,
                                                 ieee802154_tx_info_t *info)
{
    return dev->driver->get_tx_status(dev, info);
}

/**
 * @brief Write IEEE802.15.4 addresses into the hardware address filter
 *
 * @pre the device is on
 * @pre the device supports address filtering
 *      (@ref ieee802154_radio_has_addr_filter() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] short_addr IEEE802.15.4 short address
 * @param[in] ext_addr IEEE802.15.4 extended address
 * @param[in] pan_id IEEE802.15.4 PAN ID
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_hw_addr_filter(ieee802154_dev_t *dev,
                                                      uint8_t *short_addr,
                                                      uint8_t *ext_addr,
                                                      uint16_t pan_id)
{
    return dev->driver->set_hw_addr_filter(dev, short_addr, ext_addr, pan_id);
}

/**
 * @brief Set the number of retransmissions
 *
 * @pre the device is on
 * @pre the device supports frame retransmissions
 *      (@ref ieee802154_radio_has_frame_retries() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] retries the number of retransmissions
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_frame_retries(ieee802154_dev_t *dev, int retries)
{
    return dev->driver->set_frame_retries(dev, retries);
}

/**
 * @brief Set the CSMA-CA parameters
 *
 * @pre the device is on
 * @pre the device supports frame retransmissions
 *      (@ref ieee802154_radio_has_frame_retries() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] bd parameters of the exponential backoff
 * @param[in] retries number of CSMA-CA retries. If @p restries < 0,
 *                    retransmissions with CSMA-CA are disabled
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_csma_params(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd, int8_t retries)
{
    return dev->driver->set_csma_params(dev, bd, retries);
}

/**
 * @brief Set IEEE802.15.4 promiscuous mode
 *
 * @pre the device is on
 * @pre the device supports address filtering
 *      (@ref ieee802154_radio_has_addr_filter() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] enable whether the promiscuous mode should be enabled or not
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_promiscuous(ieee802154_dev_t *dev, bool enable)
{
    return dev->driver->set_promiscuous(dev, enable);
}

/**
 * @brief Turn on the device
 *
 * @pre the device driver init function was already called
 *
 * @post the transceiver state is @ref IEEE802154_TRX_STATE_TRX_OFF
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_on(ieee802154_dev_t *dev)
{
    return dev->driver->on(dev);
}

/**
 * @brief Check if the device supports ACK timeout
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_ack_timeout(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_ACK_TIMEOUT);
}

/**
 * @brief Check if the device supports frame retransmissions
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_frame_retries(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_FRAME_RETRIES);
}

/**
 * @brief Check if the device supports address filtering
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_addr_filter(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_HW_ADDR_FILTER);
}

/**
 * @brief Check if the device supports Auto ACK
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_auto_ack(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_AUTO_ACK);
}

/**
 * @brief Check if the device supports the IEEE802.15.4 Sub-GHz band
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_sub_ghz(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_SUB_GHZ);
}

/**
 * @brief Check if the device supports the IEEE802.15.4 2.4 GHz band
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_24_ghz(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_24_GHZ);
}

#endif /* IEEE802154_RADIO_H */
