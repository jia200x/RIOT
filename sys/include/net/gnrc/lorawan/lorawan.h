/*
 * Copyright (C) 2017 Fundación Inria Chile
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup net_gnrc_lorawan
 * @{
 *
 * @file
 * @brief   GNRC LoRaWAN API definition
 *
 * @author  José Ignacio Alamos <jose.alamos@haw-hamburg.de>
 * @author  Francisco Molina <femolina@uc.cl>
 */
#ifndef NET_GNRC_LORAWAN_LORAWAN_H
#define NET_GNRC_LORAWAN_LORAWAN_H

#include "gnrc_lorawan_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief maximum timer drift in percentage
 *
 * E.g a value of 0.1 means there's a positive drift of 0.1% (set timeout to
 * 1000 ms => triggers after 1001 ms)
 */
#ifndef CONFIG_GNRC_LORAWAN_TIMER_DRIFT
#define CONFIG_GNRC_LORAWAN_TIMER_DRIFT 1
#endif

/**
 * @brief the minimum symbols to detect a LoRa preamble
 */
#ifndef CONFIG_GNRC_LORAWAN_MIN_SYMBOLS_TIMEOUT
#define CONFIG_GNRC_LORAWAN_MIN_SYMBOLS_TIMEOUT 50
#endif

/**
 * @brief enable or disable low power mode callbacks
 */
#ifndef CONFIG_GNRC_LORAWAN_ENABLE_LPM_STORAGE
#define CONFIG_GNRC_LORAWAN_ENABLE_LPM_STORAGE (0)
#endif


#define GNRC_LORAWAN_REQ_STATUS_SUCCESS (0)     /**< MLME or MCPS request successful status */
#define GNRC_LORAWAN_REQ_STATUS_DEFERRED (1)    /**< the MLME or MCPS confirm message is asynchronous */

typedef enum {
    MIB_ACTIVATION_METHOD
} mlme_mib_type_t;

typedef struct {
    mlme_mib_type_t type;
    union {
        int activation;
    } param;
} mlme_mib_t;

/**
 * @brief MAC (sub) Layer Management Entity (MLME) request representation
 */
typedef struct {
    union {
        mlme_lorawan_join_t join;
        mlme_mib_t mib;
    } param;        /**< parameters of the MLME request */
    uint8_t type;   /**< type of the MLME request */
} mlme_request_t;

/**
 * @brief Mac Common Part Sublayer (MCPS) request representation
 */
typedef struct {
    union {
        mcps_data_t data;
    } param;        /**< parameters of the MCPS request */
    uint8_t type;   /**< type of the MCPS request */
} mcps_request_t;

/**
 * @brief MAC (sub) Layer Management Entity (MLME) confirm representation
 */
typedef struct {
    int16_t status; /**< status of the MLME confirm */
    uint8_t type;   /**< type of the MLME confirm */
    union {
        mlme_link_req_confirm_t link_req;
        mlme_mib_t mib;
    } param; /**< parameters of the MLME confirm */
} mlme_confirm_t;

/**
 * @brief Mac Common Part Sublayer (MCPS) confirm representation
 */
typedef struct {
    void *data;     /**< data of the MCPS confirm */
    int16_t status; /**< status of the MCPS confirm */
    uint8_t type;   /**< type of the MCPS confirm */
} mcps_confirm_t;

/**
 * @brief Mac Common Part Sublayer (MCPS) indication representation
 */
typedef struct {
    uint8_t type; /**< type of the MCPS indication */
    union {
        mcps_data_t data;
    } param; /**< params of the MCPS indication */
} mcps_indication_t;

/**
 * @brief MAC (sub) Layer Management Entity (MLME) indication representation
 */
typedef struct {
    uint8_t type; /* type of the MLME indication */
} mlme_indication_t;

/**
 * @brief Indicate the MAC layer there was a timeout event
 *
 * @param[in] mac pointer to the MAC descriptor
 */
void gnrc_lorawan_event_timeout(gnrc_lorawan_t *mac);

/**
 * @brief Indicate the MAC layer when the transmission finished
 *
 * @param[in] mac pointer to the MAC descriptor
 */
void gnrc_lorawan_event_tx_complete(gnrc_lorawan_t *mac);

/**
 * @brief Init GNRC LoRaWAN
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] nwkskey buffer to store the NwkSKey. Should be at least 16 bytes long
 * @param[in] appskey buffer to store the AppsKey. Should be at least 16 bytes long
 */
void gnrc_lorawan_init(gnrc_lorawan_t *mac, uint8_t *nwkskey, uint8_t *appskey);

/**
 * @brief Perform a MLME request
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] mlme_request the MLME request
 * @param[out] mlme_confirm the MLME confirm. `mlme_confirm->status` could either
 *             be GNRC_LORAWAN_REQ_STATUS_SUCCESS if the request was OK,
 *             GNRC_LORAWAN_REQ_STATUS_DEFERRED if the confirmation is deferred
 *             or an standard error number
 */
void gnrc_lorawan_mlme_request(gnrc_lorawan_t *mac, mlme_request_t *mlme_request,
                               mlme_confirm_t *mlme_confirm);

/**
 * @brief Perform a MCPS request
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] mcps_request the MCPS request
 * @param[out] mcps_confirm the MCPS confirm. `mlme_confirm->status` could either
 *             be GNRC_LORAWAN_REQ_STATUS_SUCCESS if the request was OK,
 *             GNRC_LORAWAN_REQ_STATUS_DEFERRED if the confirmation is deferred
 *             or an standard error number
 */
void gnrc_lorawan_mcps_request(gnrc_lorawan_t *mac, mcps_request_t *mcps_request,
                               mcps_confirm_t *mcps_confirm);

/**
 * @brief Fetch a LoRaWAN packet from the radio.
 *
 *        To be called on radio RX done event.
 *
 * @param[in] mac pointer to the MAC descriptor
 */
void gnrc_lorawan_recv(gnrc_lorawan_t *mac);

/**
 * @brief Setup GNRC LoRaWAN netdev layers
 *
 * @param mac pointer to the MAC descriptor
 * @param lower pointer to the lower netdev device (radio)
 */
void gnrc_lorawan_setup(gnrc_lorawan_t *mac, netdev_t *lower);

#if CONFIG_GNRC_LORAWAN_ENABLE_LPM_STORAGE || DOXYGEN
/**
 * @brief Request a graceful MAC shutdown.
 *
 *        When shutdown procedure finishes, an asynchronous call to
 *        @reg gnrc_lorawan_save_cb will be called.
 *
 *        The MAC will be disabled after a successful shutdown.
 *
 * @note @ref CONFIG_GNRC_LORAWAN_ENABLE_LPM_STORAGE must be set in order
 * @note to use this feature.
 *
 * @param mac pointer to the MAC descriptor
 */
void gnrc_lorawan_shutdown(gnrc_lorawan_t *mac);

/**
 * @brief Application-defined callback for saving MAC state in a non-volatile
 *        storage.
 *
 *        It will be called if CONFIG_GNRC_LORAWAN_ENABLE_LPM_STORAGE is enabled.
 *        It's safe to go to low power mode inside this function
 *
 * @param buf Buffer containing internal MAC state to be saved.
 * @param len Number of bytes to be saved
 *
 */
int gnrc_lorawan_save_cb(uint8_t pos, uint8_t *buf, size_t len);

/**
 * @brief Application-defined callback for restoring MAC state from a
 *        non-volatile storage
 *
 * @param buf Buffer containing internal MAC state to be saved.
 * @param len Number of bytes to be saved
 *
 * @return number of bytes written in the non-volatile storage
 * @return negative number for ignoring restore data
 */
int gnrc_lorawan_restore_cb(uint8_t pos, uint8_t *buf, size_t len);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LORAWAN_LORAWAN_H */
/** @} */
