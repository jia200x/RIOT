/*
 * Copyright (C) 2021 HAW Hamburg
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
#ifndef OPENDSME_H
#define OPENDSME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int opendsme_init(bool pan_coord);
int opendsme_is_associated(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENDSME_H */
/** @} */
