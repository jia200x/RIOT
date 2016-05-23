/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     ppp_lcp
 * @file
 * @brief       Implementation of PPP's LCP protocol
 *
 * @author      José Ignacio Alamos <jialamos@uc.cl>
 * @}
 */

#include "net/gnrc/ppp/lcp.h"
#include "net/gnrc/ppp/ppp.h"
#include <inttypes.h>
#include "net/gnrc/ppp/opt.h"
#include "net/gnrc/pkt.h"
#include "net/ppp/hdr.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/nettype.h"
#include <errno.h>
#include "net/gnrc/ppp/fsm.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#if ENABLE_DEBUG
/* For PRIu16 etc. */
#include <inttypes.h>
#endif

static cp_conf_t *lcp_get_conf_by_code(ppp_fsm_t *cp, uint8_t code)
{
	switch(code)
	{
		case LCP_OPT_MRU:
			return &cp->conf[LCP_MRU];
		case LCP_OPT_ACCM:
			return &cp->conf[LCP_ACCM];
		case LCP_OPT_AUTH:
			return &cp->conf[LCP_AUTH];
		default:
			return NULL;
	}
}

uint8_t lcp_mru_is_valid(ppp_option_t *opt)
{
	uint8_t *payload = ppp_opt_get_payload(opt);
	uint16_t u16 = ((*payload)<<8) + *(payload+1);
	if(u16 > LCP_MAX_MRU){
		return false;
	}
	return true;
}

uint8_t lcp_mru_build_nak_opts(uint8_t *buf)
{
	uint8_t len = 4;
	ppp_option_t *opt = (ppp_option_t*) buf;
	uint8_t *payload = ppp_opt_get_payload(opt);
	if(opt)
	{
		ppp_opt_set_type(opt, 1);	
		ppp_opt_set_length(opt, len);
		*payload = (LCP_DEFAULT_MRU & 0xFF00) >> 8;
		*(payload+1) = LCP_DEFAULT_MRU & 0xFF;
	}
	return len;
}


void lcp_mru_set(ppp_fsm_t *lcp, ppp_option_t *opt, uint8_t peer)
{
	DEBUG("Called LCP MRU set, but still not implemented!\n");
	if(peer)
	{
		((lcp_t*) lcp)->peer_mru = byteorder_ntohs(*((network_uint16_t*) ppp_opt_get_payload(opt)));
	}
	else
	{
		((lcp_t*) lcp)->mru = byteorder_ntohs(*((network_uint16_t*) ppp_opt_get_payload(opt)));
	}
}


uint8_t lcp_accm_is_valid(ppp_option_t *opt)
{
	/*Flags are always valid*/
	return true;
}

uint8_t lcp_accm_build_nak_opts(uint8_t *buf)
{
	/* Never called */
	return true;
}

void lcp_accm_set(ppp_fsm_t *lcp, ppp_option_t *opt, uint8_t peer)
{
	gnrc_pppdev_t *dev = ((ppp_protocol_t*) lcp)->pppdev;
	DEBUG("Setting ACCM\n");
	if(peer)
		dev->netdev->driver->set(dev->netdev, PPPOPT_ACCM_RX, (void*) ppp_opt_get_payload(opt), 4);
	else
		dev->netdev->driver->set(dev->netdev, PPPOPT_ACCM_TX, (void*) ppp_opt_get_payload(opt), 4);
}

uint8_t lcp_auth_is_valid(ppp_option_t *opt)
{
	network_uint16_t *u16;
	u16 = (network_uint16_t*) ppp_opt_get_payload(opt);
	uint16_t val = byteorder_ntohs(*u16);

	/*Only accept PAP*/
	DEBUG("Auth is %04x\n", val);
	if(val == 0xc023)
		return true;

	return false;
}

uint8_t lcp_auth_build_nak_opts(uint8_t *buf)
{
	uint8_t len = 4;
	ppp_option_t *opt = (ppp_option_t*) buf;
	uint8_t *payload = ppp_opt_get_payload(opt);

	if(opt)
	{
		ppp_opt_set_type(opt, 3);	
		ppp_opt_set_length(opt, len);
		*payload = (0xC0);
		*(payload+1) = 0x23;
	}
	return len;
}

void lcp_auth_set(ppp_fsm_t *lcp, ppp_option_t *opt, uint8_t peer)
{
	lcp_t *l = (lcp_t*) lcp;
	DEBUG("Setting Auth (to PAP for the moment) \n");
	if(peer)
	{
		DEBUG("Setting target\n");
		lcp->targets = (lcp->targets & 0xFF00) | (ID_PAP & 0xFF);
		l->local_auth = 1;	
	}
	else
	{
		l->remote_auth = 1;
	}
}

static void lcp_config_init(ppp_fsm_t *lcp)
{
	lcp->conf = LCP_NUMOPTS ? ((lcp_t*) lcp)->lcp_opts : NULL;

	lcp->conf[LCP_MRU].type = LCP_OPT_MRU;
	lcp->conf[LCP_MRU].default_value = byteorder_htonl(3500);
	lcp->conf[LCP_MRU].size = 2;
	lcp->conf[LCP_MRU].flags = 0;
	lcp->conf[LCP_MRU].next = &lcp->conf[LCP_ACCM];
	lcp->conf[LCP_MRU].is_valid = &lcp_mru_is_valid;
	lcp->conf[LCP_MRU].build_nak_opts = &lcp_mru_build_nak_opts;
	lcp->conf[LCP_MRU].set = &lcp_mru_set;

	lcp->conf[LCP_ACCM].type = LCP_OPT_ACCM;
	lcp->conf[LCP_ACCM].default_value = byteorder_htonl(0xFFFFFFFF);
	lcp->conf[LCP_ACCM].size = 4;
	lcp->conf[LCP_ACCM].flags = 0;
	lcp->conf[LCP_ACCM].next = &lcp->conf[LCP_AUTH];
	lcp->conf[LCP_ACCM].is_valid = &lcp_accm_is_valid;
	lcp->conf[LCP_ACCM].build_nak_opts = &lcp_accm_build_nak_opts;
	lcp->conf[LCP_ACCM].set = &lcp_accm_set;

	lcp->conf[LCP_AUTH].type = LCP_OPT_AUTH;
	lcp->conf[LCP_AUTH].default_value = byteorder_htonl(0xFFFFFFFF);
	lcp->conf[LCP_AUTH].size = 2;
	lcp->conf[LCP_AUTH].flags = 0;
	lcp->conf[LCP_AUTH].next = NULL;
	lcp->conf[LCP_AUTH].is_valid = &lcp_auth_is_valid;
	lcp->conf[LCP_AUTH].build_nak_opts = &lcp_auth_build_nak_opts;
	lcp->conf[LCP_AUTH].set = &lcp_auth_set;
}


int lcp_handler(struct ppp_protocol_t *protocol, uint8_t ppp_event, void *args)
{
	ppp_fsm_t *lcp = (ppp_fsm_t*) protocol;
	if(ppp_event == PPP_MONITOR)
	{
		/*Send Echo Request*/
		DEBUG("Sending echo request");
		gnrc_pktsnip_t *pkt = pkt_build(GNRC_NETTYPE_LCP, PPP_ECHO_REQ, lcp->cr_sent_identifier++, NULL);
		gnrc_ppp_send(protocol->pppdev, pkt);
		return 0;
	}
	else if(ppp_event == PPP_UL_FINISHED)
	{
		send_ppp_event(&protocol->msg, ppp_msg_set((lcp->targets >> 8) & 0xffff, PPP_UL_FINISHED));
		return 0;
	}
	else
	{
		return fsm_handle_ppp_msg(protocol, ppp_event, args);
	}
}

int lcp_init(gnrc_pppdev_t *ppp_dev, ppp_fsm_t *lcp)
{
	((ppp_protocol_t*) lcp)->pppdev = ppp_dev;
	fsm_init(ppp_dev, lcp);
	lcp_config_init(lcp);

	lcp->supported_codes = FLAG_CONF_REQ | FLAG_CONF_ACK | FLAG_CONF_NAK | FLAG_CONF_REJ | FLAG_TERM_REQ | FLAG_TERM_ACK | FLAG_CODE_REJ | FLAG_ECHO_REQ | FLAG_ECHO_REP | FLAG_DISC_REQ;
	((ppp_protocol_t*)lcp)->id = ID_LCP;
	((lcp_t*)lcp)->pr_id = 0;
	lcp->prottype = GNRC_NETTYPE_LCP;
	lcp->restart_timer = LCP_RESTART_TIMER;
	lcp->get_conf_by_code = &lcp_get_conf_by_code;
	lcp->prot.handler = &lcp_handler;
	lcp->targets = ((ID_PPPDEV & 0xffff) << 8) | (BROADCAST_NCP & 0xffff);
	((lcp_t*) lcp)->mru = 1500;
	((lcp_t*) lcp)->peer_mru = 1500;
	((lcp_t*) lcp)->remote_auth = 0;
	((lcp_t*) lcp)->local_auth = 0;
	return 0;
}
