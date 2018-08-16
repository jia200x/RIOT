#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"
#include "net/lora.h"
#include "net/gnrc/lorawan/lorawan.h"
#include "errno.h"

#define PKT_WRITE_BYTE(CURSOR, BYTE) *(CURSOR++) = BYTE

#define PKT_WRITE(CURSOR, SRC, LEN) do {\
    memcpy(CURSOR, SRC, LEN); \
    CURSOR += LEN; \
} while (0);

#define MTYPE_MASK           0b11100000
#define MTYPE_JOIN_REQUEST   0b000
#define MTYPE_JOIN_ACCEPT    0b001
#define MTYPE_UNCNF_UPLINK   0b010
#define MTYPE_UNCNF_DOWNLINK 0b011
#define MTYPE_CNF_UPLINK     0b100
#define MTYPE_CNF_DOWNLINK   0b101
#define MTYPE_REJOIN_REQ     0b110
#define MTYPE_PROPIETARY     0b111

#define MAJOR_MASK     0b11
#define MAJOR_LRWAN_R1 0b00

#define DEV_ADDR_LEN 4
#define FCTRL_LEN 1
#define FCNT_LEN 2
#define FOPTS_MAX_LEN 16


#define ADR_MASK 0x80
#define ADR_ACK_REQ_MASK 0x40
#define ACK_MASK 0x20
#define FPENDING_MASK 0x10
#define FOPTS_MASK 0x0F


#define JOIN_REQUEST_SIZE 23
#define MIC_SIZE 4

static size_t build_join_req_pkt(uint8_t *appeui, uint8_t *deveui, uint8_t *appkey, uint8_t *dev_nonce, uint8_t *packet)
{
    uint8_t *p = packet;

    uint8_t mhdr = 0;

    /* Message type */
    mhdr &= ~MTYPE_MASK;
    mhdr |= MTYPE_JOIN_REQUEST << 5;

    /* Major */
    mhdr &= ~MAJOR_MASK;
    mhdr |= MAJOR_LRWAN_R1;

    PKT_WRITE_BYTE(p, mhdr);
    PKT_WRITE(p, appeui, 8);
    PKT_WRITE(p, deveui, 8);

    PKT_WRITE(p, dev_nonce, 2);

    uint32_t mic = calculate_mic(packet, JOIN_REQUEST_SIZE-MIC_SIZE, appkey);

    PKT_WRITE_BYTE(p, mic & 0xFF);
    PKT_WRITE_BYTE(p, (mic >> 8) & 0xFF);
    PKT_WRITE_BYTE(p, (mic >> 16) & 0xFF);
    PKT_WRITE_BYTE(p, (mic >> 24) & 0xFF);

    return JOIN_REQUEST_SIZE;
}

void gnrc_lorawan_send_join_request(gnrc_netif_t *netif)
{
    netdev_t *dev = netif->dev;

    uint8_t buf[24];

    uint32_t channel_freq = 868300000;
    //TODO
    uint8_t bw = LORA_BW_125_KHZ;
    uint8_t sf = LORA_SF7;
    uint8_t cr = LORA_CR_4_5;

    dev->driver->set(dev, NETOPT_CHANNEL_FREQUENCY, &channel_freq, sizeof(channel_freq));
    dev->driver->set(dev, NETOPT_BANDWIDTH, &bw, sizeof(bw));
    dev->driver->set(dev, NETOPT_SPREADING_FACTOR, &sf, sizeof(sf));
    dev->driver->set(dev, NETOPT_CODING_RATE, &cr, sizeof(cr));

    /* Dev Nonce */
    uint32_t random_number;
    dev->driver->get(dev, NETOPT_RANDOM, &random_number, sizeof(random_number));
    printf("Random: %i\n", (unsigned) random_number);

    netif->lorawan.dev_nonce[0] = random_number & 0xFF;
    netif->lorawan.dev_nonce[1] = (random_number >> 8) & 0xFF;

    /* build join request */
    size_t pkt_size = build_join_req_pkt(netif->lorawan.appeui, netif->lorawan.deveui, netif->lorawan.appkey, netif->lorawan.dev_nonce, buf);

    iolist_t iolist = {
        .iol_base = buf,
        .iol_len = pkt_size
    };

    for(unsigned int i=0;i<pkt_size;i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");
    uint8_t syncword = LORA_SYNCWORD_PUBLIC;

    dev->driver->set(dev, NETOPT_SYNCWORD, &syncword, sizeof(syncword));

    if (dev->driver->send(dev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    }
    puts("Sent");
}
