#include "assert.h"
#include "opendsme/DSMEMessage.h"
#include "net/gnrc/pktdump.h"
#include "net/gnrc.h"

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
}

void DSMEMessage::copyTo(DSMEMessageElement* msg)
{
    Serializer s(this->getPayload(), DESERIALIZATION);
    msg->serialize(s);
    DSME_ASSERT(this->getPayload()+msg->getSerializationLength() == s.getData());
}

uint8_t DSMEMessage::getMPDUSymbols()
{
    /* Not used by OpenDSME */
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

int DSMEMessage::loadBuffer(iolist_t *pkt)
{
    int res = -ENOBUFS;
    if (pkt == NULL) {
        DSME_ASSERT(false);
        goto end;
    }
    this->pkt = (gnrc_pktsnip_t*) pkt; 
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

void DSMEMessage::releaseMessage()
{
    DSME_ASSERT(!free);
    if (pkt) {
        gnrc_pktbuf_release(pkt);
    }
    free = true;
}

iolist_t *DSMEMessage::getIolPayload()
{
    return (iolist_t*) pkt;
}

iolist_t *DSMEMessage::clearMessage()
{
    pkt = NULL;
    free = false;
    prepare();
}

void DSMEMessage::dispatchMessage()
{
    DSME_ASSERT(!free);
    if (!gnrc_netapi_dispatch_receive(GNRC_NETTYPE_UNDEF, GNRC_NETREG_DEMUX_CTX_ALL,
                                      pkt)) {
        releaseMessage();
        return;
    }
    else {
        free = true;
    }
}
}

