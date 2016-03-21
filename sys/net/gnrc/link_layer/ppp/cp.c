
/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net_ppp_cp
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
#include "net/gnrc/ppp/cp.h"
#include "net/gnrc/ppp/cp_fsm.h"
#include "net/ppp/pkt.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#if ENABLE_DEBUG
/* For PRIu16 etc. */
#include <inttypes.h>
#endif



static int _handle_cp_rcr(ppp_cp_t *cp)
{
	if (cp->metadata.opts_status_content  & (OPT_HAS_NAK | OPT_HAS_REJ))
	{
		cp->event = E_RCRm;
	}
	else
	{
		cp->event = E_RCRp;
	}

return 0; /*TODO: Fix output*/
}
static int _handle_cp_rca(ppp_cp_t *cp)
{
	cp_pkt_t *pkt = cp->metadata.pkt;
	/* Identifier should match */
	uint8_t pkt_id = ppp_pkt_get_id(pkt);
	uint8_t pkt_length = ppp_pkt_get_length(pkt);

	if (pkt_id != cp->cr_sent_identifier)
	{
		return -1; /* TODO: Fix error code*/
	}

	if (cp->cr_sent_size != pkt_length || memcmp(cp->cr_sent_opts,pkt->payload,pkt_length-sizeof(cp_hdr_t)))
	{
		printf("I'm here\n");
		return -1; /* TODO: Error code*/
	}

	cp->event = E_RCA;
return 0; /*TODO: Fix output*/
}

/* Fix params for request */
static int _handle_cp_nak(ppp_cp_t *cp)
{
	cp->negotiate_nak(cp->cp_options, &cp->metadata);
	cp->event = E_RCN;
	return 0; /*TODO: Fix output*/
}

#if 0
static int _handle_cp_rej(ppp_cp_t *cp, cp_pkt_t *pkt)
{
	l_lcp->event = E_RCJ;
}

static int _handle_cp_term_req(ppp_cp_t *cp, cp_pkt_t *pkt)
{
	cp->tr_identifier = cp_pkt->hdr->id;
	cp->event = E_RTR;
}

static int _handle_cp_term_ack(ppp_cp_t *cp, cp_pkt_t *pkt)
{
	cp->event = E_RTA;
}

static int _handle_cp_code_rej(ppp_cp_t *cp, gnrc_pktsnip_t *pkt)
{
	cp->event = E_RXJm;
}
#endif

void handle_cp_pkt(ppp_cp_t *cp, cp_pkt_t *pkt)
{
	cp->metadata.pkt = pkt;
	ppp_pkt_gen_metadata(&cp->metadata, pkt, cp->get_option_status);

	int type = ppp_pkt_get_code(pkt);
	
	switch(type){
		case PPP_CONF_REQ:
			_handle_cp_rcr(cp);
			break;
		case PPP_CONF_ACK:
			_handle_cp_rca(cp);
			break;
		case PPP_CONF_NAK:
			_handle_cp_nak(cp);
			break;
		case PPP_CONF_REJ:
			break;
		case PPP_TERM_REQ:
			break;
		case PPP_TERM_ACK:
			break;
		case PPP_CODE_REJ:
			break;
		case PPP_PROT_REJ:
			break;
		case PPP_ECHO_REQ:
			break;
		case PPP_ECHO_REP:
			break;
		case PPP_DISC_REQ:
			break;
		case PPP_IDENT:
			break;
		case PPP_TIME_REM:
			break;
	}
}

void ppp_pkt_gen_metadata(cp_pkt_metadata_t *metadata, cp_pkt_t *pkt, int (*get_opt_status)(void*))
{
	uint8_t code = ppp_pkt_get_code(pkt);
	metadata->pkt = pkt;
	metadata->opts_status_content=0;

	void *curr_opt;
	uint16_t curr_status;

	/* Check if current code has options */
	if (code == PPP_CONF_REQ)
	{
		ppp_opts_init(&metadata->opts, pkt);
		curr_opt = ppp_opts_get_head(&metadata->opts);
		for(int i=0; i<ppp_opts_get_num(&metadata->opts); i++)
		{
			curr_status = get_opt_status(curr_opt);
			switch(curr_status)
			{
				case CP_CREQ_ACK:
					metadata->opts_status_content |= OPT_HAS_ACK;
					break;
				case CP_CREQ_NAK:
					metadata->opts_status_content |= OPT_HAS_NAK;
					break;
				case CP_CREQ_REJ:
					metadata->opts_status_content |= OPT_HAS_REJ;
					break;
			}
			curr_opt = ppp_opts_next(&metadata->opts);
			metadata->tagged_opts[i] = curr_status;
		}
	}
}
