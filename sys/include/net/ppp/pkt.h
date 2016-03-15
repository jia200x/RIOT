
#ifndef PPP_PKT_H_
#define PPP_PKT_H_

#include <inttypes.h>

#include "net/gnrc.h"
#include "net/gnrc/netdev2.h"

#ifdef __cplusplus
extern "C" {
#endif
#define PPP_PAYLOAD_SIZE (2000)

/*  PPP pkt header struct */
typedef struct __attribute__((packed)){
	uint8_t code;
	uint8_t id;
	uint16_t length;
} cp_hdr_t;

/* A PPP packet*/
typedef struct __attribute__((packed))
{
	cp_hdr_t *hdr;
	uint16_t payload[PPP_PAYLOAD_SIZE]; 
} cp_pkt_t;


int ppp_pkt_populate(uint8_t *data, size_t length, cp_pkt_t *cp_pkt);

#ifdef __cplusplus
}
#endif

#endif /* PPP_PKT_H_ */
/** @} */
