/*
 * Copyright (C) 2022 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    pkg_opendsme_dsmeplatform DSME Message interface implementation.
 * @ingroup     pkg_opendsme
 * @brief       Implementation of the DSME Message interface for GNRC.
 *
 * @{
 *
 * @file
 * @brief   DSME Message interface implementation for GNRC.
 *
 * @author  José I. Álamos <jose.alamos@haw-hamburg.de>
 */

#ifndef PKG_OPENDSME_DSMEMESSAGE_H
#define PKG_OPENDSME_DSMEMESSAGE_H

#include <stdint.h>
#include <string.h>

#include "dsmeLayer/messages/IEEE802154eMACHeader.h"
#include "interfaces/IDSMEMessage.h"
#include "iolist.h"
#include "mac_services/dataStructures/DSMEMessageElement.h"
#include "net/gnrc/pktbuf.h"
#include "opendsme/dsme_settings.h"
#include "opendsme/dsme_platform.h"

namespace dsme {

class DSMEPlatform;

class DSMEMessage : public IDSMEMessage {
public:
    /************* IDSMEMessage implementation *************/

    /* Prepend a header to current message */
    void prependFrom(DSMEMessageElement* msg) override;

    /* Decapsulate header to a message */
    void decapsulateTo(DSMEMessageElement* msg) override;

    /* Copy payload to DSME Message Element */
    void copyTo(DSMEMessageElement* msg);

    /* Check whether the message has payload */
    bool hasPayload() {
        return this->pkt != NULL && this->pkt->size > 0;
    }

    /* get the symbol counter at the end of the SFD */
    uint32_t getStartOfFrameDelimiterSymbolCounter() override {
        return startOfFrameDelimiterSymbolCounter;
    }

    /* set the symbol counter at the end of the SFD */
    void setStartOfFrameDelimiterSymbolCounter(uint32_t symbolCounter) override {
        startOfFrameDelimiterSymbolCounter = symbolCounter;
    }

    /* get the total number of symbols in current frame */
    uint16_t getTotalSymbols() {
        DSME_ASSERT(pkt);
				uint16_t bytes = macHdr.getSerializationLength()
                                 + pkt->size
                                 + 2 /* FCS */
                                 + 4 /* Preamble */
                                 + 1 /* SFD */
                                 + 1; /* PHY Header */
				return bytes*2; /* 4 bit per symbol */
		}

    /* not used */
    uint8_t getMPDUSymbols() override;

    /* get header */
    IEEE802154eMACHeader& getHeader() {
        return macHdr;
    }

    /* get LQI of the message */
    uint8_t getLQI() override {
    		return messageLQI;
    }

    /* Check whether the message was received via MCPS */
    bool getReceivedViaMCPS() override {
            return receivedViaMCPS;
    }

    /* Indicated that message was received via MCPS */
    void setReceivedViaMCPS(bool receivedViaMCPS) override {
            this->receivedViaMCPS = receivedViaMCPS;
    }

    /* Whether the message is being sent */
    bool getCurrentlySending() override {
            return currentlySending;
    }

    /* Indicate that the message is being sent */
    void setCurrentlySending(bool currentlySending) override {
            this->currentlySending = currentlySending;
    }

    /* Increase retry counter for current message */
    void increaseRetryCounter() override {
    		this->retryCounter++;
    }

    /* Get retry counter */
    uint8_t getRetryCounter() override {
    		return this->retryCounter;
    }

    /* Get payload length */
    uint8_t getPayloadLength() {
            DSME_ASSERT(pkt);
            return pkt->size;
		}

    /* Get RSSI of frame */
    int8_t getRSSI() override {
    		return this->messageRSSI;
    }

    /* preallocate buffer with a given length */
    int loadBuffer(size_t len);

    /* load a GNRC packet into the internal openDSME message representation */
    int loadBuffer(iolist_t *pkt);

    /* get buffer associated to the current payload */
    uint8_t *getPayload() {
        DSME_ASSERT(pkt);
        return (uint8_t*) pkt->data;
    }

    /* Drop a number of bytes from the header */
    int dropHdr(size_t len);

    /* Release the message */
    void releaseMessage();

    /* Dispatch the message to upper layers */
    void dispatchMessage();

    /* get the IOLIST representation of the message */
    iolist_t *getIolPayload();

    /* clear the message */
    iolist_t *clearMessage();

    /* whether the message is being sent on the first try */
    bool firstTry;

    /* whether the message is free */
    bool free;
private:
    /* Constructor and destructor */
    DSMEMessage() :
						messageRSSI(0),
						messageLQI(0),
    				receivedViaMCPS(false),
    				currentlySending(false),
						retryCounter(0)
        {
        }

		~DSMEMessage() {
		}
  
    /* prepare message */
    void prepare() {
        currentlySending = false;
        firstTry = true;
        receivedViaMCPS = false;
        retryCounter = 0;

        macHdr.reset();
    }

    /* Descriptor of the MAC header */
    IEEE802154eMACHeader macHdr;

    /* Stores the RSSI of the received frame */
    uint8_t messageRSSI;

    /* Stores the LQI of the received frame */
    uint8_t  messageLQI;

    /* Stores a pointer to the GNRC Pktbuf representation */
    gnrc_pktsnip_t *pkt;

    /* Stores the timestamp of the preamble */
    uint32_t startOfFrameDelimiterSymbolCounter;

    /* Stores whether the message was received via MCPS */
    bool receivedViaMCPS;

    /* Stores whether the frame is currently being sent */
    bool currentlySending;

    /* number of retransmission attempts */
    uint8_t retryCounter;

    gnrc_netif_t *netif;

    /* declare related classes as friends */
    friend class DSMEPlatform;
    friend class DSMEMessageElement;
    friend class DSMEPlatformBase;
    friend class DSMEMessageBuffer;
};

}

#endif /* PKG_OPENDSME_DSMEMESSAGE_H *

/** @} */
