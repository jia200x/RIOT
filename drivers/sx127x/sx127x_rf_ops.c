#include "checksum/ucrc16.h"

#include "sx127x.h"
#include "sx127x_internal.h"
#include "sx127x_registers.h"

#include "board.h"

#define ENABLE_DEBUG 0
#include "debug.h"

static const ieee802154_radio_ops_t sx127x_ops;

static int _write(ieee802154_dev_t *hal, const iolist_t *iolist)
{
    uint8_t size = iolist_size(iolist);
    uint16_t chksum = 0;
    sx127x_t *dev = hal->priv;
    /* Include CRC */
    sx127x_set_payload_length(dev, size+2);

    /* Full buffer used for Tx */
    sx127x_reg_write(dev, SX127X_REG_LR_FIFOTXBASEADDR, 0x00);
    sx127x_reg_write(dev, SX127X_REG_LR_FIFOADDRPTR, 0x00);
    /* Write payload buffer */
    for (const iolist_t *iol = iolist; iol; iol = iol->iol_next) {
        if (iol->iol_len > 0) {
            sx127x_write_fifo(dev, iol->iol_base, iol->iol_len);
            DEBUG("[sx127x] Wrote to payload buffer.\n");
            chksum = ucrc16_calc_le(iol->iol_base, iol->iol_len,
                                UCRC16_CCITT_POLY_LE, chksum);
        }
    }
    chksum = byteorder_htols(chksum).u16;
    sx127x_write_fifo(dev, (uint8_t*) &chksum, sizeof(chksum));

    return 0;
}

static int _request_op(ieee802154_dev_t *hal, ieee802154_hal_op_t op, void *ctx)
{
    (void) ctx;
    sx127x_t *dev = hal->priv;
    switch (op) {
    case IEEE802154_HAL_OP_TRANSMIT:
        sx127x_reg_write(dev, SX127X_REG_LR_IRQFLAGSMASK,
                         SX127X_RF_LORA_IRQFLAGS_RXTIMEOUT |
                         SX127X_RF_LORA_IRQFLAGS_RXDONE |
                         SX127X_RF_LORA_IRQFLAGS_PAYLOADCRCERROR |
                         SX127X_RF_LORA_IRQFLAGS_VALIDHEADER |
                         /* SX127X_RF_LORA_IRQFLAGS_TXDONE | */
                         SX127X_RF_LORA_IRQFLAGS_CADDONE |
                         SX127X_RF_LORA_IRQFLAGS_FHSSCHANGEDCHANNEL |
                         SX127X_RF_LORA_IRQFLAGS_CADDETECTED);

        /* Set TXDONE interrupt to the DIO0 line */
        sx127x_reg_write(dev, SX127X_REG_DIOMAPPING1,
                         (sx127x_reg_read(dev, SX127X_REG_DIOMAPPING1) &
                          SX127X_RF_LORA_DIOMAPPING1_DIO0_MASK) |
                         SX127X_RF_LORA_DIOMAPPING1_DIO0_01);

        sx127x_set_state(dev, SX127X_RF_TX_RUNNING);
        /* Put chip into transfer mode */
        sx127x_set_op_mode(dev, SX127X_RF_OPMODE_TRANSMITTER );
        break;
    case IEEE802154_HAL_OP_SET_RX:
        sx127x_set_rx_timeout(dev, 0);
        sx127x_set_rx(dev);
        break;
    case IEEE802154_HAL_OP_SET_IDLE:
        sx127x_set_standby(dev);
        break;
    case IEEE802154_HAL_OP_CCA:
        sx127x_set_standby(dev);
        sx127x_start_cad(dev);
        break;
    default:
        assert(false);
        break;
    }

    return 0;
}

/* TODO: Implement properly */
static int _confirm_op(ieee802154_dev_t *hal, ieee802154_hal_op_t op, void *ctx)
{
    sx127x_t *dev = hal->priv;
    switch (op) {
        case IEEE802154_HAL_OP_CCA:
            *((bool*) ctx) = !dev->cad_detected;
            break;
        default:
            break;
    }
    return 0;
}

static int _read(ieee802154_dev_t *hal, void *buf, size_t max_size,
                          ieee802154_rx_info_t *info)
{

    (void) info;
    sx127x_t *dev = hal->priv;
    uint8_t size = 0;
    uint16_t chksum = 0;
    uint16_t exp_chksum;

    /* TODO */
    if (info) {
        info->lqi = 255;
        info->rssi = 100;
    }

    /* Size including CRC */
    size = sx127x_reg_read(dev, SX127X_REG_LR_RXNBBYTES);

    /* Exclude CRC */
    if (size > max_size + 2) {
        sx127x_reg_write(dev, SX127X_REG_LR_FIFORXBASEADDR, 0);
        sx127x_reg_write(dev, SX127X_REG_LR_FIFOADDRPTR, 0);
        return -ENOBUFS;
    }

    if (size < 5) {
        sx127x_reg_write(dev, SX127X_REG_LR_FIFORXBASEADDR, 0);
        sx127x_reg_write(dev, SX127X_REG_LR_FIFOADDRPTR, 0);
        return -EBADMSG;
    }

    if (buf == NULL) {
        sx127x_reg_write(dev, SX127X_REG_LR_FIFORXBASEADDR, 0);
        sx127x_reg_write(dev, SX127X_REG_LR_FIFOADDRPTR, 0);
        return size-2;
    }

    /* Read the last packet from FIFO */
    uint8_t last_rx_addr = sx127x_reg_read(dev, SX127X_REG_LR_FIFORXCURRENTADDR);
    sx127x_reg_write(dev, SX127X_REG_LR_FIFOADDRPTR, last_rx_addr);
    sx127x_read_fifo(dev, (uint8_t *)buf, size-2);
    sx127x_read_fifo(dev, (uint8_t *) &exp_chksum, sizeof(exp_chksum));

    chksum = ucrc16_calc_le(buf, size-2,
                        UCRC16_CCITT_POLY_LE, chksum);
    chksum = byteorder_htols(chksum).u16;

    /* Validate checksum */
    if (chksum != exp_chksum) {
        return -EBADMSG;
    }

    return size-2;
}

static int _request_on(ieee802154_dev_t *hal)
{
    sx127x_t *dev = hal->priv;
    gpio_set(RADIO_TCXO_VCC_PIN);
    sx127x_set_standby(dev);
    return 0;
}

static int _confirm_on(ieee802154_dev_t *dev)
{
    (void) dev;
    return 0;
}

static int _config_phy(ieee802154_dev_t *hal, const ieee802154_phy_conf_t *conf)
{
    sx127x_t *dev = hal->priv;
    uint8_t channel = conf->channel;
    assert(channel >= 11 && channel <= 26);

    if (channel == 26) {
        sx127x_set_channel(dev, 869525000LU);
    }
    else {
        sx127x_set_channel(dev, (channel-11)*200000LU + 865500000LU);
    }
    return 0;
}

static int _off(ieee802154_dev_t *hal)
{
    sx127x_t *dev = hal->priv;
    sx127x_set_sleep(dev);
    gpio_clear(RADIO_TCXO_VCC_PIN);
    return 0;
}

int _len(ieee802154_dev_t *hal)
{
    /* Exclude CRC */
    return sx127x_reg_read(hal->priv, SX127X_REG_LR_RXNBBYTES) - 2;
}

int _set_cca_mode(ieee802154_dev_t *dev, ieee802154_cca_mode_t mode)
{
    (void) dev;
    (void) mode;
    return 0;
}

static int _config_addr_filter(ieee802154_dev_t *dev, ieee802154_af_cmd_t cmd, const void *value)
{
    /* Not needed for now */
    (void) dev;
    (void) cmd;
    (void) value;
    return 0;
}

static int _config_src_addr_match(ieee802154_dev_t *dev, ieee802154_src_match_t cmd, const void *value)
{
    /* TODO */
    (void) dev;
    (void) cmd;
    (void) value;
    return 0;
}

static int _set_csma_params(ieee802154_dev_t *dev, const ieee802154_csma_be_t *bd,
                            int8_t retries)
{
    /* TODO */
    (void) dev;
    (void) bd;
    (void) retries;
    /* Ignore for now */
    return 0;
}

static int _set_frame_filter_mode(ieee802154_dev_t *dev, ieee802154_filter_mode_t mode)
{
    /* can be ignored for now */
    (void) dev;
    (void) mode;
    return 0;
}

int sx127x_setup(sx127x_t *dev, const sx127x_params_t *params, ieee802154_dev_t *hal)
{
    dev->irq = 0;
    dev->params = *params;
    sx127x_radio_settings_t settings;

    settings.channel = SX127X_CHANNEL_DEFAULT;
    settings.modem = SX127X_MODEM_DEFAULT;
    settings.state = SX127X_RF_IDLE;

    dev->settings = settings;

    /* Launch initialization of driver and device */
    DEBUG("[sx127x] netdev: initializing driver...\n");
    if (sx127x_init(dev, hal) != SX127X_INIT_OK) {
        DEBUG("[sx127x] netdev: initialization failed\n");
        return -1;
    }

    sx127x_init_radio_settings(dev);
    sx127x_set_syncword(dev, 0x17);
    /* Put chip into sleep */
    sx127x_set_sleep(dev);

    DEBUG("[sx127x] netdev: initialization done\n");

    return 0;
}

void sx127x_hal_setup(sx127x_t *dev, ieee802154_dev_t *hal)
{
    hal->driver = &sx127x_ops;
    hal->priv = dev;
}

void _on_dio0_irq(ieee802154_dev_t *hal, volatile uint8_t interruptReg)
{
    sx127x_t *dev = hal->priv;

    switch (dev->settings.state) {
    case SX127X_RF_RX_RUNNING:

        if ((interruptReg & SX127X_RF_LORA_IRQFLAGS_PAYLOADCRCERROR_MASK) ==
            SX127X_RF_LORA_IRQFLAGS_PAYLOADCRCERROR) {
            sx127x_reg_write(dev, SX127X_REG_LR_IRQFLAGS,
                             SX127X_RF_LORA_IRQFLAGS_PAYLOADCRCERROR);
            //sx127x_set_state(dev, SX127X_RF_IDLE);
            hal->cb(hal, IEEE802154_RADIO_INDICATION_CRC_ERROR);
        }
        else {
            sx127x_reg_write(dev, SX127X_REG_LR_IRQFLAGS, SX127X_RF_LORA_IRQFLAGS_RXDONE);
            hal->cb(hal, IEEE802154_RADIO_INDICATION_RX_DONE);
        }
        break;
    case SX127X_RF_TX_RUNNING:
        /* Clear IRQ */
        sx127x_reg_write(dev, SX127X_REG_LR_IRQFLAGS,
                         SX127X_RF_LORA_IRQFLAGS_TXDONE);
        sx127x_set_state(dev, SX127X_RF_IDLE);

        hal->cb(hal, IEEE802154_RADIO_CONFIRM_TX_DONE);
        break;
    case SX127X_RF_IDLE:
        DEBUG("[sx127x] netdev: sx127x_on_dio0: IDLE state\n");
        break;
    default:
        DEBUG("[sx127x] netdev: sx127x_on_dio0: unknown state [%d]\n",
              dev->settings.state);
        break;
    }
}

void sx127x_hal_task_handler(ieee802154_dev_t *hal)
{
    sx127x_t *dev = hal->priv;
    assert(dev);

    uint8_t interruptReg = sx127x_reg_read(dev, SX127X_REG_LR_IRQFLAGS);

    if (interruptReg & (SX127X_RF_LORA_IRQFLAGS_TXDONE |
                        SX127X_RF_LORA_IRQFLAGS_RXDONE)) {
        _on_dio0_irq(hal, interruptReg);
    }

    if (interruptReg & SX127X_RF_LORA_IRQFLAGS_VALIDHEADER) {

        sx127x_reg_write(dev, SX127X_REG_LR_IRQFLAGS, SX127X_RF_LORA_IRQFLAGS_VALIDHEADER);
        hal->cb(hal, IEEE802154_RADIO_INDICATION_RX_START);
    }

    if (interruptReg & SX127X_RF_LORA_IRQFLAGS_CADDONE) {
        dev->cad_detected = interruptReg & SX127X_RF_LORA_IRQFLAGS_CADDETECTED;

        sx127x_reg_write(dev, SX127X_REG_LR_IRQFLAGS, SX127X_RF_LORA_IRQFLAGS_CADDONE |
                                                      SX127X_RF_LORA_IRQFLAGS_CADDETECTED);
        hal->cb(hal, IEEE802154_RADIO_CONFIRM_CCA);
    }
#if 0
    if (interruptReg & SX127X_RF_LORA_IRQFLAGS_RXTIMEOUT) {
        _on_dio1_irq(dev);
    }

    if (interruptReg & SX127X_RF_LORA_IRQFLAGS_FHSSCHANGEDCHANNEL) {
        _on_dio2_irq(dev);
    }

    if (interruptReg & (SX127X_RF_LORA_IRQFLAGS_CADDETECTED |
                        SX127X_RF_LORA_IRQFLAGS_CADDONE |
                        SX127X_RF_LORA_IRQFLAGS_VALIDHEADER)) {
        _on_dio3_irq(dev);
    }
#endif
}

static int _set_cca_threshold(ieee802154_dev_t *dev, int8_t threshold)
{
    (void) dev;
    (void) threshold;
    return 0;
}

static const ieee802154_radio_ops_t sx127x_ops = {
    .caps =  IEEE802154_CAP_24_GHZ
          | IEEE802154_CAP_IRQ_CRC_ERROR
          | IEEE802154_CAP_IRQ_RX_START
          | IEEE802154_CAP_IRQ_TX_START
          | IEEE802154_CAP_IRQ_TX_DONE
          | IEEE802154_CAP_IRQ_CCA_DONE
          | IEEE802154_CAP_PHY_OQPSK,

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
