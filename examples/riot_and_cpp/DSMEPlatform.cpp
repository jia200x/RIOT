#include "DSMEPlatform.h"
#include "ztimer.h"
#include "iolist.h"
#include "event.h"
#include "event/thread.h"

#include "cc2538_rf.h"
#include "net/ieee802154/radio.h"

extern ieee802154_dev_t cc2538_rf_dev;

namespace dsme {

ztimer_t send_timer;
event_t send_timer_ev;
DSMEPlatform* DSMEPlatform::instance = nullptr;
uint8_t DSMEPlatform::state = STATE_READY;
Delegate<void(bool)> DSMEPlatform::txEndCallback;
static event_t timer_event;
static event_t tx_done_event;


static void _send_timer_ev_handler(event_t *ev)
{
    dsme::DSMEPlatform::instance-> send_pkt();
    ztimer_set(ZTIMER_USEC, &send_timer, 2000000);
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
    dsme::DSMEPlatform::txEndCallback(true);
}

static void _hal_radio_cb(ieee802154_dev_t *dev, ieee802154_trx_ev_t status)
{
    switch (status) {
        case IEEE802154_RADIO_CONFIRM_TX_DONE:
            event_post(EVENT_PRIO_HIGHEST, &tx_done_event);
            break;
        case IEEE802154_RADIO_INDICATION_RX_START:
            break;
        case IEEE802154_RADIO_INDICATION_CRC_ERROR:
            break;
        case IEEE802154_RADIO_INDICATION_TX_START:
            break;
        case IEEE802154_RADIO_INDICATION_RX_DONE:
            break;
        case IEEE802154_RADIO_CONFIRM_CCA:
            break;
        default:
            puts(";/");
            assert(false);
    }
}

static void _timer_cb(void *arg)
{
    event_post(EVENT_PRIO_HIGHEST, &timer_event);
}


void DSMEPlatform::send_pkt()
{
    if(!this->mac_pib.macAssociatedPANCoord) {
        puts("Discarding message");
        return;
    }

    IDSMEMessage* message = getEmptyMessage();
    IEEE802154MacAddress dst;
    dst.setShortAddress(0x1234);
    mcps_sap::DATA::request_parameters params;

    message->getHeader().setSrcAddrMode(SHORT_ADDRESS);
    message->getHeader().setDstAddrMode(SHORT_ADDRESS);
    message->getHeader().setDstAddr(dst);

    message->getHeader().setSrcPANId(this->mac_pib.macPANId);
    message->getHeader().setDstPANId(this->mac_pib.macPANId);

    // Both PAN IDs are equal and we are using short addresses
    // so suppress the PAN ID -> This should not be the task
    // for the user of the MCPS, but it is specified like this... TODO
    params.panIdSuppressed = true;

    params.msdu = message;
    params.msduHandle = 0; // TODO
    //params.gtsTx = !dst.isBroadcast();
    //params.gtsTx = par("gtsTx");
    params.gtsTx = false;
    params.ackTx = false;
    params.indirectTx = false;
    params.ranging = NON_RANGING;
    params.uwbPreambleSymbolRepetitions = 0;
    params.dataRate = 0; // DSSS -> 0
    params.seqNumSuppressed = false;
    params.sendMultipurpose = false;

    this->mcps_sap.getDATA().request(params);
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

    send_timer.callback = _send_timer_cb;
    send_timer.arg = this;
    send_timer_ev.handler = _send_timer_ev_handler;
    timer_event.handler = _timer_ev_handler;
    tx_done_event.handler = _tx_done_handler;
    
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

    cc2538_rf_dev.cb = _hal_radio_cb;
    cc2538_init();
    ieee802154_radio_request_on(&cc2538_rf_dev);
    while (ieee802154_radio_confirm_on(&cc2538_rf_dev) == -EAGAIN) {}
    ieee802154_radio_set_csma_params(&cc2538_rf_dev, NULL, -1);

    /* Call more radio stuff here... */

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

    ztimer_set(ZTIMER_USEC, &send_timer, 2005000);
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
    uint32_t delta = (symbolCounterValue - getSymbolCounter() - 1) << 4;
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
    ieee802154_phy_conf_t conf = {
        .phy_mode = IEEE802154_PHY_OQPSK,
        .channel = channel,
        .page = 0,
        .pow = 6,
    };
    int res = ieee802154_radio_config_phy(&cc2538_rf_dev, &conf);
    assert(res == 0);
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
    int res = ieee802154_radio_request_set_trx_state(&cc2538_rf_dev, IEEE802154_TRX_STATE_TX_ON);
    assert(res == 0);
    while(ieee802154_radio_confirm_set_trx_state(&cc2538_rf_dev) == -EAGAIN);
    res = ieee802154_radio_write(&cc2538_rf_dev, &iol);
    assert(res == 0);

    return true;
}

bool DSMEPlatform::sendNow()
{
    puts("S");
    int res = ieee802154_radio_request_transmit(&cc2538_rf_dev);
    assert(res == 0);
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
    int res = ieee802154_radio_request_set_trx_state(&cc2538_rf_dev, IEEE802154_TRX_STATE_RX_ON);
    assert(res == 0);
    while(ieee802154_radio_confirm_set_trx_state(&cc2538_rf_dev) == -EAGAIN) {}
}

void DSMEPlatform::turnTransceiverOff()
{
    int res = ieee802154_radio_request_set_trx_state(&cc2538_rf_dev, IEEE802154_TRX_STATE_TRX_OFF);
    assert(res == 0);
    while(ieee802154_radio_confirm_set_trx_state(&cc2538_rf_dev) == -EAGAIN) {}
}

}
