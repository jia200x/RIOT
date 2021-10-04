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

#include "shell.h"
#include "shell_commands.h"

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

static const shell_command_t shell_commands[] = {
    { "status", "check whether the node is associated or not", status_cmd },
    { "start", "start OpenDSME", start_cmd },
    { NULL, NULL, NULL }
};

int main(void)
{
    printf("\n************ RIOT and OpenDSME ***********\n");
    printf("\n");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
