#include "assert.h"
#include "opendsme/DSMEMessage.h"

namespace dsme {

void DSMEMessage::prependFrom(DSMEMessageElement *msg)
{
    gnrc_pktsnip_t *head = gnrc_pktbuf_add(pkt, NULL, msg->getSerializationLength(), GNRC_NETTYPE_UNDEF);
    pkt = head;
    assert(head);
    Serializer s((uint8_t*) head->data, SERIALIZATION);
    msg->serialize(s);
}

void DSMEMessage::decapsulateTo(DSMEMessageElement* me)
{
    assert(false);
}

void DSMEMessage::copyTo(DSMEMessageElement* msg)
{
    assert(false);
}

uint8_t DSMEMessage::getMPDUSymbols()
{
    assert(false);
    return 0;
}
}

