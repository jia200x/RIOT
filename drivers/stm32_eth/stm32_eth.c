/*
 * Copyright (C) 2016 TriaGnoSys GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     drivers_stm32_common
 * @{
 *
 * @file
 * @brief       Netdev wrapper for stm32 ethernet
 *
 * @author      Víctor Ariño <victor.arino@triagnosys.com>
 *
 * @}
 */

#include "periph_conf.h"
#include "mutex.h"
#include "net/ethernet.h"
#include "net/ethernet/hal.h"
#include "iolist.h"
#define ENABLE_DEBUG (0)
#include "debug.h"

#include <string.h>
static mutex_t _tx = MUTEX_INIT;
static mutex_t _rx = MUTEX_INIT;

void stm32_eth_set_mac(const char *mac);
void stm32_eth_get_mac(char *out);
int stm32_eth_init(void);
int stm32_eth_receive_blocking(char *data, unsigned max_len);
int stm32_eth_send(const struct iolist *iolist);
int stm32_eth_get_rx_status_owned(void);

void stm32_eth_rx_complete(void);
void stm32_eth_isr(void);

void stm32_eth_task_handler(void)
{
    if(stm32_eth_get_rx_status_owned()) {
        stm32_eth_rx_complete();
    }
}

void isr_eth(void)
{
    volatile unsigned tmp = ETH->DMASR;

    if ((tmp & ETH_DMASR_TS)) {
        ETH->DMASR = ETH_DMASR_TS | ETH_DMASR_NIS;
        mutex_unlock(&_tx);
    }

    if ((tmp & ETH_DMASR_RS)) {
        ETH->DMASR = ETH_DMASR_RS | ETH_DMASR_NIS;
        mutex_unlock(&_rx);
        stm32_eth_isr();
    }

    /* printf("r:%x\n\n", tmp); */

    cortexm_isr_end();
}

static int _recv(ethernet_hal_t *dev, void *buf, size_t len)
{
    (void) dev;
    if(!stm32_eth_get_rx_status_owned()){
                mutex_lock(&_rx);
    }
    int ret = stm32_eth_receive_blocking((char *)buf, len);
    DEBUG("stm32_eth_netdev: _recev: %d\n", ret);

    return ret;
}

static int _send(ethernet_hal_t *dev, const struct iolist *iolist)
{
    int ret = 0;
    if(stm32_eth_get_rx_status_owned()) {
        mutex_lock(&_tx);
    }
    ret = stm32_eth_send(iolist);
    DEBUG("stm32_eth_netdev: _send: %d %d\n", ret, iolist_size(iolist));
    if (ret < 0)
    {
        return ret;
    }

    dev->cbs->tx_done(dev);

    return ret;
}

bool _link(ethernet_hal_t *dev)
{
    (void) dev;
    return true; 
}

int _hw_addr(ethernet_hal_t *dev, uint8_t *addr, int set)
{
    (void) dev;
    if(set) {
        stm32_eth_set_mac((char *)addr);
    }
    else {
        stm32_eth_get_mac((char*) addr);
    }
    return 0;
}
#if 0
static int _set(netdev_t *dev, netopt_t opt, const void *value, size_t max_len)
{
    int res = -1;

    switch (opt) {
        case NETOPT_ADDRESS:
            assert(max_len >= ETHERNET_ADDR_LEN);
            stm32_eth_set_mac((char *)value);
            res = ETHERNET_ADDR_LEN;
            break;
        default:
            res = netdev_eth_set(dev, opt, value, max_len);
            break;
    }

    return res;
}

static int _get(netdev_t *dev, netopt_t opt, void *value, size_t max_len)
{
    int res = -1;

    switch (opt) {
        case NETOPT_ADDRESS:
            assert(max_len >= ETHERNET_ADDR_LEN);
            stm32_eth_get_mac((char *)value);
            res = ETHERNET_ADDR_LEN;
            break;
        default:
            res = netdev_eth_get(dev, opt, value, max_len);
            break;
    }

    return res;
}
#endif

static ethernet_driver_t driver = {
    .recv = _recv,
    .send = _send,
    .hw_addr = _hw_addr,
    .link = _link,
};

void stm32_eth_hal_setup(ethernet_hal_t *dev)
{
    dev->driver = &driver;
}
#if 0
static int _init(netdev_t *netdev)
{
    (void)netdev;
    return stm32_eth_init();
}
#endif
