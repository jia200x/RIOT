/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_at86rf2xx
 * @{
 *
 * @file
 * @brief       RF Ops adaption for the AT86RF2xx drivers
 *
 * @author      Jose I. Alamos <jose.alamos@haw-hamburg.de>
 *
 * @}
 */

#include <string.h>
#include <assert.h>
#include <errno.h>

#include "iolist.h"

#include "net/eui64.h"
#include "net/ieee802154.h"
#include "net/netdev.h"
#include "net/netdev/ieee802154.h"

#include "at86rf2xx.h"
#include "at86rf2xx_netdev.h"
#include "at86rf2xx_internal.h"
#include "at86rf2xx_registers.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static void _irq_handler(void *arg)
{
    netdev_t *dev = (netdev_t *) arg;

    if (dev->event_callback) {
        dev->event_callback(dev, NETDEV_EVENT_ISR);
    }
}

static void _prepare(netdev_ieee802154_t *netdev)
{
    at86rf2xx_t *dev = (at86rf2xx_t *)netdev;
    at86rf2xx_tx_prepare(dev);
}

static void _tx_load(netdev_ieee802154_t *netdev, const uint8_t *data, size_t len)
{
    at86rf2xx_t *dev = (at86rf2xx_t *)netdev;
    at86rf2xx_tx_load(dev, data, len);
}

static void _cmd(netdev_ieee802154_t *netdev, rf_ops_cmd_t cmd)
{
    at86rf2xx_t *dev = (at86rf2xx_t *)netdev;
    switch(cmd) {
        case NETDEV_IEEE802154_CMD_TX_NOW:
            at86rf2xx_tx_exec(dev);
            break;
        default:
            assert(false);
    }
}

static int _set_channel(netdev_ieee802154_t *netdev, uint8_t page, uint8_t channel)
{
    at86rf2xx_t *dev = (at86rf2xx_t *)netdev;
#ifdef MODULE_AT86RF212B
    if ((page != 0) && (page != 2)) {
        return -EINVAL;
    }
#else
    if (page != 0) {
        return -EINVAL;
    }
#endif

    at86rf2xx_set_page(dev, page);
    at86rf2xx_set_chan(dev, channel);
    return 0;
}

static int _init(netdev_ieee802154_t *netdev)
{
    at86rf2xx_t *dev = (at86rf2xx_t *)netdev;

    /* initialize GPIOs */
    spi_init_cs(dev->params.spi, dev->params.cs_pin);
    gpio_init(dev->params.sleep_pin, GPIO_OUT);
    gpio_clear(dev->params.sleep_pin);
    gpio_init(dev->params.reset_pin, GPIO_OUT);
    gpio_set(dev->params.reset_pin);
    gpio_init_int(dev->params.int_pin, GPIO_IN, GPIO_RISING, _irq_handler, dev);

    /* reset device to default values and put it into RX state */
    at86rf2xx_reset(dev);

    netdev->state = IEEE802154_PHY_RX_ON;

    /* test if the SPI is set up correctly and the device is responding */
    if (at86rf2xx_reg_read(dev, AT86RF2XX_REG__PART_NUM) != AT86RF2XX_PARTNUM) {
        DEBUG("[at86rf2xx] error: unable to read correct part number\n");
        return -1;
    }

    return 0;
}

static void _set_hw_addr_filtering(netdev_ieee802154_t *netdev, const ieee802154_address_filter_t *addr_filter)
{
    at86rf2xx_t *dev = (at86rf2xx_t *)netdev;
    switch(addr_filter->type) {
        case IEEE802154_SHORT_ADDR:
            at86rf2xx_set_addr_short(dev, addr_filter->param.short_addr);
            break;
        case IEEE802154_LONG_ADDR:
            at86rf2xx_set_addr_long(dev, addr_filter->param.long_addr);
            break;
        case IEEE802154_PAN_ID:
            at86rf2xx_set_pan(dev, addr_filter->param.pan_id);
            break;
    }
}

static void _set_tx_power(netdev_ieee802154_t *netdev, int32_t power)
{
    at86rf2xx_t *dev = (at86rf2xx_t *)netdev;
    at86rf2xx_set_txpower(dev, (int16_t) power);
}

static int _extended(netdev_ieee802154_t *netdev, ieee802154_ext_t opt, const void *data)
{
    at86rf2xx_t *dev = (at86rf2xx_t *)netdev;
    switch(opt) {
        case IEEE802154_EXT_SET_HW_ADDR:
            _set_hw_addr_filtering(netdev, data);
            break;
        case IEEE802154_EXT_SET_FRAME_PENDING:
            at86rf2xx_set_option(dev, AT86RF2XX_OPT_ACK_PENDING,
                                 ((const bool *)data)[0]);
            break;
        case IEEE802154_EXT_PROMISCUOUS:
            at86rf2xx_set_option(dev, AT86RF2XX_OPT_PROMISCUOUS,
                                 ((const bool *)data)[0]);
            break;
        case IEEE802154_EXT_SET_FRAME_RETRIES:
            at86rf2xx_set_max_retries(dev, *((const uint8_t *)data));
            break;
        case IEEE802154_EXT_SET_CSMA_PARAMS:
            at86rf2xx_set_option(dev, AT86RF2XX_OPT_CSMA,
                                 ((const bool *)data)[0]);
            break;
        case IEEE802154_EXT_ED_THRESHOLD:
            at86rf2xx_set_cca_threshold(dev, *((const int8_t *)data));
            break;
        default:
            break;
    }
    return 0;
}

int _set_trx_state(netdev_ieee802154_t *netdev, ieee802154_phy_const_t state)
{
    at86rf2xx_t *dev = (at86rf2xx_t *)netdev;

    switch(state) {
        case IEEE802154_PHY_TX_ON:
            at86rf2xx_tx_prepare(dev);
            break;
        case IEEE802154_PHY_RX_ON:
            at86rf2xx_set_state(dev, AT86RF2XX_STATE_RX_AACK_ON);
            break;
        case IEEE802154_PHY_TRX_OFF:
            at86rf2xx_set_state(dev, AT86RF2XX_STATE_TRX_OFF);
            break;
        case IEEE802154_PHY_FORCE_TRX_OFF:
            at86rf2xx_set_state(dev, AT86RF2XX_STATE_TRX_OFF);
            break;
        default:
            break;
    }

    return 0;
}

static void _on(netdev_ieee802154_t *netdev)
{
    (void) netdev;
}

static void _off(netdev_ieee802154_t *netdev)
{
    (void) netdev;
}

static void _ed(netdev_ieee802154_t *netdev)
{
    (void) netdev;
}

static void _cca(netdev_ieee802154_t *netdev)
{
    (void) netdev;
}

static void _cca_mode(netdev_ieee802154_t *netdev, netdev_ieee802154_cca_mode_t mode, 
            ieee802154_cca_opt_t opt)
{
    /* TBD */
    (void) netdev;
    (void) mode;
    (void) opt;
}

const netdev_ieee802154_rf_ops_t at86rf2xx_rf_ops = {
    .init = _init,
    .prepare = _prepare,
    .tx_load = _tx_load,
    .cmd = _cmd,
    .on = _on,
    .off = _off,
    .ed = _ed,
    .cca = _cca,
    .cca_mode = _cca_mode,
    .set_channel = _set_channel,
    .set_tx_power = _set_tx_power,
    .extended = _extended,
    .set_trx_state = _set_trx_state,
};

