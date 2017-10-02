/*
 * Copyright (C) Inria Chile 2016
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
 * @brief       Test for raw IPv6 connections
 *
 * @author      Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * This test application tests the gnrc_conn_ip module. If you select protocol 58 you can also
 * test if gnrc is able to deal with multiple subscribers to ICMPv6 (gnrc_icmpv6 and this
 * application).
 *
 * @}
 */

#include <stdio.h>
#include "shell.h"

int main(void)
{
	/* start the shell */
     puts("Initialization successful - starting the shell now");
     char line_buf[SHELL_DEFAULT_BUFSIZE];
     shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

     puts("Start Loop");

}


