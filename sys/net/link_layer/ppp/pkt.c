
/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net_ppp_pkt
 * @file
 * @brief       Implementation of the of Generic Control Protocol for PPP
 *
 * @author      José Ignacio Alamos <jialamos@uc.cl>
 * @}
 */

#include <errno.h>
#include <string.h>

#include "msg.h"
#include "thread.h"
#include "net/gnrc.h"
#include "net/ppptype.h"
#include "net/gnrc/ppp/ppp.h"
#include "net/gnrc/ppp/lcp.h"
#include "net/hdlc/hdr.h"



/*TODO return error if populate went bad */
static int ppp_pkt_populate(uint8_t *data, size_t length, cp_pkt_t *cp_pkt)
{
	cp_hdr_t hdr = (cp_hdr_t*) pkt->data;
	cp_pkt->hdr = &cp_data;

	if (hdr->length != pkt->size) {
		/* TODO: Error code*/
		return 0;
	}

	int status = _parse_cp_options(cp_pkt->opts, pkt->data+sizeof(cp_hdr_t),(size_t) pkt->length-sizeof(cp_hdr_t));
	return 0;
}
