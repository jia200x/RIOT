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
#include "event/thread.h"
#include "luid.h"

#if IS_USED(MODULE_SX127X)
#include "sx127x.h"
#include "sx127x_params.h"
#endif

#include "net/gnrc.h"
#include "net/gnrc/netreg.h"

#include "shell.h"
#include "shell_commands.h"
#include "net/l2util.h"
#include "fmt.h"

static void _cb(uint16_t cmd, gnrc_pktsnip_t *pkt,
                                       void *ctx)
{
    (void) cmd;
    (void) pkt;
    (void) ctx;
    uint8_t addr[2];
    gnrc_netif_hdr_t *hdr = (gnrc_netif_hdr_t*) pkt->data;
    memcpy(addr, gnrc_netif_hdr_get_src_addr(hdr), 2);
    pkt = gnrc_pktbuf_remove_snip(pkt, pkt);
    /* first 2 bytes are the id */
    uint8_t *id = pkt->data;
    printf("[info];RECV;%02x:%02x;%02x;%02x%02x\n", addr[0], addr[1], pkt->size, id[0], id[1]);
    gnrc_pktbuf_release(pkt);
}

static gnrc_netreg_entry_cbd_t _cbd = {.cb = _cb};
gnrc_netreg_entry_t payload_dump = GNRC_NETREG_ENTRY_INIT_CB(GNRC_NETREG_DEMUX_CTX_ALL, &_cbd);

char str_addr[sizeof("00:00")];
char line_buf[SHELL_DEFAULT_BUFSIZE];
uint16_t node_id;
gnrc_pktsnip_t *pkt;

static uint16_t counter = 0;

extern void heap_stats(void);
event_queue_t *opendsme_get_evq(void);
gnrc_netif_t *opendsme_get_netif(void);
int gnrc_netif_dsme_create(gnrc_netif_t *netif, char *stack, int stacksize,
                                 char priority, const char *name, netdev_t *dev);


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

    printf("[info];ADDR;%s\n", str_addr);

    return 0;
}

static int start_cmd(int argc, char **argv)
{
    (void) argc;
    bool pan_coord;
    if (strcmp(argv[1], "pan_coord") == 0) {
        pan_coord = true;
    }
    else {
        pan_coord = false;
    }

    gnrc_netapi_set(opendsme_get_netif()->pid, NETOPT_PAN_COORD, 0, &pan_coord, sizeof(pan_coord));
    gnrc_netapi_set(opendsme_get_netif()->pid, NETOPT_LINK, 0, NULL, 0);

    return 0;
}

static int txtsnd_cmd(int argc, char **argv)
{
    (void) argc;
    if (strlen(argv[1]) > sizeof(str_addr)) {
        puts("[error];Addr to big");
        return -1;
    }
    uint8_t addr[2];
    l2util_addr_from_str(argv[1], (uint8_t*) &addr);
    pkt = gnrc_pktbuf_add(NULL, NULL, CONFIG_OPENDSME_PAYLOAD_LENGTH, GNRC_NETTYPE_UNDEF);
    if (pkt == NULL) {
        puts("[error];Not enough space");
    }
    uint8_t *p = pkt->data;
    p[0] = counter >> 8;
    p[1] = counter & 0xFF;
    counter++;
    gnrc_pktsnip_t *netif_hdr = gnrc_netif_hdr_build(NULL, 0, addr, 2);
    pkt = gnrc_pkt_prepend(pkt, netif_hdr);
    gnrc_netapi_send(opendsme_get_netif()->pid, pkt);
    return 0;
}

static int id_cmd(int argc, char **argv)
{
    (void) argc;
    node_id = (uint16_t) ((argv[1][0] << 8) + (argv[1][1]));
    return 0;
}

uint16_t get_node_id(void)
{
    return node_id;
}

#if !IS_ACTIVE(CONFIG_OPENDSME_USE_CAP) && IS_ACTIVE(CONFIG_DSME_PLATFORM_STATIC_GTS)
static int gts_cmd(int argc, char **argv)
{
    (void) argc;
    dsme_alloc_t alloc;
    memset(&alloc, 0, sizeof(alloc));
    l2util_addr_from_str(argv[1], (uint8_t*) &alloc.addr);
    alloc.tx = scn_u32_dec(argv[2],1);
    alloc.superframe_id = scn_u32_dec(argv[3],1);
    alloc.slot_id = scn_u32_dec(argv[4],1);
    alloc.channel_id = scn_u32_dec(argv[5], 1);
    gnrc_netapi_set(opendsme_get_netif()->pid, NETOPT_GTS_ALLOC, 0, &alloc, sizeof(alloc));

    return 0;
}
#endif

static const shell_command_t shell_commands[] = {
    { "status", "check whether the node is associated or not", status_cmd },
    { "start", "start OpenDSME", start_cmd },
    { "txtsnd", "transmit frame", txtsnd_cmd },
#if !IS_ACTIVE(CONFIG_OPENDSME_USE_CAP) && IS_ACTIVE(CONFIG_DSME_PLATFORM_STATIC_GTS)
    { "gts", "add gts slot", gts_cmd },
#endif
    { "id", "set node id", id_cmd },
    { NULL, NULL, NULL }
};

static char dsme_stack[2000];
int main(void)
{
    printf("\n************ RIOT and OpenDSME ***********\n");
    printf("\n");
    gnrc_netif_dsme_create(opendsme_get_netif(), dsme_stack, 2000, THREAD_PRIORITY_MAIN - 1,"dsme", NULL);
    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &payload_dump);

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
