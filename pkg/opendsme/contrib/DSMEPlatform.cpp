#include "opendsme/DSMEPlatform.h"
#include "ztimer.h"
#include "iolist.h"
#include "event.h"
#include "event/thread.h"
#include "luid.h"
#include "dsmeAdaptionLayer/scheduling/TPS.h"
#include "board.h"

#if IS_USED(MODULE_CC2538_RF)
#include "cc2538_rf.h"
#endif

#if IS_USED(MODULE_NRF802154)
#include "nrf802154.h"
#endif

#ifndef PAN_COORD
#define PAN_COORD 0
#endif

#ifndef ENABLE_SEND
#define ENABLE_SEND PAN_COORD
#endif

#include "net/ieee802154/radio.h"

ieee802154_dev_t _radio;

namespace dsme {

ztimer_t send_timer;
event_t send_timer_ev;
ztimer_t cca_timer;
event_t cca_timer_ev;
ztimer_t acktimer;
event_t acktimer_ev;
DSMEPlatform* DSMEPlatform::instance = nullptr;
uint8_t DSMEPlatform::state = STATE_READY;
Delegate<void(bool)> DSMEPlatform::txEndCallback;
static event_t timer_event;
static event_t tx_done_event;
static event_t rx_done_event;
static event_t request_slot_ev;
static uint32_t rx_sfd;
static bool wait_for_ack;
static bool changed;

static void _handle_rx_offload(event_t *ev)
{
    dsme::DSMEPlatform::instance->rx_offload();
}

void DSMEPlatform::rx_offload()
{
    IDSMEMessage *message = this->message;
    this->message = nullptr;
    receiveFromAckLayerDelegate(message);
}

static event_t rx_offload_ev;

static void _start_of_cfp_handler(event_t *ev)
{
    dsme::DSMEPlatform::instance->getDSME().handleStartOfCFP();
}

static event_t start_of_cfp_ev;

static void _cca_timer_ev_handler(event_t *ev)
{
    dsme::DSMEPlatform::instance->getDSME().dispatchCCAResult(true);
}

static void _acktimer_ev_handler(event_t *ev)
{
    //puts("sendAck");
    if (changed) {
        puts(":(");
    }
    dsme::DSMEPlatform::instance->sendNow();
}

static void _cca_timer_cb(void *arg)
{
    event_post(EVENT_PRIO_HIGHEST, &cca_timer_ev);
}

static void _acktimer_cb(void *arg)
{
    event_post(EVENT_PRIO_HIGHEST, &acktimer_ev);
}

static void _send_timer_ev_handler(event_t *ev)
{
    //dsme::DSMEPlatform::instance->send_pkt(dsme::DSMEPlatform::instance->panDescriptorToSyncTo.coordAddress.getShortAddress());
    dsme::DSMEPlatform::instance->send_pkt(0xd565);
    ztimer_set(ZTIMER_USEC, &send_timer, 200000);
    //puts("S");
}

static void _send_timer_cb(void *arg)
{
    event_post(EVENT_PRIO_HIGHEST, &send_timer_ev);
}

static void _timer_ev_handler(event_t *ev)
{
    dsme::DSMEPlatform::instance->getDSME().getEventDispatcher().timerInterrupt();
}

static void _tx_done_handler(event_t *ev)
{
    int res = ieee802154_radio_confirm_transmit(&_radio, NULL);
    DSME_ASSERT(res >= 0);
    changed = true;
    //puts("RXON");
    res = ieee802154_radio_set_rx(&_radio);
    DSME_ASSERT(res == 0);
    if (wait_for_ack) {
        wait_for_ack = false;
        ieee802154_radio_set_frame_filter_mode(&_radio, IEEE802154_FILTER_ACK_ONLY);
    }
    else {
        ieee802154_radio_set_frame_filter_mode(&_radio, IEEE802154_FILTER_ACCEPT);
    }
    dsme::DSMEPlatform::txEndCallback(true);
    DSMEPlatform::state = DSMEPlatform::STATE_READY;
}

void DSMEPlatform::handle_rx()
{
    DSME_ASSERT(DSMEPlatform::state == STATE_READY);
    DSMEMessage *message = getEmptyMessage();
    message->setStartOfFrameDelimiterSymbolCounter(rx_sfd);

    int res;
    changed = true;
    res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
    int len = ieee802154_radio_len(&_radio);
    res = message->loadBuffer(len);
    DSME_ASSERT(res >= 0);
    ieee802154_rx_info_t info;

    ieee802154_radio_read(&_radio, message->getPayload(), 127, &info);
    message->messageLQI = info.lqi;
    message->radio_last_rssi = info.rssi;
    const uint8_t *buf = message->getPayload();
    if (buf[0] & IEEE802154_FCF_TYPE_ACK) {
        //puts("ACK");
    }
    bool success = message->getHeader().deserializeFrom(buf, len);
    if (!success) {
        puts(":(");
    }
    message->dropHdr(message->getHeader().getSerializationLength());

    res = ieee802154_radio_set_rx(&_radio);
    DSME_ASSERT(res == 0);

    getDSME().getAckLayer().receive(message);
}

static void _rx_done_handler(event_t *ev)
{
    dsme::DSMEPlatform::instance->handle_rx();
}

static void _hal_radio_cb(ieee802154_dev_t *dev, ieee802154_trx_ev_t status)
{
    switch (status) {
        case IEEE802154_RADIO_CONFIRM_TX_DONE:
            event_post(EVENT_PRIO_HIGHEST, &tx_done_event);
            break;
        case IEEE802154_RADIO_INDICATION_RX_START:
            rx_sfd = dsme::DSMEPlatform::instance->getSymbolCounter();
            break;
        case IEEE802154_RADIO_INDICATION_CRC_ERROR:
            break;
        case IEEE802154_RADIO_INDICATION_TX_START:
            break;
        case IEEE802154_RADIO_INDICATION_RX_DONE:
            event_post(EVENT_PRIO_HIGHEST, &rx_done_event);
            break;
        case IEEE802154_RADIO_CONFIRM_CCA:
            break;
        default:
            DSME_ASSERT(false);
    }
}

static void _timer_cb(void *arg)
{
    event_post(EVENT_PRIO_HIGHEST, &timer_event);
}

void DSMEPlatform::send_pkt(uint16_t addr)
{
    if(!this->mac_pib.macAssociatedPANCoord) {
        puts("Discarding message");
        return;
    }

    //puts("S");
    DSMEMessage* message = getEmptyMessage();
    message->loadBuffer(4);
    IEEE802154MacAddress dst;
    dst.setShortAddress(addr);
    mcps_sap::DATA::request_parameters params;

    message->getHeader().setSrcAddrMode(SHORT_ADDRESS);
    message->getHeader().setDstAddrMode(SHORT_ADDRESS);
    message->getHeader().setDstAddr(dst);

    message->getHeader().setSrcPANId(this->mac_pib.macPANId);
    message->getHeader().setDstPANId(this->mac_pib.macPANId);

    this->dsmeAdaptionLayer.sendMessage(message);
}

DSMEPlatform::DSMEPlatform() :
				phy_pib(),
				mac_pib(phy_pib),

				mcps_sap(dsme),
				mlme_sap(dsme),
                dsmeAdaptionLayer(dsme),
				initialized(false){
    instance = this;
    this->timer.callback = _timer_cb;
    this->timer.arg = this;

    send_timer.callback = _send_timer_cb;
    send_timer.arg = this;
    send_timer_ev.handler = _send_timer_ev_handler;
    cca_timer.callback = _cca_timer_cb;
    cca_timer.arg = this;
    acktimer.callback = _acktimer_cb;
    acktimer.arg = this;
    acktimer_ev.handler = _acktimer_ev_handler;
    cca_timer_ev.handler = _cca_timer_ev_handler;
    timer_event.handler = _timer_ev_handler;
    tx_done_event.handler = _tx_done_handler;
    rx_done_event.handler = _rx_done_handler;
    rx_offload_ev.handler = _handle_rx_offload;
    start_of_cfp_ev.handler = _start_of_cfp_handler;
    
    for (int i=0; i<DSME_POOL_SIZE; i++) {
        this->pool[i].free = true;
    }
}

DSMEPlatform::~DSMEPlatform()
{
}

/**
 * Creates an IEEE802154MacAddress out of an uint16_t short address
 */
void DSMEPlatform::translateMacAddress(uint16_t& from, IEEE802154MacAddress& to)
{
    if(from == 0xFFFF) {
        to = IEEE802154MacAddress(IEEE802154MacAddress::SHORT_BROADCAST_ADDRESS);
    } else {
        to.setShortAddress(from);
    }
}

void DSMEPlatform::initialize()
{
    this->instance = this;
    this->dsme.setPHY_PIB(&(this->phy_pib));
    this->dsme.setMAC_PIB(&(this->mac_pib));
    this->dsme.setMCPS(&(this->mcps_sap));
    this->dsme.setMLME(&(this->mlme_sap));

    /* Initialize channels */
    constexpr uint8_t MAX_CHANNELS = 16;
    uint8_t channels[MAX_CHANNELS];

    uint8_t num = MAX_CHANNELS;

    channelList_t DSSS2450_channels(num);
    for(uint8_t i = 0; i < num; i++) {
        DSSS2450_channels[i] = 11 + i;
    }

    phy_pib.setDSSS2450ChannelPage(DSSS2450_channels);

    /* Initialize Address */
    IEEE802154MacAddress address;

    uint16_t id;
    luid_base(&id, 2);
    translateMacAddress(id, this->mac_pib.macExtendedAddress);

    _radio.cb = _hal_radio_cb;
#if IS_USED(MODULE_CC2538_RF)
    cc2538_rf_hal_setup(&_radio);
    cc2538_init();
#elif IS_USED(MODULE_NRF802154)
    nrf802154_hal_setup(&_radio);
    nrf802154_init();
#else
#error "Please select a radio"
#endif
    ieee802154_radio_request_on(&_radio);
    while (ieee802154_radio_confirm_on(&_radio) == -EAGAIN) {}
    ieee802154_radio_set_csma_params(&_radio, NULL, -1);
    ieee802154_radio_set_frame_filter_mode(&_radio, IEEE802154_FILTER_ACCEPT);

    uint8_t ext_addr[8];
    network_uint16_t short_addr;
    /* TODO: Configure address */

    /* Call more radio stuff here... */

    this->mac_pib.macShortAddress = this->mac_pib.macExtendedAddress.getShortAddress();
    ext_addr[0] = this->mac_pib.macExtendedAddress.a1() >> 8;
    ext_addr[1] = this->mac_pib.macExtendedAddress.a1() & 0xFF;
    ext_addr[2] = this->mac_pib.macExtendedAddress.a2() >> 8;
    ext_addr[3] = this->mac_pib.macExtendedAddress.a2() & 0xFF;
    ext_addr[4] = this->mac_pib.macExtendedAddress.a3() >> 8;
    ext_addr[5] = this->mac_pib.macExtendedAddress.a3() & 0xFF;
    ext_addr[6] = this->mac_pib.macExtendedAddress.a4() >> 8;
    ext_addr[7] = this->mac_pib.macExtendedAddress.a4() & 0xFF;

    short_addr.u8[0] = this->mac_pib.macExtendedAddress.getShortAddress() >> 8;
    short_addr.u8[1] = this->mac_pib.macExtendedAddress.getShortAddress() & 0xFF;

    this->mac_pib.macIsPANCoord = PAN_COORD;
    if(this->mac_pib.macIsPANCoord) {
      DSME_PRINTF("This node is PAN coordinator\n");
      this->mac_pib.macPANId = 0x23;
    }
    ieee802154_radio_config_addr_filter(&_radio, IEEE802154_AF_PANID, &this->mac_pib.macPANId);
    ieee802154_radio_config_addr_filter(&_radio, IEEE802154_AF_SHORT_ADDR, &short_addr);
    ieee802154_radio_config_addr_filter(&_radio, IEEE802154_AF_EXT_ADDR, &ext_addr);

    this->mac_pib.macCapReduction = false;

    this->mac_pib.macAssociatedPANCoord = this->mac_pib.macIsPANCoord;
    this->mac_pib.macSuperframeOrder = 3;
    this->mac_pib.macMultiSuperframeOrder = 4;
    this->mac_pib.macBeaconOrder = 7;

    this->mac_pib.macMinBE = 7;
    this->mac_pib.macMaxBE = 8;
    this->mac_pib.macMaxCSMABackoffs = 5;
    this->mac_pib.macMaxFrameRetries = 3;

    this->mac_pib.macDSMEGTSExpirationTime = 16;
    this->mac_pib.macResponseWaitTime = 244;
    this->mac_pib.macChannelDiversityMode = Channel_Diversity_Mode::CHANNEL_HOPPING;

    this->phy_pib.phyCurrentChannel = 26;
		
    this->dsmeAdaptionLayer.setIndicationCallback(DELEGATE(&DSMEPlatform::handleDataMessageFromMCPSWrapper, *this));
    this->dsmeAdaptionLayer.setConfirmCallback(DELEGATE(&DSMEPlatform::handleConfirmFromMCPSWrapper, *this));

    if (ENABLE_SEND) {
        ztimer_set(ZTIMER_USEC, &send_timer, 10000000);
    }
    this->dsme.initialize(this);

    channelList_t scanChannels;
    scanChannels.add(26);
    TPS* tps = new TPS(this->dsmeAdaptionLayer);
    tps->setAlpha(0.1);
    tps->setMinFreshness(this->mac_pib.macDSMEGTSExpirationTime);
    scheduling = tps;
    this->dsmeAdaptionLayer.initialize(scanChannels,8,scheduling);
    this->initialized = true;
}

void DSMEPlatform::start()
{
    DSME_ASSERT(this->initialized);
    this->dsme.start();
    this->dsmeAdaptionLayer.startAssociation();
}

bool DSMEPlatform::isAssociated()
{
    return this->mac_pib.macAssociatedPANCoord;
}

void DSMEPlatform::handleDataMessageFromMCPSWrapper(IDSMEMessage* msg)
{
    this->handleDataMessageFromMCPS(static_cast<DSMEMessage*>(msg));
}

void DSMEPlatform::handleConfirmFromMCPSWrapper(IDSMEMessage* msg, DataStatus::Data_Status dataStatus) {
    this->handleConfirmFromMCPS(static_cast<DSMEMessage*>(msg), dataStatus);
}

void DSMEPlatform::handleConfirmFromMCPS(DSMEMessage* msg, DataStatus::Data_Status dataStatus) {
    if (dataStatus == DataStatus::Data_Status::SUCCESS) {
        //puts(":)!");
    }
    IDSMEMessage *m = static_cast<IDSMEMessage*>(msg);
    releaseMessage(m);
}

void DSMEPlatform::handleDataMessageFromMCPS(DSMEMessage* msg)
{
    //puts("R! :)");
    LED1_TOGGLE;
    IDSMEMessage *m = static_cast<IDSMEMessage*>(msg);
    releaseMessage(m);
}

bool DSMEPlatform::isReceptionFromAckLayerPossible()
{
    /* TODO: */
    return DSMEPlatform::state == STATE_READY;
}

void DSMEPlatform::handleReceivedMessageFromAckLayer(IDSMEMessage* message)
{
    DSME_ASSERT(receiveFromAckLayerDelegate);
    DSME_ASSERT(!this->message);
    this->message = message;
    event_post(EVENT_PRIO_HIGHEST, &rx_offload_ev);
}

DSMEMessage *DSMEPlatform::getEmptyMessage()
{
    DSMEMessage *msg = NULL;
    for (int i=0; i<DSME_POOL_SIZE; i++) {
        if (this->pool[i].free) {
            msg = &this->pool[i];
            break;
        }
    }
    DSME_ASSERT(msg);
    msg->clearMessage();
    signalNewMsg(msg);
    return msg;
}

void DSMEPlatform::releaseMessage(IDSMEMessage* msg)
{
    DSMEMessage *m = static_cast<DSMEMessage*>(msg);
    m->releaseMessage();
}

void DSMEPlatform::startTimer(uint32_t symbolCounterValue)
{
    uint32_t now = ztimer_now(ZTIMER_USEC);
    uint32_t offset = now & 0xF;
    uint32_t delta = ((symbolCounterValue - getSymbolCounter()) << 4) - offset;
    ztimer_set(ZTIMER_USEC, &timer, delta);
}

uint32_t DSMEPlatform::getSymbolCounter()
{
    return ztimer_now(ZTIMER_USEC) >> 4;
}

void DSMEPlatform::scheduleStartOfCFP()
{
    event_post(EVENT_PRIO_HIGHEST, &start_of_cfp_ev);
}

void DSMEPlatform::signalAckedTransmissionResult(bool success, uint8_t transmissionAttempts, IEEE802154MacAddress receiver)
{

}

bool DSMEPlatform::setChannelNumber(uint8_t channel)
{
    ieee802154_phy_conf_t conf = {
        .phy_mode = IEEE802154_PHY_OQPSK,
        .channel = channel,
        .page = 0,
        .pow = 6,
    };
    int res;
    changed = true;
    //puts("TRX_OFF");
    res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
    res = ieee802154_radio_config_phy(&_radio, &conf);
    DSME_ASSERT(res == 0);
    //puts("RXON");
    res = ieee802154_radio_set_rx(&_radio);
    DSME_ASSERT(res == 0);
    return true;
}

uint8_t DSMEPlatform::getChannelNumber()
{
    DSME_ASSERT(false);
    return 0;
}

bool DSMEPlatform::prepareSendingCopy(IDSMEMessage* msg, Delegate<void(bool)> txEndCallback)
{
    DSMEMessage *m = (DSMEMessage*) msg;
    DSMEPlatform::state = STATE_SEND;
    DSMEPlatform::txEndCallback = txEndCallback;
    uint8_t mhr[23];
    uint8_t mhr_len = msg->getHeader().getSerializationLength();
    uint8_t *p = mhr;
    msg->getHeader().serializeTo(p);
    iolist_t iol = {
        .iol_next = (iolist_t*) m->getIolPayload(),
        .iol_base = mhr,
        .iol_len = mhr_len,
    };
    wait_for_ack= false;
    /* TODO */
    if (mhr[0] & 0x20) {
        wait_for_ack = true;
    }
    if ((mhr[0] & IEEE802154_FCF_TYPE_MASK) == IEEE802154_FCF_TYPE_ACK) {
        //puts("A");
    }
    changed = true;
    int res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
    res = ieee802154_radio_write(&_radio, &iol);
    DSME_ASSERT(res == 0);

    return true;
}

bool DSMEPlatform::sendNow()
{
    //puts("S");
    int res = ieee802154_radio_request_transmit(&_radio);
    DSME_ASSERT(res == 0);
    return true;
}

void DSMEPlatform::abortPreparedTransmission()
{

}

bool DSMEPlatform::sendDelayedAck(IDSMEMessage* ackMsg, IDSMEMessage* receivedMsg, Delegate<void(bool)> txEndCallback)
{
    DSMEMessage *m = (DSMEMessage*) ackMsg;
    DSME_ASSERT(m != nullptr);

    uint8_t ack[3];
    uint8_t mhr_len = ackMsg->getHeader().getSerializationLength();
    DSME_ASSERT(mhr_len == 3);

    uint8_t *p = ack;
    ackMsg->getHeader().serializeTo(p);

    DSMEPlatform::txEndCallback = txEndCallback;

    iolist_t iol = {
        .iol_next = NULL,
        .iol_base = ack,
        .iol_len = mhr_len,
    };

    changed = true;
    int res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
    res = ieee802154_radio_write(&_radio, &iol);
    DSME_ASSERT(res == 0);

    // Preamble (4) | SFD (1) | PHY Hdr (1) | MAC Payload | FCS (2)
    uint32_t endOfReception = receivedMsg->getStartOfFrameDelimiterSymbolCounter() + receivedMsg->getTotalSymbols() - 2 * 4 // Preamble
                              - 2 * 1;                                                                                      // SFD
    uint32_t ackTime = endOfReception + aTurnaroundTime;
    uint32_t now = getSymbolCounter();
    uint32_t diff = ackTime - now;
    //printf("A:%02x\n", (int)diff);
    //printf("R: %02x\n", receivedMsg->getTotalSymbols());
    //printf("E: %02x\n", (int) (now - receivedMsg->getStartOfFrameDelimiterSymbolCounter()));

    changed = false;
    ztimer_set(ZTIMER_USEC, &acktimer, diff * aSymbolDuration);
    return true;
}

void DSMEPlatform::setReceiveDelegate(receive_delegate_t receiveDelegate)
{
    this->receiveFromAckLayerDelegate = receiveDelegate;
}

bool DSMEPlatform::startCCA()
{
    /* TODO: This MUST be implemented properly */
    ztimer_set(ZTIMER_USEC, &cca_timer, 16*8);
    return true;
}

void DSMEPlatform::turnTransceiverOn()
{
}

void DSMEPlatform::turnTransceiverOff()
{
    changed = true;
    //puts("TRX_OFF");
    int res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
}

}
