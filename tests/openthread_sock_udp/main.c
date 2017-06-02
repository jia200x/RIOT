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

static void _recv(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
	puts("It works!");
}

int main(void)
{
	ot_udp_context_t ctx;

	ctx.type = OPENTHREAD_NET_SOCKET_CREATE;
	ctx.cb = _recv;
	ctx.port = 1313;

	ot_call_command("udp", &ctx, NULL);

	while(true) {
		/* Nothing to do here */
	}

    return 0;
}
