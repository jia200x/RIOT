#include "DSMEPlatform.h"
#include "ztimer.h"
#include "iolist.h"
#include "event.h"
#include "event/thread.h"
#include "luid.h"
#include "dsmeAdaptionLayer/scheduling/TPS.h"

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

static void _cca_timer_ev_handler(event_t *ev)
{
    dsme::DSMEPlatform::instance->getDSME().dispatchCCAResult(true);
}

static void _request_slot_ev_timer(event_t *ev)
{
    
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
    dsme::DSMEPlatform::instance->send_pkt(0x9fad);
    ztimer_set(ZTIMER_USEC, &send_timer, 2000000);
    puts("S");
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
    res = ieee802154_radio_request_set_trx_state(&_radio, IEEE802154_TRX_STATE_RX_ON);
    DSME_ASSERT(res == 0);
    while(ieee802154_radio_confirm_set_trx_state(&_radio) == -EAGAIN) {}
    if (wait_for_ack) {
        wait_for_ack = false;
        ieee802154_radio_set_frame_filter_mode(&_radio, IEEE802154_FILTER_ACK_ONLY);
    }
    else {
        ieee802154_radio_set_frame_filter_mode(&_radio, IEEE802154_FILTER_ACCEPT);
    }
    dsme::DSMEPlatform::txEndCallback(true);
}

void DSMEPlatform::handle_rx()
{
    DSMEMessage *message = getEmptyMessage();
    message->setStartOfFrameDelimiterSymbolCounter(rx_sfd);

    int res;
    changed = true;
    //puts("R");
    //puts("TRX_OFF");
    while((res = ieee802154_radio_request_set_trx_state(&_radio, IEEE802154_TRX_STATE_TRX_OFF) == -EBUSY)) {}
    DSME_ASSERT(res == 0);
    while (ieee802154_radio_confirm_set_trx_state(&_radio) == -EAGAIN) {}
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

    res = ieee802154_radio_request_set_trx_state(&_radio, IEEE802154_TRX_STATE_RX_ON);
    DSME_ASSERT(res == 0);
    while(ieee802154_radio_confirm_set_trx_state(&_radio) == -EAGAIN) {}

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

    puts("S");
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

#if 0
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
#endif
    this->dsmeAdaptionLayer.sendMessage(message);
}

DSMEPlatform::DSMEPlatform() :
				phy_pib(),
				mac_pib(phy_pib),

				mcps_sap(dsme),
				mlme_sap(dsme),
                dsmeAdaptionLayer(dsme),
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
    cca_timer.callback = _cca_timer_cb;
    cca_timer.arg = this;
    acktimer.callback = _acktimer_cb;
    acktimer.arg = this;
    acktimer_ev.handler = _acktimer_ev_handler;
    cca_timer_ev.handler = _cca_timer_ev_handler;
    timer_event.handler = _timer_ev_handler;
    tx_done_event.handler = _tx_done_handler;
    rx_done_event.handler = _rx_done_handler;
    
    for (int i=0; i<7; i++) {
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
    this->mac_pib.macSuperframeOrder = 6;
    this->mac_pib.macMultiSuperframeOrder = 6;
    this->mac_pib.macBeaconOrder = 7;

    this->mac_pib.macMinBE = 7;
    this->mac_pib.macMaxBE = 8;
    this->mac_pib.macMaxCSMABackoffs = 5;
    this->mac_pib.macMaxFrameRetries = 3;

    this->mac_pib.macDSMEGTSExpirationTime = 255;
    this->mac_pib.macResponseWaitTime = 244;

    this->phy_pib.phyCurrentChannel = 26;
		
#if 0
    this->mlme_sap.getASSOCIATE().indication(DELEGATE(&DSMEPlatform::handleASSOCIATION_indication, *this));
    this->mlme_sap.getASSOCIATE().confirm(DELEGATE(&DSMEPlatform::handleASSOCIATION_confirm, *this));
    this->mcps_sap.getDATA().indication(DELEGATE(&DSMEPlatform::handleDataIndication, *this));
    this->mcps_sap.getDATA().confirm(DELEGATE(&DSMEPlatform::handleDataConfirm, *this));
    this->mlme_sap.getSCAN().confirm(DELEGATE(&DSMEPlatform::handleSCAN_confirm, *this));

    this->mlme_sap.getSYNC_LOSS().indication(DELEGATE(&DSMEPlatform::handleSyncLossIndication, *this));
    this->mlme_sap.getBEACON_NOTIFY().indication(DELEGATE(&DSMEPlatform::handleBEACON_NOTIFY_indication, *this));
    this->mlme_sap.getDSME_GTS().indication(DELEGATE(&DSMEPlatform::handleDSME_GTS_indication, *this));
    this->mlme_sap.getDSME_GTS().confirm(DELEGATE(&DSMEPlatform::handleDSME_GTS_confirm, *this));
#endif
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

void DSMEPlatform::handleASSOCIATION_indication(mlme_sap::ASSOCIATE_indication_parameters& params) {
    //LOG_INFO("Association requested from 0x" << params.deviceAddress.getShortAddress() << ".");

    mlme_sap::ASSOCIATE::response_parameters response_params;
    response_params.deviceAddress = params.deviceAddress;
    response_params.assocShortAddress = params.deviceAddress.a4();
    response_params.status = AssociationStatus::SUCCESS;
    response_params.channelOffset = this->mac_pib.macChannelOffset;
    if(params.hoppingSequenceRequest == true || params.hoppingSequenceId == 1) {
        response_params.hoppingSequence = this->mac_pib.macHoppingSequenceList;
    }

    // TODO update list of associated devices

    this->mlme_sap.getASSOCIATE().response(response_params);
    puts("IND");
    return;
}

void DSMEPlatform::handleASSOCIATION_confirm(mlme_sap::ASSOCIATE_confirm_parameters& params) {
    //this->associationCompleteDelegate(params.status);
    if(params.status == AssociationStatus::SUCCESS) {
        puts("Association completed successfully.");
        ieee802154_radio_config_addr_filter(&_radio, IEEE802154_AF_PANID, &this->mac_pib.macPANId);
        ztimer_set(ZTIMER_USEC, &send_timer, 2005000);
#if 0
        if (!par("isPANCoordinator")) {
            request_slot();
            EV_INFO << "Node doesn't have slot. Requesting one" << endl;
        }
#endif

    } else {
        puts("Association failed.");
        //this->scanOrSyncInProgress = false;
        //startAssociation();
    }
}

void DSMEPlatform::associate(uint16_t coordPANId, AddrMode addrMode, IEEE802154MacAddress& coordAddress, uint8_t channel)
{
    CapabilityInformation capabilityInformation;
    capabilityInformation.alternatePANCoordinator = false;
    capabilityInformation.deviceType = 1;
    capabilityInformation.powerSource = 0;
    capabilityInformation.receiverOnWhenIdle = 1;
    capabilityInformation.associationType = 1; // TODO: FastA? 1 -> yes, 0 -> no
    capabilityInformation.reserved = 0;
    capabilityInformation.securityCapability = 0;
    capabilityInformation.allocateAddress = 1;

    mlme_sap::ASSOCIATE::request_parameters params;
    params.channelNumber = channel;
    params.channelPage = this->phy_pib.phyCurrentPage;
    params.coordAddrMode = SHORT_ADDRESS;
    params.coordPanId = coordPANId;
    params.coordAddress = coordAddress;
    params.capabilityInformation = capabilityInformation;
    params.channelOffset = this->mac_pib.macChannelOffset;
    params.hoppingSequenceId = 1;
    params.hoppingSequenceRequest = true;

    // TODO start timer for macMaxFrameTotalWaitTime, report NO_DATA on timeout
    this->mlme_sap.getASSOCIATE().request(params);
}

void DSMEPlatform::handleBEACON_NOTIFY_indication(mlme_sap::BEACON_NOTIFY_indication_parameters& params) {
    if(!this->mac_pib.macAutoRequest) {
        //this->recordedPanDescriptors.add(params.panDescriptor);
    }

    uint16_t shortAddress = params.panDescriptor.coordAddress.getShortAddress();
#if 0
    if(!heardCoordinators.contains(shortAddress)) {
        heardCoordinators.add(shortAddress);
    }
#endif

    if(this->syncActive) {
        // TODO check if the beacon is actually from the PAN described in the activePanDescriptor
        this->syncActive = false;
        /* Associate */
        puts("ASSOCIATE");
        associate(this->panDescriptorToSyncTo.coordPANId, this->panDescriptorToSyncTo.coordAddrMode, this->panDescriptorToSyncTo.coordAddress,
                                                                 this->panDescriptorToSyncTo.channelNumber);
        //handleSCAN_confirm(&this->panDescriptorToSyncTo);
    }

    // TODO CROSS-LAYER-CALLS, no interface for this information
#if 0
    LOG_INFO("Checking whether to become a coordinator: "
             << "isAssociated:" << this->mac_pib.macAssociatedPANCoord << ", isCoordinator:"
             << this->mac_pib.macIsCoord << ", numHeardCoordinators:" << ((uint16_t)heardCoordinators.getLength()) << ".");
    if(this->mac_pib.macAssociatedPANCoord && !this->mac_pib.macIsCoord && heardCoordinators.getLength() < 2) {
        uint16_t random_value = this->dsmeAdaptionLayer.getDSME().getPlatform().getRandom() % 3;
        if(random_value < 1) {
            mlme_sap::START::request_parameters request_params;
            request_params.panCoordinator = false;
            // TODO: fill rest;

            LOG_INFO("Turning into a coordinator now.");
            this->dsmeAdaptionLayer.getMLME_SAP().getSTART().request(request_params);

            mlme_sap::START_confirm_parameters confirm_params;
            bool confirmed = this->dsmeAdaptionLayer.getMLME_SAP().getSTART().confirm(&confirm_params);
            DSME_ASSERT(confirmed);
            DSME_ASSERT(confirm_params.status == StartStatus::SUCCESS);
        }
    }

    return;
#endif
}

void DSMEPlatform::handleSyncLossIndication(mlme_sap::SYNC_LOSS_indication_parameters& params) {
    puts("handleSyncLossIndication");
    DSME_ASSERT(false);
#if 0
    if(params.lossReason == LossReason::BEACON_LOST) {
        LOG_ERROR("Beacon tracking lost.");
        // DSME_SIM_ASSERT(false); TODO
    } else {
        LOG_ERROR("Tracking lost for unsupported reason: " << (uint16_t)params.lossReason);
        DSME_ASSERT(false);
    }

    if(this->syncActive) {
        this->syncActive = false;
        this->scanAndSyncCompleteDelegate(nullptr);
    } else {
        this->syncLossAfterSyncedDelegate();
    }
    return;
#endif
}

void DSMEPlatform::handleSCAN_confirm(mlme_sap::SCAN_confirm_parameters& params) {
    PanDescriptorList* list = &params.panDescriptorList;
    if (list->size() > 0) {
        this->panDescriptorToSyncTo = (*list)[0];
        mlme_sap::SYNC::request_parameters syncParams;
        syncParams.channelNumber = this->panDescriptorToSyncTo.channelNumber;
        syncParams.channelPage = this->panDescriptorToSyncTo.channelPage;
        syncParams.trackBeacon = true;
        syncParams.syncParentShortAddress = this->panDescriptorToSyncTo.coordAddress.getShortAddress();
        syncParams.syncParentSdIndex = this->panDescriptorToSyncTo.dsmePANDescriptor.getBeaconBitmap().getSDIndex();
        this->syncActive = true;
        this->mlme_sap.getSYNC().request(syncParams);
        puts("FOUND CANDIDATES. Request sync");
    }
    else {
        puts("FOUND NO CANDIDATES");
    }
#if 0
    DSME_ASSERT(!this->syncActive);

    PanDescriptorList* list;

    if(!this->mac_pib.macAutoRequest) {
        list = &this->recordedPanDescriptors;
    } else {
        list = &params.panDescriptorList;
    }

    if(list->size() == 0) {
        //EV_INFO << "SCAN: Found no candidates" << endl;
        this->scanOrSyncInProgress = false;
        startScan();
        return;
    }

    // Find the coordinator with the best LQI,
    // then find the coordinator with this LQI,
    // but the best RSSI.

    uint8_t bestLQI = 0;
    int8_t bestRSSI = INT8_MIN;
    uint8_t bestIdx = 0xFF;

    for(uint8_t i = 0; i < list->size(); i++) {
        if((*list)[i].linkQuality > bestLQI) {
            bestLQI = (*list)[i].linkQuality;
        }
    }

    if(bestLQI < this->dsme->getPlatform().getMinCoordinatorLQI()) {
        //this->scanAndSyncCompleteDelegate(nullptr);
        //EV_INFO << "SCAN: No candidate with LQI above threshold" << endl;
        startScan();
        return;
    }

    for(uint8_t i = 0; i < list->size(); i++) {
        if((*list)[i].linkQuality == bestLQI) {
            if((*list)[i].rssi > bestRSSI) {
                bestRSSI = (*list)[i].rssi;
                bestIdx = i;
            }
        }
    }

    this->panDescriptorToSyncTo = (*list)[bestIdx];

    mlme_sap::SYNC::request_parameters syncParams;
    syncParams.channelNumber = this->panDescriptorToSyncTo.channelNumber;
    syncParams.channelPage = this->panDescriptorToSyncTo.channelPage;
    syncParams.trackBeacon = true;
    syncParams.syncParentShortAddress = this->panDescriptorToSyncTo.coordAddress.getShortAddress();
    syncParams.syncParentSdIndex = this->panDescriptorToSyncTo.dsmePANDescriptor.getBeaconBitmap().getSDIndex();
    this->syncActive = true;
    //EV_INFO << "SCAN: complete. Requesting to sync" << endl;
    this->mlme_sap.getSYNC().request(syncParams);

    return;
#endif
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
    DSME_ASSERT(this->initialized);
    this->dsme.start();
    this->dsmeAdaptionLayer.startAssociation();
#if 0
    if(!this->mac_pib.macAssociatedPANCoord) {
        LOG_DEBUG("Device is not associated with PAN.");
        startAssociation();
    }
#endif
}

void DSMEPlatform::handleDataIndication(mcps_sap::DATA_indication_parameters& params)
{
    puts("Received DATA message from MCPS. :)");
    IDSMEMessage *msg = params.msdu;
    releaseMessage(msg);
    if (PAN_COORD) {
        send_pkt(msg->getHeader().getSrcAddr().getShortAddress());
    }
}

void DSMEPlatform::handleDataConfirm(mcps_sap::DATA_confirm_parameters& params)
{
    IDSMEMessage* msg = params.msduHandle;
    if(params.status != DataStatus::SUCCESS) {
        puts("FAILED TO SEND DATA");
    }
    else {
        puts("SUCCESS");
    }
    releaseMessage(msg);
    /* TODO */
}

void DSMEPlatform::handleDataMessageFromMCPSWrapper(IDSMEMessage* msg)
{
    this->handleDataMessageFromMCPS(static_cast<DSMEMessage*>(msg));
}

void DSMEPlatform::handleConfirmFromMCPSWrapper(IDSMEMessage* msg, DataStatus::Data_Status dataStatus) {
    this->handleConfirmFromMCPS(static_cast<DSMEMessage*>(msg), dataStatus);
}
void DSMEPlatform::handleConfirmFromMCPS(DSMEMessage* msg, DataStatus::Data_Status dataStatus) {
    puts(":)");
    if (dataStatus == DataStatus::Data_Status::SUCCESS) {
        puts("NO TE LO PUEDO CREER!");
    }
    printf("%02x\n", (int) dataStatus );
    IDSMEMessage *m = static_cast<IDSMEMessage*>(msg);
    releaseMessage(m);
    int a=0;
    for (int i=0;i<7;i++) {
        if (this->pool[i].free) {
            a++;
        }
    }
    printf("%02x\n", a);
}

void DSMEPlatform::handleDataMessageFromMCPS(DSMEMessage* msg)
{
    puts("LLEGO?");
    IDSMEMessage *m = static_cast<IDSMEMessage*>(msg);
    releaseMessage(m);
}

bool DSMEPlatform::isReceptionFromAckLayerPossible()
{
    /* TODO: */
    return true;
}

void DSMEPlatform::handleReceivedMessageFromAckLayer(IDSMEMessage* message)
{
    DSME_ASSERT(receiveFromAckLayerDelegate);
    receiveFromAckLayerDelegate(message);
}

int a;
int b;
DSMEMessage *DSMEPlatform::getEmptyMessage()
{
    DSMEMessage *msg = NULL;
    for (int i=0; i<7; i++) {
        if (this->pool[i].free) {
            msg = &this->pool[i];
            break;
        }
    }
    if (!msg) {
        printf("%02x %02x", a, b);
        gnrc_pktbuf_stats();
    }
    DSME_ASSERT(msg);
    a++;
    msg->pkt = NULL;
    msg->free = false;
    msg->receivedViaMCPS = false;
    signalNewMsg(msg);
    return msg;
}

void DSMEPlatform::releaseMessage(IDSMEMessage* msg)
{
    DSMEMessage *m = static_cast<DSMEMessage*>(msg);
    DSME_ASSERT(!m->free);
    if (m->pkt) {
        gnrc_pktbuf_release(m->pkt);
    }
    m->free = true;
    b++;
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
    dsme::DSMEPlatform::instance->getDSME().handleStartOfCFP();
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
    int res;
    changed = true;
    //puts("TRX_OFF");
    while((res = ieee802154_radio_request_set_trx_state(&_radio, IEEE802154_TRX_STATE_TRX_OFF) == -EBUSY)) {}
    DSME_ASSERT(res == 0);
    while (ieee802154_radio_confirm_set_trx_state(&_radio) == -EAGAIN) {}
    res = ieee802154_radio_config_phy(&_radio, &conf);
    DSME_ASSERT(res == 0);
    //puts("RXON");
    res = ieee802154_radio_request_set_trx_state(&_radio, IEEE802154_TRX_STATE_RX_ON);
    DSME_ASSERT(res == 0);
    while(ieee802154_radio_confirm_set_trx_state(&_radio) == -EAGAIN) {}
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
    wait_for_ack= false;
    /* TODO */
    if (mhr[0] & 0x20) {
        wait_for_ack = true;
    }
    if ((mhr[0] & IEEE802154_FCF_TYPE_MASK) == IEEE802154_FCF_TYPE_ACK) {
        //puts("A");
    }
    changed = true;
    int res = ieee802154_radio_request_set_trx_state(&_radio, IEEE802154_TRX_STATE_TX_ON);
    DSME_ASSERT(res == 0);
    while(ieee802154_radio_confirm_set_trx_state(&_radio) == -EAGAIN);
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
    DSME_ASSERT(false);
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
    int res = ieee802154_radio_request_set_trx_state(&_radio, IEEE802154_TRX_STATE_TX_ON);
    DSME_ASSERT(res == 0);
    res = ieee802154_radio_write(&_radio, &iol);
    DSME_ASSERT(res == 0);
    while(ieee802154_radio_confirm_set_trx_state(&_radio) == -EAGAIN);

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
    int res = ieee802154_radio_request_set_trx_state(&_radio, IEEE802154_TRX_STATE_TRX_OFF);
    DSME_ASSERT(res == 0);
    while(ieee802154_radio_confirm_set_trx_state(&_radio) == -EAGAIN) {}
}

}
