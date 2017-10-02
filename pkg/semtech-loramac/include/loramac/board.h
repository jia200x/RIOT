#ifndef LORAMAC_BOARD_DEFINITIONS_H

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "loramac/timer.h"
#include "loramac/radio.h"
#include "loramac/utilities.h"

//TODO: Remove this line
#define REG_LR_SYNCWORD 0x39

/*!
 * Radio wakeup time from SLEEP mode
 */
#define RADIO_OSC_STARTUP                           1 // [ms]

/*!
 * Radio PLL lock and Mode Ready delay which can vary with the temperature
 */
#define RADIO_SLEEP_TO_RX                           2 // [ms]

/*!
 * Radio complete Wake-up Time with margin for temperature compensation
 */
#define RADIO_WAKEUP_TIME ( RADIO_OSC_STARTUP + RADIO_SLEEP_TO_RX )

#endif

