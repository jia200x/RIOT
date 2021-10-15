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

#if IS_USED(MODULE_SX127X)
#include "sx127x.h"
#include "sx127x_params.h"
#endif

#include "net/gnrc/pktdump.h"
#include "net/gnrc.h"

#include "shell.h"
#include "shell_commands.h"
#include "net/l2util.h"

char str_addr[sizeof("00:00")];
char line_buf[SHELL_DEFAULT_BUFSIZE];

static int status_cmd(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    if (opendsme_is_associated()) {
        puts("Associated");
    }
    else {
        puts("Not associated");
    }

    network_uint16_t addr;
    opendsme_get_short_addr(&addr);
    l2util_addr_to_str((uint8_t*) &addr, sizeof(addr), str_addr);

    printf("%s\n", str_addr);

    return 0;
}

static int start_cmd(int argc, char **argv)
{
    (void) argc;
    bool pan_coord;
    if (strcmp(argv[1], "pan_coord") == 0) {
        puts("Starting as PAN coordinator");
        pan_coord = true;
    }
    else {
        puts("Starting as regular coordinator");
        pan_coord = false;
    }

    opendsme_init(pan_coord);
    return 0;
}

static int txtsnd_cmd(int argc, char **argv)
{
    (void) argc;
    network_uint16_t addr;
    l2util_addr_from_str(argv[1], (uint8_t*) &addr);
    gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, argv[2], strlen(argv[2]), GNRC_NETTYPE_UNDEF);
    opendsme_send_frame(&addr, 2, pkt);
    return 0;
}

static const shell_command_t shell_commands[] = {
    { "status", "check whether the node is associated or not", status_cmd },
    { "start", "start OpenDSME", start_cmd },
    { "txtsnd", "transmit frame", txtsnd_cmd },
    { NULL, NULL, NULL }
};

int main(void)
{
    printf("\n************ RIOT and OpenDSME ***********\n");
    printf("\n");

    gnrc_netreg_entry_t dump = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                          gnrc_pktdump_pid);
    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &dump);

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}

#if IS_USED(MODULE_SX127X)
/* HACK */
void sx127x_init_dsme(sx127x_t *dev, void *radio)
{
    sx127x_hal_setup(dev, radio);
    sx127x_setup(dev, sx127x_params, radio);
}
#endif
