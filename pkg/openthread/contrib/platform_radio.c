/*
 * Copyright (C) 2016 José Ignacio Alamos
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  José Ignacio Alamos <jialamos@uc.cl>
 */

#include <stdio.h>
#include <assert.h>
#include <platform/radio.h>
#include "ot.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

#include "errno.h"
#include "net/ethernet/hdr.h"
#include "net/ethertype.h"
#include "byteorder.h"
#include "net/netdev2/ieee802154.h"
#include "net/ieee802154.h"
#include <string.h>

static bool sDisabled;

static RadioPacket sTransmitFrame;
static RadioPacket sReceiveFrame;
static int8_t Rssi;

static netdev2_t *_dev;

/* asks the driver the current 15.4 channel */
uint16_t get_channel(void)
{
    uint16_t channel;

    _dev->driver->get(_dev, NETOPT_CHANNEL, &channel, sizeof(uint16_t));
    return channel;
}

/* set 15.4 channel */
int set_channel(uint16_t channel)
{
    return _dev->driver->set(_dev, NETOPT_CHANNEL, &channel, sizeof(uint16_t));
}

/*get transmission power from driver */
int16_t get_power(void)
{
    int16_t power;

    _dev->driver->get(_dev, NETOPT_TX_POWER, &power, sizeof(int16_t));
    return power;
}

/* set transmission power */
int set_power(int16_t power)
{
    return _dev->driver->set(_dev, NETOPT_TX_POWER, &power, sizeof(int16_t));
}

/* set IEEE802.15.4 PAN ID */
int set_panid(uint16_t panid)
{
    return _dev->driver->set(_dev, NETOPT_NID, &panid, sizeof(uint16_t));
}

/* set extended HW address */
int set_long_addr(uint8_t *ext_addr)
{
    return _dev->driver->set(_dev, NETOPT_ADDRESS_LONG, ext_addr, IEEE802154_LONG_ADDRESS_LEN);
}

/* set short address */
int set_addr(uint16_t addr)
{
    return _dev->driver->set(_dev, NETOPT_ADDRESS, &addr, sizeof(uint16_t));
}

/* check the state of promiscuous mode */
netopt_enable_t is_promiscuous(void)
{
    netopt_enable_t en;

    _dev->driver->get(_dev, NETOPT_PROMISCUOUSMODE, &en, sizeof(en));
    return en == NETOPT_ENABLE ? true : false;;
}

/* set the state of promiscuous mode */
int set_promiscuous(netopt_enable_t enable)
{
    return _dev->driver->set(_dev, NETOPT_PROMISCUOUSMODE, &enable, sizeof(enable));
}

/* wrapper for getting device state */
int get_state(void)
{
    netopt_state_t en;

    _dev->driver->get(_dev, NETOPT_STATE, &en, sizeof(netopt_state_t));
    return en;
}

/* wrapper for setting device state */
void set_state(netopt_state_t state)
{
    _dev->driver->set(_dev, NETOPT_STATE, &state, sizeof(netopt_state_t));
}

/* checks if the device is off */
bool is_off(void)
{
    return get_state() == NETOPT_STATE_OFF;
}

/* sets device state to OFF */
void ot_off(void)
{
    set_state(NETOPT_STATE_OFF);
}

/* sets device state to SLEEP */
void ot_sleep(void)
{
    set_state(NETOPT_STATE_SLEEP);
}

/* check if device state is IDLE */
bool is_idle(void)
{
    return get_state() == NETOPT_STATE_IDLE;
}

/* set device state to IDLE */
void ot_idle(void)
{
    set_state(NETOPT_STATE_IDLE);
}

/* check if device is receiving a packet */
bool is_rx(void)
{
    return get_state() == NETOPT_STATE_RX;
}

/* turn on packet reception */
void enable_rx(void)
{
    netopt_enable_t enable = true;

    _dev->driver->set(_dev, NETOPT_RX_LISTENING, &enable, sizeof(enable));
}

/* turn off packet reception */
void disable_rx(void)
{
    netopt_enable_t enable = true;

    _dev->driver->set(_dev, NETOPT_RX_LISTENING, &enable, sizeof(enable));
}

/* init framebuffers and initial state */
void openthread_radio_init(netdev2_t *dev, uint8_t *tb, uint8_t *rb)
{
    sTransmitFrame.mPsdu = tb;
    sTransmitFrame.mLength = 0;
    sReceiveFrame.mPsdu = rb;
    sReceiveFrame.mLength = 0;
    _dev = dev;
}

/* Called upon NETDEV2_EVENT_RX_COMPLETE event */
void recv_pkt(otInstance *aInstance, netdev2_t *dev)
{
    netdev2_ieee802154_rx_info_t rx_info;
    /* Read frame length from driver */
    int len = dev->driver->recv(dev, NULL, 0, &rx_info);
    Rssi = rx_info.rssi;

    /* Since OpenThread does the synchronization of rx/tx, it's necessary to turn off the receiver now */
    ot_idle();
    disable_rx();

    /* very unlikely */
    if ((len > (unsigned) UINT16_MAX)) {
        otPlatRadioReceiveDone(aInstance, NULL, kThreadError_Abort);
        return;
    }

    /* Fill OpenThread receive frame */
    sReceiveFrame.mLength = len;
    sReceiveFrame.mPower = get_power();


    /* Read received frame */
    int res = dev->driver->recv(dev, (char *) sReceiveFrame.mPsdu, len, NULL);

    /* Tell OpenThread that receive has finished */
    otPlatRadioReceiveDone(aInstance, res > 0 ? &sReceiveFrame : NULL, res > 0 ? kThreadError_None : kThreadError_Abort);
}

/* Called upon TX event */
void send_pkt(otInstance *aInstance, netdev2_t *dev, netdev2_event_t event)
{
    /* Tell OpenThread transmission is done depending on the NETDEV2 event */
    switch (event) {
        case NETDEV2_EVENT_TX_COMPLETE:
            DEBUG("openthread: NETDEV2_EVENT_TX_COMPLETE\n");
            otPlatRadioTransmitDone(aInstance, NULL, false, kThreadError_None);
            break;
        case NETDEV2_EVENT_TX_COMPLETE_DATA_PENDING:
            DEBUG("openthread: NETDEV2_EVENT_TX_COMPLETE_DATA_PENDING\n");
            otPlatRadioTransmitDone(aInstance, NULL, true, kThreadError_None);
            break;
        case NETDEV2_EVENT_TX_NOACK:
            DEBUG("openthread: NETDEV2_EVENT_TX_NOACK\n");
            otPlatRadioTransmitDone(aInstance, NULL, false, kThreadError_NoAck);
            break;
        case NETDEV2_EVENT_TX_MEDIUM_BUSY:
            DEBUG("openthread: NETDEV2_EVENT_TX_MEDIUM_BUSY\n");
            otPlatRadioTransmitDone(aInstance, NULL, false, kThreadError_ChannelAccessFailure);
            break;
        default:
            break;
    }

    /* Since the transmission is finished, turn off reception */
    disable_rx();
    ot_idle();

}

/* OpenThread will call this for setting PAN ID */
void otPlatRadioSetPanId(otInstance *aInstance, uint16_t panid)
{
    DEBUG("openthread: otPlatRadioSetPanId: setting PAN ID to %04x\n", panid);
    set_panid(((panid & 0xff) << 8) | ((panid >> 8) & 0xff));
}

/* OpenThread will call this for setting extended address */
void otPlatRadioSetExtendedAddress(otInstance *aInstance, uint8_t *aExtendedAddress)
{
    DEBUG("openthread: otPlatRadioSetExtendedAddress\n");
    uint8_t reversed_addr[IEEE802154_LONG_ADDRESS_LEN];
    for (int i = 0; i < IEEE802154_LONG_ADDRESS_LEN; i++) {
        reversed_addr[i] = aExtendedAddress[IEEE802154_LONG_ADDRESS_LEN - 1 - i];
    }
    set_long_addr(reversed_addr);
}

/* OpenThread will call this for setting short address */
void otPlatRadioSetShortAddress(otInstance *aInstance, uint16_t aShortAddress)
{
    DEBUG("openthread: otPlatRadioSetShortAddress: setting address to %04x\n", aShortAddress);
    set_addr(((aShortAddress & 0xff) << 8) | ((aShortAddress >> 8) & 0xff));
}

/* OpenThread will call this for enabling the radio */
ThreadError otPlatRadioEnable(otInstance *aInstance)
{
    (void) aInstance;

    ThreadError error;

    if (sDisabled)
    {
        sDisabled = false;
        error = kThreadError_None;
    }
    else
    {
        error = kThreadError_InvalidState;
    }

return error;
}

/* OpenThread will call this for disabling the radio */
ThreadError otPlatRadioDisable(otInstance *aInstance)
{
    (void) aInstance;

    ThreadError error;

    if (!sDisabled)
    {
        sDisabled = true;
        error = kThreadError_None;
    }
    else
    {
        error = kThreadError_InvalidState;
    }

    return error;
}

bool otPlatRadioIsEnabled(otInstance *aInstance)
{
    (void) aInstance;

    return !sDisabled;
}

/* OpenThread will call this for setting device state to SLEEP */
ThreadError otPlatRadioSleep(otInstance *aInstance)
{
   (void) aInstance;

    ot_sleep();
    return kThreadError_None;
}

#if 0
/* OpenThread will call this for setting the device state to IDLE */
ThreadError otPlatRadioIdle(otInstance *aInstance)
{
    DEBUG("openthread: otPlatRadioIdle\n");

    if (is_rx() || is_off()) {
        DEBUG("openthread: OtPlatRadioIdle: Busy\n");
        return kThreadError_Busy;
    }

    /* OpenThread will call this before calling otPlatRadioTransmit.
     * If a packet is received between this function and otPlatRadioTransmit OpenThread will fail! */
    disable_rx();
    ot_idle();

    return kThreadError_None;
}
#endif

/*OpenThread will call this for waiting the reception of a packet */

ThreadError otPlatRadioReceive(otInstance *aInstance, uint8_t aChannel)
{
    (void) aInstance;

    set_channel(aChannel);
    return kThreadError_None;
}


/* OpenThread will call this function to get the transmit buffer */
RadioPacket *otPlatRadioGetTransmitBuffer(otInstance *aInstance)
{
    DEBUG("openthread: otPlatRadioGetTransmitBuffer\n");
    return &sTransmitFrame;
}

/* OpenThread will call this for transmitting a packet*/
ThreadError otPlatRadioTransmit(otInstance *aInstance, RadioPacket *aPacket)
{
    (void) aInstance;
    struct iovec pkt;

    /* Populate iovec with transmit data */
    pkt.iov_base = aPacket->mPsdu;
    pkt.iov_len = aPacket->mLength;

    /*Set channel and power based on transmit frame */
    set_channel(aPacket->mChannel);
    set_power(aPacket->mPower);

    /* send packet though netdev2 */
    _dev->driver->send(_dev, &pkt, 1);

    return kThreadError_None;
}

/* OpenThread will call this for getting the radio caps */
otRadioCaps otPlatRadioGetCaps(otInstance *aInstance)
{
    DEBUG("openthread: otPlatRadioGetCaps\n");
    /* all drivers should handle ACK, including call of NETDEV2_EVENT_TX_NOACK */
    return kRadioCapsAckTimeout;
}

/* OpenThread will call this for getting the state of promiscuous mode */
bool otPlatRadioGetPromiscuous(otInstance *aInstance)
{
    DEBUG("openthread: otPlatRadioGetPromiscuous\n");
    return is_promiscuous();
}

/* OpenThread will call this for setting the state of promiscuous mode */
void otPlatRadioSetPromiscuous(otInstance *aInstance, bool aEnable)
{
    DEBUG("openthread: otPlatRadioSetPromiscuous\n");
    set_promiscuous((aEnable) ? NETOPT_ENABLE : NETOPT_DISABLE);
}

int8_t otPlatRadioGetRssi(otInstance *aInstance)
{
    (void) aInstance;
    return Rssi;
}

void otPlatRadioEnableSrcMatch(otInstance *aInstance, bool aEnable)
{
    (void)aInstance;
    (void)aEnable;
}

ThreadError otPlatRadioAddSrcMatchShortEntry(otInstance *aInstance, const uint16_t aShortAddress)
{
    (void)aInstance;
    (void)aShortAddress;
    return kThreadError_None;
}

ThreadError otPlatRadioAddSrcMatchExtEntry(otInstance *aInstance, const uint8_t *aExtAddress)
{
    (void)aInstance;
    (void)aExtAddress;
    return kThreadError_None;
}

ThreadError otPlatRadioClearSrcMatchShortEntry(otInstance *aInstance, const uint16_t aShortAddress)
{
    (void)aInstance;
    (void)aShortAddress;
    return kThreadError_None;
}

ThreadError otPlatRadioClearSrcMatchExtEntry(otInstance *aInstance, const uint8_t *aExtAddress)
{
    (void)aInstance;
    (void)aExtAddress;
    return kThreadError_None;
}

void otPlatRadioClearSrcMatchShortEntries(otInstance *aInstance)
{
    (void)aInstance;
}

void otPlatRadioClearSrcMatchExtEntries(otInstance *aInstance)
{
    (void)aInstance;
}

ThreadError otPlatRadioEnergyScan(otInstance *aInstance, uint8_t aScanChannel, uint16_t aScanDuration)
{
    (void)aInstance;
    (void)aScanChannel;
    (void)aScanDuration;
    return kThreadError_NotImplemented;
}

void otPlatRadioGetIeeeEui64(otInstance *aInstance, uint8_t *aIeee64Eui64)
{
    _dev->driver->get(_dev, NETOPT_IPV6_IID, aIeee64Eui64, sizeof(eui64_t));
}
/** @} */
