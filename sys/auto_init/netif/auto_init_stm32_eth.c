/**
 * @ingroup     sys_auto_init_gnrc_netif
 * @{
 *
 * @brief       Auto initzialize stm32 ethernet driver
 *
 * @author      Robin Lösch <robin@chilio.net>
 */

#ifdef MODULE_STM32_ETH

#include <stdio.h>
#include "stm32_eth.h"
#include "net/gnrc/netif/ethernet.h"
#include "net/gnrc/netif/internal.h"
#include "net/netdev.h"

static netdev_t stm32eth;
static char stack[THREAD_STACKSIZE_DEFAULT];
static gnrc_netif_t *netif;
int stm32_eth_init(void);

void stm32_eth_task_handler(void);

static void _th(void *arg)
{
    (void) arg;
    stm32_eth_task_handler();
}

static const gnrc_netif_task_handler_t stm32_eth_th = {
    .th = _th,
};

void stm32_eth_isr(void)
{
    msg_t msg = { .type = NETDEV_MSG_TYPE_EVENT,
                  .content = { .ptr = (gnrc_netif_task_handler_t*) &stm32_eth_th } };

    if (msg_send(&msg, netif->pid) <= 0) {
        puts("gnrc_netif: possibly lost interrupt.");
    }
}

void stm32_eth_rx_complete(void)
{
    stm32eth.event_callback(&stm32eth, NETDEV_EVENT_RX_COMPLETE);
}

void auto_init_stm32_eth(void)
{
  /* setup netdev device */
  stm32_eth_netdev_setup(&stm32eth);

  if(stm32_eth_init() < 0) {
      /* Don't even start the thread if this doesn't init... */
      return;
  }

  /* initialize netdev <-> gnrc adapter state */
  netif = gnrc_netif_ethernet_create(stack, THREAD_STACKSIZE_DEFAULT, GNRC_NETIF_PRIO, "stm32_eth",
                             &stm32eth);
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_STM32_ETH */
/** @} */
