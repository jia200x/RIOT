#ifndef DSMEPLATFORM_H
#define DSMEPLATFORM_H

#include <stdint.h>
#include <stdlib.h>

#include <string>
#include "random.h"

#include "opendsme/DSMEMessage.h"
#include "helper/DSMEDelegate.h"
#include "interfaces/IDSMEPlatform.h"
#include "mac_services/dataStructures/IEEE802154MacAddress.h"
#include "mac_services/mcps_sap/MCPS_SAP.h"
#include "mac_services/mlme_sap/MLME_SAP.h"
#include "mac_services/pib/MAC_PIB.h"
#include "mac_services/pib/PHY_PIB.h"
#include "mac_services/pib/dsme_phy_constants.h"
#include "mac_services/DSME_Common.h"
#include "opendsme/dsme_settings.h"
#include "ztimer.h"
#include "net/gnrc/netif.h"

#include "dsmeLayer/DSMELayer.h"
#include "dsmeAdaptionLayer/DSMEAdaptionLayer.h"
#include "byteorder.h"

namespace dsme {

struct DSMESettings;
class DSMELayer;
class DSMEAdaptionLayer;

class DSMEPlatform : public IDSMEPlatform {
public:
    DSMEPlatform();

    ~DSMEPlatform();

    void initialize(bool pan_coord);
    void send_pkt(uint16_t addr, iolist_t *pkt);

    uint8_t getChannelNumber() override;
    bool setChannelNumber(uint8_t k) override;

     /**
    * Directly send packet without delay and without CSMA
    * but keep the message (the caller has to ensure that the message is eventually released)
    * This might lead to an additional memory copy in the platform
    */
    bool prepareSendingCopy(IDSMEMessage* msg, Delegate<void(bool)> txEndCallback) override;
    bool prepareSendingCopy(DSMEMessage* msg, Delegate<void(bool)> txEndCallback);

    bool sendNow() override;

    void abortPreparedTransmission() override;

    /**
    * Send an ACK message, delay until aTurnaRoundTime after reception_time has expired
    */
    bool sendDelayedAck(IDSMEMessage *ackMsg, IDSMEMessage *receivedMsg, Delegate<void(bool)> txEndCallback) override;
    bool sendDelayedAck(DSMEMessage *ackMsg, DSMEMessage *receivedMsg, Delegate<void(bool)> txEndCallback);

    void setReceiveDelegate(receive_delegate_t receiveDelegate) override;

    bool isReceptionFromAckLayerPossible() override;

    void handle_rx();
    void handleReceivedMessageFromAckLayer(IDSMEMessage* message) override;
    void handleReceivedMessageFromAckLayer(DSMEMessage* message);
    void rx_offload();

    DSMEMessage* getEmptyMessage() override;

    void releaseMessage(IDSMEMessage* msg) override;
    void releaseMessage(DSMEMessage* msg);

    bool startCCA() override;

    void startTimer(uint32_t symbolCounterValue) override;

    uint32_t getSymbolCounter() override;

    uint16_t getRandom() override {
            return (random_uint32() % UINT16_MAX);
    }

    void updateVisual() override;

    void scheduleStartOfCFP();

    // Beacons with LQI lower than this will not be considered when deciding for a coordinator to associate to
    // TODO for Cooja, LQI is always 105. Has to be adjusted for other platforms.
    uint8_t getMinCoordinatorLQI() override{
            return 100;
    };

    void turnTransceiverOn();
    void turnTransceiverOff();

    IEEE802154MacAddress& getAddress() {
            return this->mac_pib.macExtendedAddress;
    }

    void start();

    void printSequenceChartInfo(DSMEMessage* msg, bool outgoing);

    DSMELayer& getDSME() {
            return dsme;
    }

    bool isAssociated();
    void allocateGTS(uint8_t superframeID, uint8_t slotID, uint8_t channelID, Direction direction, uint16_t address);

    void signalAckedTransmissionResult(bool success, uint8_t transmissionAttempts, IEEE802154MacAddress receiver) override;
    void signalGTSChange(bool deallocation, IEEE802154MacAddress counterpart, uint16_t superframeID, uint8_t gtSlotID, uint8_t channel, Direction direction) override;
    void signalQueueLength(uint32_t length) override;
    void signalPacketsPerCAP(uint32_t packets) override;
    void signalFailedPacketsPerCAP(uint32_t packets) override;
    bool isRxEnabledOnCap() override;

    static DSMEPlatform* instance;

    static uint8_t state;

    enum {
        STATE_READY = 0, STATE_CCA_WAIT = 1, STATE_SEND = 2,
    };

    static Delegate<void(bool)> txEndCallback;
    void getShortAddress(network_uint16_t *addr);
    /*TODO*/
    gnrc_netif_t netif;

protected:
    /** @brief Copy constructor is not allowed.
     */
    DSMEPlatform(const DSMEPlatform&);
    /** @brief Assignment operator is not allowed.
     */
    DSMEPlatform& operator=(const DSMEPlatform&);

    void signalNewMsg(DSMEMessage* msg);
    virtual void signalReleasedMsg(DSMEMessage* msg) {}

    void handleDataMessageFromMCPSWrapper(IDSMEMessage* msg);
    void handleDataMessageFromMCPS(DSMEMessage* msg);

    void handleConfirmFromMCPSWrapper(IDSMEMessage* msg, DataStatus::Data_Status dataStatus);
    void handleConfirmFromMCPS(DSMEMessage* msg, DataStatus::Data_Status dataStatus);

    std::string printDSMEManagement(uint8_t management, DSMESABSpecification& sabSpec, CommandFrameIdentifier cmd);

    void translateMacAddress(uint16_t& from, IEEE802154MacAddress& to);

    //DSMEMessageBuffer messageBuffer;		//has to be placed above DSMELayer (and DSMEAdaptionLayer) otherwise destructor tries to release messages that are already destructed

    PHY_PIB phy_pib;
    MAC_PIB mac_pib;

    DSMELayer dsme;

    mcps_sap::MCPS_SAP mcps_sap;
    mlme_sap::MLME_SAP mlme_sap;

    DSMEAdaptionLayer dsmeAdaptionLayer;

    bool initialized;
    bool scanOrSyncInProgress{false};
    bool associationInProgress{false};
    bool syncActive{false};
    bool rx_on_cap{true};

    receive_delegate_t receiveFromAckLayerDelegate;

    ztimer_t timer;

    IDSMEMessage *message;
    GTSScheduling* scheduling = nullptr;
};

}

#endif
