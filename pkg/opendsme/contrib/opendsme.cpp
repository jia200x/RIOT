#include "opendsme/DSMEPlatform.h"
#include "opendsme/opendsme.h"
#include "mac_services/DSME_Common.h"

dsme::DSMEPlatform m_dsme;

extern "C" {
static bool _pan_coord;
extern void heap_stats(void);
static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{

}

static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif)
{

}

static int _get(gnrc_netif_t *netif, gnrc_netapi_opt_t *opt)
{

}

static void _start(bool pan_coord)
{
    printf("[info];");
    heap_stats();
    opendsme_init(pan_coord);
}

static int _set(gnrc_netif_t *netif, const gnrc_netapi_opt_t *opt)
{
    switch (opt->opt) {
        case NETOPT_LINK:
            _start(_pan_coord);
            break;
        case NETOPT_PAN_COORD:
            if (*((bool*)opt->data) == true) {
                puts("[info];ROLE;PAN_COORD");
                _pan_coord = true;
            }
            else {
                puts("[info];ROLE;CHILD");
                _pan_coord = false;
            }
            break;
        default:
            assert(false);
            break;
    }
}


static void _init(gnrc_netif_t *netif)
{
    // TODO:
    // netif->device_type = (uint8_t)tmp;
    //_update_l2addr_from_dev(netif);
    netif->flags = 0;
}

static const gnrc_netif_ops_t dsme_ops = {
    .init = _init,
    .send = _send,
    .recv = _recv,
    .get = _get,
    .set = _set,
};

int gnrc_netif_dsme_create(gnrc_netif_t *netif, char *stack, int stacksize,
                                 char priority, const char *name, netdev_t *dev)
{
    return gnrc_netif_create(netif, stack, stacksize, priority, name, dev,
                             &dsme_ops);
}
event_queue_t *opendsme_get_evq(void)
{
    return &m_dsme.netif.evq;
}
gnrc_netif_t *opendsme_get_netif(void)
{
    return &m_dsme.netif;
}

}

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

void opendsme_allocate_gts(uint8_t superframeID, uint8_t slotID, uint8_t channelID, bool tx, uint16_t address)
{
    m_dsme.allocateGTS(superframeID, slotID, channelID, tx ? dsme::Direction::TX : dsme::Direction::RX, address);
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
