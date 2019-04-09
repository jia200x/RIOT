/*
 * Copyright (C) 2017 Hamburg University of Applied Sciences
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       OpenWSN test application
 *
 * @author      Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 *
 * @}
 */

#include <stdio.h>

#include "od.h"
#include "shell.h"
#include "openwsn.h"
#include "net/ipv6/addr.h"

#include "opendefs.h"
#include "errno.h"
#include "02a-MAClow/IEEE802154E.h"
#include "02b-MAChigh/sixtop.h"
#include "02b-MAChigh/schedule.h"
#include "03b-IPv6/icmpv6rpl.h"
#include "04-TRAN/openudp.h"
#include "inc/opendefs.h"
#include "cross-layers/openqueue.h"
#include "cross-layers/idmanager.h"
#include "cross-layers/packetfunctions.h"

static int txtsend(int argc, char **argv)
{
    uint8_t addr[8];
    if(argc > 2) {
        fmt_hex_bytes(addr, argv[1]);
        openwsn_mcps_data_request(addr, argv[2], strlen(argv[2]));
    }
    else {
        openwsn_mcps_data_request(NULL, argv[1], strlen(argv[1]));
    }

    return 0;
}

static int slotframe_cmd(int argc, char **argv)
{
    openwsn_mlme_set_slotframe_request(atoi(argv[1]));
    return 0;
}

int pa(int argc, char **argv)
{
    open_addr_t *addr = idmanager_getMyID(ADDR_64B);

    for(unsigned i=0;i<8;i++) {
        printf("%02x", addr->addr_64b[i]);
    }
    printf("\n");
}

void _manage_cell_usage(char *cmd) {
    printf("Usage: %s <add|rem> <slot_offset> <channel_offset> <adv|tx|rx>" \
            "[neighbour_address]\n", cmd);
}
void manage_cell(int argc, char **argv)
{
    uint8_t addr[8];
    int res;
    if(argc < 5) {
        _manage_cell_usage(argv[0]);
        return;
    }

    bool add = strcmp(argv[1], "add") == 0 ? true : false;
    ow_cell_t type;
    if(strcmp(argv[4], "adv") == 0) {
        type = OW_CELL_ADV;
    }
    else if(strcmp(argv[4], "tx") == 0) {
        type = OW_CELL_TX;
    }
    else if(strcmp(argv[4], "rx") == 0) {
        type = OW_CELL_RX;
    }
    else {
        _manage_cell_usage(argv[0]);
        return;
    }

    if (argc > 5) {
        if(type == OW_CELL_ADV) {
            _manage_cell_usage(argv[0]);
            return;
        }
        fmt_hex_bytes(addr, argv[5]);
        res = openwsn_mlme_set_link_request(add, atoi(argv[2]), type, TRUE, atoi(argv[3]),
            addr);
    }
    else {
        res = openwsn_mlme_set_link_request(add, atoi(argv[2]), type, TRUE, atoi(argv[3]),
                NULL);
    }

    if(res == 0) {
        puts("Successfully set link");
    }
    else {
        puts("Something went wrong (duplicate link?)");
    }
}

void role_cmd(int argc, char **argv)
{
    char *role = argv[1];
    if(strcmp(role, "pancoord") == 0) {
        openwsn_mlme_set_role(OW_ROLE_PAN_COORDINATOR);
    } else if(strcmp(role, "coord") == 0) {
        openwsn_mlme_set_role(OW_ROLE_COORDINATOR);
    }
    else if(strcmp(role, "leaf") == 0){
        openwsn_mlme_set_role(OW_ROLE_LEAF);
    }
    else {
        printf("Usage: %s <pancoord|coord|leaf>\n", argv[0]);
    }
    puts("OK");
}

/* Callback from MAC layer */
void mlme_sync_indication(void)
{
    puts("Synchronized!");
}

/* Callback from MAC layer */
void mlme_sync_loss_indication(void)
{
    puts("Sync loss");
}

/* Callback from MAC layer */
void ow_mcps_data_confirm(int status)
{
    if(status == 0) {
        puts("Successfully sent data");
    }
    else if (status == -ENOBUFS) {
        puts("Not enough buffer space");
    }
    else {
        puts("Error while sending packet");
    }
}

/* Callback from MAC layer */
void ow_mcps_data_indication(char *data, size_t data_len)
{
    printf("Received message: ");
    for(unsigned i=0;i<data_len;i++) {
        printf("%c", data[i]);
    }
    puts("");
}

static const shell_command_t shell_commands[] = {
    { "slotframe", "Set slotframe length", slotframe_cmd },
    { "txtsnd", "Set node as RPL DODAG root node", txtsend },
    { "cell", "Manage cells", manage_cell},
    {"pa", "Print address", pa},
    {"role", "Set role of node", role_cmd},
    { NULL, NULL, NULL }
};

int main(void)
{
    puts("OpenWSN MAC test");

    printf("You are running RIOT on a(n) %s board.\n", RIOT_BOARD);
    printf("This board features a(n) %s MCU.\n", RIOT_MCU);

    openwsn_bootstrap();

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
}
