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

#include "board.h"
#include "event.h"
#include "thread.h"
#include "xtimer.h"

#define JOIN (1)
#define GET (2)
#define FOO (3)
typedef struct _sap_primitive sap_primitive_t;
typedef struct _mac mac_t;

struct _sap_primitive {
    event_t event;
    thread_t *thread;
    mac_t *mac;
    uint8_t request;
    void *data;
    int response;
};

struct _mac {
    event_queue_t evq;
    kernel_pid_t pid;
    int (*mlme_request)(mac_t *mac, uint8_t request, void *data);
    void (*mlme_confirm)(mac_t *mac, uint8_t request, int status, void *data);
};

static xtimer_t my_timer;

int my_mlme_request(mac_t *mac, uint8_t request, void *data)
{
    (void) mac;
    (void) data;
    if(request == JOIN) {
        puts("REQUEST: The higher layer requested to JOIN. Alles gut. Init MAC join procedure and hope we receive the packet");
        xtimer_set(&my_timer, 2000000);
    }
    else if(request == GET) {
        puts("REQUEST: The higher layer requested to GET a parameter");
        mac->mlme_confirm(mac, GET, 0, NULL);
    }
    else {
        return -EINVAL;
    }

    return 0;
}

void my_mlme_confirm(mac_t *mac, uint8_t request, int status, void *data)
{
    (void) mac;
    (void) request;
    (void) status;
    (void) data;

    if(request == GET) {
        puts("CONFIRM: The MAC layer receives the requested GET parameter");
    }
    else if(request == JOIN) {
        if(status == 1) {
            puts("CONFIRM: Got the packet! Successfully joined!");
        }
        else {
            puts("CONFIRM: There was an error joining");
        }
    }
}

static void _mlme_request(event_t *event)
{
    sap_primitive_t *request = (sap_primitive_t*) event;
    request->response = request->mac->mlme_request(request->mac, request->request, request->data);
    thread_flags_set(request->thread, 1u<<13);
}

static mac_t _mac = {.mlme_request = my_mlme_request, .mlme_confirm = my_mlme_confirm};

static void _do_join(event_t *event)
{
    (void) event;
    /* Success on join! */
    _mac.mlme_confirm(&_mac, JOIN, 1, NULL);
}

int mac_mlme_request(mac_t *mac, uint8_t request, void *data)
{
    int res;
    if(thread_getpid() == mac->pid) {
        puts("Calling from MAC");
        res = mac->mlme_request(mac, request, data);
    }
    else {
        puts("Calling from another thread");
        sap_primitive_t mlme_request = { .event.handler = _mlme_request, .mac = mac,
        .request = request, .data=data, .thread=(thread_t*) sched_active_thread};
        event_post(&mac->evq, (event_t*) &mlme_request);
        thread_flags_wait_any(1u<<13);
        res = mlme_request.response;
    }
    return res;
}

static char stack[THREAD_STACKSIZE_DEFAULT];

/*
static void _mlme_confirm(event_t *event)
{
   printf("triggered 0x%08x\n", (unsigned)event);
}
*/

//static sap_primitive_t mlme_confirm = { .event.handler = _mlme_confirm };

static void *_thread(void *arg)
{
    (void) arg;
    mac_mlme_request(&_mac, GET, NULL);
    return 0;
}

static void timer_cb(void *arg)
{
    event_t *event = arg;
    event_post(&_mac.evq, event);
}

int main(void)
{
    event_queue_init(&_mac.evq);
    _mac.pid = thread_getpid();

    event_t join = {.handler = _do_join};

    my_timer.callback = timer_cb;
    my_timer.arg = &join;
    thread_create(stack, THREAD_STACKSIZE_DEFAULT, THREAD_PRIORITY_MAIN-1, 0, _thread, NULL, "test");

    /* Try a MLME request with a wrong req_id */
    if(mac_mlme_request(&_mac, 200, NULL) < 0) {
        puts("MLME request failed :). All OK, since REQ=200 does not exist");
    }
    if(mac_mlme_request(&_mac, JOIN, NULL)) {
        puts("Join MLME request went super duper :)");
    }

    event_loop(&_mac.evq);
    return 0;
}
