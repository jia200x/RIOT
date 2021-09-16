#ifndef DSME_PLATFORM_H
#define DSME_PLATFORM_H
#include <assert.h>
#include <stdio.h>

#define ASSERT(x) assert(x)
//#define DSME_ASSERT(x) do {if (!(x)){__asm__("bkpt #0");while(1) {}}} while(0)
#define DSME_ASSERT(x) do {if (!(x)){printf("%s:%i\n", __FILE__, __LINE__);while(1) {}}} while(0)
#define DSME_SIM_ASSERT(x) assert(x)

#define LOG_INFO(x)
#define LOG_INFO_PURE(x)
#define LOG_INFO_PREFIX
#define LOG_ERROR(x)

#define HEXOUT std::hex
#define DECOUT std::dec

#define LOG_ENDL std::endl

#define LOG_DEBUG(x)
#define LOG_DEBUG_PURE(x)
#define LOG_DEBUG_PREFIX
#endif
