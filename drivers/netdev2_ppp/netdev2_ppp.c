/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_netdev2_eth
 * @{
 *
 * @file
 * @brief       Common code for netdev2 ethernet drivers
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 *
 * @}
 */

#include <assert.h>
#include <errno.h>

#include "net/netdev2.h"
#include "net/eui64.h"
#include "net/ethernet.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

int netdev2_ppp_get(netdev2_t *dev, netopt_t opt, void *value, size_t max_len)
{
	netdev2_ppp_t *pppdev = (netdev2_ppp_t*) dev;
    int res;
    switch (opt) {
        case NETOPT_PPP_LCP_STATE:
            *((uint8_t *) value) = pppdev->lcp->state;
            res = 0;
            break;
        case NETOPT_PPP_AUTH_STATE:
            *((uint8_t *) value) = pppdev->pap->state;
            res = 0;
            break;
        case NETOPT_PPP_IPCP_STATE:
            *((uint8_t *) value) = pppdev->ipcp->state;
            res = 0;
            break;
        case NETOPT_PPP_IS_IPV6_READY:
            res = pppdev->ipv4->state == PROTOCOL_UP;
            *((uint8_t *) value) = res;
            break;
        case NETOPT_IS_PPP_IF:
            res = 1;
            break;
        default:
            res = dev->driver->get(dev, opt, value, value_len);
            break;
    }
    return res;
}

int netdev2_ppp_set(netdev2_t *dev, netopt_t opt, void *value, size_t value_len)
{
	netdev2_ppp_t *pppdev = (netdev2_ppp_t*) dev;
    int res;
    switch (opt) {
        case NETOPT_TUNNEL_IPV4_ADDRESS:
            ((ppp_ipv4_t *) pppdev->ipv4)->tunnel_addr = *((ipv4_addr_t *) value);
            res = 0;
            break;
        case NETOPT_TUNNEL_UDP_PORT:
            ((ppp_ipv4_t *) pppdev->ipv4)->tunnel_port = *((uint16_t *) value);
            res = 0;
            break;
        case NETOPT_APN_USER:
            memcpy(((pap_t *) pppdev->pap)->username, value, value_len);
            ((pap_t *) pppdev->pap)->user_size = value_len;
            res = 0;
            break;
        case NETOPT_APN_PASS:
            memcpy(((pap_t *) pppdev->pap)->password, value, value_len);
            ((pap_t *) pppdev->pap)->pass_size = value_len;
            res = 0;
            break;
        default:
            res = dev->driver->set(dev, opt, value, value_len);
            break;
    }
    return res;
}
