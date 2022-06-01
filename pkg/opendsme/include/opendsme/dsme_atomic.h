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

#ifndef OPENDSME_DSME_ATOMIC_H
#define OPENDSME_DSME_ATOMIC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Since openDSME runs on one thread, this is not required */
#define dsme_atomicBegin()
#define dsme_atomicEnd()

#endif /* OPENDSME_DSME_ATOMIC_H */

#ifdef __cplusplus
}
#endif
/** @} */
