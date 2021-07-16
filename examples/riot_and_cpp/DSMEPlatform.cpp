#include "DSMEPlatform.h"
#include "ztimer.h"
#include "iolist.h"
#include "event.h"
#include "event/thread.h"

namespace dsme {

DSMEPlatform* DSMEPlatform::instance = nullptr;
uint8_t DSMEPlatform::state = STATE_READY;
Delegate<void(bool)> DSMEPlatform::txEndCallback;
static void _timer_ev_handler(event_t *ev)
{
    dsme::DSMEPlatform::instance->getDSME().getEventDispatcher().timerInterrupt();
}

static event_t timer_event;

static void _timer_cb(void *arg)
{
    event_post(EVENT_PRIO_HIGHEST, &timer_event);
}


DSMEPlatform::DSMEPlatform() :
				phy_pib(),
				mac_pib(phy_pib),

				mcps_sap(dsme),
				mlme_sap(dsme),

				messagesInUse(0),
				initialized(false),
        channel(MIN_CHANNEL),
				currentTXLength(0){
    instance = this;
    this->timer.callback = _timer_cb;
    this->timer.arg = this;

    timer_event.handler = _timer_ev_handler;
    
    this->head = &this->pool[0];
    for (int i=0; i<7; i++) {
        this->pool[i].next = &this->pool[i+1];
    }
    this->pool[7].next = NULL;
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

    uint16_t id = 0x1234;
    translateMacAddress(id, this->mac_pib.macExtendedAddress);

    this->mac_pib.macShortAddress = this->mac_pib.macExtendedAddress.getShortAddress();
    this->mac_pib.macIsPANCoord = true;
    if(this->mac_pib.macIsPANCoord) {
      DSME_PRINTF("This node is PAN coordinator\n");
      this->mac_pib.macPANId = 0x23;
    }

    this->mac_pib.macCapReduction = false;

    this->mac_pib.macAssociatedPANCoord = this->mac_pib.macIsPANCoord;
    this->mac_pib.macSuperframeOrder = 4;
    this->mac_pib.macMultiSuperframeOrder = 5;
    this->mac_pib.macBeaconOrder = 7;

    this->mac_pib.macMinBE = 7;
    this->mac_pib.macMaxBE = 8;
    this->mac_pib.macMaxCSMABackoffs = 5;
    this->mac_pib.macMaxFrameRetries = 3;

    this->mac_pib.macDSMEGTSExpirationTime = 50;
    this->mac_pib.macResponseWaitTime = 244;

    this->phy_pib.phyCurrentChannel = 26;
		
    this->mcps_sap.getDATA().indication(DELEGATE(&DSMEPlatform::handleDataIndication, *this));
    this->mcps_sap.getDATA().confirm(DELEGATE(&DSMEPlatform::handleDataConfirm, *this));

    this->dsme.initialize(this);

    this->initialized = true;
}

void DSMEPlatform::startScan()
{
    channelList_t scanChannels;
    scanChannels.add(26);
    uint8_t scanDuration = 8;
    if(!this->scanOrSyncInProgress) {
        this->scanOrSyncInProgress = true;
        DSME_ASSERT(!this->syncActive);

        //this->recordedPanDescriptors.clear();

        mlme_sap::SCAN::request_parameters params;

        LOG_INFO("Initiating passive scan");
        params.scanType = ScanType::PASSIVE;

        params.scanChannels = scanChannels;
        params.scanDuration = scanDuration;
        params.channelPage = this->phy_pib.phyCurrentPage;
        params.linkQualityScan = false;

        this->mlme_sap.getSCAN().request(params);
    } else {
        LOG_INFO("Scan already in progress.");
    }
}

void DSMEPlatform::startAssociation()
{
    if(!this->associationInProgress) {
        startScan();
    } else {
        LOG_INFO("Association already in progress.");
    }
}

void DSMEPlatform::start()
{
    assert(this->initialized);
    this->dsme.start();
    if(!this->mac_pib.macAssociatedPANCoord) {
        LOG_DEBUG("Device is not associated with PAN.");
        startAssociation();
    }
}

void DSMEPlatform::handleDataIndication(mcps_sap::DATA_indication_parameters& params)
{
    assert(false);
    LOG_DEBUG("Received DATA message from MCPS.");
}

void DSMEPlatform::handleDataConfirm(mcps_sap::DATA_confirm_parameters& params)
{
    assert(false);
}

void DSMEPlatform::handleDataMessageFromMCPSWrapper(IDSMEMessage* msg)
{
    assert(false);
    this->handleDataMessageFromMCPS(static_cast<DSMEMessage*>(msg));
}

void DSMEPlatform::handleDataMessageFromMCPS(DSMEMessage* msg)
{
    assert(false);
}

bool DSMEPlatform::isReceptionFromAckLayerPossible()
{
    assert(false);
    return true;
}

void DSMEPlatform::handleReceivedMessageFromAckLayer(IDSMEMessage* message)
{
    assert(false);
}

DSMEMessage *DSMEPlatform::getEmptyMessage()
{
    DSMEMessage *msg = &this->head->msg;
    this->head = head->next;
    assert(msg);
    msg->pkt = NULL;
    msg->receivedViaMCPS = false;
    signalNewMsg(msg);
    return msg;
}

void DSMEPlatform::releaseMessage(IDSMEMessage* msg)
{
    DSMEMessagePool *entry = (DSMEMessagePool*) msg;
    gnrc_pktbuf_release(entry->msg.pkt);
    entry->next = this->head;
    this->head = entry;
}

void DSMEPlatform::startTimer(uint32_t symbolCounterValue)
{
    uint32_t delta = (symbolCounterValue - getSymbolCounter()) << 4;
    ztimer_set(ZTIMER_USEC, &timer, delta);
}

uint32_t DSMEPlatform::getSymbolCounter()
{
    return ztimer_now(ZTIMER_USEC) >> 4;
}

void DSMEPlatform::scheduleStartOfCFP()
{
}

void DSMEPlatform::signalAckedTransmissionResult(bool success, uint8_t transmissionAttempts, IEEE802154MacAddress receiver)
{

}

#if 0
void DSMEPlatform::signalGTSChange(bool deallocation, IEEE802154MacAddress counterpart) {
}

void DSMEPlatform::signalQueueLength(uint32_t length) {
}

void DSMEPlatform::signalPacketsTXPerSlot(uint32_t packets) {
}

void DSMEPlatform::signalPacketsRXPerSlot(uint32_t packets) {
}

void DSMEPlatform::signalPacketsPerCAP(uint32_t packets) {
}

void DSMEPlatform::signalFailedPacketsPerCAP(uint32_t packets) {
}

void DSMEPlatform::signalFailedCCAs(uint32_t failedAttempts) {
}

void DSMEPlatform::signalPRRCAP(double prr) {
}

void DSMEPlatform::signalSuccessPacketsCAP(uint32_t packets) {
}
#endif

bool DSMEPlatform::setChannelNumber(uint8_t channel)
{
    return true;
}

uint8_t DSMEPlatform::getChannelNumber()
{
    assert(false);
    return 0;
}

bool DSMEPlatform::prepareSendingCopy(IDSMEMessage* msg, Delegate<void(bool)> txEndCallback)
{
    DSMEMessage *m = (DSMEMessage*) msg;
    gnrc_pktsnip_t *pkt = m->pkt;
    DSMEPlatform::state = STATE_SEND;
    DSMEPlatform::txEndCallback = txEndCallback;
    uint8_t mhr[23];
    uint8_t mhr_len = msg->getHeader().getSerializationLength();
    uint8_t *p = mhr;
    msg->getHeader().serializeTo(p);
    iolist_t iol = {
        .iol_next = (iolist_t*) pkt,
        .iol_base = mhr,
        .iol_len = mhr_len,
    };
    return true;
}

bool DSMEPlatform::sendNow()
{
    assert(false);
    return true;
}

void DSMEPlatform::abortPreparedTransmission()
{
    assert(false);
}

bool DSMEPlatform::sendDelayedAck(IDSMEMessage* ackMsg, IDSMEMessage* receivedMsg, Delegate<void(bool)> txEndCallback)
{
    assert(false);
    return true;
}

void DSMEPlatform::setReceiveDelegate(receive_delegate_t receiveDelegate)
{
    this->receiveFromAckLayerDelegate = receiveDelegate;
}

bool DSMEPlatform::startCCA()
{
    assert(false);
    return true;
}

void DSMEPlatform::turnTransceiverOn()
{
    //puts("Radio on! :)");
}

void DSMEPlatform::turnTransceiverOff()
{
    //puts("Radio off! :)");
}

}
