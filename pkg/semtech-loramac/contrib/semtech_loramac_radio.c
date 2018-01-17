/*
 * Copyright (C) 2017 Fundacion Inria Chile
 *               2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     pkg_semtech-loramac
 * @file
 * @brief       Compatibility functions for controlling the radio driver
 *
 * @note        Even if the function names are related to SX1276, they use
 *              the RIOT SX127x driver and can be used with SX1272 and SX1276
 *              devices.
 *
 * @author      Jose Ignacio Alamos <jialamos@uc.cl>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @author      Francisco Molina <francisco.molina@inria.cl>
 * @}
 */

#include "net/lora.h"
#include "net/netdev.h"

#include "sx127x.h"
#include "sx127x_internal.h"
#include "sx127x_netdev.h"

#include "semtech-loramac/board.h"

#include "radio/radio.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

extern sx127x_t sx127x;

#define LORAMAC_RX_WINDOW_DURATION  (600UL * US_PER_MS)

/*
 * Radio driver functions implementation wrappers, the netdev2 object
 * is known within the scope of the function
 */
void SX1276Init(RadioEvents_t *events)
{
    (void) events;
    sx127x_init(&sx127x);
}

RadioState_t SX1276GetStatus(void)
{
    return (RadioState_t)sx127x_get_state(&sx127x);
}

void SX1276SetModem(RadioModems_t modem)
{
    sx127x_set_modem(&sx127x, (uint8_t)modem);
}

void SX1276SetChannel(uint32_t freq)
{
    sx127x_set_channel(&sx127x, freq);
}

bool SX1276IsChannelFree(RadioModems_t modem, uint32_t freq,
                         int16_t rssiThresh, uint32_t maxCarrierSenseTime )
{
    (void) modem;
    (void) maxCarrierSenseTime;
    return sx127x_is_channel_free(&sx127x, freq, rssiThresh);
}

uint32_t SX1276Random(void)
{
    return sx127x_random(&sx127x);
}

void SX1276SetRxConfig(RadioModems_t modem, uint32_t bandwidth,
                       uint32_t spreading_factor, uint8_t coding_rate,
                       uint32_t bandwidthAfc, uint16_t preambleLen,
                       uint16_t symbTimeout, bool fixLen,
                       uint8_t payloadLen,
                       bool crcOn, bool freqHopOn, uint8_t hopPeriod,
                       bool iqInverted, bool rxContinuous)
{
    (void) bandwidthAfc;
    (void) symbTimeout;
    (void) fixLen;
    sx127x_set_modem(&sx127x, modem);
    sx127x_set_bandwidth(&sx127x, bandwidth);
    sx127x_set_spreading_factor(&sx127x, spreading_factor);
    sx127x_set_coding_rate(&sx127x, coding_rate);
    sx127x_set_preamble_length(&sx127x, preambleLen);
    sx127x_set_fixed_header_len_mode(&sx127x, false);
    sx127x_set_payload_length(&sx127x, payloadLen);
    sx127x_set_crc(&sx127x, crcOn);
    sx127x_set_freq_hop(&sx127x, freqHopOn);
    sx127x_set_hop_period(&sx127x, hopPeriod);
    sx127x_set_iq_invert(&sx127x, iqInverted);
    sx127x_set_rx_timeout(&sx127x, LORAMAC_RX_WINDOW_DURATION);
    sx127x_set_rx_single(&sx127x, !rxContinuous);
    sx127x_set_rx(&sx127x);
}

void SX1276SetTxConfig(RadioModems_t modem, int8_t power, uint32_t fdev,
                       uint32_t bandwidth, uint32_t spreading_factor,
                       uint8_t coding_rate, uint16_t preambleLen,
                       bool fixLen, bool crcOn, bool freqHopOn,
                       uint8_t hopPeriod, bool iqInverted, uint32_t timeout)
{
    (void) fdev;
    (void) fixLen;
    sx127x_set_modem(&sx127x, modem);
    sx127x_set_freq_hop(&sx127x, freqHopOn);
    sx127x_set_bandwidth(&sx127x, bandwidth);
    sx127x_set_coding_rate(&sx127x, coding_rate);
    sx127x_set_spreading_factor(&sx127x, spreading_factor);
    sx127x_set_crc(&sx127x, crcOn);
    sx127x_set_freq_hop(&sx127x, freqHopOn);
    sx127x_set_hop_period(&sx127x, hopPeriod);
    sx127x_set_fixed_header_len_mode(&sx127x, false);
    sx127x_set_iq_invert(&sx127x, iqInverted);
    sx127x_set_payload_length(&sx127x, 0);
    sx127x_set_tx_power(&sx127x, power);
    sx127x_set_preamble_length(&sx127x, preambleLen);
    sx127x_set_rx_single(&sx127x, false);
    sx127x_set_tx_timeout(&sx127x, timeout * US_PER_MS); /* base unit us, LoRaMAC ms */
}

uint32_t SX1276GetTimeOnAir(RadioModems_t modem, uint8_t pktLen)
{
    (void) modem;
    return sx127x_get_time_on_air(&sx127x, pktLen);
}

void SX1276Send(uint8_t *buffer, uint8_t size)
{
    netdev_t *dev = (netdev_t *)&sx127x;
    struct iovec vec[1];
    vec[0].iov_base = buffer;
    vec[0].iov_len = size;
    dev->driver->send(dev, vec, 1);
}

void SX1276SetSleep(void)
{
    sx127x_set_sleep(&sx127x);
}

void SX1276SetStby(void)
{
    sx127x_set_standby(&sx127x);
}

void SX1276SetRx(uint32_t timeout)
{
    (void) timeout;
    sx127x_set_rx(&sx127x);
}

void SX1276StartCad(void)
{
    sx127x_start_cad(&sx127x);
}

int16_t SX1276ReadRssi(RadioModems_t modem)
{
    sx127x_set_modem(&sx127x, (uint8_t)modem);
    return sx127x_read_rssi(&sx127x);
}

void SX1276Write(uint8_t addr, uint8_t data)
{
    sx127x_reg_write(&sx127x, addr, data);
}

uint8_t SX1276Read(uint8_t addr)
{
    return sx127x_reg_read(&sx127x, addr);
}

void SX1276WriteBuffer(uint8_t addr, uint8_t *buffer, uint8_t size)
{
    sx127x_reg_write_burst(&sx127x, addr, buffer, size);
}

void SX1276ReadBuffer(uint8_t addr, uint8_t *buffer, uint8_t size)
{
    sx127x_reg_read_burst(&sx127x, addr, buffer, size);
}

void SX1276SetMaxPayloadLength(RadioModems_t modem, uint8_t max)
{
    (void) modem;
    sx127x_set_max_payload_len(&sx127x, max);
}

bool SX1276CheckRfFrequency(uint32_t frequency)
{
    (void) frequency;
    /* Implement check. Currently all frequencies are supported */
    return true;
}

void SX1276SetTxContinuousWave(uint32_t freq, int8_t power, uint16_t time)
{
    (void) freq;
    (void) power;
    (void) time;
    /* TODO */
}

void SX1276SetPublicNetwork(bool enable)
{
    if (enable) {
        /* Use public network syncword */
        sx127x_set_syncword(&sx127x, LORA_SYNCWORD_PUBLIC);
    }
    else {
        /* Use private network syncword */
        sx127x_set_syncword(&sx127x, LORA_SYNCWORD_PRIVATE);
    }
}

/**
 * LoRa function callbacks
 */
const struct Radio_s Radio =
{
    SX1276Init,
    SX1276GetStatus,
    SX1276SetModem,
    SX1276SetChannel,
    SX1276IsChannelFree,
    SX1276Random,
    SX1276SetRxConfig,
    SX1276SetTxConfig,
    SX1276CheckRfFrequency,
    SX1276GetTimeOnAir,
    SX1276Send,
    SX1276SetSleep,
    SX1276SetStby,
    SX1276SetRx,
    SX1276StartCad,
    SX1276SetTxContinuousWave,
    SX1276ReadRssi,
    SX1276Write,
    SX1276Read,
    SX1276WriteBuffer,
    SX1276ReadBuffer,
    SX1276SetMaxPayloadLength,
    SX1276SetPublicNetwork
};
