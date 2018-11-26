/*
 * Copyright (C) 2016 Unwired Devices <info@unwds.com>
 *               2017 Inria Chile
 *               2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 * @file
 * @brief       Test application for SX127X modem driver
 *
 * @author      Eugene P. <ep@unwds.com>
 * @author      José Ignacio Alamos <jose.alamos@inria.cl>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @}
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread.h"
#include "xtimer.h"
#include "shell.h"
#include "shell_commands.h"

#include "net/netdev.h"
#include "net/lora.h"
#include "random.h"

#include "board.h"

#include "sx127x_internal.h"
#include "sx127x_params.h"
#include "sx127x_netdev.h"
#include "net/gnrc/netapi.h"

#include "net/gnrc/pktbuf.h"

#define SX127X_LORA_MSG_QUEUE   (16U)
#define SX127X_STACKSIZE        (THREAD_STACKSIZE_DEFAULT)

#define MSG_TYPE_ISR                (0x3456)
#define MSG_TYPE_TIMEOUT            (0x3457)

#define PKT_WRITE_BYTE(CURSOR, BYTE) *(CURSOR++) = BYTE

#define PKT_WRITE(CURSOR, SRC, LEN) do {\
    memcpy(CURSOR, SRC, LEN); \
    CURSOR += LEN; \
} while (0);


//static char stack[SX127X_STACKSIZE];
//static kernel_pid_t _recv_pid;

//static uint8_t message[32];
static sx127x_t sx127x;

/*Little endian! */
uint8_t  appeui[8] = { 0x2E, 0x91, 0x00, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
uint8_t  deveui[8] = { 0x17, 0xFF, 0x06, 0xDA, 0x7E, 0xD5, 0xB3, 0x70 };
uint8_t  appkey[16] = { 0x16, 0xC4, 0x79, 0x33, 0xD6, 0x1F, 0x17, 0x22, 0x43, 0x7C, 0xF9, 0x99, 0x2D, 0xAB, 0x37, 0x3F };

int8_t nwkskey[16] = { 0xC8, 0x81, 0x26, 0x2B, 0xC8, 0x7A, 0xD7, 0x91, 0x75, 0xB2, 0x0F, 0xDA, 0xB6, 0x83, 0x07, 0xEB };
uint8_t appskey[16] = { 0xAA, 0x35, 0xDE, 0x79, 0xCC, 0x6A, 0x66, 0x98, 0xC8, 0x65, 0x32, 0x0F, 0x7A, 0x76, 0xF1, 0xEA };

uint8_t dev_nonce[2];
uint8_t dev_addr[4] = { 0x06, 0x1A, 0x01, 0x26 };

uint16_t fcnt = 0;


extern uint32_t calculate_mic(uint8_t *buf, size_t size, uint8_t *appkey);
int lora_setup_cmd(int argc, char **argv) {

    if (argc < 4) {
        puts("usage: setup "
             "<bandwidth (125, 250, 500)> "
             "<spreading factor (7..12)> "
             "<code rate (5..8)>");
        return -1;
    }

    /* Check bandwidth value */
    int bw = atoi(argv[1]);
    uint8_t lora_bw;
    switch (bw) {
        case 125:
            puts("setup: setting 125KHz bandwidth");
            lora_bw = LORA_BW_125_KHZ;
            break;

        case 250:
            puts("setup: setting 250KHz bandwidth");
            lora_bw = LORA_BW_250_KHZ;
            break;

        case 500:
            puts("setup: setting 500KHz bandwidth");
            lora_bw = LORA_BW_500_KHZ;
            break;

        default:
            puts("[Error] setup: invalid bandwidth value given, "
                 "only 125, 250 or 500 allowed.");
            return -1;
    }

    /* Check spreading factor value */
    uint8_t lora_sf = atoi(argv[2]);
    if (lora_sf < 7 || lora_sf > 12) {
        puts("[Error] setup: invalid spreading factor value given");
        return -1;
    }

    /* Check coding rate value */
    int cr = atoi(argv[3]);;
    if (cr < 5 || cr > 8) {
        puts("[Error ]setup: invalid coding rate value given");
        return -1;
    }
    uint8_t lora_cr = (uint8_t)(cr - 4);

    /* Configure radio device */
    netdev_t *netdev = (netdev_t*) &sx127x;
    netdev->driver->set(netdev, NETOPT_BANDWIDTH,
                        &lora_bw, sizeof(lora_bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR,
                        &lora_sf, sizeof(lora_sf));
    netdev->driver->set(netdev, NETOPT_CODING_RATE,
                        &lora_cr, sizeof(lora_cr));

    puts("[Info] setup: configuration set with success");

    return 0;
}

int random_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    netdev_t *netdev = (netdev_t*) &sx127x;
    printf("random: number from sx127x: %u\n",
           (unsigned int) sx127x_random((sx127x_t*) netdev));

    /* reinit the transceiver to default values */
    sx127x_init_radio_settings((sx127x_t*) netdev);

    return 0;
}

int register_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: register <get | set>");
        return -1;
    }

    if (strstr(argv[1], "get") != NULL) {
        if (argc < 3) {
            puts("usage: register get <all | allinline | regnum>");
            return -1;
        }

        if (strcmp(argv[2], "all") == 0) {
            puts("- listing all registers -");
            uint8_t reg = 0, data = 0;
            /* Listing registers map */
            puts("Reg   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
            for (unsigned i = 0; i <= 7; i++) {
                printf("0x%02X ", i << 4);

                for (unsigned j = 0; j <= 15; j++, reg++) {
                    data = sx127x_reg_read(&sx127x, reg);
                    printf("%02X ", data);
                }
                puts("");
            }
            puts("-done-");
            return 0;
        }
        else if (strcmp(argv[2], "allinline") == 0) {
            puts("- listing all registers in one line -");
            /* Listing registers map */
            for (uint16_t reg = 0; reg < 256; reg++) {
                printf("%02X ", sx127x_reg_read(&sx127x, (uint8_t) reg));
            }
            puts("- done -");
            return 0;
        }
        else {
            long int num = 0;
            /* Register number in hex */
            if (strstr(argv[2], "0x") != NULL) {
                num = strtol(argv[2], NULL, 16);
            }
            else {
                num = atoi(argv[2]);
            }

            if (num >= 0 && num <= 255) {
                printf("[regs] 0x%02X = 0x%02X\n",
                       (uint8_t) num,
                       sx127x_reg_read(&sx127x, (uint8_t) num));
            }
            else {
                puts("regs: invalid register number specified");
                return -1;
            }
        }
    }
    else if (strstr(argv[1], "set") != NULL) {
        if (argc < 4) {
            puts("usage: register set <regnum> <value>");
            return -1;
        }

        long num, val;

        /* Register number in hex */
        if (strstr(argv[2], "0x") != NULL) {
            num = strtol(argv[2], NULL, 16);
        }
        else {
            num = atoi(argv[2]);
        }

        /* Register value in hex */
        if (strstr(argv[3], "0x") != NULL) {
            val = strtol(argv[3], NULL, 16);
        }
        else {
            val = atoi(argv[3]);
        }

        sx127x_reg_write(&sx127x, (uint8_t) num, (uint8_t) val);
    }
    else {
        puts("usage: register get <all | allinline | regnum>");
        return -1;
    }

    return 0;
}

int send_cmd(int argc, char **argv)
{
    if (argc <= 1) {
        puts("usage: send <payload>");
        return -1;
    }

    printf("sending \"%s\" payload (%u bytes)\n",
           argv[1], (unsigned)strlen(argv[1]) + 1);

    iolist_t iolist = {
        .iol_base = argv[1],
        .iol_len = (strlen(argv[1]) + 1)
    };

    netdev_t *netdev = (netdev_t*) &sx127x;
    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    }

    return 0;
}

int listen_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    netdev_t *netdev = (netdev_t*) &sx127x;
    /* Switch to continuous listen mode */
    const netopt_enable_t single = false;
    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
    const uint32_t timeout = 0;
    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));

    /* Switch to RX state */
    uint8_t state = NETOPT_STATE_RX;
    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));

    printf("Listen mode set\n");

    return 0;
}

int channel_cmd(int argc, char **argv)
{
    if(argc < 2) {
        puts("usage: channel <get|set>");
        return -1;
    }

    netdev_t *netdev = (netdev_t*) &sx127x;
    uint32_t chan;
    if (strstr(argv[1], "get") != NULL) {
        netdev->driver->get(netdev, NETOPT_CHANNEL_FREQUENCY, &chan, sizeof(chan));
        printf("Channel: %i\n", (int) chan);
        return 0;
    }

    if (strstr(argv[1], "set") != NULL) {
        if(argc < 3) {
            puts("usage: channel set <channel>");
            return -1;
        }
        chan = atoi(argv[2]);
        netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan, sizeof(chan));
        printf("New channel set\n");
    }
    else {
        puts("usage: channel <get|set>");
        return -1;
    }

    return 0;
}


int send_something_cmd(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    uint8_t dr = atoi(argv[1]);
    gnrc_pktsnip_t *pkt;
    gnrc_netapi_set(3, NETOPT_DATARATE, 0, &dr, sizeof(dr));
    pkt = gnrc_pktbuf_add(NULL, "RIOT", 4, GNRC_NETTYPE_UNDEF);
    gnrc_netapi_send(3, pkt);
    return 0;
}

int lora_abp(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    uint8_t join_method = 0;
    gnrc_netapi_set(3, NETOPT_ADDRESS, 0, dev_addr, 4);
    gnrc_netapi_set(3, NETOPT_NWKSKEY, 0, nwkskey, 16);
    gnrc_netapi_set(3, NETOPT_APPSKEY, 0, appskey, 16);
    gnrc_netapi_set(3, NETOPT_JOIN, 0, &join_method, 1);

    return 0;
}

int lora_cmd(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    gnrc_netapi_set(3, NETOPT_ADDRESS_LONG, 0, deveui, 8);
    gnrc_netapi_set(3, NETOPT_APPEUI, 0, appeui, 8);
    gnrc_netapi_set(3, NETOPT_APPKEY, 0, appkey, 16);
    uint8_t join_method = 1;
    gnrc_netapi_set(3, NETOPT_JOIN, 0, &join_method, 1);

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "ota",      "Perform OTA",     lora_cmd},
    { "abp",      "Perform OTA",     lora_abp},
    { "test2",    "Test ULora 2",     send_something_cmd},
    { "setup",    "Initialize LoRa modulation settings",     lora_setup_cmd},
    { "random",   "Get random number from sx127x",           random_cmd },
    { "channel",  "Get/Set channel frequency (in Hz)",       channel_cmd },
    { "register", "Get/Set value(s) of registers of sx127x", register_cmd },
    { "send",     "Send raw payload string",                 send_cmd },
    { "listen",   "Start raw payload listener",              listen_cmd },
    { NULL, NULL, NULL }
};


int main(void)
{
    /* start the shell */
    puts("Initialization successful - starting the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
