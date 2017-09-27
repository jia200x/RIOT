/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech
 
Description: Timer objects and scheduling management
 
License: Revised BSD License, see LICENSE.TXT file include in the project
 
Maintainer: Miguel Luis and Gregory Cristian
*/

#include "loramac/board_definitions.h"
#include "xtimer.h"

void TimerInit( TimerEvent_t *obj, void ( *cb )( void ) )
{
    obj->dev.target = 0;
    obj->running = 0;
    obj->dev.callback = (xtimer_callback_t) cb;
}

void TimerReset( TimerEvent_t *obj )
{
    TimerStop(obj);
    TimerStart(obj);
}
 
void TimerStart( TimerEvent_t *obj )
{
    obj->running = 1;
    xtimer_set(&(obj->dev), obj->timeout);
}
 
void TimerStop( TimerEvent_t *obj )
{
    obj->running = 0;
    xtimer_remove(&(obj->dev));
}
 
void TimerSetValue( TimerEvent_t *obj, uint32_t value )
{
    if(obj->running)
        xtimer_remove(&(obj->dev));
    obj->timeout = value*1000;
}

TimerTime_t TimerGetCurrentTime( void )
{
    uint64_t CurrentTime = xtimer_now_usec64();
    return ( ( TimerTime_t )CurrentTime );
}
 
TimerTime_t TimerGetElapsedTime( TimerTime_t savedTime )
{
    uint64_t CurrentTime = xtimer_now_usec64();
    return ( TimerTime_t )( CurrentTime - savedTime );
}
 
TimerTime_t TimerGetFutureTime( TimerTime_t eventInFuture )
{
    uint64_t CurrentTime = xtimer_now_usec64();
    return ( TimerTime_t )( CurrentTime + eventInFuture );
}

void TimerLowPowerHandler( void )
{

}
