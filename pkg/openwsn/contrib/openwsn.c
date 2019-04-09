/*
 * Copyright (C) 2018 Hamburg University of Applied Sciences
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 */

#include "scheduler.h"
#include "openstack.h"
#include "board_ow.h"
#include "radio.h"

#ifdef OW_MAC_ONLY
#include "errno.h"
#include "event.h"
#endif

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define OW_SCHED_NAME            "ow_network"
#define OW_SCHED_PRIO            (THREAD_PRIORITY_MAIN - 1)
#define OW_SCHED_STACKSIZE       (THREAD_STACKSIZE_DEFAULT)

static kernel_pid_t _pid = KERNEL_PID_UNDEF;
static char _stack[OW_SCHED_STACKSIZE];

#ifdef OW_MAC_ONLY
extern void mlme_sync_indication(void);
extern void mlme_sync_loss_indication(void);
extern void ow_mcps_data_confirm(int status);
#endif

static void *_event_loop(void *arg);

void openwsn_bootstrap(void)
{
    DEBUG("[openwsn_bootstrap]: init RIOT board\n");
    board_init_ow();

    DEBUG("[openwsn_bootstrap]: radio_init\n");
    /* initializes an own thread for the radio driver */
    radio_init();

    DEBUG("[openwsn_bootstrap]: network thread\n");
    if (_pid <= KERNEL_PID_UNDEF) {
        _pid = thread_create(_stack, OW_SCHED_STACKSIZE, OW_SCHED_PRIO,
                             THREAD_CREATE_STACKTEST, _event_loop, NULL,
                             OW_SCHED_NAME);
        if (_pid <= 0) {
            DEBUG("[openwsn_bootstrap]: couldn't create thread\n");
        }
    }
}

#ifdef OW_MAC_ONLY
static void _sixtop_management_fired(event_t *event)
{
    timer_sixtop_management_fired();
}

static void _sixtop_sendEb_fired(event_t *event)
{
    timer_sixtop_sendEb_fired();
}

static void _sixtop_notify_send_done(event_t *event)
{
    task_sixtopNotifSendDone();
}

static void _sixtop_notify_receive(event_t *event)
{
    task_sixtopNotifReceive();
}

static void _indicate_sync(event_t *event)
{
    mlme_sync_indication();
}

static void _indicate_sync_loss(event_t *event)
{
    mlme_sync_loss_indication();
}

event_t ev_ieee154e_indicate_sync = { .handler = _indicate_sync };
event_t ev_ieee154e_indicate_sync_loss = { .handler = _indicate_sync_loss };

event_t ev_sixtop_management_fired = { .handler = _sixtop_management_fired };
event_t ev_sixtop_sendEb_fired = { .handler = _sixtop_sendEb_fired };
event_t ev_sixtop_notify_send_done = { .handler = _sixtop_notify_send_done };
event_t ev_sixtop_notify_receive = { .handler = _sixtop_notify_receive };

event_queue_t queue;

void ieee154e_indicate_sync(void)
{
    event_post(&queue, &ev_ieee154e_indicate_sync);
}

void ieee154e_indicate_sync_loss(void)
{
    event_post(&queue, &ev_ieee154e_indicate_sync_loss);
}
#endif

static void *_event_loop(void *arg)
{
    DEBUG("[openwsn_bootstrap]: init openstack\n");
#ifdef OW_MAC_ONLY
    event_queue_init(&queue);
    openserial_init();
    idmanager_init();    // call first since initializes EUI64 and isDAGroot
    openqueue_init();
    openrandom_init();
    opentimers_init();
    ieee154e_init();
    schedule_init();
    sixtop_init();
    neighbors_init();
    event_loop(&queue);
#else
    openstack_init();

    DEBUG("[openwsn_bootstrap]: init scheduler\n");
    scheduler_init();

    DEBUG("[openwsn_bootstrap]: start scheduler");
    /* starts the OpenWSN scheduler which contains a loop */
    scheduler_start();
#endif
    return NULL;
}

