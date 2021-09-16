#ifndef DSMEMESSAGE_H
#define DSMEMESSAGE_H

#include <stdint.h>
#include <string.h>

#include "opendsme/dsme_settings.h"
#include "opendsme/dsme_platform.h"

extern "C" {
//#include "net/mac/mac.h"
//#include "dev/radio.h"
}

#include "mac_services/dataStructures/DSMEMessageElement.h"
#include "dsmeLayer/messages/IEEE802154eMACHeader.h"
#include "interfaces/IDSMEMessage.h"
#include "net/gnrc/pktbuf.h"

namespace dsme {

class DSMEPlatform;

class DSMEMessage : public IDSMEMessage {
public:
//////////////////////////////////////////////////////////////////////////////////
		/* IDSMEMessage */
    void prependFrom(DSMEMessageElement* msg) override;

    void decapsulateTo(DSMEMessageElement* msg) override;

    void copyTo(DSMEMessageElement* msg);

#if 0
    uint8_t getByte(uint8_t pos) {
        DSME_ASSERT(false);
        //return payload[pos];
        return 0;
    }
#endif

    bool hasPayload() {
        return this->pkt != NULL;
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

    //TODO
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

//////////////////////////////////////////////////////////////////////////////////
    void clearMessage() {
        if (pkt) {
            gnrc_pktbuf_release(pkt);
        }
        pkt = NULL;
    }

    uint8_t getPayloadLength() {
            DSME_ASSERT(pkt);
            return pkt->size;
		}

    int8_t getRSSI() override {
    		return this->radio_last_rssi;
    }

    int loadBuffer(size_t len);
    uint8_t *getPayload() {
        DSME_ASSERT(pkt);
        return (uint8_t*) pkt->data;
    }

    int dropHdr(size_t len);

//    radio_value_t getChannelSent() {
//    		return this->channelSent;
//    }

//    mac_callback_t getMacCallbackFunction() const {
//    		return macCallbackFunction;
//    }

//		void setMacCallbackFunction(mac_callback_t macCallbackFunction) {
//				this->macCallbackFunction = macCallbackFunction;
//		}

#if 0
		void* getMacCallbackPointer() const {
				return macCallbackPointer;
		}

		void setMacCallbackPointer(void* macCallbackPointer) {
				this->macCallbackPointer = macCallbackPointer;
		}

		void makeCopyFrom(DSMEMessage* msg, const uint8_t * cbuf, uint8_t * buf) {
                DSME_ASSERT(false);
				this->firstTry = msg->firstTry;
				//this->payloadLength = msg->getPayloadLength();
				this->radio_last_rssi = msg->getRSSI();
				//this->channelSent = msg->getChannelSent();
				this->messageLQI = msg->getLQI();
				this->receivedViaMCPS = msg->getReceivedViaMCPS();
				this->currentlySending = msg->getCurrentlySending();
				this->retryCounter = msg->getRetryCounter();
				this->startOfFrameDelimiterSymbolCounter = msg->getStartOfFrameDelimiterSymbolCounter();
				memcpy((const uint8_t*) this->getPayload(), msg->getPayload(), msg->getPayloadLength());

				msg->getHeader().serializeTo(buf);
				//const uint8_t * buf = buffer;
				this->getHeader().deserializeFrom(cbuf, 2);
		}
#endif

    bool firstTry;
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

    //mac_callback_t macCallbackFunction; /* callback for this packet */
//		void *macCallbackPointer; /* MAC callback parameter */

    uint32_t startOfFrameDelimiterSymbolCounter;
    bool receivedViaMCPS; // TODO better handling?
    bool currentlySending;
    uint8_t retryCounter;

    friend class DSMEPlatform;
    friend class DSMEMessageElement;
    friend class DSMEPlatformBase;
    friend class DSMEMessageBuffer;
};

}

#endif
