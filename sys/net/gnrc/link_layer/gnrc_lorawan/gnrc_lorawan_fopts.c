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

void gnrc_lorawan_process_fopts(gnrc_netif_t *netif, gnrc_pktsnip_t *fopts)
{
    if (fopts == NULL) {
        return;
    }
    
    uint8_t index = 0;
    uint8_t *data = fopts->data;

    /* TODO: Better boundary check */
    while(index < fopts->size) {
        switch(data[index++]) {
            case 0x02:
                if(!(gnrc_lorawan_get_pending_fopt(netif, 0x02) > 0)) {
                    puts("Received unexpected LinkCheckAns. Stop processing");
                    return;
                }
                printf("Modulation margin: %idb\n", data[index++]);
                printf("Number of gateways: %i\n", data[index++]);
                gnrc_lorawan_set_pending_fopt(netif, 0x02, false);
                break;
            default:
                /* Unrecognized option. Stop processing */
                return;
        }
    }
}

uint8_t gnrc_lorawan_build_options(gnrc_netif_t *netif, fopt_buffer_t *buf)
{
    size_t size = 0;
    size += _mlme_link_check_req(netif, buf);
    return size;
}
