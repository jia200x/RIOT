#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include "net/gnrc.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#include "net/netdev/lora.h"
#include "net/lora.h"

#include "net/ieee802154/radio.h"

#include "event/thread.h"
#include "ztimer.h"

#include "thread.h"
#include "periph/timer.h"

#include "kernel_defines.h"
#include "event.h"

const uint8_t llcc68_max_sf = LORA_SF11;
const uint8_t sx126x_max_sf = LORA_SF12;

#include "sx126x.h"
#include "sx126x_params.h"
#include "sx126x_internal.h"

#include "checksum/ucrc16.h"

static const ieee802154_radio_ops_t sx126x_ops;
static ieee802154_dev_t *_sx126x_hal_dev;

static int _set_state(sx126x_t *dev, sx126x_state_t state);
static int _get_state(sx126x_t *dev, void* val);


void sx126x_hal_setup(sx126x_t *dev, ieee802154_dev_t *hal)
{
    hal->driver = &sx126x_ops;
    hal->priv = dev;

    _sx126x_hal_dev = hal;
}

#if IS_USED(MODULE_SX126X_STM32WL)

void _sx126x_handler(event_t *event)
{
    (void) event;
    ieee802154_dev_t *hal = _sx126x_hal_dev;
    sx126x_hal_task_handler(hal);
}

event_t sx126x_ev = {.handler = _sx126x_handler};

void sx126x_hal_task_handler(ieee802154_dev_t *hal);
void isr_subghz_radio(void)
{
    /* Disable NVIC to avoid ISR conflict in CPU. */
    ieee802154_dev_t *hal = _sx126x_hal_dev;
    //sx126x_t *dev = hal->priv;
    NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);
    NVIC_ClearPendingIRQ(SUBGHZ_Radio_IRQn);
    //event_post(dev->evq, &sx126x_ev);
    sx126x_hal_task_handler(hal);
    cortexm_isr_end();
}
#endif 

void sx126x_setup(sx126x_t *dev, uint8_t index)
{
    (void)dev;
    (void)index;
}

static int _set_state(sx126x_t *dev, sx126x_state_t state)
{
    switch (state) {
    case STATE_IDLE:
        DEBUG("[sx126x] netdev: set STATE_STANDBY\n");
        sx126x_set_standby(dev, SX126X_CHIP_MODE_STBY_XOSC);
        break;

    case STATE_RX:
        DEBUG("[sx126x] netdev: set STATE_RX\n");
#if IS_USED(MODULE_SX126X_RF_SWITCH)
        /* Refer Section 4.2 RF Switch in Application Note (AN5406) */
        if (dev->params->set_rf_mode) {
            dev->params->set_rf_mode(dev, SX126X_RF_MODE_RX);
        }
#endif
        sx126x_cfg_rx_boosted(dev, true);
        sx126x_set_rx(dev, SX126X_RX_SINGLE_MODE);
        break;

    case STATE_TX:
        DEBUG("[sx126x] netdev: set STATE_TX\n");
#if IS_USED(MODULE_SX126X_RF_SWITCH)
        if (dev->params->set_rf_mode) {
            dev->params->set_rf_mode(dev, dev->params->tx_pa_mode);
        }
#endif
        sx126x_set_tx(dev, 0);
        break;

    default:
        return -ENOTSUP;
    }
    dev->state = state;
    return sizeof(sx126x_state_t);
}

static int _get_state(sx126x_t *dev, void *val)
{
    sx126x_chip_status_t radio_status;

    sx126x_get_status(dev, &radio_status);
    sx126x_state_t state = STATE_IDLE;

    switch (radio_status.chip_mode) {
    case SX126X_CHIP_MODE_RFU:
    case SX126X_CHIP_MODE_STBY_RC:
    case SX126X_CHIP_MODE_STBY_XOSC:
        state = STATE_IDLE;
        break;

    case SX126X_CHIP_MODE_TX:
        state = STATE_TX;
        break;

    case SX126X_CHIP_MODE_RX:
        state = STATE_RX;
        break;

    default:
        break;
    }
    dev->state = state;
    memcpy(val, &state, sizeof(sx126x_state_t));
    return sizeof(sx126x_state_t);
}

void sx126x_hal_task_handler(ieee802154_dev_t *hal)
{
    sx126x_t *dev = hal->priv;
    assert(dev);
    uint8_t val;
    (void)_get_state(dev, &val);
    sx126x_irq_mask_t irq_mask;

    sx126x_get_and_clear_irq_status(dev, &irq_mask);

    if (sx126x_is_stm32wl(dev)) {
        if (irq_mask & SX126X_IRQ_TX_DONE) {
            DEBUG("[sx126x] netdev: SX126X_IRQ_TX_DONE\n");
            hal->cb(hal, IEEE802154_RADIO_CONFIRM_TX_DONE);
        }
        if (irq_mask & SX126X_IRQ_RX_DONE) {
            DEBUG("[sx126x] netdev: SX126X_IRQ_RX_DONE\n");
        
            hal->cb(hal, IEEE802154_RADIO_INDICATION_RX_DONE);
        }
        if (irq_mask & SX126X_IRQ_PREAMBLE_DETECTED) {
            DEBUG("[sx126x] netdev: SX126X_IRQ_PREAMBLE_DETECTED\n");
        }
        if (irq_mask & SX126X_IRQ_SYNC_WORD_VALID) {
            DEBUG("[sx126x] netdev: SX126X_IRQ_SYNC_WORD_VALID\n");
            
        }
        if (irq_mask & SX126X_IRQ_HEADER_VALID) {
            DEBUG("[sx126x] netdev: SX126X_IRQ_HEADER_VALID\n");
            hal->cb(hal, IEEE802154_RADIO_INDICATION_RX_START);
        }
        if (irq_mask & SX126X_IRQ_HEADER_ERROR) {
            DEBUG("[sx126x] netdev: SX126X_IRQ_HEADER_ERROR\n");
        }
        if (irq_mask & SX126X_IRQ_CRC_ERROR) {
            DEBUG("[sx126x] netdev: SX126X_IRQ_CRC_ERROR\n");
            hal->cb(hal, IEEE802154_RADIO_INDICATION_CRC_ERROR);
        }
        if (irq_mask & SX126X_IRQ_CAD_DONE) {
            if (irq_mask & SX126X_IRQ_CAD_DETECTED){
                DEBUG("[sx126x] netdev: SX126X_IRQ_CAD_DETECTED \n");
                dev->cad_detected = true;
            }
            DEBUG("[sx126x] netdev: SX126X_IRQ_CAD_DONE\n");
            hal->cb(hal, IEEE802154_RADIO_CONFIRM_CCA);
            
        }
        if (irq_mask & SX126X_IRQ_TIMEOUT) {
            DEBUG("[sx126x] netdev: SX126X_IRQ_TIMEOUT\n");
        }

        if (!irq_mask) {
            DEBUG("[sx126x] netdev: SX126X_IRQ_NONE\n");
        }

        #if IS_USED(MODULE_SX126X_STM32WL)
            NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
        #endif  
    }
}

                        /*RADIO HAL FUNCTIONS*/

static int _write(ieee802154_dev_t *hal, const iolist_t *iolist){


    sx126x_t *dev = hal->priv;
    (void)dev;
    size_t pos = 0;
    /* Full buffer used for Tx */
    sx126x_set_buffer_base_address(dev, 0x80, 0x00);
    /* Write payload buffer */
    for (const iolist_t *iol = iolist; iol; iol = iol->iol_next) {
        if (iol->iol_len > 0) {
            sx126x_write_buffer(dev, pos + 0x80, iol->iol_base, iol->iol_len);
            DEBUG("[sx126x] netdev: send: wrote data to payload buffer.\n");
            pos += iol->iol_len;
        }
    }

    sx126x_set_lora_payload_length(dev, pos);
    DEBUG("[sx126x] writing (size: %d).\n", (pos));
    return 0;
}

static int _request_op(ieee802154_dev_t *hal, ieee802154_hal_op_t op, void *ctx){
    sx126x_t *dev = hal->priv;
    (void)dev;
    (void)ctx;
    DEBUG("[sx126x] request_op\n");
    switch (op) {
        case IEEE802154_HAL_OP_TRANSMIT:
        _set_state(dev, STATE_TX);

        break;
        case IEEE802154_HAL_OP_SET_RX:

        _set_state(dev, STATE_RX);

        break;

        case IEEE802154_HAL_OP_SET_IDLE:

        _set_state(dev, STATE_IDLE);

        break;

        case IEEE802154_HAL_OP_CCA:
            DEBUG("[sx126x] netdev: HAL_OP_CCA (CAD Detection state)\n");
            dev->cad_detected = 0;
            _set_state(dev, STATE_IDLE);
            sx126x_set_cad(dev);

        break;

        default:
        assert(false);
        break;

    }
    return 0;
}

static int _confirm_op(ieee802154_dev_t *hal, ieee802154_hal_op_t op, void *ctx){
    DEBUG("[sx126x] confirm_op\n");
    sx126x_t *dev = hal->priv;
    (void)op;
    (void)ctx;
    ieee802154_tx_info_t *info = ctx;
    bool eagain = false;
    sx126x_state_t state;
    _get_state(dev, &state);

    
switch (op){
    case IEEE802154_HAL_OP_TRANSMIT:
       if (info) {
            info->status = TX_STATUS_SUCCESS;
        }
        while(state == STATE_TX)
            _get_state(dev, &state);
        
        DEBUG("[sx126x] confirm_op: TRANSMIT\n");
        break;

    case IEEE802154_HAL_OP_SET_RX:
        if(state!=STATE_RX) 
            eagain = true;
        DEBUG("[sx126x] confirm_op: RX\n");
    break;

    case IEEE802154_HAL_OP_SET_IDLE:
        eagain = (state != STATE_IDLE);
        DEBUG("[sx126x] confirm_op: IDLE\n");
        break;

    case IEEE802154_HAL_OP_CCA:
            *((bool*) ctx) = !dev->cad_detected;
        DEBUG("[sx126x] confirm_op: CCA\n");
    break;

    default:
        eagain = false;
        assert(false);
        break;
    
}

    if (eagain) {
        DEBUG("EAGAIN");
        return -EAGAIN;
    }

    return 0;
}

static int _len(ieee802154_dev_t *hal){
    (void)hal;
    DEBUG("[sx126x] _len\n");
    sx126x_t *dev = hal->priv;
    sx126x_rx_buffer_status_t rx_buffer_status;
    sx126x_get_rx_buffer_status(dev, &rx_buffer_status);
    dev->size = rx_buffer_status.pld_len_in_bytes;
    return dev->size;
}

static int _read(ieee802154_dev_t *hal, void *buf, size_t max_size, ieee802154_rx_info_t *info)
{
    (void)hal;
    (void)buf;
    (void)info;

    DEBUG("[sx126x] _read\n");
    sx126x_t* dev = hal->priv;

        /* Getting information about last received packet */
    netdev_lora_rx_info_t *packet_info = (void*)info;
    sx126x_rx_buffer_status_t rx_buffer_status;
    sx126x_pkt_status_lora_t pkt_status;
    sx126x_get_rx_buffer_status(dev, &rx_buffer_status);

    dev->size = rx_buffer_status.pld_len_in_bytes;
    sx126x_get_lora_pkt_status(dev, &pkt_status);
    if (packet_info) {
        packet_info->snr = pkt_status.snr_pkt_in_db;
        packet_info->rssi = pkt_status.rssi_pkt_in_dbm;
    }

      /* Put PSDU to the output buffer */
    
    if (buf == NULL) {
        return dev->size;
    }

    if (dev->size > max_size) {
        return -ENOBUFS;
    }

    if (dev->size < 3) {
        return -EBADMSG;
    }
    sx126x_read_buffer(dev, rx_buffer_status.buffer_start_pointer, (uint8_t*)buf, dev->size);
    DEBUG("[sx126x] first 3 bytes of received packet: %d %d %d\n", *(uint8_t*)buf, *((uint8_t*)buf+1), *((uint8_t*)buf+2));
    return dev->size;
}

static int _set_cca_threshold(ieee802154_dev_t *hal, int8_t threshold)
{
    (void)hal;
    (void)threshold;

    return 0;
}

static int _config_phy(ieee802154_dev_t *hal, const ieee802154_phy_conf_t *conf){
    (void)hal;
    (void)conf;
    DEBUG("[sx126x] _config_phy\n");
    sx126x_t *dev = hal->priv;
    uint8_t channel = conf->channel;
    int8_t pow = conf->pow;
    assert(channel >= 11 && channel <= 26);

    if (channel == 26) {
        sx126x_set_channel(dev, 869525000LU);
    }
    else {
        sx126x_set_channel(dev, (channel-11)*200000LU + 865500000LU);
    }

    assert(pow >= -14 && pow <= 22);

    DEBUG("FREQ = %ld, POWER = %d \n", (channel-11)*200000LU + 865500000LU, pow);
    sx126x_set_tx_params(dev, pow, SX126X_RAMP_10_US);
    return 0;
}

static int _off(ieee802154_dev_t *hal)
{
    (void)hal;
    DEBUG("[sx126x] _off\n");
    sx126x_t *dev = hal->priv;
    (void) dev;
    //sx126x_set_sleep(dev, SX126X_SLEEP_CFG_COLD_START);
    return 0;
}

static int _config_addr_filter(ieee802154_dev_t *hal, ieee802154_af_cmd_t cmd, const void *value)
{
    (void)hal;
    (void)cmd;
    (void)value;
    return 0;
}

static int _request_on(ieee802154_dev_t *hal)
{
    (void)hal;
    sx126x_t *dev = hal->priv;
    DEBUG("[sx126x] _request_on\n");
    _set_state(dev, STATE_IDLE);
    return 0;
}

static int _confirm_on(ieee802154_dev_t *hal)
{
    (void)hal;
    DEBUG("[sx126x] _confirm_on\n");

    return 0;
}

static int _set_cca_mode(ieee802154_dev_t *hal, ieee802154_cca_mode_t mode)
{
    (void)hal;
    sx126x_t* dev = hal->priv;
    DEBUG("[sx126x] netdev: set_cca_mode \n");
    dev->cad_params.cad_exit_mode = SX126X_CAD_ONLY,
    dev->cad_params.cad_detect_min = 10,
    dev->cad_params.cad_detect_peak = 22,
    dev->cad_params.cad_symb_nb = SX126X_CAD_02_SYMB,
    dev->cad_params.cad_timeout = 0x000001;
    sx126x_set_cad_params(dev, &dev->cad_params);
    (void)mode;

    return 0;
}

static int _config_src_addr_match(ieee802154_dev_t *hal, ieee802154_src_match_t cmd, const void *value)
{
    (void)hal;
    (void)cmd;
    (void)value;

    return -ENOTSUP;
}

static int _set_frame_filter_mode(ieee802154_dev_t *hal, ieee802154_filter_mode_t mode)
{
    (void) hal;
    (void) mode;
    return 0;
}

static int _set_csma_params(ieee802154_dev_t *hal, const ieee802154_csma_be_t *bd, int8_t retries)
{
    (void)hal;
    (void)bd;
    (void)retries;

    return -ENOTSUP;
}


static const ieee802154_radio_ops_t sx126x_ops = {
    .caps = IEEE802154_CAP_IRQ_CRC_ERROR
          | IEEE802154_CAP_IRQ_RX_START
          | IEEE802154_CAP_IRQ_TX_DONE
          | IEEE802154_CAP_IRQ_CCA_DONE,
    .write = _write,
    .read = _read,
    .request_on = _request_on,
    .confirm_on = _confirm_on,
    .len = _len,
    .off = _off,
    .request_op = _request_op,
    .confirm_op = _confirm_op,
    .set_cca_threshold = _set_cca_threshold,
    .set_cca_mode = _set_cca_mode,
    .config_phy = _config_phy,
    .set_csma_params = _set_csma_params,
    .config_addr_filter = _config_addr_filter,
    .config_src_addr_match = _config_src_addr_match,
    .set_frame_filter_mode = _set_frame_filter_mode,
};
