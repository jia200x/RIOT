/*
 * Copyright (C) 2017 Fundacion Inria Chile
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief       OpenThread test application
 *
 * @author      Jose Ignacio Alamos <jose.alamos@inria.cl>
 */

#include <stdio.h>

#include "net/ipv6/addr.h"
#include "openthread/ip6.h"
#include "openthread/thread.h"
#include "openthread/udp.h"
#include "ot.h"
#include "shell.h"
#include "shell_commands.h"

static ot_udp_context_t ctx;

static void _recv(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
	puts("Received");
	ot_pkt_info_t *recv_info = aContext;

    size_t payload_len = otMessageGetLength(aMessage) - otMessageGetOffset(aMessage);

    if (recv_info->data == NULL || payload_len > recv_info->max_len) {
        return;
    }

    otMessageRead(aMessage, otMessageGetOffset(aMessage), recv_info->data, payload_len);
	recv_info->len = payload_len;

	printf("Received data with len: %i\n ", recv_info->len);
	for(int i=0;i<payload_len;i++)
	{
		printf("%02x ", ((char*) recv_info->data)[i]);
	}
	puts(" ");
}

int ip_addr(int argc, char **argv)
{
	uint8_t num_addresses=0;	
	ot_call_command("ipaddr", NULL, &num_addresses);

	otNetifAddress current_addr;
	for(unsigned i = 0; i < num_addresses; i++) {
		ot_call_command("ipaddr", &i, &current_addr);
		char addrstr[IPV6_ADDR_MAX_STR_LEN];
		printf("inet6 %s\n", ipv6_addr_to_str(addrstr, (ipv6_addr_t *) &current_addr.mAddress.mFields, sizeof(addrstr)));
	}

	return 0;
}

static void _send(char *addr_str, char *port_str, char *data, size_t len)
{
    uint16_t port;
    ipv6_addr_t addr;

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, addr_str) == NULL) {
        puts("Error: unable to parse destination address");
        return;
    }
    /* parse port */
    port = atoi(port_str);
    if (port == 0) {
        puts("Error: unable to parse destination port");
        return;
    }

	ctx.ip_addr = addr;
	ctx.port = port;
	ctx.tx_buf = data;
	ctx.tx_len = len;

	ctx.type = OPENTHREAD_NET_SEND;

	ot_call_command("udp", &ctx, NULL);
}

int udp_cmd(int argc, char **argv)
{
    if (strcmp(argv[1], "send") == 0) {
        _send(argv[2], argv[3], argv[4], strlen(argv[4]));
    }
    else {
        puts("error: invalid command");
    }
    return 0;
}

static const shell_command_t shell_commands[] = {
    { "udp", "send data over UDP", udp_cmd },
    { "ipaddr", "print ip addresses", ip_addr },
    { NULL, NULL, NULL }
};

char rx_buf[100];
ot_pkt_info_t recv_info;
int main(void)
{

	ctx.type = OPENTHREAD_NET_SOCKET_CREATE;
	ctx.cb = _recv;
	ctx.port = 1313;
	recv_info.data = rx_buf;
	recv_info.max_len = sizeof(rx_buf);

	ctx.rx_ctx = &recv_info;
	ot_call_command("udp", &ctx, NULL);

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
