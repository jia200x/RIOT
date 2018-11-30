#ifndef NET_GNRC_LORAWAN_REGION_H
#define NET_GNRC_LORAWAN_REGION_H

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LORAWAN_REGION_H */
