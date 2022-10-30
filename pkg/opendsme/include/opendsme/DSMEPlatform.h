/*
 * Copyright (C) 2022 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    pkg_opendsme_dsmeplatform DSME Platform interface implementation.
 * @ingroup     pkg_opendsme
 * @brief       Implementation of the DSME Platform interface.
 *
 * @{
 *
 * @file
 * @brief   DSME Platform interface implementation
 *
 * @author  José I. Álamos <jose.alamos@haw-hamburg.de>
 */

#ifndef PKG_OPENDSME_DSMEPLATFORM_H
#define PKG_OPENDSME_DSMEPLATFORM_H

#include <stdint.h>
#include <stdlib.h>

#include <string>
#include "random.h"

#include "byteorder.h"
#include "dsmeAdaptionLayer/DSMEAdaptionLayer.h"
#include "dsmeLayer/DSMELayer.h"
#include "helper/DSMEDelegate.h"
#include "interfaces/IDSMEPlatform.h"
#include "mac_services/dataStructures/IEEE802154MacAddress.h"
#include "mac_services/mcps_sap/MCPS_SAP.h"
#include "mac_services/mlme_sap/MLME_SAP.h"
#include "mac_services/pib/MAC_PIB.h"
#include "mac_services/pib/PHY_PIB.h"
#include "mac_services/pib/dsme_phy_constants.h"
#include "mac_services/DSME_Common.h"
#include "net/gnrc/netif.h"
#include "net/ieee802154/radio.h"
#include "opendsme/dsme_settings.h"
#include "opendsme/DSMEMessage.h"
#include "ztimer.h"


namespace dsme {

struct DSMESettings;
class DSMELayer;
class DSMEAdaptionLayer;

class DSMEPlatform : public IDSMEPlatform {
public:
    /* Constructor and destructor */
    DSMEPlatform();
    ~DSMEPlatform();

    /* Pointer to the DSME instance */
    static DSMEPlatform* instance;

    /* Initialize MAC with a role (PAN coordinator, child) */
    void initialize(bool pan_coord);

    /* To be called by the upper layer in order to send a frame */
    void sendFrame(uint16_t addr, iolist_t *pkt);

    /* Start DSME */
    void start();

    /* Get the DSME layer */
    DSMELayer& getDSME() {
            return dsme;
    }

    /* Check whether the node associated */
    bool isAssociated();

    /* Allocate a GTS slot */
#if IS_ACTIVE(CONFIG_IEEE802154_DSME_STATIC_GTS)
    void allocateGTS(uint8_t superframeID, uint8_t slotID, uint8_t channelID, Direction direction, uint16_t address, uint16_t numSlots);
#endif

    /* Get short address */
    void getShortAddress(network_uint16_t *addr);

    void getExtendedAddress(uint8_t *addr);

    /* Set GTS or CAP transmission */
    void setGTSTransmission(bool gts);

    /* Set ACK_REQ bit */
    void setAckReq(bool ackReq);

    /* Request to offload the CCA Done event */
    void offloadCCAEvent();

    /* Request to offload the TX Done event */
    void offloadTXDoneEvent();

    /* Indicate the MAC layer that the reception started */
    void indicateRxStart();

    /* Request to offload RX Done event */
    void offloadRXDoneEvent();

    /* Request to offload ACK Timer event */
    void offloadACKTimer();

    /* Request to offload Timer event */
    void offloadTimerEvent();

    /* Set the platform state */
    void setPlatformState(uint8_t state) {
        this->state = state;
    }

    /* Process the CCA Done event */
    void processCCAEvent();

    /* Process the TX Done event */
    void processTXDoneEvent();

    /* Process the RX Done event from radio */
    void processRxDone();

    /* Process the offload event of received frame */
    void processRxOffload();

    /* Set the GNRC netif */
    void setGNRCNetif(gnrc_netif_t *netif) {
        this->netif = netif;
    }

    enum {
        STATE_READY = 0, STATE_CCA_WAIT = 1, STATE_SEND = 2,
    };

    /*********** IDSMEPlatform implementation ***********/

    /* Get channel number */
    uint8_t getChannelNumber() override;

    /* Set channel number */
    bool setChannelNumber(uint8_t k) override;

    /**
    * Directly send packet without delay and without CSMA
    * but keep the message (the caller has to ensure that the message is eventually released)
    * This might lead to an additional memory copy in the platform
    */
    bool sendNow() override;

    /* Prepare the next transmission */
    bool prepareSendingCopy(IDSMEMessage* msg, Delegate<void(bool)> txEndCallback) override;
    bool prepareSendingCopy(DSMEMessage* msg, Delegate<void(bool)> txEndCallback);

    /* Abort an already prepared transmission */
    void abortPreparedTransmission() override;

    /**
    * Send an ACK message, delay until aTurnaRoundTime after reception_time has expired
    */
    bool sendDelayedAck(IDSMEMessage *ackMsg, IDSMEMessage *receivedMsg, Delegate<void(bool)> txEndCallback) override;
    bool sendDelayedAck(DSMEMessage *ackMsg, DSMEMessage *receivedMsg, Delegate<void(bool)> txEndCallback);

    /* Set the receive callback */
    void setReceiveDelegate(receive_delegate_t receiveDelegate) override;

    /* Check whether the ACK layer is busy */
    bool isReceptionFromAckLayerPossible() override;

    /* Handle reception of frames from ACK Layer */
    void handleReceivedMessageFromAckLayer(IDSMEMessage* message) override;
    void handleReceivedMessageFromAckLayer(DSMEMessage* message);

    /* Get an empty message */
    DSMEMessage* getEmptyMessage() override;

    /* Release a message */
    void releaseMessage(IDSMEMessage* msg) override;
    void releaseMessage(DSMEMessage* msg);

    /* Start CCA procedure */
    bool startCCA() override;

    /* Start timer */
    void startTimer(uint32_t symbolCounterValue) override;

    /* Get elapsed number of symbols since initialization */
    uint32_t getSymbolCounter() override;

    /* Get a uint16_t random number */
    uint16_t getRandom() override {
            return (random_uint32() % UINT16_MAX);
    }

    /* Not used */
    void updateVisual() override;

    /* Callback to offload the start of CFP */
    void scheduleStartOfCFP();

    /* Beacons with LQI lower than this will not be considered when deciding for a coordinator to associate to. */
    uint8_t getMinCoordinatorLQI() override{
            return CONFIG_IEEE802154_DSME_MIN_COORD_LQI;
    };

    /* Turn on transceiver */
    void turnTransceiverOn();

    /* Turn off transceiver */
    void turnTransceiverOff();

    /* Get extended address */
    IEEE802154MacAddress& getAddress() {
            return this->mac_pib.macExtendedAddress;
    }

    /* Signal finish of ACK'd transmission */
    void signalAckedTransmissionResult(bool success, uint8_t
                                       transmissionAttempts, IEEE802154MacAddress receiver) override;

    /* Signal a change in GTS status */
    void signalGTSChange(bool deallocation, IEEE802154MacAddress counterpart,
                         uint16_t superframeID, uint8_t gtSlotID, uint8_t channel, Direction
                         direction) override;

    /* Signal a change in queue length */
    void signalQueueLength(uint32_t length) override;

    /* Signal the number of packets transmitted during the last CAP */
    void signalPacketsPerCAP(uint32_t packets) override;

    /* Signal the number of failed packets transmitted during the last CAP */
    void signalFailedPacketsPerCAP(uint32_t packets) override;

    /* Callback to check where RX is on during CAP */
    bool isRxEnabledOnCap() override;

protected:
    /** @brief Copy constructor is not allowed.
     */
    DSMEPlatform(const DSMEPlatform&);
    /** @brief Assignment operator is not allowed.
     */
    DSMEPlatform& operator=(const DSMEPlatform&);

    /* Delegate callback for TX end */
    Delegate<void(bool)> txEndCallback;

    /* Signal creation of new message */
    void signalNewMsg(DSMEMessage* msg);

    /* Signal release of message */
    virtual void signalReleasedMsg(DSMEMessage* msg) {}

    /* DSMEAdaptionLayer wrapper for MCPS Indication callbacks */
    void handleDataMessageFromMCPSWrapper(IDSMEMessage* msg);
    void handleDataMessageFromMCPS(DSMEMessage* msg);

    /* DSMEAdaptionLayer wrapper for MCPS Confirm callbacks */
    void handleConfirmFromMCPSWrapper(IDSMEMessage* msg, DataStatus::Data_Status dataStatus);
    void handleConfirmFromMCPS(DSMEMessage* msg, DataStatus::Data_Status dataStatus);

    /* Translate MAC Address represnetation */
    void translateMacAddress(uint16_t& from, IEEE802154MacAddress& to);

    event_queue_t *getEventQueue() {
        return &this->netif->evq;
    }

    /* Holds the PHY Information Base */
    PHY_PIB phy_pib;

    /* Holds the MAC Information Base */
    MAC_PIB mac_pib;

    /* Descriptor of the DSME MAC */
    DSMELayer dsme;

    /* Descriptor of the MCPS Service Access Point */
    mcps_sap::MCPS_SAP mcps_sap;

    /* Descriptor of the MLME Service Access Point */
    mlme_sap::MLME_SAP mlme_sap;

    /* Descriptor of the DSME Adaption Layer */
    DSMEAdaptionLayer dsmeAdaptionLayer;

    /* Whether the MAC is initialized */
    bool initialized;

    /* Whether there is a scan or sync in progress */
    bool scanOrSyncInProgress{false};

    /* Whether the association is in progress */
    bool associationInProgress{false};

    /* Whether the MAC is synchronized */
    bool syncActive{false};

    /* Whether the MAC keeps the receiver on during CAP */
    bool rx_on_cap{true};

    /* Delegate callback for passing frames to the ACK layer */
    receive_delegate_t receiveFromAckLayerDelegate;

    /* Pointer to the GNRC interface */
    gnrc_netif_t *netif;

    /* Timer used for the MAC */
    ztimer_t timer;

    /* Used to hold an incoming message before passing it to the MAC */
    IDSMEMessage *message;

    /* Pointer to the scheduler */
    GTSScheduling* scheduling = nullptr;

    /* Event used for CCA Done */
    event_t cca_ev;

    /* Event used for ACK Timeout */
    event_t acktimer_ev;

    /* Event used for timer events */
    event_t timer_event;

    /* Event used for TX Done */
    event_t tx_done_event;

    /* Event used for RX Done */
    event_t rx_done_event;

    /* Event used for GTS request */
    event_t request_slot_ev;

    /* Event used for offloading the receive procedure */
    event_t rx_offload_ev;

    /* Event used for offloading the start of a CFP */
    event_t start_of_cfp_ev;

    /* Timestamp (in number of symbols) of the last received preamble */
    uint32_t rx_sfd;

    /* Timer used for ACK timeout events */
    ztimer_t acktimer;

    /* State of the platform layer */
    uint8_t state;

    /* Whether the MAC expects an ACK frame */
    bool wait_for_ack;

    /* Whether there is a pending TX frame */
    bool pending_tx;

    /* Pointer to the IEEE 802.15.4 HAL descriptor */
    ieee802154_dev_t *radio;
};

}

#endif /* PKG_OPENDSME_DSMEPLATFORM_H */

/** @} */
