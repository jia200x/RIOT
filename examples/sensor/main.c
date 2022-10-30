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
#include <math.h>

#include "shell.h"
#include "msg.h"
#include "event.h"
#include "event/thread.h"
#include "ztimer.h"
#include "gcoap_example.h"
#include "net/cord/common.h"
#include "net/cord/ep_standalone.h"
#include "opendsme/opendsme.h"

#define NODE_INFO  "SOME NODE INFORMATION"
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static int _notify_cmd(int argc, char **argv);
static uint8_t req;
/*
N         1         2         3         4
D                                        
1  0.019627  0.019627  0.019627  0.019627
2  0.199279  0.253955  0.261766  0.261766
3  0.386741  0.621070  0.699179  0.714801
4  0.511716  0.925696  0.995995  0.995995
*/

#if 0
static double lookup_table[4][4] = {
    {0.019627,0.019627,0.019627,0.019627},
    {0.199279,0.253955,0.261766,0.261766},
    {0.386741,0.621070,0.699179,0.714801},
    {0.511716,0.925696,0.995995,0.995995},
};
#endif

static int T = 30;

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

    alloc.num_slots = ((uint16_t) scn_u32_dec(argv[7],1));

    gnrc_netapi_set(iface, NETOPT_GTS_ALLOC, 0, &alloc, sizeof(alloc));

    return 0;
}
#endif
static int _set_t_cmd(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    T = scn_u32_dec(argv[1],2);
    printf("INFO;T;%i\n", T);
    return 0;
}

static const shell_command_t shell_commands[] = {
    //{ "coap", "CoAP example", gcoap_cli_cmd },
    { "notify", "Notify observers", _notify_cmd },
    { "set_t", "Set average TX interval", _set_t_cmd },
#if IS_ACTIVE(CONFIG_IEEE802154_DSME_STATIC_GTS)
    { "gts", "GTS", _opendsme_gts_cmd },
#endif
    { NULL, NULL, NULL }
};

/* we will use a custom event handler for dumping cord_ep events */
static void _on_ep_event(cord_ep_standalone_event_t event)
{
    switch (event) {
        case CORD_EP_REGISTERED:
            puts("RD endpoint event: now registered with a RD");
            break;
        case CORD_EP_DEREGISTERED:
            puts("RD endpoint event: dropped client registration");
            break;
        case CORD_EP_UPDATED:
            puts("RD endpoint event: successfully updated client registration");
            break;
    }
}

/* define some dummy CoAP resources */
static ssize_t _handler_dummy(coap_pkt_t *pdu,
                              uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;

    /* get random data */
    int16_t val = 23;

    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
    size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);
    resp_len += fmt_s16_dec((char *)pdu->payload, val);
    return resp_len;
}

static ssize_t _handler_notify(coap_pkt_t *pdu,
                              uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;

    char uri[CONFIG_NANOCOAP_URI_MAX];
    /* get random data */
    int16_t val = 23;

    if (coap_get_uri_query(pdu, (uint8_t *)uri) <= 0) {
        return coap_reply_simple(pdu, COAP_CODE_INTERNAL_SERVER_ERROR, buf,
                                 len, COAP_FORMAT_TEXT, NULL, 0);
    }
    char *eq = strchr(uri, '=');
    eq++;
    int deadline = scn_u32_dec(eq,2);
    int slots = -1;
    if (deadline == 5 && T == 5) {
        slots = 3;
    }
    else if (deadline == 15 && T == 5) {
        slots = 2;
    }
    else if (deadline == 5 && T == 15) {
        slots = 2;
    }
    else if (deadline == 15 && T == 15) {
        slots = 1;
    }
    assert(slots >= 0);
#if 0
    float seconds_in_msf = ((float) 3.84
            * (1 << (CONFIG_IEEE802154_DSME_MULTISUPERFRAME_ORDER
                    - CONFIG_IEEE802154_DSME_SUPERFRAME_ORDER)));

    int n = floor(deadline / seconds_in_msf);
    printf("%f\n", seconds_in_msf);
    if (n > 4) {
        n = 4;
    }
    n=n-1;
    /* Lookup table */
    int slots = -1;
    for (int i=0;i<3;i++) {
        float min_T = seconds_in_msf / lookup_table[n][i];
        if (min_T <= T) {
            slots = (i+1);
            break;
        }
    }
#endif
    //printf("n(lookup): %i\n", n);
    printf("Slots: %i\n", slots);

    ieee802154_dsme_alloc_t alloc;
    memset(&alloc, 0, sizeof(alloc));
    /* HARDCODED */
    kernel_pid_t iface = 9;
    l2util_addr_from_str("AF:DA", (uint8_t*) &alloc.addr);

    /* Whether slot is TX or RX */ 
    alloc.tx = 1;

    /* Set the superframe ID of the current slot. Valid superframe IDs are
     * ({0..n-1}, with `n` the number of superframes per multisuperframe) */
    alloc.superframe_id = 0;

    /* Set the slot ID. Valid slot IDs are:
     * {0..7} if superframe_id == 0,
     * {8..14} if (superframe_id != 0 && CONFIG_IEEE802154_DSME_CAP_REDUCTION=1)
     * {0..14} otherwise
     */
    alloc.slot_id = 0;

    /* Set the channel offset. Valid values for O-QPSK are {0..15} */
    alloc.channel_id = 0;

    alloc.num_slots = slots;
    printf("Required slots;%i\n", slots);

    gnrc_netapi_set(iface, NETOPT_GTS_ALLOC, 0, &alloc, sizeof(alloc));

    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
    size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);
    resp_len += fmt_s16_dec((char *)pdu->payload, val);
    return resp_len;
}

static ssize_t _handler_put(coap_pkt_t *pdu,
                              uint8_t *buf, size_t len, void *ctx)
{
    (void) ctx;
    if (pdu->payload_len <= 5) {
        char payload[6] = { 0 };
        memcpy(payload, (char *)pdu->payload, pdu->payload_len);
        //req_count = (uint16_t)strtoul(payload, NULL, 10);
        printf("PUT;RECV;");
        for (unsigned i=0;i<pdu->payload_len;i++) {
            printf("%02x ", payload[i]);
        }
        printf("\n");
        return gcoap_response(pdu, buf, len, COAP_CODE_CHANGED);
    }
    return gcoap_response(pdu, buf, len, COAP_CODE_BAD_REQUEST);
}

static ssize_t _handler_info(coap_pkt_t *pdu,
                             uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;

    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
    size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);
    size_t slen = sizeof(NODE_INFO);
    memcpy(pdu->payload, NODE_INFO, slen);
    return resp_len + slen;
}

static const coap_resource_t _resources[] = {
    { "/put", COAP_PUT, _handler_put, NULL },
    { "/node/info",  COAP_GET, _handler_info, NULL },
    { "/sense/hum",  COAP_MATCH_SUBTREE | COAP_GET, _handler_notify, NULL },
    { "/sense/temp", COAP_GET, _handler_dummy, NULL },
};

static gcoap_listener_t _listener = {
    .resources     = (coap_resource_t *)&_resources[0],
    .resources_len = ARRAY_SIZE(_resources),
    .next          = NULL
};

static int _notify_cmd(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    size_t len;
    uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;

    /* send Observe notification for /cli/stats */
    switch (gcoap_obs_init(&pdu, &buf[0], CONFIG_GCOAP_PDU_BUF_SIZE,
            &_resources[2])) {
    case GCOAP_OBS_INIT_OK:
        printf("NOTIFY;%i\n", req);
        coap_opt_add_format(&pdu, COAP_FORMAT_TEXT);
        len = coap_opt_finish(&pdu, COAP_OPT_FINISH_PAYLOAD);
        len += fmt_u16_dec((char *)pdu.payload, req++);
        gcoap_obs_send(&buf[0], len, &_resources[2]);
        break;
    case GCOAP_OBS_INIT_UNUSED:
        printf("gcoap_cli: no observer for /cli/stats\n");
        break;
    case GCOAP_OBS_INIT_ERR:
        printf("gcoap_cli: error initializing /cli/stats notification\n");
        break;
    }
    return 0;
}



int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");
    //server_init();
    gcoap_register_listener(&_listener);

    /* register event callback with cord_ep_standalone */
    cord_ep_standalone_reg_cb(_on_ep_event);

    puts("Client information:");
    printf("  ep: %s\n", cord_common_get_ep());
    printf("  lt: %is\n", (int)CONFIG_CORD_LT);

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    //event_post(EVENT_PRIO_LOWEST, &ev);
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
