/*
 * Copyright (C) 2017 Hamburg University of Applied Sciences
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     pkg_openwsn
 * @{
 *
 * @file
 *
 * @author      Tengfei Chang <tengfei.chang@gmail.com>, July 2012
 * @author      Peter Kietzmann <peter.kietzmann@haw-hamburg.de>, July 2017
 * @author      Michel Rottleuthner <michel.rottleuthner@haw-hamburg.de>, April 2019
 *
 * @}
 */

#include "sctimer.h"

#include "periph/rtt.h"

#define LOG_LEVEL LOG_NONE
#include "log.h"

#if RTT_FREQUENCY != 32768U
    #error "RTT_FREQUENCY not supported"
#endif

/**
 * @brief   TODO
 */
#define OW_SCTIMER_MIN_COMP_ADVANCE     (10)

/**
 * @brief   TODO
 */
#define OW_SCTIMER_ISR_NOW_OFFSET       (10)

typedef struct {
    sctimer_cbt sctimer_cb;
    bool convert;
    bool convertUnlock;
} sctimer_vars_t;


sctimer_vars_t sctimer_vars;

static void sctimer_isr_internal(void *arg)
{
    (void)arg;

    if (sctimer_vars.sctimer_cb != NULL) {
        sctimer_vars.sctimer_cb();
    }
}

void sctimer_init(void)
{
    LOG_DEBUG("%s\n", __FUNCTION__);
    memset(&sctimer_vars, 0, sizeof(sctimer_vars_t));
    rtt_init();
}

void sctimer_set_callback(sctimer_cbt cb)
{
    LOG_DEBUG("%s\n", __FUNCTION__);
    sctimer_vars.sctimer_cb = cb;
}

void sctimer_setCompare(uint32_t val)
{
    DISABLE_INTERRUPTS();

    uint32_t cnt = rtt_get_counter();

    /* ATTENTION! This needs to be an unsigned type */
    if ((int32_t)(val - cnt) < OW_SCTIMER_MIN_COMP_ADVANCE) {
        rtt_set_alarm(cnt + OW_SCTIMER_ISR_NOW_OFFSET, sctimer_isr_internal, NULL);
    }
    else {
        rtt_set_alarm(val, sctimer_isr_internal, NULL);
    }

    ENABLE_INTERRUPTS();
}

uint32_t sctimer_readCounter(void)
{
    uint32_t now = rtt_get_counter();
    return now;
}

void sctimer_enable(void)
{
    LOG_DEBUG("%s\n", __FUNCTION__);
    rtt_poweron();
}

void sctimer_disable(void)
{
    LOG_DEBUG("%s\n", __FUNCTION__);
    rtt_poweroff();
}
