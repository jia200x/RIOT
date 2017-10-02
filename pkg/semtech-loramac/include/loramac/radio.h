#ifndef LORAMAC_RADIO_H_
#define LORAMAC_RADIO_H_

#include "net/netdev.h"
#include "semtech-loramac/src/radio/radio.h"

#include "sx127x.h"
#include "sx127x_internal.h"


sx127x_t* radio_get_ptr(void);

void radio_set_ptr(sx127x_t* ptr);

RadioEvents_t* radio_get_event_ptr(void);

void radio_set_event_ptr(RadioEvents_t *events );

#endif
