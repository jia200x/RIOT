#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/gnrc/lorawan/lorawan.h"

int gnrc_lorawan_fopts_mlme_link_check_req(gnrc_netif_t *netif, lorawan_buffer_t *buf)
{
    (void) netif;
    if(buf) {
        assert(buf->index + 1 <= buf->size);
        buf->data[buf->index++] = 0x02;
    }

    return 1;
}

static int _mlme_link_check_ans(gnrc_netif_t *netif, lorawan_buffer_t *fopt)
{
    (void) netif;
    if(fopt) {
        netif->lorawan.last_margin = fopt->data[fopt->index++];
        netif->lorawan.last_num_gateways = fopt->data[fopt->index++];
        printf("Modulation margin: %idb\n", netif->lorawan.last_margin);
        printf("Number of gateways: %i\n", netif->lorawan.last_num_gateways);
    }

    /* Return read bytes */
    return 2;
}

int gnrc_lorawan_perform_fopt(uint8_t cid, gnrc_netif_t *netif, lorawan_buffer_t *fopt)
{
    int ret = -EINVAL;

    switch(cid) {
        case 0x02:
            ret = _mlme_link_check_ans(netif, fopt);
            break;
        default:
            puts("Undefined option");
            break;
    }

    return ret;
}

int gnrc_lorawan_fopt_read_cid(lorawan_buffer_t *fopt, uint8_t *cid)
{
    if(fopt->index >= fopt->size) {
        return -EINVAL;
    }

    *cid = fopt->data[fopt->index++];
    return gnrc_lorawan_perform_fopt(*cid, NULL, NULL);
}

void gnrc_lorawan_process_fopts(gnrc_netif_t *netif, uint8_t *fopts, size_t size)
{
    if (!fopts || !size) {
        puts("No options");
        return;
    }

    lorawan_buffer_t buf;
    gnrc_lorawan_buffer_reset(&buf, fopts, size);

    uint8_t cid;
    while(gnrc_lorawan_fopt_read_cid(&buf, &cid) >= 0) {
        if(!(gnrc_lorawan_get_pending_fopt(netif, cid) > 0)) {
            puts("Received unexpected FOpt. Stop processing ");
            break;
        }

        gnrc_lorawan_set_pending_fopt(netif, cid, false);
        if (gnrc_lorawan_perform_fopt(cid, netif, &buf) < 0) {
            break;
        }

    }
}

uint8_t gnrc_lorawan_build_options(gnrc_netif_t *netif, lorawan_buffer_t *buf)
{
    size_t size = 0;
    size += gnrc_lorawan_get_pending_fopt(netif, 0x02) > 0 ? gnrc_lorawan_fopts_mlme_link_check_req(netif, buf) : 0;
    return size;
}
