#ifndef DSMEMESSAGE_H
#define DSMEMESSAGE_H

#include <stdint.h>
#include <string.h>

#include "opendsme/dsme_settings.h"

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

    uint8_t getByte(uint8_t pos) {
        return payload[pos];
    }

    bool hasPayload() {
        return (payloadLength > 0);
    }

    // gives the symbol counter at the end of the SFD
    uint32_t getStartOfFrameDelimiterSymbolCounter() override {
        return startOfFrameDelimiterSymbolCounter;
    }

    void setStartOfFrameDelimiterSymbolCounter(uint32_t symbolCounter) override {
        startOfFrameDelimiterSymbolCounter = symbolCounter;
    }

    uint16_t getTotalSymbols() {
				uint16_t bytes = macHdr.getSerializationLength()
																	 + payloadLength
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
    		this->setPayloadLength(0);
    }

    uint8_t getPayloadLength() {
				return payloadLength;
		}

    int8_t getRSSI() override {
    		return this->radio_last_rssi;
    }

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
#endif

		void makeCopyFrom(DSMEMessage* msg, const uint8_t * cbuf, uint8_t * buf) {
				this->firstTry = msg->firstTry;
				this->payloadLength = msg->getPayloadLength();
				this->radio_last_rssi = msg->getRSSI();
				//this->channelSent = msg->getChannelSent();
				this->messageLQI = msg->getLQI();
				this->receivedViaMCPS = msg->getReceivedViaMCPS();
				this->currentlySending = msg->getCurrentlySending();
				this->retryCounter = msg->getRetryCounter();
				this->startOfFrameDelimiterSymbolCounter = msg->getStartOfFrameDelimiterSymbolCounter();
				memcpy(this->getPayload(), msg->getPayload(), msg->getPayloadLength());

				msg->getHeader().serializeTo(buf);
				//const uint8_t * buf = buffer;
				this->getHeader().deserializeFrom(cbuf, 2);
		}

    bool firstTry;

private:
    DSMEMessage() :
        		payloadLength(0),
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

    uint8_t* getPayload() {
        return payload;
    }
  
    void setPayloadLength(uint8_t length) {
        payloadLength = length;
    }
  
    IEEE802154eMACHeader macHdr;
    uint8_t payload[DSME_PACKET_MAX_LEN];
    uint8_t payloadLength;
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
