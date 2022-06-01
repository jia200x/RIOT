/*
 * Copyright (C) 2022 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     pkg_opendsme
 *
 * @{
 *
 * @file
 *
 * @author      José I. Álamos <jose.alamos@haw-hamburg.de>
 */

#ifndef OPENDSME_DSME_PLATFORM_H
#define OPENDSME_DSME_PLATFORM_H

#include <assert.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASSERT(x) assert(x)
#define DSME_ASSERT(x) do { if (!(x)) { printf("%s:%i\n", __FILE__, __LINE__); \
                                        while (1) {} } } while (0)
#define DSME_SIM_ASSERT(x)

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

#ifdef __cplusplus
}
#endif

#endif /* OPENDSME_DSME_PLATFORM_H */
/** @} */
