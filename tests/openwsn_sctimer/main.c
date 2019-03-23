#include "stdint.h"
#include "string.h"

#include "board.h"
#include "debugpins.h"
#include "sctimer.h"

#include "ps.h"

//=========================== defines =========================================

#define SCTIMER_PERIOD     (32768) // @32kHz = 1s or 2 s?

//=========================== prototypes ======================================

void cb_compare(void);

//=========================== main ============================================

/**
\brief The program starts executing here.
*/
int main(void)
{

   openwsn_bootstrap();
   sctimer_set_callback(cb_compare);
   sctimer_setCompare(sctimer_readCounter()+SCTIMER_PERIOD);
   printf("main sctimer: %" PRIu32 ", next wakeup: %" PRIu32 "\n",
      sctimer_readCounter(), sctimer_readCounter()+ SCTIMER_PERIOD);
   LED0_TOGGLE;
}

//=========================== callbacks =======================================
int coutner=0;
void cb_compare(void) {

   //puts("cb_compare");
   printf("%i. cb_compare, sctimer: %" PRIu32 ", next wakeup: %" PRIu32 "\n",
      coutner++, sctimer_readCounter(), sctimer_readCounter()+ SCTIMER_PERIOD);

   LED0_TOGGLE;

   // schedule again
   sctimer_setCompare(sctimer_readCounter()+SCTIMER_PERIOD);
}
