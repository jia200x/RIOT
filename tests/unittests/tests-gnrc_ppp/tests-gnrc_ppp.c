/*
 * Copyright (C) 2014 José Ignacio Alamos <jialamos@uc.cl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 */

#include <errno.h>
#include <stdlib.h>

#include "embUnit.h"

#include "unittests-constants.h"
#include "tests-gnrc_ppp.h"


/* Dummy get_option_status for testing fake prot*/
static int fakeprot_get_option_status(cp_opt_hdr_t *opt)
{
	/* if type > 2, reject */
	if (opt->type > 2)
	{
		return CP_CREQ_REJ;
	}
	
	/* Nak every packet with u16 payload < 10 */
	uint8_t *p = (uint8_t*) opt;
	printf("Value 0: %i\n", (*p));
	printf("Value 1: %i\n", *(p+1));
	uint16_t u16 = (*(p+sizeof(cp_opt_hdr_t))<<8) + *(p+sizeof(cp_opt_hdr_t)+1);
	printf("Value type: %i\n", u16);
	if (u16 > 10)
	{
		return CP_CREQ_NAK;
	}
	return CP_CREQ_ACK;

}

static void test_gnrc_ppp_lcp_recv_cr_ack(void)
{
	/*Make fake ctrl prot*/
	ppp_cp_t fake_prot;
	fake_prot.get_opt_status = &fakeprot_get_option_status;

	/* |--ConfigureReq--|--Identifier--|--Length(MSB)--|--Length(LSB)--|--Type--|--Length--|--MRU(MSB)--|--MRU(LSB)--| */
	uint8_t good_packet[8] = {0x01,0x00,0x00,0x08,0x01,0x04,0x00,0x01};
	cp_pkt_t cp_pkt;
	ppp_pkt_init(good_pkt, 8, &cp_pkt);

	handle_cp_pkt(&fake_prot, &cp_pkt);

}


Test *tests_gnrc_ppp_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_gnrc_ppp_lcp_recv_cr_ack),
    };

    EMB_UNIT_TESTCALLER(gnrc_ppp_tests, NULL, NULL, fixtures);

    return (Test *)&gnrc_ppp_tests;
}

void tests_gnrc_ppp(void)
{
    TESTS_RUN(tests_gnrc_ppp_tests());
}
/** @} */
