#ifndef DSMEPLATFORM_H
#define DSMEPLATFORM_H

#include <stdint.h>
#include <stdlib.h>

#include <string>

#include "opendsme/DSMEMessage.h"
//#include "dsmeAdaptionLayer/DSMEAdaptionLayer.h"
#include "helper/DSMEDelegate.h"
#include "interfaces/IDSMEPlatform.h"
#include "mac_services/dataStructures/IEEE802154MacAddress.h"
#include "mac_services/mcps_sap/MCPS_SAP.h"
#include "mac_services/mlme_sap/MLME_SAP.h"
#include "mac_services/pib/MAC_PIB.h"
#include "mac_services/pib/PHY_PIB.h"
#include "opendsme/dsme_settings.h"
#include "ztimer.h"

//#include "DSMEMessageBuffer.h"
//#include "HandleMessageTask.h"

#include "dsmeLayer/DSMELayer.h"
//#include "dsmeAdaptionLayer/DSMEAdaptionLayer.h"

namespace dsme {

/**
 * Contains parameters to configure an exponential backoff algorithm.
 *
 * @li unitBackoff
 */
typedef struct mac_backoffCfg_t {
    /** minimum backoff exponent (initial value of the BE) */
    uint8_t minBE;

    /** maximum backoff exponent used for the backoff algorithm */
    uint8_t maxBE;

    /**
     * Defines the number of retries this MAL SHALL use
     * when it is configured to perform a CCA before
     * sending and this CCA fails
     * */
    uint8_t maxBackoffRetries;

    /** duration of unit backoff period in bits; t = (unitBackoff bit)/txRate*/
    uint16_t unitBackoff;
} mac_backoffCfg_t;

/**
 * Contains parameters needed for the configuration of an acknowledgement
 * algorithm.
 */
typedef struct mac_ackCfg_t {
    /** Maximum number of frame retransmissions, before packet is discarded */
    uint8_t maxFrameRetries;
    /** Duration to wait for an ACK, before assuming a transmission error */
    uint16_t ackWaitDuration;
} mac_ackCfg_t;

typedef uint8_t mac_result_t;

struct DSMESettings;
class DSMELayer;
class DSMEAdaptionLayer;

class DSMEPlatform : public IDSMEPlatform {
public:
    DSMEPlatform();

    ~DSMEPlatform();

    void initialize();
    void startAssociation();
    void startScan();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /* IDSMEPlatformBase */

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

		void handleReceivedMessageFromAckLayer(IDSMEMessage* message) override;
		void handleReceivedMessageFromAckLayer(DSMEMessage* message);

		DSMEMessage* getEmptyMessage() override;

		void releaseMessage(IDSMEMessage* msg) override;
		void releaseMessage(DSMEMessage* msg);

		bool startCCA() override;

		void startTimer(uint32_t symbolCounterValue) override;

		uint32_t getSymbolCounter() override;

		uint16_t getRandom() override {
				return (rand() % UINT16_MAX);
		}

		void updateVisual() override {};

		void scheduleStartOfCFP();

		// Beacons with LQI lower than this will not be considered when deciding for a coordinator to associate to
		// TODO for Cooja, LQI is always 105. Has to be adjusted for other platforms.
		uint8_t getMinCoordinatorLQI() override{
				return 100;
		};

        void turnTransceiverOn();
        void turnTransceiverOff();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//void requestFromPacketbufPending(mac_callback_t sent, void *ptr);

		IEEE802154MacAddress& getAddress() {
				return this->mac_pib.macExtendedAddress;
		}

		void start();

		void printSequenceChartInfo(DSMEMessage* msg, bool outgoing);

		void invokeMessageTask();

		DSMELayer& getDSME() {
				return dsme;
		}

    void requestPending();

    bool isAssociated();

    mac_result_t getMCPSTtransmitStatus();

    void signalAckedTransmissionResult(bool success, uint8_t transmissionAttempts, IEEE802154MacAddress receiver) override;

    static DSMEPlatform* instance;

    static uint8_t state;

    enum {
        STATE_READY = 0, STATE_CCA_WAIT = 1, STATE_SEND = 2,
    };

    static Delegate<void(bool)> txEndCallback;

protected:
    /** @brief Copy constructor is not allowed.
     */
    DSMEPlatform(const DSMEPlatform&);
    /** @brief Assignment operator is not allowed.
     */
    DSMEPlatform& operator=(const DSMEPlatform&);

    static uint32_t getSFDTimestamp();
    virtual void signalNewMsg(DSMEMessage* msg) {}
    virtual void signalReleasedMsg(DSMEMessage* msg) {}

    void handleDataMessageFromMCPSWrapper(IDSMEMessage* msg);
    void handleDataMessageFromMCPS(DSMEMessage* msg);

    void handleDataIndication(mcps_sap::DATA_indication_parameters& params);
    void handleDataConfirm(mcps_sap::DATA_confirm_parameters& params);
		void handleConfirmFromMCPSWrapper(IDSMEMessage* msg, DataStatus::Data_Status dataStatus);
		void handleConfirmFromMCPS(DSMEMessage* msg, DataStatus::Data_Status dataStatus);

    std::string printDSMEManagement(uint8_t management, DSMESABSpecification& sabSpec, CommandFrameIdentifier cmd);

    void translateMacAddress(uint16_t& from, IEEE802154MacAddress& to);
    //void convertDSMEMacAddressToLinkaddr(IEEE802154MacAddress *from, linkaddr_t *to);

		//DSMEMessageBuffer messageBuffer;		//has to be placed above DSMELayer (and DSMEAdaptionLayer) otherwise destructor tries to release messages that are already destructed

		PHY_PIB phy_pib;
		MAC_PIB mac_pib;

		DSMELayer dsme;

		mcps_sap::MCPS_SAP mcps_sap;
		mlme_sap::MLME_SAP mlme_sap;

		//DSMEAdaptionLayer dsmeAdaptionLayer;

		uint16_t messagesInUse;

    bool initialized;
    bool scanOrSyncInProgress{false};
    bool associationInProgress{false};
    bool syncActive{false};

		receive_delegate_t receiveFromAckLayerDelegate;

		DSMESettings* settings;

		/** @brief the bit rate at which we transmit */
		double bitrate;

    //HandleMessageTask handleMessageTask;

    uint8_t bufferUp[DSME_PACKET_MAX_LEN];
    uint8_t bufferDown[DSME_PACKET_MAX_LEN];

    static mac_ackCfg_t ackCfg;
    static mac_backoffCfg_t backoffCfg;

    uint8_t channel;
    mac_result_t MCPS_transmit_status;

    uint8_t currentTXLength;
    ztimer_t timer;

    struct DSMEMessagePool {
        DSMEMessage msg;
        DSMEMessagePool *next;
    };
    DSMEMessagePool *head;
    DSMEMessagePool pool[8];
};

}

#endif
