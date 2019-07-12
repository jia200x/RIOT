#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/lora.h"
#include "net/gnrc/lorawan.h"
#include "net/gnrc/lorawan/region.h"
#include "net/gnrc/netif/lorawan_base.h"
#include "errno.h"
#include "net/gnrc/pktbuf.h"

#include "net/lorawan/hdr.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

uint32_t gnrc_lorawan_random_get(gnrc_lorawan_t *mac) {
    netdev_t *dev = _netif_from_lw_mac(mac)->dev;
    uint32_t random_number;
    dev->driver->get(dev, NETOPT_RANDOM, &random_number, sizeof(random_number));
    return random_number;
}
