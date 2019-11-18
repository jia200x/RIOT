/*
 * Copyright (C) 2018 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * Implements @ref net_netif for @ref net_gnrc
 *
 * @file
 * @author  Martine Lenders <m.lenders@fu-berlin.de>
 */

#include <string.h>

#include "fmt.h"
#include "net/gnrc.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netif/internal.h"

#include "net/netif.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/* GNRC implementation of netif_recv.
 *
 * This function converts the `netif_pkt` representation
 * into a `gnrc_pkt` representation. Note that the
 * netbuf implementation of GNRC uses the gnrc_pktsnip_t
 * pointer as netbuf context, so we can get fetch the packet
 * from there.
 *
 * Also, we need to create the gnrc_netif_hdr to pass the
 * L2 metadata to the upper layers. And also remove the
 * L2 header.
 *
 * Note that if a PHY layer didn't call the netbuf allocation
 * function (e.g the PHY layer had a static framebuffer like
 * the ESP-WIFI implementation), it's possible to set the
 * allocation context (netif_pkt->ctx) to NULL.
 * So, it's possible to only allocate the MSDU
 * and not the full packet!! This would reduce the number
 * of memcpy in those cases.
 */
int netif_recv(netif_t *netif, netif_pkt_t *pkt)
{
    gnrc_pktsnip_t *gnrc_pkt = pkt->ctx;
    gnrc_netif_t *gnrc_netif = (gnrc_netif_t*) netif;
    netdev_t *dev = gnrc_netif->dev;

    gnrc_pktsnip_t *netif_snip = gnrc_netif_hdr_build(pkt->src_l2addr, pkt->src_l2addr_len, pkt->dst_l2addr, pkt->dst_l2addr_len);

    if (netif_snip == NULL) {
        DEBUG("_recv_ieee802154: no space left in packet buffer\n");
        gnrc_pktbuf_release(gnrc_pkt);
        return 0;
    }

    /* We can avoid the following logic if we know the PHY layer
     * didn't allocate the packet (see the description of this function).
     *
     * In that case, we would need to call
     * `gnrc_pktbuf_add(pkt->msdu.iol_base, pkt->msdu.iol_len, GNRC_NETTYPE_UNDEF)`
     * instead of removing the snip and reallocating data.
     */
    gnrc_pktsnip_t *ieee802154_hdr = gnrc_pktbuf_mark(gnrc_pkt, ((uint8_t*) pkt->msdu.iol_base) - ((uint8_t*) gnrc_pkt->data), GNRC_NETTYPE_UNDEF);

    gnrc_pktbuf_remove_snip(gnrc_pkt, ieee802154_hdr);
    gnrc_pktbuf_realloc_data(gnrc_pkt, pkt->msdu.iol_len);
    gnrc_netif_hdr_t *hdr = netif_snip->data;
    hdr->lqi = pkt->lqi;
    hdr->rssi = pkt->rssi;
    gnrc_netif_hdr_set_netif(hdr, (gnrc_netif_t*) netif);
    dev->driver->get(dev, NETOPT_PROTO, &gnrc_pkt->type, sizeof(gnrc_pkt->type));

    hdr->flags |= pkt->flags;

    LL_APPEND(gnrc_pkt, netif_snip);

    if (!gnrc_netapi_dispatch_receive(gnrc_pkt->type, GNRC_NETREG_DEMUX_CTX_ALL, gnrc_pkt))
    {
        return 0;
    }

    return 1;
}

int netif_get_name(netif_t *iface, char *name)
{
    gnrc_netif_t *netif = (gnrc_netif_t*) iface;

    int res = 0;
    res += fmt_str(name, "if");
    res += fmt_u16_dec(&name[res], netif->pid);
    name[res] = '\0';
    return res;
}

int netif_get_opt(netif_t *netif, netopt_t opt, uint16_t context,
                  void *value, size_t max_len)
{
    gnrc_netif_t *iface = (gnrc_netif_t*) netif;
    return gnrc_netapi_get(iface->pid, opt, context, value, max_len);
}

int netif_set_opt(netif_t *netif, netopt_t opt, uint16_t context,
                  void *value, size_t value_len)
{
    gnrc_netif_t *iface = (gnrc_netif_t*) netif;
    return gnrc_netapi_set(iface->pid, opt, context, value, value_len);
}

/** @} */
