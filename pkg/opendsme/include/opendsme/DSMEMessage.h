#ifndef DSMEMESSAGE_H
#define DSMEMESSAGE_H

#include <stdint.h>
#include <string.h>

#include "opendsme/dsme_settings.h"
#include "opendsme/dsme_platform.h"

#include "mac_services/dataStructures/DSMEMessageElement.h"
#include "dsmeLayer/messages/IEEE802154eMACHeader.h"
#include "interfaces/IDSMEMessage.h"

#include "net/gnrc/pktbuf.h"
#include "iolist.h"

namespace dsme {

class DSMEPlatform;

class DSMEMessage : public IDSMEMessage {
public:
//////////////////////////////////////////////////////////////////////////////////
		/* IDSMEMessage */
    void prependFrom(DSMEMessageElement* msg) override;

    void decapsulateTo(DSMEMessageElement* msg) override;

    void copyTo(DSMEMessageElement* msg);

    bool hasPayload() {
        return this->pkt != NULL && this->pkt->size > 0;
    }

    // gives the symbol counter at the end of the SFD
    uint32_t getStartOfFrameDelimiterSymbolCounter() override {
        return startOfFrameDelimiterSymbolCounter;
    }

    void setStartOfFrameDelimiterSymbolCounter(uint32_t symbolCounter) override {
        startOfFrameDelimiterSymbolCounter = symbolCounter;
    }

    uint16_t getTotalSymbols() {
        DSME_ASSERT(pkt);
				uint16_t bytes = macHdr.getSerializationLength()
																	 + pkt->size
																	 + 2 // FCS
																	 + 4 // Preamble
																	 + 1 // SFD
																	 + 1; // PHY Header
				return bytes*2; // 4 bit per symbol
		}

    uint8_t getMPDUSymbols() override;
    IEEE802154eMACHeader& getHeader() {
        return macHdr;
    }

    uint8_t getLQI() override {
    		return messageLQI;
    }

    bool getReceivedViaMCPS() override {
            return receivedViaMCPS;
    }

    void setReceivedViaMCPS(bool receivedViaMCPS) override {
            this->receivedViaMCPS = receivedViaMCPS;
    }

    bool getCurrentlySending() override {
            return currentlySending;
    }

    void setCurrentlySending(bool currentlySending) override {
            this->currentlySending = currentlySending;
    }

    void increaseRetryCounter() override {
    		this->retryCounter++;
    }

    uint8_t getRetryCounter() override {
    		return this->retryCounter;
    }

    uint8_t getPayloadLength() {
            DSME_ASSERT(pkt);
            return pkt->size;
		}

    int8_t getRSSI() override {
    		return this->radio_last_rssi;
    }

    int loadBuffer(size_t len);
    int loadBuffer(iolist_t *pkt);
    uint8_t *getPayload() {
        DSME_ASSERT(pkt);
        return (uint8_t*) pkt->data;
    }

    int dropHdr(size_t len);
    void releaseMessage();
    void dispatchMessage();
    iolist_t *getIolPayload();
    iolist_t *clearMessage();

    bool firstTry;
    bool free;
private:
    DSMEMessage() :
						radio_last_rssi(0),
						channelSent(0),
						messageLQI(0),
    				receivedViaMCPS(false),
    				currentlySending(false),
						retryCounter(0)
        {
        }

		~DSMEMessage() {
		}
  
    void prepare() {
        currentlySending = false;
        firstTry = true;
        receivedViaMCPS = false;
        retryCounter = 0;

        macHdr.reset();
    }

    IEEE802154eMACHeader macHdr;
    uint8_t radio_last_rssi;
    uint8_t channelSent;
    uint8_t  messageLQI;
    gnrc_pktsnip_t *pkt;

    uint32_t startOfFrameDelimiterSymbolCounter;
    bool receivedViaMCPS;
    bool currentlySending;
    uint8_t retryCounter;

    friend class DSMEPlatform;
    friend class DSMEMessageElement;
    friend class DSMEPlatformBase;
    friend class DSMEMessageBuffer;
};

}

#endif
