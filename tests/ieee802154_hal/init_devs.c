/*
 * Copyright (C) 2020 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for ieee802154_hal
 *
 * @author      José I. Alamos <jose.alamos@haw-hamburg.de>
 *
 * @}
 */

#include "assert.h"
#include "kernel_defines.h"
#include "net/ieee802154/radio.h"
#include "event.h"
#include "event/thread.h"
#include "common.h"

#ifdef MODULE_CC2538_RF
#include "cc2538_rf.h"
#endif

#ifdef MODULE_NRF802154
#include "nrf802154.h"
#endif

#ifdef MODULE_SX127X
#include "sx127x.h"
#include "sx127x_params.h"
static sx127x_t sx127x_dev;
#endif

#ifdef MODULE_SX127X
ieee802154_dev_t *sx127x_hal;

void sx127x_hal_task_handler(ieee802154_dev_t *hal);

void _sx127x_handler(event_t *event)
{
    (void) event;
    sx127x_hal_task_handler(sx127x_hal);
}

event_t sx127x_ev = {.handler = _sx127x_handler};

void sx127x_isr(void *arg)
{
    (void) arg;
    event_post(EVENT_PRIO_HIGHEST, &sx127x_ev);
}
#endif

void ieee802154_hal_test_init_devs(ieee802154_dev_cb_t cb, void *opaque)
{
    /* Call the init function of the device (this should be handled by
     * `auto_init`) */
    ieee802154_dev_t *radio = NULL;
    (void) radio;
#ifdef MODULE_CC2538_RF
    if ((radio = cb(IEEE802154_DEV_TYPE_CC2538_RF, opaque)) ){
        cc2538_rf_hal_setup(radio);
        cc2538_init();
    }
#endif

#ifdef MODULE_NRF802154
    if ((radio = cb(IEEE802154_DEV_TYPE_NRF802154, opaque)) ){
        nrf802154_hal_setup(radio);
        nrf802154_init();
    }
#endif

#ifdef MODULE_SX127X
    if ((radio = cb(IEEE802154_DEV_TYPE_SX127X, opaque)) ){
        sx127x_hal = radio;
        sx127x_hal_setup(&sx127x_dev, radio);
        sx127x_setup(&sx127x_dev, sx127x_params, radio);
    }
#endif
}
