#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/gnrc/lorawan/lorawan.h"

static uint8_t _mlme_link_check_req(gnrc_netif_t *netif, fopt_buffer_t *buf)
{
    uint8_t opt = netif->lorawan.fopts[0] & (1<<2);

    if (!opt) {
        return 0;
    }

    if(buf) {
        assert(buf->index + 1 <= buf->size);
        buf->data[buf->index++] = 0x02;
    }

    return 1;
}

int gnrc_lorawan_perform_fopt(gnrc_netif_t *netif, fopt_buffer_t *fopt)
{
    int err = -EINVAL;
    if(fopt->index >= fopt->size) {
        goto end;
    }

    /* TODO: Better boundary check */
    switch(fopt->data[fopt->index++]) {
        case 0x02:
            if(!(gnrc_lorawan_get_pending_fopt(netif, 0x02) > 0)) {
                puts("Received unexpected LinkCheckAns. Stop processing");
                break;
            }
            printf("Modulation margin: %idb\n", fopt->data[fopt->index++]);
            printf("Number of gateways: %i\n", fopt->data[fopt->index++]);
            gnrc_lorawan_set_pending_fopt(netif, 0x02, false);
            err = 0;
            break;
        default:
            puts("Undefined option");
            break;
    }

end:
    return err;

}

void gnrc_lorawan_process_fopts(gnrc_netif_t *netif, gnrc_pktsnip_t *fopts)
{
    if (fopts == NULL || fopts->data == NULL) {
        puts("No options");
        return;
    }

    fopt_buffer_t buf = {
        .data = fopts->data,
        .size = fopts->size,
        .index = 0
    };

    while(gnrc_lorawan_perform_fopt(netif, &buf) == 0) {}
}

uint8_t gnrc_lorawan_build_options(gnrc_netif_t *netif, fopt_buffer_t *buf)
{
    size_t size = 0;
    size += _mlme_link_check_req(netif, buf);
    return size;
}
