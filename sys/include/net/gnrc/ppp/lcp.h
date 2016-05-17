#ifndef PPP_LCP_H_
#define PPP_LCP_H_

#include "net/gnrc/ppp/fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCP_OPT_MRU (1)
#define LCP_OPT_ACCM (2)
#define LCP_OPT_AUTH (3)

#define LCP_MAX_MRU (2000)
#define LCP_DEFAULT_MRU (1500)

#define LCP_MRU_EN (1)
#define LCP_RESTART_TIMER (3000000U)


typedef enum{
	LCP_MRU,
	LCP_ACCM,
	LCP_AUTH,
	LCP_NUMOPTS
} lcp_options_t;


typedef struct lcp_t
{
	ppp_fsm_t fsm;
	cp_conf_t lcp_opts[LCP_NUMOPTS];
	uint8_t pr_id;
	uint16_t peer_mru;
	uint16_t mru;
	uint8_t local_auth;
	uint8_t remote_auth;
} lcp_t;

struct gnrc_pppdev_t;
struct ppp_fsm_t;

int lcp_init(struct gnrc_pppdev_t *ppp_dev, struct ppp_fsm_t *lcp);

#ifdef __cplusplus
}
#endif

#endif /* PPP_LCP_H_ */
/** @} */
