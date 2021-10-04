#include "opendsme/DSMEPlatform.h"
#include "opendsme/opendsme.h"

dsme::DSMEPlatform m_dsme;

int opendsme_init(bool pan_coord)
{
    m_dsme.initialize(pan_coord);
    m_dsme.start();
}

int opendsme_is_associated(void)
{
    return m_dsme.isAssociated();
}

void opendsme_get_short_addr(network_uint16_t *addr)
{
    m_dsme.getShortAddress(addr);
}

#if 0
void opendsme_get_ext_addr(eui64_t *addr)
{
}
#endif

void opendsme_send_frame(void *addr, size_t addr_len, gnrc_pktsnip_t *pkt)
{
    if (addr_len == 2) {
        uint16_t _addr = byteorder_ntohs(*((network_uint16_t*) addr));
        m_dsme.send_pkt(_addr, (iolist_t*) pkt);
    }
}
