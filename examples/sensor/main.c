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

#include "shell.h"
#include "msg.h"
#include "event.h"
#include "event/thread.h"
#include "ztimer.h"
#include "gcoap_example.h"
#include "net/cord/common.h"
#include "net/cord/ep_standalone.h"

#define NODE_INFO  "SOME NODE INFORMATION"
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

static const shell_command_t shell_commands[] = {
    { "coap", "CoAP example", gcoap_cli_cmd },
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
    { "/node/info",  COAP_GET, _handler_info, NULL },
    { "/sense/hum",  COAP_GET, _handler_dummy, NULL },
    { "/sense/temp", COAP_GET, _handler_dummy, NULL }
};

static gcoap_listener_t _listener = {
    .resources     = (coap_resource_t *)&_resources[0],
    .resources_len = ARRAY_SIZE(_resources),
    .next          = NULL
};

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
