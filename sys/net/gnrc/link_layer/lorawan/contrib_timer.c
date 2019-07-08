#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/lora.h"
#include "net/gnrc/lorawan.h"
#include "net/gnrc/lorawan/region.h"
#include "errno.h"
#include "net/gnrc/pktbuf.h"

#include "net/lorawan/hdr.h"

#include "random.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"
/* This factor is used for converting "real" seconds into microcontroller
 * microseconds. This is done in order to correct timer drift.
 */
#define _DRIFT_FACTOR (int) (MS_PER_SEC * 100 / (100 + CONFIG_GNRC_LORAWAN_TIMER_DRIFT))

void gnrc_lorawan_timer_set(gnrc_lorawan_t *mac, uint32_t ms)
{
    gnrc_netif_lorawan_t *netif = (gnrc_netif_lorawan_t*) mac;
    netif->msg.type = MSG_TYPE_TIMEOUT;
    xtimer_set_msg(&netif->rx, _DRIFT_FACTOR*ms, &netif->msg, thread_getpid());
}

void gnrc_lorawan_timer_stop(gnrc_lorawan_t *mac)
{
    gnrc_netif_lorawan_t *netif = (gnrc_netif_lorawan_t*) mac;
    xtimer_remove(&netif->rx);
}

void gnrc_lorawan_timer_usleep(gnrc_lorawan_t *mac, uint32_t us)
{
    (void) mac;
    xtimer_usleep(us);
}
