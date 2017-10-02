/*
 * Copyright (C) Cr0s
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief   Common header for sx1276 tests
 *
 * @author  Cr0s
 */
#ifndef COMMON_H_
#define COMMON_H_


#include <stdint.h>
#include "board.h"

#ifdef AT86RF2XX_PARAMS_BOARD
	#define SAMR21_XPRO
#else
	#define NZ32_SC151
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name SX1276 configuration
 * @{
 */
#define RF_FREQUENCY                                902900000   // Hz, 915MHz

#define TX_OUTPUT_POWER                             10          // dBm

#define LORA_PREAMBLE_LENGTH                        8           // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         10          // Symbols

#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION                           false

#ifdef NZ32_SC151

#define SX1276_DIO0 GPIO_PIN(PORT_B, 0)
#define SX1276_DIO1 GPIO_PIN(PORT_B, 1)
#define SX1276_DIO2 GPIO_PIN(PORT_C, 6)
#define SX1276_DIO3 GPIO_PIN(PORT_A, 10)

#define SX1276_RESET GPIO_PIN(PORT_A, 9)

/** SX1276 SPI */

#define USE_SPI_0

#ifdef USE_SPI_1
#define SX1276_SPI SPI_1
#define SX1276_SPI_NSS GPIO_PIN(PORT_C, 8)
#define SX1276_SPI_MODE SPI_CONF_FIRST_RISING
#define SX1276_SPI_SPEED SPI_SPEED_1MHZ
#endif

#ifdef USE_SPI_0
#define SX1276_SPI SPI_0
#define SX1276_SPI_NSS GPIO_PIN(PORT_C, 8)
#define SX1276_SPI_MODE SPI_CONF_FIRST_RISING
#define SX1276_SPI_SPEED SPI_SPEED_1MHZ
#endif

#endif

#ifdef SAMR21_XPRO

#define SX1276_DIO0 GPIO_PIN(PA, 13)
#define SX1276_DIO1 GPIO_PIN(PA, 7)
#define SX1276_DIO2 GPIO_PIN(PA, 6)
#define SX1276_DIO3 GPIO_PIN(PA, 18)

#define SX1276_RESET GPIO_PIN(PA, 28)

/** SX1276 SPI */

#define USE_SPI_1

#ifdef USE_SPI_1
#define SX1276_SPI SPI_1
#define SX1276_SPI_NSS GPIO_PIN(PA, 19)
#define SX1276_SPI_MODE SPI_CONF_FIRST_RISING
#define SX1276_SPI_SPEED SPI_SPEED_1MHZ
#endif

#ifdef USE_SPI_0
#define SX1276_SPI SPI_0
#define SX1276_SPI_NSS GPIO_PIN(PA, 4)
#define SX1276_SPI_MODE SPI_CONF_FIRST_RISING
#define SX1276_SPI_SPEED SPI_SPEED_1MHZ
#endif

#endif


void init_radio(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H_ */
/** @} */
