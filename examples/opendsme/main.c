/*
 * Copyright (C) 2021 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief       OpenDSME example
 *
 * @author      José I. Álamos <jose.alamos@haw-hamburg.de>
 */

#include <stdio.h>
#include <string.h>
#include "opendsme/opendsme.h"

#include "net/gnrc.h"
#include "net/gnrc/pktdump.h"
#include "net/gnrc/netreg.h"

#include "shell.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#if IS_ACTIVE(CONFIG_IEEE802154_DSME_STATIC_GTS)
static int gts_cmd(int argc, char **argv)
{
    (void) argc;
    ieee802154_dsme_alloc_t alloc;
    if (argc < 7) {
        printf("Usage: %s <iface> <target_addr> <tx> <superframe_id> <slot_id> <channel_id>",argv[0]);
        return 1;
    }
    memset(&alloc, 0, sizeof(alloc));
    kernel_pid_t iface = scn_u32_dec(argv[1],1);
    l2util_addr_from_str(argv[2], (uint8_t*) &alloc.addr);
    alloc.tx = scn_u32_dec(argv[3],1);
    alloc.superframe_id = scn_u32_dec(argv[4],1);
    alloc.slot_id = scn_u32_dec(argv[5],1);
    alloc.channel_id = scn_u32_dec(argv[6], 1);
    gnrc_netapi_set(iface, NETOPT_GTS_ALLOC, 0, &alloc, sizeof(alloc));

    return 0;
}
#endif

static const shell_command_t shell_commands[] = {
#if IS_ACTIVE(CONFIG_IEEE802154_DSME_STATIC_GTS)
    { "gts", "add gts slot", gts_cmd },
#endif
    { NULL, NULL, NULL }
};

int main(void)
{
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    gnrc_netreg_entry_t dump = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                          gnrc_pktdump_pid);
    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &dump);

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
