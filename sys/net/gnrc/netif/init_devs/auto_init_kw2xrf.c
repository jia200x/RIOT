/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 * Copyright (C) 2016 PHYTEC Messtechnik GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/*
 * @ingroup sys_auto_init_gnrc_netif
 * @{
 *
 * @file
 * @brief   Auto initialization for kw2xrf network interfaces
 *
 * @author  Kaspar Schleiser <kaspar@schleiser.de>
 * @author  Jonas Remmert <j.remmert@phytec.de>
 * @author  Sebastian Meiling <s@mlng.net>
 */

#include "log.h"
#include "board.h"
#include "net/gnrc/netif/ieee802154.h"
#include "net/gnrc.h"
#include "include/init_devs.h"
#include "net/netdev/ieee802154_submac.h"

#include "kw2xrf.h"
#include "kw2xrf_params.h"

/**
 * @brief   Define stack parameters for the MAC layer thread
 * @{
 */
#define KW2XRF_MAC_STACKSIZE     (IEEE802154_STACKSIZE_DEFAULT)
#ifndef KW2XRF_MAC_PRIO
#define KW2XRF_MAC_PRIO          (GNRC_NETIF_PRIO)
#endif

#define KW2XRF_NUM ARRAY_SIZE(kw2xrf_params)

static kw2xrf_t kw2xrf_devs[KW2XRF_NUM];
static netdev_ieee802154_submac_t kw2xrf_netdev[KW2XRF_NUM];
static char _kw2xrf_stacks[KW2XRF_NUM][KW2XRF_MAC_STACKSIZE];
static gnrc_netif_bhp_ctx_t kw2xrf_ctx[KW2XRF_NUM];

static void kw2xrf_irq_event_handler(event_t *evt){
    gnrc_netif_bhp_ctx_t *c = container_of(evt, gnrc_netif_bhp_ctx_t, event);
    netdev_t *netdev = c->netif.dev;
    netdev_ieee802154_t *netdev_ieee802154 = container_of(netdev, netdev_ieee802154_t, netdev);
    netdev_ieee802154_submac_t *netdev_submac = container_of(netdev_ieee802154,
                                                             netdev_ieee802154_submac_t,
                                                             dev);
    kw2xrf_radio_hal_irq_handler(&netdev_submac->submac.dev);
}

void auto_init_kw2xrf(void)
{
    for (unsigned i = 0; i < KW2XRF_NUM; i++) {
        const kw2xrf_params_t *p = &kw2xrf_params[i];

        LOG_DEBUG("[auto_init_netif] initializing kw2xrf #%u\n", i);

        /* Init radio */
        kw2xrf_init(&kw2xrf_devs[i], (kw2xrf_params_t*) p,&kw2xrf_netdev[i].submac.dev,
                        gnrc_netif_bhp_irq_handler, &kw2xrf_ctx[i]);

        netdev_register(&kw2xrf_netdev[i].dev.netdev, NETDEV_KW2XRF, i);
        netdev_ieee802154_submac_init(&kw2xrf_netdev[i]);

        /* Setup events */
        gnrc_netif_bhp_init(&kw2xrf_ctx[i], kw2xrf_irq_event_handler);

        gnrc_netif_ieee802154_create(&kw2xrf_ctx[i].netif, _kw2xrf_stacks[i], KW2XRF_MAC_STACKSIZE,
                                     KW2XRF_MAC_PRIO, "kw2xrf",
                                     &kw2xrf_netdev[i].dev.netdev);

    }
}
/** @} */
