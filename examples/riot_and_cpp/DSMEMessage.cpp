#include "assert.h"
#include "opendsme/DSMEMessage.h"

namespace dsme {

void DSMEMessage::prependFrom(DSMEMessageElement *msg)
{
    gnrc_pktsnip_t *head = gnrc_pktbuf_add(pkt, NULL, msg->getSerializationLength(), GNRC_NETTYPE_UNDEF);
    DSME_ASSERT(msg->getSerializationLength() <= 127);
    DSME_ASSERT(head);
    int res = gnrc_pktbuf_merge(head);
    DSME_ASSERT(res == 0);
    DSME_ASSERT(gnrc_pkt_len(head) <= 127);
    pkt = head;
    assert(head);
    Serializer s((uint8_t*) head->data, SERIALIZATION);
    msg->serialize(s);
}

void DSMEMessage::decapsulateTo(DSMEMessageElement* me)
{
    this->copyTo(me);
    this->dropHdr(me->getSerializationLength());
    //this->setPayloadLength(this->getPayloadLength()-me->getSerializationLength());
}

void DSMEMessage::copyTo(DSMEMessageElement* msg)
{
    Serializer s(this->getPayload(), DESERIALIZATION);
    msg->serialize(s);
    DSME_ASSERT(this->getPayload()+msg->getSerializationLength() == s.getData());
}

uint8_t DSMEMessage::getMPDUSymbols()
{
    assert(false);
    return 0;
}

int DSMEMessage::loadBuffer(size_t len)
{
    int res = -ENOBUFS;
    gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, NULL, len, GNRC_NETTYPE_UNDEF);
    DSME_ASSERT(len <= 127);
    if (pkt == NULL) {
        DSME_ASSERT(false);
        goto end;
    }
    this->pkt = pkt; 
    res = 0;

end:
    return res;
}

int DSMEMessage::dropHdr(size_t len)
{
    gnrc_pktsnip_t *hdr = gnrc_pktbuf_mark(this->pkt, len, GNRC_NETTYPE_UNDEF);
    if (!hdr) {
        DSME_ASSERT(false);
        return -EINVAL;
    }
    return 0;
}
}

