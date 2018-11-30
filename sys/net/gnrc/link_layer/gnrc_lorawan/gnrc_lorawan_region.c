#include "net/gnrc/lorawan/region.h"

/* TODO: Implement in struct */
static uint32_t lorawan_channels[16] = {
    868100000,
    868300000,
    868500000,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

/*TODO: Implement properly... */
static const uint8_t rx1_dr_offset[8][6] = {
    {0,0,0,0,0,0},
    {1,0,0,0,0,0},
    {2,1,0,0,0,0},
    {3,2,1,0,0,0},
    {4,3,2,1,0,0},
    {5,4,3,2,1,0},
    {6,5,4,3,2,1},
    {7,6,5,4,3,2},
};

uint8_t gnrc_lorawan_rx1_get_dr_offset(uint8_t dr_up, uint8_t dr_offset) {
    return rx1_dr_offset[dr_up][dr_offset];
}

uint32_t gnrc_lorawan_pick_channel(gnrc_netif_t *netif) {
    /* TODO: Implement when add/remove channel gets implemented */
    netdev_t *netdev = netif->dev;
    uint32_t random_number;
    netdev->driver->get(netdev, NETOPT_RANDOM, &random_number, sizeof(random_number));
    
    return lorawan_channels[random_number % 3];
}

void gnrc_lorawan_process_cflist(gnrc_netif_t *netif, uint8_t *cflist) {
    (void) netif;
    /* TODO: Check CFListType to 0 */
    for(int i=3;i<8;i++) {
        lorawan_channels[i] = (cflist[2] << 16 | cflist[1] << 8 | cflist [0]) *100;
        cflist += 3;
    }
}
