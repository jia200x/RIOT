#include "net/gnrc/lorawan/region.h"

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

/*TODO: Reimplement */
static size_t _get_num_used_channels(gnrc_netif_t *netif) {
    size_t count = 0;
    for(int i=0;i<16;i++) {
        if(netif->lorawan.channel[i])
            count++;
    }
    return count;
}

/*TODO: Reimplement */
static uint32_t _get_nth_channel(gnrc_netif_t *netif, size_t n) {
    int i=0;
    uint32_t channel = 0;
    while(n) {
        if(netif->lorawan.channel[i]) {
            n--;
            channel = netif->lorawan.channel[i];
            i++;
        }
    }
    return channel;
}

void gnrc_lorawan_channels_init(gnrc_netif_t *netif)
{
    netif->lorawan.channel[0] = GNRC_LORAWAN_LC_1;
    netif->lorawan.channel[1] = GNRC_LORAWAN_LC_2;
    netif->lorawan.channel[2] = GNRC_LORAWAN_LC_3;

    for(unsigned i=GNRC_LORAWAN_DEFAULT_CHANNELS;i<16;i++) {
        netif->lorawan.channel[i] = 0;
    }
}

uint32_t gnrc_lorawan_pick_channel(gnrc_netif_t *netif) {
    /* TODO: Implement when add/remove channel gets implemented */
    netdev_t *netdev = netif->dev;
    uint32_t random_number;
    netdev->driver->get(netdev, NETOPT_RANDOM, &random_number, sizeof(random_number));
    
    return _get_nth_channel(netif, 1+(random_number % _get_num_used_channels(netif)));
}

/*TODO: Reimplement! */
void gnrc_lorawan_process_cflist(gnrc_netif_t *netif, uint8_t *cflist)
{
    /* TODO: Check CFListType to 0 */
    for(int i=3;i<8;i++) {
        netif->lorawan.channel[i] = (cflist[2] << 16 | cflist[1] << 8 | cflist [0]) *100;
        cflist += 3;
    }
}
