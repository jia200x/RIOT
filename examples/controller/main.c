/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example application for demonstrating the RIOT network stack
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "fmt.h"
#include "shell.h"
#include "msg.h"
#include "opendsme/opendsme.h"

#define MAIN_QUEUE_SIZE     (8)

#if IS_ACTIVE(CONFIG_IEEE802154_DSME_STATIC_GTS)
static int _opendsme_gts_cmd(int argc, char **argv)
{
    (void) argc;
    ieee802154_dsme_alloc_t alloc;
    if (argc < 8) {
        printf("Usage: %s <iface> <target_addr> <tx> <superframe_id> <slot_id> <channel_id> <num_slots>\n",argv[0]);
        return 1;
    }
    memset(&alloc, 0, sizeof(alloc));
    kernel_pid_t iface = scn_u32_dec(argv[1],1);
    l2util_addr_from_str(argv[2], (uint8_t*) &alloc.addr);

    /* Whether slot is TX or RX */ 
    alloc.tx = scn_u32_dec(argv[3],1);

    /* Set the superframe ID of the current slot. Valid superframe IDs are
     * ({0..n-1}, with `n` the number of superframes per multisuperframe) */
    alloc.superframe_id = scn_u32_dec(argv[4],1);

    /* Set the slot ID. Valid slot IDs are:
     * {0..7} if superframe_id == 0,
     * {8..14} if (superframe_id != 0 && CONFIG_IEEE802154_DSME_CAP_REDUCTION=1)
     * {0..14} otherwise
     */
    alloc.slot_id = scn_u32_dec(argv[5],1);

    /* Set the channel offset. Valid values for O-QPSK are {0..15} */
    alloc.channel_id = scn_u32_dec(argv[6], 1);

    alloc.num_slots = scn_u32_dec(argv[7],1);

    gnrc_netapi_set(iface, NETOPT_GTS_ALLOC, 0, &alloc, sizeof(alloc));

    return 0;
}
#endif

static const shell_command_t shell_commands[] = {
#if IS_ACTIVE(CONFIG_IEEE802154_DSME_STATIC_GTS)
    { "gts", "GTS", _opendsme_gts_cmd },
#endif
    { NULL, NULL, NULL }
};
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
