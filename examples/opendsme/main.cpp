/*
 * Copyright (C) 2021 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief       OpenDSME example
 *
 * @author      José I. Álamos <jose.alamos@haw-hamburg.de>
 */

#include <cstdio>
#include "opendsme/DSMEPlatform.h"

dsme::DSMEPlatform m_dsme;

using namespace std;

/* main */
int main()
{
    printf("\n************ RIOT and OpenDSME ***********\n");
    printf("\n");

    m_dsme.initialize();
    m_dsme.start();

    return 0;
}
