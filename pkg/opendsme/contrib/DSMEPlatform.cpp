#include "opendsme/opendsme.h"
#include "opendsme/DSMEPlatform.h"
#include "ztimer.h"
#include "iolist.h"
#include "event.h"
#include "event/thread.h"
#include "luid.h"
#include "dsmeAdaptionLayer/scheduling/TPS.h"
#include "dsmeAdaptionLayer/scheduling/StaticScheduling.h"
#include "board.h"

#ifdef MODULE_SX127X
#include "sx127x.h"
static sx127x_t sx127x_dev;
#endif

#if IS_USED(MODULE_CC2538_RF)
#include "cc2538_rf.h"
#endif

#if IS_USED(MODULE_NRF802154)
#include "nrf802154.h"
#endif

#ifndef CAP_REDUCTION
#define CAP_REDUCTION 0
#endif

#ifndef USE_CAD
#define USE_CAD 1
#endif

#ifndef CONFIG_DSME_PLATFORM_ACK_REQ
#define CONFIG_DSME_PLATFORM_ACK_REQ 1
#endif

#ifndef CONFIG_DSME_PLATFORM_SF_PER_MSF
#define CONFIG_DSME_PLATFORM_SF_PER_MSF (2)
#endif

#include "net/ieee802154/radio.h"

ieee802154_dev_t _radio;
event_queue_t *dsme_ev;
extern "C" {
extern void heap_stats(void);
uint16_t get_node_id();
void sx127x_init_dsme(sx127x_t *dev, void *radio);
void sx127x_hal_task_handler(ieee802154_dev_t *hal);
event_t sx127x_ev;
void sx127x_isr(void *arg)
{
    (void) arg;
    event_post(dsme_ev, &sx127x_ev);
}
}

namespace dsme {

event_t cca_ev;
ztimer_t acktimer;
ztimer_t cca_timer; /* used only if USE_CAD == 0 */
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
static bool pending_tx;

#ifdef MODULE_SX127X

void _sx127x_handler(event_t *event)
{
    (void) event;
    sx127x_hal_task_handler(&_radio);
}


#endif

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
    dsme::DSMEPlatform::instance->updateVisual();
}

static event_t start_of_cfp_ev;

static void _cca_ev_handler(event_t *ev)
{
    bool clear;
    if (IS_ACTIVE(USE_CAD)) {
        clear = ieee802154_radio_confirm_cca(&_radio);
    }
    else {
        clear = true;
    }
    printf("[info];CCA;");
    if (clear) {
        printf("1");
    }
    else {
        printf("0");
    }
    printf("\n");
    DSMEPlatform::state = DSMEPlatform::STATE_READY;
    dsme::DSMEPlatform::instance->getDSME().dispatchCCAResult(clear);
}

static void _acktimer_ev_handler(event_t *ev)
{
    dsme::DSMEPlatform::instance->sendNow();
}

static void _acktimer_cb(void *arg)
{
    event_post(dsme_ev, &acktimer_ev);
}

static void _cca_timer_cb(void *arg)
{
    event_post(dsme_ev, &cca_ev);
}

static void _timer_ev_handler(event_t *ev)
{
    dsme::DSMEPlatform::instance->getDSME().getEventDispatcher().timerInterrupt();
}

static void _tx_done_handler(event_t *ev)
{
    int res = ieee802154_radio_confirm_transmit(&_radio, NULL);
    pending_tx = false;
    DSME_ASSERT(res >= 0);
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
    puts("TXD");
    DSMEPlatform::state = DSMEPlatform::STATE_READY;
}

void DSMEPlatform::handle_rx()
{
    if (DSMEPlatform::state != STATE_READY) {
        puts("[info];NOT_READY");
        return;
    }

    DSMEMessage *message = getEmptyMessage();
    message->setStartOfFrameDelimiterSymbolCounter(rx_sfd);

    int res;
    res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
    int len = ieee802154_radio_len(&_radio);
    if (len > 127 || len < 0) {
        puts("DROP");
        ieee802154_radio_read(&_radio, NULL, 127, NULL);
        res = ieee802154_radio_set_rx(&_radio);
        DSME_ASSERT(res == 0);
        return;
    }
    res = message->loadBuffer(len);
    DSME_ASSERT(res >= 0);
    ieee802154_rx_info_t info;

    res = ieee802154_radio_read(&_radio, message->getPayload(), 127, &info);
    if (res < 0) {
        puts(":(");
        message->releaseMessage();
        res = ieee802154_radio_set_rx(&_radio);
        DSME_ASSERT(res == 0);
        return;
    }
    uint8_t *p = (uint8_t*) message->getPayload();
    printf("RECV: ");
    for (unsigned i=0;i<res;i++) {
        printf("%02x ", p[i]);
    }
    printf("\n");
    message->messageLQI = info.lqi;
    message->radio_last_rssi = info.rssi;
    const uint8_t *buf = message->getPayload();

    bool success = message->getHeader().deserializeFrom(buf, len);
    if (!success) {
        puts(":/");
        message->releaseMessage();
        res = ieee802154_radio_set_rx(&_radio);
        DSME_ASSERT(res == 0);
        return;
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
            event_post(dsme_ev, &tx_done_event);
            break;
        case IEEE802154_RADIO_INDICATION_RX_START:
            rx_sfd = dsme::DSMEPlatform::instance->getSymbolCounter();
            break;
        case IEEE802154_RADIO_INDICATION_CRC_ERROR:
            break;
        case IEEE802154_RADIO_INDICATION_TX_START:
            break;
        case IEEE802154_RADIO_INDICATION_RX_DONE:
            event_post(dsme_ev, &rx_done_event);
            break;
        case IEEE802154_RADIO_CONFIRM_CCA:
            event_post(dsme_ev, &cca_ev);
            break;
        default:
            DSME_ASSERT(false);
    }
}

static void _timer_cb(void *arg)
{
    event_post(dsme_ev, &timer_event);
}

void DSMEPlatform::send_pkt(uint16_t addr, iolist_t *pkt)
{
    /* First 2 bytes are the ID */
    uint8_t *id = static_cast<uint8_t*>(pkt->iol_base);
    printf("[info];TXTSND;%02x:%02x;%02x%02x\n",addr>>8,addr & 0xFF, id[0] << 8, id[1]);
    if(!this->mac_pib.macAssociatedPANCoord) {
        puts("Discarding message");
        return;
    }

    DSMEMessage* message = getEmptyMessage();
    if (message->loadBuffer(pkt) < 0) {
        puts("Couldn't load buffer");
        return;
    }

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

    acktimer.callback = _acktimer_cb;
    acktimer.arg = this;
    cca_timer.callback = _cca_timer_cb;
    cca_timer.arg = this;
    acktimer_ev.handler = _acktimer_ev_handler;
    cca_ev.handler = _cca_ev_handler;
    timer_event.handler = _timer_ev_handler;
    tx_done_event.handler = _tx_done_handler;
    rx_done_event.handler = _rx_done_handler;
    rx_offload_ev.handler = _handle_rx_offload;
    start_of_cfp_ev.handler = _start_of_cfp_handler;
#if IS_USED(MODULE_SX127X)
    sx127x_ev.handler = _sx127x_handler;
#endif
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

void DSMEPlatform::initialize(bool pan_coord)
{
    printf("[config];CAP;%01x\n",IS_ACTIVE(CONFIG_OPENDSME_USE_CAP));
    printf("[config];PAYLOAD_LENGTH;%01x\n", CONFIG_OPENDSME_PAYLOAD_LENGTH);
    printf("[config];USE_CAD;%01x\n",IS_ACTIVE(USE_CAD));
    printf("[config];CAP_REDUCTION;%01x\n",CAP_REDUCTION);
    printf("[config];STATIC_GTS;%01x\n",IS_ACTIVE(CONFIG_DSME_PLATFORM_STATIC_GTS));
    printf("[config];ACK_REQ;%01x\n",IS_ACTIVE(CONFIG_DSME_PLATFORM_ACK_REQ));
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

    uint16_t id = get_node_id();
    translateMacAddress(id, this->mac_pib.macExtendedAddress);

    _radio.cb = _hal_radio_cb;
#if IS_USED(MODULE_CC2538_RF)
    cc2538_rf_hal_setup(&_radio);
    cc2538_init();
#elif IS_USED(MODULE_NRF802154)
    nrf802154_hal_setup(&_radio);
    nrf802154_init();
#elif IS_USED(MODULE_SX127X)
    sx127x_init_dsme(&sx127x_dev, &_radio);
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

    this->mac_pib.macIsPANCoord = pan_coord;
    this->mac_pib.macIsCoord = pan_coord;
    if(this->mac_pib.macIsPANCoord) {
      DSME_PRINTF("This node is PAN coordinator\n");
      this->mac_pib.macPANId = 0x23;
    }
    ieee802154_radio_config_addr_filter(&_radio, IEEE802154_AF_PANID, &this->mac_pib.macPANId);
    ieee802154_radio_config_addr_filter(&_radio, IEEE802154_AF_SHORT_ADDR, &short_addr);
    ieee802154_radio_config_addr_filter(&_radio, IEEE802154_AF_EXT_ADDR, &ext_addr);

    this->mac_pib.macCapReduction = CAP_REDUCTION;

    this->mac_pib.macAssociatedPANCoord = this->mac_pib.macIsPANCoord;
    this->mac_pib.macSuperframeOrder = 3;
    this->mac_pib.macMultiSuperframeOrder = 3 + (CONFIG_DSME_PLATFORM_SF_PER_MSF - 1);
    this->mac_pib.macBeaconOrder = 4;

    this->mac_pib.macMinBE = 3;
    this->mac_pib.macMaxBE = 5;
    this->mac_pib.macMaxCSMABackoffs = 4;
    this->mac_pib.macMaxFrameRetries = 3;

    this->mac_pib.macDSMEGTSExpirationTime = 16;
    this->mac_pib.macResponseWaitTime = 244;
    this->mac_pib.macChannelDiversityMode = Channel_Diversity_Mode::CHANNEL_ADAPTATION;

    this->phy_pib.phyCurrentChannel = 12;
		
    this->dsmeAdaptionLayer.setIndicationCallback(DELEGATE(&DSMEPlatform::handleDataMessageFromMCPSWrapper, *this));
    this->dsmeAdaptionLayer.setConfirmCallback(DELEGATE(&DSMEPlatform::handleConfirmFromMCPSWrapper, *this));

    this->dsme.initialize(this);

    channelList_t scanChannels;
    scanChannels.add(12);
    if (IS_ACTIVE(CONFIG_DSME_PLATFORM_STATIC_GTS)) {
        StaticScheduling* staticScheduling = new StaticScheduling(this->dsmeAdaptionLayer);
        staticScheduling->setNegotiateChannels(false);
        scheduling = staticScheduling;
    }
    else {
        TPS* tps = new TPS(this->dsmeAdaptionLayer);
        tps->setAlpha(0.1);
        tps->setMinFreshness(this->mac_pib.macDSMEGTSExpirationTime);
        scheduling = tps;
    }
    this->dsmeAdaptionLayer.initialize(scanChannels,2,scheduling);
    this->initialized = true;
    dsme_ev = &this->netif.evq;
}

#if IS_ACTIVE(CONFIG_DSME_PLATFORM_STATIC_GTS)
void DSMEPlatform::allocateGTS(uint8_t superframeID, uint8_t slotID, uint8_t channelID, Direction direction, uint16_t address)
{
    static_cast<StaticScheduling*>(scheduling)->allocateGTS(superframeID, slotID, channelID, direction, address);
}
#endif

void DSMEPlatform::start()
{
    DSME_ASSERT(this->initialized);
    this->dsme.start();
    this->dsmeAdaptionLayer.startAssociation();
}

void DSMEPlatform::getShortAddress(network_uint16_t *addr)
{
    addr->u8[0] = this->mac_pib.macExtendedAddress.getShortAddress() >> 8;
    addr->u8[1] = this->mac_pib.macExtendedAddress.getShortAddress() & 0xFF;
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
        /* TODO: Add to statistics */
    }
    IDSMEMessage *m = static_cast<IDSMEMessage*>(msg);
    releaseMessage(m);
}

void DSMEPlatform::handleDataMessageFromMCPS(DSMEMessage* msg)
{
    msg->dispatchMessage();
}

bool DSMEPlatform::isReceptionFromAckLayerPossible()
{
    return DSMEPlatform::state == STATE_READY;
}

void DSMEPlatform::handleReceivedMessageFromAckLayer(IDSMEMessage* message)
{
    DSME_ASSERT(receiveFromAckLayerDelegate);
    DSME_ASSERT(!this->message);
    this->message = message;
    event_post(dsme_ev, &rx_offload_ev);
}

DSMEMessage *DSMEPlatform::getEmptyMessage()
{
    DSMEMessage *msg = new DSMEMessage();
    DSME_ASSERT(msg);
    msg->clearMessage();
    signalNewMsg(msg);
    return msg;
}

void DSMEPlatform::signalNewMsg(DSMEMessage* msg)
{
}

void DSMEPlatform::releaseMessage(IDSMEMessage* msg)
{
    DSMEMessage *m = static_cast<DSMEMessage*>(msg);
    m->releaseMessage();
    //delete m;
}

void DSMEPlatform::startTimer(uint32_t symbolCounterValue)
{
    uint32_t now = ztimer_now(ZTIMER_MSEC);
    /* This works even if there's an overflow */
    int32_t delta = ((symbolCounterValue - getSymbolCounter()));

    ztimer_set(ZTIMER_MSEC, &timer, (uint32_t) delta);
}

uint32_t DSMEPlatform::getSymbolCounter()
{
    return ztimer_now(ZTIMER_MSEC);
}

void DSMEPlatform::scheduleStartOfCFP()
{
    event_post(dsme_ev, &start_of_cfp_ev);
}

void DSMEPlatform::signalAckedTransmissionResult(bool success, uint8_t transmissionAttempts, IEEE802154MacAddress receiver)
{
    uint16_t short_addr = receiver.getShortAddress();

    printf("[info];TX_INFO;%02x;%02x;%04x\n", success, transmissionAttempts, short_addr);
}

/*
 * Signal GTS allocation or deallocation
 */
void DSMEPlatform::signalGTSChange(bool deallocation, IEEE802154MacAddress counterpart, uint16_t superframeID, uint8_t gtSlotID, uint8_t channel, Direction direction) {
    uint16_t short_addr = counterpart.getShortAddress();
    printf("[info];GTSC;");
    if (deallocation) {
        printf("DEALLOC");
    }
    else {
        printf("ALLOC");
    }
    printf(";%04x;%02x;%02x;%02x;%02x\n", short_addr, superframeID, gtSlotID, channel, direction);
}

void DSMEPlatform::signalQueueLength(uint32_t length) {
    printf("[info];QL;%02x\n", length);
}

/*
 * Number of packets sent per CAP
 */
void DSMEPlatform::signalPacketsPerCAP(uint32_t packets) {
    printf("[info];PCAP;%02x\n", packets);
}

/*
 * Number of failed packets per CAP
 */
void DSMEPlatform::signalFailedPacketsPerCAP(uint32_t packets) {
    printf("[info];FPPCAP;%04x\n", packets);
}

void DSMEPlatform::updateVisual()
{
    printf("[info];ASSOC;");
    if (isAssociated()) {
        printf("1");
    }
    else {
        printf("0");
    }
    printf("\n");
    printf("[info];");
    heap_stats();

}

bool DSMEPlatform::setChannelNumber(uint8_t channel)
{
    ieee802154_phy_conf_t conf = {
        .phy_mode = IEEE802154_PHY_OQPSK,
        .channel = channel,
        .page = 0,
        .pow = CONFIG_IEEE802154_DEFAULT_TXPOWER,
    };
    int res;
    res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
    res = ieee802154_radio_config_phy(&_radio, &conf);
    DSME_ASSERT(res == 0);

    /* TODO: Find a better solution */
    ieee802154_radio_config_addr_filter(&_radio, IEEE802154_AF_PANID, &this->mac_pib.macPANId);

    res = ieee802154_radio_set_rx(&_radio);
    DSME_ASSERT(res == 0);
    return true;
}

uint8_t DSMEPlatform::getChannelNumber()
{
    /* Apparently not used by OpenDSME */
    DSME_ASSERT(false);
    return 0;
}

bool DSMEPlatform::prepareSendingCopy(IDSMEMessage* msg, Delegate<void(bool)> txEndCallback)
{
    DSMEMessage *m = (DSMEMessage*) msg;
    DSMEPlatform::state = STATE_SEND;
    DSMEPlatform::txEndCallback = txEndCallback;
    uint8_t mhr[IEEE802154_MAX_HDR_LEN];
    uint8_t mhr_len = msg->getHeader().getSerializationLength();
    uint8_t *p = mhr;
    msg->getHeader().serializeTo(p);
    iolist_t iol = {
        .iol_next = (iolist_t*) m->getIolPayload(),
        .iol_base = mhr,
        .iol_len = mhr_len,
    };

    printf("[info];TX;");
    for (iolist_t *io=&iol;io;io=io->iol_next) {
        uint8_t *p = static_cast<uint8_t*>(io->iol_base);
        for (unsigned i=0;i<io->iol_len;i++) {
            printf("%02x ", p[i]);
        }
    }
    printf("\n");
    if (mhr[0] & IEEE802154_FCF_ACK_REQ) {
        wait_for_ack = true;
    }
    else {
        wait_for_ack= false;
    }

    int res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
    res = ieee802154_radio_write(&_radio, &iol);
    DSME_ASSERT(res == 0);

    return true;
}

bool DSMEPlatform::sendNow()
{
    int res = ieee802154_radio_request_transmit(&_radio);
    DSME_ASSERT(res == 0);
    pending_tx = true;
    return true;
}

void DSMEPlatform::abortPreparedTransmission()
{
     /* Nothing to do here, since the Radio HAL will drop the frame if
      * the write function is called again */
    puts("[info];ABORT");
}

bool DSMEPlatform::sendDelayedAck(IDSMEMessage* ackMsg, IDSMEMessage* receivedMsg, Delegate<void(bool)> txEndCallback)
{
    DSMEMessage *m = (DSMEMessage*) ackMsg;
    DSME_ASSERT(m != nullptr);

    uint8_t ack[IEEE802154_ACK_FRAME_LEN - IEEE802154_FCS_LEN];
    uint8_t mhr_len = ackMsg->getHeader().getSerializationLength();
    DSME_ASSERT(mhr_len == sizeof(ack));

    uint8_t *p = ack;
    ackMsg->getHeader().serializeTo(p);

    DSMEPlatform::txEndCallback = txEndCallback;

    iolist_t iol = {
        .iol_next = NULL,
        .iol_base = ack,
        .iol_len = mhr_len,
    };

    printf("[info];TX;");
    for (iolist_t *io=&iol;io;io=io->iol_next) {
        uint8_t *p = static_cast<uint8_t*>(io->iol_base);
        for (unsigned i=0;i<io->iol_len;i++) {
            printf("%02x ", p[i]);
        }
    }
    printf("\n");

    int res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
    res = ieee802154_radio_write(&_radio, &iol);
    DSME_ASSERT(res == 0);

    /* Preamble (4) | SFD (1) | PHY Hdr (1) | MAC Payload | FCS (2) */
    uint32_t endOfReception = receivedMsg->getStartOfFrameDelimiterSymbolCounter()
                              + receivedMsg->getTotalSymbols()
                              - 2 * 4  /* Preamble */
                              - 2 * 1; /* SFD */
    uint32_t ackTime = endOfReception + aTurnaroundTime;
    uint32_t now = getSymbolCounter();
    uint32_t diff = ackTime - now;

    ztimer_set(ZTIMER_MSEC, &acktimer, diff);
    return true;
}

void DSMEPlatform::setReceiveDelegate(receive_delegate_t receiveDelegate)
{
    this->receiveFromAckLayerDelegate = receiveDelegate;
}

bool DSMEPlatform::startCCA()
{
    if (pending_tx) {
        return false;
    }
    if (IS_ACTIVE(USE_CAD)) {
        ieee802154_radio_request_cca(&_radio);
    }
    else {
        ztimer_set(ZTIMER_MSEC, &cca_timer, 12);
    }
    puts("[INFO];REQ_CCA");
    DSMEPlatform::state = STATE_CCA_WAIT;
    return true;
}

void DSMEPlatform::turnTransceiverOn()
{
    /* TODO */
}

void DSMEPlatform::turnTransceiverOff()
{
    int res = ieee802154_radio_set_idle(&_radio, true);
    DSME_ASSERT(res == 0);
}

}
