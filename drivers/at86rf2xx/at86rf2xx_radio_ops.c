#include "at86rf2xx.h"
#include "at86rf2xx_internal.h"
#include "at86rf2xx_registers.h"
#include "net/ieee802154/radio.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static int set_trx_state(ieee802154_dev_t *dev, ieee802154_trx_state_t state);
static const ieee802154_radio_ops_t at86rf2xx_ops;

static inline bool _is_sleep(ieee802154_dev_t *dev)
{
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    return _dev->is_sleep;
}

/* This wakes up the radio! */
/* This doesn't check sizes!! */
static int prepare(ieee802154_dev_t *dev, iolist_t *pkt)
{
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    uint8_t len = 0;
    bool sleep = _is_sleep(dev);

    if (sleep) {
        at86rf2xx_assert_awake((at86rf2xx_t*) dev);
    }

    /* load packet data into FIFO */
    for (const iolist_t *iol = pkt; iol; iol = iol->iol_next) {
        if (iol->iol_len) {
            at86rf2xx_sram_write(_dev, len + 1, iol->iol_base, iol->iol_len);
            len += iol->iol_len;
        }
    }
    len += IEEE802154_FCS_LEN;

    /* write frame length field in FIFO */
    at86rf2xx_sram_write(_dev, 0, &len, 1);
    return 0;
}

static int transmit(ieee802154_dev_t *dev, ieee802154_tx_mode_t mode)
{
    if ((IS_ACTIVE(AT86RF2XX_EXT) && mode != IEEE802154_TX_MODE_CSMA_CA) ||
        mode != IEEE802154_TX_MODE_DIRECT) {
        return -ENOTSUP;
    }
    at86rf2xx_tx_exec((at86rf2xx_t*) dev);
    return 0;
}

static int _len(ieee802154_dev_t *dev)
{
    /* get the size of the received packet */
    uint8_t phr;

#if defined(MODULE_AT86RFA1) || defined(MODULE_AT86RFR2)
    phr = TST_RX_LENGTH;
#else
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    at86rf2xx_sram_read(_dev, 0, &phr, 1);
#endif

    return phr;
}

/* The radio should be woken up */
static int _read(ieee802154_dev_t *dev, void *buf, size_t size, ieee802154_rx_info_t *info)
{
    uint8_t phr;
    size_t pkt_len;

    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;

    /* start frame buffer access */
    at86rf2xx_fb_start(_dev);

    /* get the size of the received packet */
#if defined(MODULE_AT86RFA1) || defined(MODULE_AT86RFR2)
    phr = TST_RX_LENGTH;
#else
    at86rf2xx_fb_read(_dev, &phr, 1);
#endif

    /* ignore MSB (refer p.80) and subtract length of FCS field */
    pkt_len = (phr & 0x7f) - 2;

    /* not enough space in buf */
    if (pkt_len > size) {
        at86rf2xx_fb_stop(_dev);
        /* set device back in operation state which was used before last transmission.
         * This state is saved in at86rf2xx.c/at86rf2xx_tx_prepare() e.g RX_AACK_ON */
/* TODO: RX_CONTINUOUS here */
        return -ENOBUFS;
    }
    /* copy payload */
    at86rf2xx_fb_read(_dev, buf, pkt_len);

    /* Ignore FCS but advance fb read - we must give a temporary buffer here,
     * as we are not allowed to issue SPI transfers without any buffer */
    uint8_t tmp[2];
    at86rf2xx_fb_read(_dev, tmp, 2);
    (void)tmp;

    /* AT86RF212B RSSI_BASE_VAL + 1.03 * ED, base varies for diff. modulation and datarates
     * AT86RF232  RSSI_BASE_VAL + ED, base -91dBm
     * AT86RF233  RSSI_BASE_VAL + ED, base -94dBm
     * AT86RF231  RSSI_BASE_VAL + ED, base -91dBm
     * AT86RFA1   RSSI_BASE_VAL + ED, base -90dBm
     * AT86RFR2   RSSI_BASE_VAL + ED, base -90dBm
     *
     * AT86RF231 MAN. p.92, 8.4.3 Data Interpretation
     * AT86RF232 MAN. p.91, 8.4.3 Data Interpretation
     * AT86RF233 MAN. p.102, 8.5.3 Data Interpretation
     *
     * for performance reasons we ignore the 1.03 scale factor on the 212B,
     * which causes a slight error in the values, but the accuracy of the ED
     * value is specified as +/- 5 dB, so it should not matter very much in real
     * life.
     */
    if (info != NULL) {
        uint8_t ed = 0;
        at86rf2xx_fb_read(_dev, &(info->lqi), 1);

#if defined(MODULE_AT86RF231) || defined(MODULE_AT86RFA1) || defined(MODULE_AT86RFR2)
        /* AT86RF231 does not provide ED at the end of the frame buffer, read
         * from separate register instead */
        at86rf2xx_fb_stop(_dev);
        ed = at86rf2xx_reg_read(_dev, AT86RF2XX_REG__PHY_ED_LEVEL);
#else
        at86rf2xx_fb_read(_dev, &ed, 1);
        at86rf2xx_fb_stop(_dev);
#endif
        info->rssi = RSSI_BASE_VAL + ed;
        info->crc_ok = at86rf2xx_reg_read(_dev, AT86RF2XX_REG__PHY_RSSI) & 0x80;
        DEBUG("[at86rf2xx] LQI:%d high is good, RSSI:%d high is either good or"
              "too much interference.\n", info->lqi, info->rssi);
    }
    else {
        at86rf2xx_fb_stop(_dev);
    }

    /* TODO: Set RX state if not RX continuous */
    /* set device back in operation state which was used before last transmission.
     * This state is saved in at86rf2xx.c/at86rf2xx_tx_prepare() e.g RX_AACK_ON */

    return pkt_len;
}

static bool cca(ieee802154_dev_t *dev)
{
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    uint8_t old_state = _dev->trx_state;
    at86rf2xx_set_state(_dev, AT86RF2XX_TRX_STATE_TRX_OFF);
    bool result = at86rf2xx_cca(_dev);
    at86rf2xx_set_state(_dev, old_state);
    return result;
}

static int set_cca_threshold(ieee802154_dev_t *dev, int8_t threshold)
{
    bool sleep = _is_sleep(dev);
    if (sleep) {
        at86rf2xx_assert_awake((at86rf2xx_t*) dev);
    }
    at86rf2xx_set_cca_threshold((at86rf2xx_t*) dev, threshold);
    if (sleep) {
        at86rf2xx_sleep((at86rf2xx_t*) dev);
    }
    return 0;
}

static int _config_phy(ieee802154_dev_t *dev, ieee802154_phy_conf_t *conf)
{
    bool sleep = _is_sleep(dev);
    uint8_t channel = conf->channel;
    int16_t pow = conf->pow;
    if (sleep) {
        at86rf2xx_assert_awake((at86rf2xx_t*) dev);
    }
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    /* we must be in TRX_OFF before changing the PHY configuration */
    int prev_state = _dev->trx_state;
    at86rf2xx_set_state(_dev, AT86RF2XX_TRX_STATE_TRX_OFF);

    uint8_t phy_cc_cca = at86rf2xx_reg_read(_dev, AT86RF2XX_REG__PHY_CC_CCA);
    /* Clear previous configuration for channel number */
    phy_cc_cca &= ~(AT86RF2XX_PHY_CC_CCA_MASK__CHANNEL);

    /* Update the channel register */
    phy_cc_cca |= (channel & AT86RF2XX_PHY_CC_CCA_MASK__CHANNEL);
    at86rf2xx_reg_write(_dev, AT86RF2XX_REG__PHY_CC_CCA, phy_cc_cca);

    /* Return to the state we had before reconfiguring */
    at86rf2xx_set_state(_dev, prev_state);
    at86rf2xx_set_txpower((at86rf2xx_t*) dev, pow);
    if (sleep) {
        at86rf2xx_sleep((at86rf2xx_t*) dev);
    }
    return 0;
}

static int set_trx_state(ieee802154_dev_t *dev, ieee802154_trx_state_t state)
{
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    bool sleep = _is_sleep(dev);
    int int_state;
    switch(state) {
        case IEEE802154_TRX_STATE_TRX_OFF:
            int_state = AT86RF2XX_TRX_STATE_TRX_OFF;
            break;
        case IEEE802154_TRX_STATE_RX_ON:
            int_state = AT86RF2XX_TRX_STATE_RX_ON;
            break;
        case IEEE802154_TRX_STATE_TX_ON:
            int_state = AT86RF2XX_TRX_STATE_TX_ON;
            break;
        default:
            return -EINVAL;
    }
    if (sleep) {
        at86rf2xx_assert_awake((at86rf2xx_t*) dev);
    }
    at86rf2xx_set_state(_dev, int_state);
    return 0;
}

static int _set_sleep(ieee802154_dev_t *dev, bool sleep)
{
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;

    if(sleep) {
        at86rf2xx_sleep((at86rf2xx_t*) dev);
    }
    else {
        at86rf2xx_assert_awake((at86rf2xx_t*) dev);
    }
    _dev->is_sleep = sleep;
    return 0;
}

static bool _get_cap(ieee802154_dev_t *dev, ieee802154_rf_caps_t cap)
{
    (void) dev;
    switch(cap) {
#if IS_ACTIVE(AT86RF2XX_EXT)
        case IEEE802154_CAP_FRAME_RETRIES:
        case IEEE802154_CAP_AUTO_ACK:
        case IEEE802154_CAP_HW_ADDR_FILTER:
            return true;
#endif
        default:
            return false;
    }
}

static int set_hw_addr_filter(ieee802154_dev_t *dev, uint8_t *short_addr, uint8_t *ext_addr, uint16_t pan_id)
{
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    bool sleep = _is_sleep(dev);
    if (sleep) {
        at86rf2xx_assert_awake((at86rf2xx_t*) dev);
    }
    le_uint16_t le_pan = byteorder_btols(byteorder_htons(pan_id));
    at86rf2xx_reg_write(_dev, AT86RF2XX_REG__SHORT_ADDR_0,
                        short_addr[1]);
    at86rf2xx_reg_write(_dev, AT86RF2XX_REG__SHORT_ADDR_1,
                        short_addr[0]);
    for (int i = 0; i < 8; i++) {
        at86rf2xx_reg_write(_dev, (AT86RF2XX_REG__IEEE_ADDR_0 + i),
                ext_addr[IEEE802154_LONG_ADDRESS_LEN - 1 - i]);
    }

    at86rf2xx_reg_write(_dev, AT86RF2XX_REG__PAN_ID_0, le_pan.u8[0]);
    at86rf2xx_reg_write(_dev, AT86RF2XX_REG__PAN_ID_1, le_pan.u8[1]);
    if (sleep) {
        at86rf2xx_sleep((at86rf2xx_t*) dev);
    }
    return 0;
}

static int set_frame_retries(ieee802154_dev_t *dev, int retries)
{
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    bool sleep = _is_sleep(dev);
    if (sleep) {
        at86rf2xx_assert_awake((at86rf2xx_t*) dev);
    }
    uint8_t tmp = at86rf2xx_reg_read(_dev, AT86RF2XX_REG__XAH_CTRL_0);

    tmp &= ~(AT86RF2XX_XAH_CTRL_0__MAX_FRAME_RETRIES);
    tmp |= ((retries > 7) ? 7 : retries) << 4;
    at86rf2xx_reg_write(_dev, AT86RF2XX_REG__XAH_CTRL_0, tmp);
    if (sleep) {
        at86rf2xx_sleep((at86rf2xx_t*) dev);
    }
    return 0;
}

static int set_csma_params(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd, int8_t retries)
{
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    bool sleep = _is_sleep(dev);
    if (sleep) {
        at86rf2xx_assert_awake((at86rf2xx_t*) dev);
    }
    retries = (retries > 5) ? 5 : retries;  /* valid values: 0-5 */
    retries = (retries < 0) ? 7 : retries;  /* max < 0 => disable CSMA (set to 7) */
    uint8_t tmp = at86rf2xx_reg_read(_dev, AT86RF2XX_REG__XAH_CTRL_0);
    tmp &= ~(AT86RF2XX_XAH_CTRL_0__MAX_CSMA_RETRIES);
    tmp |= (retries << 1);
    at86rf2xx_reg_write(_dev, AT86RF2XX_REG__XAH_CTRL_0, tmp);

    if(bd) {
        uint8_t max = bd->max;
        uint8_t min = bd->min;
        max = (max > 8) ? 8 : max;
        min = (min > max) ? max : min;
        at86rf2xx_reg_write(_dev, AT86RF2XX_REG__CSMA_BE, (max << 4) | (min));
    }
    if (sleep) {
        at86rf2xx_sleep((at86rf2xx_t*) dev);
    }

    return 0;
}

static int set_promiscuous(ieee802154_dev_t *dev, bool enable)
{
    bool sleep = _is_sleep(dev);
    if (sleep) {
        at86rf2xx_assert_awake((at86rf2xx_t*) dev);
    }

    at86rf2xx_set_promiscuous((at86rf2xx_t*) dev, enable);

    if (sleep) {
        at86rf2xx_sleep((at86rf2xx_t*) dev);
    }
    return 0;
}

int at86rf2xx_init(at86rf2xx_t *dev, const at86rf2xx_params_t *params, void (*isr)(void *arg))
{
    /* State to return after receiving or transmitting */
    dev->trx_state = AT86RF2XX_STATE_TRX_OFF;
    dev->dev.driver = &at86rf2xx_ops;

#if defined(MODULE_AT86RFA1) || defined(MODULE_AT86RFR2)
    (void) params;
    /* set all interrupts off */
    at86rf2xx_reg_write(dev, AT86RF2XX_REG__IRQ_MASK, 0x00);
#else
    /* initialize device descriptor */
    dev->params = *params;
#endif

    /* initialize GPIOs */
    spi_init_cs(dev->params.spi, dev->params.cs_pin);
    gpio_init(dev->params.sleep_pin, GPIO_OUT);
    gpio_clear(dev->params.sleep_pin);
    gpio_init(dev->params.reset_pin, GPIO_OUT);
    gpio_set(dev->params.reset_pin);
    gpio_init_int(dev->params.int_pin, GPIO_IN, GPIO_RISING, isr, dev);

    /* Intentionally check if bus can be acquired,
       since getbus() drops the return value */
    if (spi_acquire(dev->params.spi, dev->params.cs_pin, SPI_MODE_0,
                                                dev->params.spi_clk) < 0) {
        DEBUG("[at86rf2xx] error: unable to acquire SPI bus\n");
        return -EIO;
    }
    spi_release(dev->params.spi);

    /* test if the device is responding */
    if (at86rf2xx_reg_read(dev, AT86RF2XX_REG__PART_NUM) != AT86RF2XX_PARTNUM) {
        DEBUG("[at86rf2xx] error: unable to read correct part number\n");
        return -ENOTSUP;
    }

    /* reset device to default values and put it into RX state */
    at86rf2xx_reset(dev);
    at86rf2xx_set_state(dev, AT86RF2XX_TRX_STATE_RX_ON);

    return 0;
}

void at86rf2xx_init_int(at86rf2xx_t *dev)
{
    /* enable interrupts */
    at86rf2xx_reg_write(dev, AT86RF2XX_REG__IRQ_MASK,
                        AT86RF2XX_IRQ_STATUS_MASK__TRX_END);
    /* clear interrupt flags */
    at86rf2xx_reg_read(dev, AT86RF2XX_REG__IRQ_STATUS);
}

#if 0
static void _isr_send_complete(ieee802154_dev_t *dev, uint8_t trac_status)
{
/* Only radios with the XAH_CTRL_2 register support frame retry reporting */
#if AT86RF2XX_HAVE_RETRIES
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    _dev->tx_retries = (at86rf2xx_reg_read(_dev, AT86RF2XX_REG__XAH_CTRL_2)
                       & AT86RF2XX_XAH_CTRL_2__ARET_FRAME_RETRIES_MASK) >>
                      AT86RF2XX_XAH_CTRL_2__ARET_FRAME_RETRIES_OFFSET;
#endif

    DEBUG("[at86rf2xx] EVT - TX_END\n");

    if(IS_ACTIVE(AT86RF2XX_EXT)) {
        switch (trac_status) {
                    case AT86RF2XX_TRX_STATE__TRAC_SUCCESS:
                    case AT86RF2XX_TRX_STATE__TRAC_SUCCESS_DATA_PENDING:
                        dev->cb(dev, IEEE802154_RF_EV_TX_DONE, NULL);
                        DEBUG("[at86rf2xx] TX SUCCESS\n");
                        break;
                    case AT86RF2XX_TRX_STATE__TRAC_NO_ACK:
                        dev->cb(dev, IEEE802154_RF_EV_TX_NO_ACK, NULL);
                        DEBUG("[at86rf2xx] TX NO_ACK\n");
                        break;
                    case AT86RF2XX_TRX_STATE__TRAC_CHANNEL_ACCESS_FAILURE:
                        dev->cb(dev, IEEE802154_RF_EV_TX_MEDIUM_BUSY, NULL);
                        DEBUG("[at86rf2xx] TX_CHANNEL_ACCESS_FAILURE\n");
                        break;
                    default:
                        DEBUG("[at86rf2xx] Unhandled TRAC_STATUS: %d\n",
                              trac_status >> 5);
                }
    }
    else {
        dev->cb(dev, IEEE802154_RF_EV_TX_DONE, NULL);
        DEBUG("[at86rf2xx] TX SUCCESS\n");
    }
}
#endif

void _irq_handler(ieee802154_dev_t *dev)
{
    uint8_t irq_mask;
    uint8_t state;

    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    /* If transceiver is sleeping register access is impossible and frames are
     * lost anyway, so return immediately.
     */
    if (_is_sleep(dev)) {
        return;
    }

    state = _dev->trx_state;
    /* read (consume) device status */
    irq_mask = at86rf2xx_clear_irq_flags(_dev);

    if (at86rf2xx_irq_has_rx_start(irq_mask)) {
        dev->cb(dev, IEEE802154_RADIO_RX_START);
    }

    if (at86rf2xx_irq_has_trx_end(irq_mask)) {
        if (state == AT86RF2XX_TRX_STATE_RX_ON) {
            dev->cb(dev, IEEE802154_RADIO_RX_DONE);
        }
        else if (state == AT86RF2XX_TRX_STATE_TX_ON) {
            dev->cb(dev, IEEE802154_RADIO_TX_DONE);
        }
    }
}

int get_tx_status(ieee802154_dev_t *dev, ieee802154_tx_info_t *info)
{
    uint8_t trac_status;
    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    trac_status = at86rf2xx_reg_read(_dev, AT86RF2XX_REG__TRX_STATE)
                  & AT86RF2XX_TRX_STATE_MASK__TRAC;
    info->frame_pending = false;
    switch (trac_status) {
                case AT86RF2XX_TRX_STATE__TRAC_SUCCESS_DATA_PENDING:
                    info->frame_pending = true;
                    /* FALL-THRU */
                case AT86RF2XX_TRX_STATE__TRAC_SUCCESS:
                    DEBUG("[at86rf2xx] TX SUCCESS\n");
#if AT86RF2XX_HAVE_RETRIES
    info->retries = (at86rf2xx_reg_read(_dev, AT86RF2XX_REG__XAH_CTRL_2)
                       & AT86RF2XX_XAH_CTRL_2__ARET_FRAME_RETRIES_MASK) >>
                      AT86RF2XX_XAH_CTRL_2__ARET_FRAME_RETRIES_OFFSET;
#else
    info->retries = -1;
#endif
                    return IEEE802154_RF_EV_TX_DONE;
                case AT86RF2XX_TRX_STATE__TRAC_NO_ACK:
                    DEBUG("[at86rf2xx] TX NO_ACK\n");
                    return IEEE802154_RF_EV_TX_NO_ACK;
                case AT86RF2XX_TRX_STATE__TRAC_CHANNEL_ACCESS_FAILURE:
                    DEBUG("[at86rf2xx] TX_CHANNEL_ACCESS_FAILURE\n");
                    return IEEE802154_RF_EV_TX_MEDIUM_BUSY;
                default:
                    DEBUG("[at86rf2xx] Unhandled TRAC_STATUS: %d\n",
                          trac_status >> 5);
            }
    return -EINVAL;
}

#if 0
void at86rf2xx_task_handler(ieee802154_dev_t *dev)
{
    uint8_t irq_mask;
    uint8_t state;
    uint8_t trac_status;

    at86rf2xx_t *_dev = (at86rf2xx_t*) dev;
    /* If transceiver is sleeping register access is impossible and frames are
     * lost anyway, so return immediately.
     */
    if (dev->flags & AT86RF2XX_FLAG_SLEEP) {
        return;
    }

    state = _dev->trx_state;
    /* read (consume) device status */
    irq_mask = at86rf2xx_clear_irq_flags(_dev);
    trac_status = at86rf2xx_reg_read(_dev, AT86RF2XX_REG__TRX_STATE)
                  & AT86RF2XX_TRX_STATE_MASK__TRAC;

    if (at86rf2xx_irq_has_rx_start(irq_mask)) {
        dev->cb(dev, IEEE802154_RF_EV_RX_START, NULL);
        DEBUG("[at86rf2xx] EVT - RX_START\n");
    }

    if (at86rf2xx_irq_has_trx_end(irq_mask)) {
        if (state == AT86RF2XX_TRX_STATE_RX_ON) {
            DEBUG("[at86rf2xx] EVT - RX_END\n");
            dev->cb(dev, IEEE802154_RF_EV_RX_DONE, NULL);
        }
        else if (state == AT86RF2XX_TRX_STATE_TX_ON) {
            _isr_send_complete(dev, trac_status);
        }
#if 0
        if (dev->flags & AT86RF2XX_FLAG_RX_CONTINUOUS) {
            state = AT86RF2XX_TRX_STATE_RX_ON; 
        }
        else {
            state = AT86RF2XX_TRX_STATE_TRX_OFF;
        }
        at86rf2xx_set_state(_dev, state);
        DEBUG("[at86rf2xx] return to idle state 0x%x\n", state);
#endif
    }
}

#endif
static int _start(ieee802154_dev_t *dev)
{
    at86rf2xx_init_int((at86rf2xx_t*) dev);
    return 0;
}

static const ieee802154_radio_ops_t at86rf2xx_ops = {
    .prepare = prepare,
    .transmit = transmit,
    .len = _len,
    .read = _read,
    .cca = cca,
    .set_cca_threshold = set_cca_threshold,
    .config_phy = _config_phy,
    .set_trx_state = set_trx_state,
    .set_sleep = _set_sleep,
    .get_cap = _get_cap,
    .set_hw_addr_filter = set_hw_addr_filter,
    .set_frame_retries = set_frame_retries,
    .set_csma_params = set_csma_params,
    .set_promiscuous = set_promiscuous,
    .irq_handler = _irq_handler,
    .get_tx_status = get_tx_status,
    .start = _start,
};
