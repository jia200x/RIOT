/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 * Copyright (C) 2017 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  Martine Lenders <mlenders@inf.fu-berlin.de>
 * @author  Kaspar Schleiser <kaspar@schleiser.de>
 */

#include <string.h>

#include "net/ethernet/hdr.h"
#include "net/ethernet.h"
#include "net/ethernet/hal.h"
#include "net/eui48.h"
#include "net/gnrc.h"
#include "net/gnrc/netif/ethernet.h"
#ifdef MODULE_GNRC_IPV6
#include "net/ipv6/hdr.h"
#endif
#ifdef MODULE_GNRC_IPV6_NIB
#include "net/gnrc/ipv6/nib.h"
#include "net/gnrc/ipv6.h"
#endif /* MODULE_GNRC_IPV6_NIB */


#define ENABLE_DEBUG (0)
#include "debug.h"

#if defined(MODULE_OD) && ENABLE_DEBUG
#include "od.h"
#endif

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt);
static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif);

static char addr_str[ETHERNET_ADDR_LEN * 3];

static void gnrc_netif_ethernet_init(gnrc_netif_t *netif)
{
    netif->device_type = NETDEV_TYPE_ETHERNET;
    gnrc_netif_ipv6_init_mtu(netif);
    ethernet_addr_get(netif->dev, netif->l2addr);
    netif->flags |= GNRC_NETIF_FLAGS_HAS_L2ADDR;
    netif->l2addr_len = sizeof(eui48_t);
    netif->cur_hl = GNRC_NETIF_DEFAULT_HL;
#ifdef MODULE_GNRC_IPV6_NIB
    gnrc_ipv6_nib_init_iface(netif);
#endif
}

static void _get_iid(ethernet_hal_t *dev, eui64_t *value)
{
    eui48_t mac;
    ethernet_addr_get(dev, mac.uint8);
    eui48_to_ipv6_iid(value, &mac);
}

int gnrc_netif_eth_set(gnrc_netif_t *netif,
                               const gnrc_netapi_opt_t *opt)
{
    int res = -ENOTSUP;

    ethernet_hal_t *dev = netif->dev;
    gnrc_netif_acquire(netif);
    switch (opt->opt) {
        case NETOPT_HOP_LIMIT:
            assert(opt->data_len == sizeof(uint8_t));
            netif->cur_hl = *((uint8_t *)opt->data);
            res = sizeof(uint8_t);
            break;
#ifdef MODULE_GNRC_IPV6
        case NETOPT_IPV6_ADDR: {
                assert(opt->data_len == sizeof(ipv6_addr_t));
                /* always assume manually added */
                uint8_t flags = ((((uint8_t)opt->context & 0xff) &
                                  ~GNRC_NETIF_IPV6_ADDRS_FLAGS_STATE_MASK) |
                                 GNRC_NETIF_IPV6_ADDRS_FLAGS_STATE_VALID);
                uint8_t pfx_len = (uint8_t)(opt->context >> 8U);
                /* acquire locks a recursive mutex so we are safe calling this
                 * public function */
                res = gnrc_netif_ipv6_addr_add_internal(netif, opt->data,
                                                        pfx_len, flags);
                if (res >= 0) {
                    res = sizeof(ipv6_addr_t);
                }
            }
            break;
        case NETOPT_IPV6_ADDR_REMOVE:
            assert(opt->data_len == sizeof(ipv6_addr_t));
            /* acquire locks a recursive mutex so we are safe calling this
             * public function */
            gnrc_netif_ipv6_addr_remove_internal(netif, opt->data);
            res = sizeof(ipv6_addr_t);
            break;
        case NETOPT_IPV6_GROUP:
            assert(opt->data_len == sizeof(ipv6_addr_t));
            /* acquire locks a recursive mutex so we are safe calling this
             * public function */
            res = gnrc_netif_ipv6_group_join_internal(netif, opt->data);
            if (res >= 0) {
                res = sizeof(ipv6_addr_t);
            }
            break;
        case NETOPT_IPV6_GROUP_LEAVE:
            assert(opt->data_len == sizeof(ipv6_addr_t));
            /* acquire locks a recursive mutex so we are safe calling this
             * public function */
            gnrc_netif_ipv6_group_leave_internal(netif, opt->data);
            res = sizeof(ipv6_addr_t);
            break;
        case NETOPT_MAX_PDU_SIZE:
            if (opt->context == GNRC_NETTYPE_IPV6) {
                assert(opt->data_len == sizeof(uint16_t));
                netif->ipv6.mtu = *((uint16_t *)opt->data);
                res = sizeof(uint16_t);
            }
            /* else set device */
            break;
#if GNRC_IPV6_NIB_CONF_ROUTER
        case NETOPT_IPV6_FORWARDING:
            assert(opt->data_len == sizeof(netopt_enable_t));
            if (*(((netopt_enable_t *)opt->data)) == NETOPT_ENABLE) {
                netif->flags |= GNRC_NETIF_FLAGS_IPV6_FORWARDING;
            }
            else {
                if (gnrc_netif_is_rtr_adv(netif)) {
                    gnrc_ipv6_nib_change_rtr_adv_iface(netif, false);
                }
                netif->flags &= ~GNRC_NETIF_FLAGS_IPV6_FORWARDING;
            }
            res = sizeof(netopt_enable_t);
            break;
        case NETOPT_IPV6_SND_RTR_ADV:
            assert(opt->data_len == sizeof(netopt_enable_t));
            gnrc_ipv6_nib_change_rtr_adv_iface(netif,
                    (*(((netopt_enable_t *)opt->data)) == NETOPT_ENABLE));
            res = sizeof(netopt_enable_t);
            break;
#endif  /* GNRC_IPV6_NIB_CONF_ROUTER */
#endif  /* MODULE_GNRC_IPV6 */
#ifdef MODULE_GNRC_SIXLOWPAN_IPHC
        case NETOPT_6LO_IPHC:
            assert(opt->data_len == sizeof(netopt_enable_t));
            if (*(((netopt_enable_t *)opt->data)) == NETOPT_ENABLE) {
                netif->flags |= GNRC_NETIF_FLAGS_6LO_HC;
            }
            else {
                netif->flags &= ~GNRC_NETIF_FLAGS_6LO_HC;
            }
            res = sizeof(netopt_enable_t);
            break;
#endif  /* MODULE_GNRC_SIXLOWPAN_IPHC */
        case NETOPT_RAWMODE:
            if (*(((netopt_enable_t *)opt->data)) == NETOPT_ENABLE) {
                netif->flags |= GNRC_NETIF_FLAGS_RAWMODE;
            }
            else {
                netif->flags &= ~GNRC_NETIF_FLAGS_RAWMODE;
            }
            /* Also propagate to the netdev device */
            /* It shouldn't apply to ethernet... */
            /* netif->dev->driver->set(netif->dev, NETOPT_RAWMODE, opt->data,
                                      opt->data_len);
                                      */
            res = sizeof(netopt_enable_t);
            break;
        default:
            break;
    }
    if (res == -ENOTSUP) {
        switch (opt->opt) {
            case NETOPT_ADDRESS:
                ethernet_addr_set(dev, netif->l2addr);
                break;
            default:
                break;
        }
    }
    gnrc_netif_release(netif);
    return res;
}

int gnrc_netif_eth_get(gnrc_netif_t *netif, gnrc_netapi_opt_t *opt)
{
    int res = -ENOTSUP;

    ethernet_hal_t *dev = netif->dev;
    gnrc_netif_acquire(netif);
    switch (opt->opt) {
        case NETOPT_6LO:
            assert(opt->data_len == sizeof(netopt_enable_t));
            *((netopt_enable_t *)opt->data) =
                    (netopt_enable_t)gnrc_netif_is_6lo(netif);
            res = sizeof(netopt_enable_t);
            break;
        case NETOPT_HOP_LIMIT:
            assert(opt->data_len == sizeof(uint8_t));
            *((uint8_t *)opt->data) = netif->cur_hl;
            res = sizeof(uint8_t);
            break;
        case NETOPT_STATS:
            /* XXX discussed this with Oleg, it's supposed to be a pointer */
            switch ((int16_t)opt->context) {
#if defined(MODULE_NETSTATS_IPV6) && defined(MODULE_GNRC_IPV6)
                case NETSTATS_IPV6:
                    assert(opt->data_len == sizeof(netstats_t *));
                    *((netstats_t **)opt->data) = &netif->ipv6.stats;
                    res = sizeof(&netif->ipv6.stats);
                    break;
#endif
#ifdef MODULE_NETSTATS_L2
                case NETSTATS_LAYER2:
                    assert(opt->data_len == sizeof(netstats_t *));
                    *((netstats_t **)opt->data) = &netif->stats;
                    res = sizeof(&netif->stats);
                    break;
#endif
                default:
                    /* take from device */
                    break;
            }
            break;
#ifdef MODULE_GNRC_IPV6
        case NETOPT_IPV6_ADDR: {
                assert(opt->data_len >= sizeof(ipv6_addr_t));
                ipv6_addr_t *tgt = opt->data;

                res = 0;
                for (unsigned i = 0;
                     (res < (int)opt->data_len) && (i < GNRC_NETIF_IPV6_ADDRS_NUMOF);
                     i++) {
                    if (netif->ipv6.addrs_flags[i] != 0) {
                        memcpy(tgt, &netif->ipv6.addrs[i], sizeof(ipv6_addr_t));
                        res += sizeof(ipv6_addr_t);
                        tgt++;
                    }
                }
            }
            break;
        case NETOPT_IPV6_ADDR_FLAGS: {
                assert(opt->data_len >= sizeof(uint8_t));
                uint8_t *tgt = opt->data;

                res = 0;
                for (unsigned i = 0;
                     (res < (int)opt->data_len) && (i < GNRC_NETIF_IPV6_ADDRS_NUMOF);
                     i++) {
                    if (netif->ipv6.addrs_flags[i] != 0) {
                        *tgt = netif->ipv6.addrs_flags[i];
                        res += sizeof(uint8_t);
                        tgt++;
                    }
                }
            }
            break;
        case NETOPT_IPV6_GROUP: {
                assert(opt->data_len >= sizeof(ipv6_addr_t));
                ipv6_addr_t *tgt = opt->data;

                res = 0;
                for (unsigned i = 0;
                     (res < (int)opt->data_len) && (i < GNRC_NETIF_IPV6_GROUPS_NUMOF);
                     i++) {
                    if (!ipv6_addr_is_unspecified(&netif->ipv6.groups[i])) {
                        memcpy(tgt, &netif->ipv6.groups[i], sizeof(ipv6_addr_t));
                        res += sizeof(ipv6_addr_t);
                        tgt++;
                    }
                }
            }
            break;
        case NETOPT_IPV6_IID:
            assert(opt->data_len >= sizeof(eui64_t));
            res = gnrc_netif_ipv6_get_iid(netif, opt->data);
            break;
        case NETOPT_MAX_PDU_SIZE:
            if (opt->context == GNRC_NETTYPE_IPV6) {
                assert(opt->data_len == sizeof(uint16_t));
                *((uint16_t *)opt->data) = netif->ipv6.mtu;
                res = sizeof(uint16_t);
            }
            /* else ask device */
            break;
#if GNRC_IPV6_NIB_CONF_ROUTER
        case NETOPT_IPV6_FORWARDING:
            assert(opt->data_len == sizeof(netopt_enable_t));
            *((netopt_enable_t *)opt->data) = (gnrc_netif_is_rtr(netif)) ?
                                              NETOPT_ENABLE : NETOPT_DISABLE;
            res = sizeof(netopt_enable_t);
            break;
        case NETOPT_IPV6_SND_RTR_ADV:
            assert(opt->data_len == sizeof(netopt_enable_t));
            *((netopt_enable_t *)opt->data) = (gnrc_netif_is_rtr_adv(netif)) ?
                                              NETOPT_ENABLE : NETOPT_DISABLE;
            res = sizeof(netopt_enable_t);
            break;
#endif  /* GNRC_IPV6_NIB_CONF_ROUTER */
#endif  /* MODULE_GNRC_IPV6 */
#ifdef MODULE_GNRC_SIXLOWPAN_IPHC
        case NETOPT_6LO_IPHC:
            assert(opt->data_len == sizeof(netopt_enable_t));
            *((netopt_enable_t *)opt->data) = (netif->flags & GNRC_NETIF_FLAGS_6LO_HC) ?
                                              NETOPT_ENABLE : NETOPT_DISABLE;
            res = sizeof(netopt_enable_t);
            break;
#endif  /* MODULE_GNRC_SIXLOWPAN_IPHC */
        default:
            break;
    }
    if (res == -ENOTSUP) {
        switch(opt->opt) {
            case NETOPT_DEVICE_TYPE:
                {
                    uint16_t *tgt = (uint16_t *)opt->data;
                    *tgt = NETDEV_TYPE_ETHERNET;
                    res = 2;
                    break;
                }
            case NETOPT_ADDR_LEN:
            case NETOPT_SRC_LEN:
                {
                    assert(opt->data_len == 2);
                    uint16_t *tgt = (uint16_t*)opt->data;
                    *tgt=6;
                    res = sizeof(uint16_t);
                    break;
                }
            case NETOPT_MAX_PDU_SIZE:
                {
                    assert(opt->data_len == 2);
                    uint16_t *val = (uint16_t*) opt->data;
                    *val = ETHERNET_DATA_LEN;
                    res = sizeof(uint16_t);
                    break;
                }
            case NETOPT_IS_WIRED:
                {
                    res = 1;
                    break;
                }
            case NETOPT_IPV6_IID:
                {
                    if(opt->data_len < sizeof(eui64_t)) {
                        res = -EOVERFLOW;
                        break;
                    }
                    _get_iid(dev, opt->data);
                    res = sizeof(eui64_t);
                }
                break;
            case NETOPT_LINK_CONNECTED:
                ethernet_link_get(dev);
                res = sizeof(bool);
                break;
            case NETOPT_ADDRESS:
                ethernet_addr_get(dev, opt->data);
                res = sizeof(eui48_t);
                break;
#ifdef MODULE_L2FILTER
            case NETOPT_L2FILTER:
                {
                    assert(max_len >= sizeof(l2filter_t **));
                    *((l2filter_t **)value) = dev->filter;
                    res = sizeof(l2filter_t **);
                    break;
                }
#endif
            default:
                break;
            }
    }
    gnrc_netif_release(netif);
    return res;
}

static const gnrc_netif_ops_t ethernet_ops = {
    .init = gnrc_netif_ethernet_init,
    .send = _send,
    .recv = _recv,
    .get = gnrc_netif_eth_get,
    .set = gnrc_netif_eth_set,
};

void gnrc_netif_recv(gnrc_netif_t *netif);

static void _tx_done(ethernet_hal_t *dev)
{
    (void) dev;
}

static void _rx_done(ethernet_hal_t *dev)
{
    gnrc_netif_t *netif = dev->ctx;
    gnrc_netif_recv(netif);
}

static const ethernet_cb_t _cb = {
    .rx_done = _rx_done,
    .tx_done = _tx_done,
};

gnrc_netif_t *gnrc_netif_ethernet_create(char *stack, int stacksize,
                                         char priority, char *name,
                                         void *dev, gnrc_netif_t *netif)
{
    ethernet_hal_t *eth_dev = dev;
    eth_dev->cbs = &_cb;
    gnrc_netif_create(stack, stacksize, priority, name, dev,
                             &ethernet_ops, netif);
    eth_dev->ctx = netif;
    return netif;
}

static inline void _addr_set_broadcast(uint8_t *dst)
{
    memset(dst, 0xff, ETHERNET_ADDR_LEN);
}

static inline void _addr_set_multicast(uint8_t *dst, gnrc_pktsnip_t *payload)
{
    switch (payload->type) {
#ifdef MODULE_GNRC_IPV6
        case GNRC_NETTYPE_IPV6:
            /* https://tools.ietf.org/html/rfc2464#section-7 */
            dst[0] = 0x33;
            dst[1] = 0x33;
            ipv6_hdr_t *ipv6 = payload->data;
            memcpy(dst + 2, ipv6->dst.u8 + 12, 4);
            break;
#endif
        default:
            _addr_set_broadcast(dst);
            break;
    }
}

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    ethernet_hdr_t hdr;
    gnrc_netif_hdr_t *netif_hdr;
    gnrc_pktsnip_t *payload;
    ethernet_hal_t *dev = netif->dev;
    int res;

    if (pkt == NULL) {
        DEBUG("gnrc_netif_ethernet: pkt was NULL\n");
        return -EINVAL;
    }

    payload = pkt->next;

    if (pkt->type != GNRC_NETTYPE_NETIF) {
        DEBUG("gnrc_netif_ethernet: First header was not generic netif header\n");
        return -EBADMSG;
    }

    if (payload) {
        hdr.type = byteorder_htons(gnrc_nettype_to_ethertype(payload->type));
    }
    else {
        hdr.type = byteorder_htons(ETHERTYPE_UNKNOWN);
    }

    netif_hdr = pkt->data;

    /* set ethernet header */
    if (netif_hdr->src_l2addr_len == ETHERNET_ADDR_LEN) {
        memcpy(hdr.dst, gnrc_netif_hdr_get_src_addr(netif_hdr),
               netif_hdr->src_l2addr_len);
    }
    else {
        ethernet_addr_get(dev, hdr.src);
    }

    if (netif_hdr->flags & GNRC_NETIF_HDR_FLAGS_BROADCAST) {
        _addr_set_broadcast(hdr.dst);
    }
    else if (netif_hdr->flags & GNRC_NETIF_HDR_FLAGS_MULTICAST) {
        if (payload == NULL) {
            DEBUG("gnrc_netif_ethernet: empty multicast packets over Ethernet "
                  "are not yet supported\n");
            return -ENOTSUP;
        }
        _addr_set_multicast(hdr.dst, payload);
    }
    else if (netif_hdr->dst_l2addr_len == ETHERNET_ADDR_LEN) {
        memcpy(hdr.dst, gnrc_netif_hdr_get_dst_addr(netif_hdr),
               ETHERNET_ADDR_LEN);
    }
    else {
        DEBUG("gnrc_netif_ethernet: destination address had unexpected "
              "format\n");
        return -EBADMSG;
    }

    DEBUG("gnrc_netif_ethernet: send to %02x:%02x:%02x:%02x:%02x:%02x\n",
          hdr.dst[0], hdr.dst[1], hdr.dst[2],
          hdr.dst[3], hdr.dst[4], hdr.dst[5]);

    iolist_t iolist = {
        .iol_next = (iolist_t *)payload,
        .iol_base = &hdr,
        .iol_len = sizeof(ethernet_hdr_t)
    };

#ifdef MODULE_NETSTATS_L2
    if ((netif_hdr->flags & GNRC_NETIF_HDR_FLAGS_BROADCAST) ||
        (netif_hdr->flags & GNRC_NETIF_HDR_FLAGS_MULTICAST)) {
        netif->stats.tx_mcast_count++;
    }
    else {
        netif->stats.tx_unicast_count++;
    }
#endif
    res = dev->driver->send(dev, &iolist);

    gnrc_pktbuf_release(pkt);

    return res;
}

static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif)
{
    ethernet_hal_t *dev = netif->dev;
    int bytes_expected = dev->driver->recv(dev, NULL, 0);
    gnrc_pktsnip_t *pkt = NULL;

    if (bytes_expected > 0) {
        pkt = gnrc_pktbuf_add(NULL, NULL,
                              bytes_expected,
                              GNRC_NETTYPE_UNDEF);

        if (!pkt) {
            DEBUG("gnrc_netif_ethernet: cannot allocate pktsnip.\n");

            /* drop the packet */
            dev->driver->recv(dev, NULL, bytes_expected);

            goto out;
        }

        int nread = dev->driver->recv(dev, pkt->data, bytes_expected);
        if (nread <= 0) {
            DEBUG("gnrc_netif_ethernet: read error.\n");
            goto safe_out;
        }
#ifdef MODULE_NETSTATS_L2
        netif->stats.rx_count++;
        netif->stats.rx_bytes += nread;
#endif

        if (nread < bytes_expected) {
            /* we've got less than the expected packet size,
             * so free the unused space.*/

            DEBUG("gnrc_netif_ethernet: reallocating.\n");
            gnrc_pktbuf_realloc_data(pkt, nread);
        }

        DEBUG("gnrc_netif_ethernet: received packet from %s of length %d\n",
              gnrc_netif_addr_to_str(pkt->data, ETHERNET_ADDR_LEN, addr_str),
              nread);
#if defined(MODULE_OD) && ENABLE_DEBUG
        od_hex_dump(pkt->data, nread, OD_WIDTH_DEFAULT);
#endif
        /* mark ethernet header */
        gnrc_pktsnip_t *eth_hdr = gnrc_pktbuf_mark(pkt, sizeof(ethernet_hdr_t), GNRC_NETTYPE_UNDEF);
        if (!eth_hdr) {
            DEBUG("gnrc_netif_ethernet: no space left in packet buffer\n");
            goto safe_out;
        }

        ethernet_hdr_t *hdr = (ethernet_hdr_t *)eth_hdr->data;

#ifdef MODULE_L2FILTER
        if (!l2filter_pass(dev->filter, hdr->src, ETHERNET_ADDR_LEN)) {
            DEBUG("gnrc_netif_ethernet: incoming packet filtered by l2filter\n");
            goto safe_out;
        }
#endif

        /* set payload type from ethertype */
        pkt->type = gnrc_nettype_from_ethertype(byteorder_ntohs(hdr->type));

        /* create netif header */
        gnrc_pktsnip_t *netif_hdr;
        netif_hdr = gnrc_pktbuf_add(NULL, NULL,
                                    sizeof(gnrc_netif_hdr_t) + (2 * ETHERNET_ADDR_LEN),
                                    GNRC_NETTYPE_NETIF);

        if (netif_hdr == NULL) {
            DEBUG("gnrc_netif_ethernet: no space left in packet buffer\n");
            pkt = eth_hdr;
            goto safe_out;
        }

        gnrc_netif_hdr_init(netif_hdr->data, ETHERNET_ADDR_LEN, ETHERNET_ADDR_LEN);
        gnrc_netif_hdr_set_src_addr(netif_hdr->data, hdr->src, ETHERNET_ADDR_LEN);
        gnrc_netif_hdr_set_dst_addr(netif_hdr->data, hdr->dst, ETHERNET_ADDR_LEN);
        gnrc_netif_hdr_set_netif(netif_hdr->data, netif);

        gnrc_pktbuf_remove_snip(pkt, eth_hdr);
        LL_APPEND(pkt, netif_hdr);
    }

out:
    return pkt;

safe_out:
    gnrc_pktbuf_release(pkt);
    return NULL;
}

/** @} */
