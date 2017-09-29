#include "loramac/board_definitions.h"
#include "sx127x.h"
#include "sx127x_internal.h"

/*!
 * \ Sx1276 driver for use of LoRaMac
 */

static sx127x_t *dev_ptr;
static RadioEvents_t *RadioEvents; 

/*
 * Radio driver functions implementation wrappers, the netdev2 object
 * is known within the scope of the function
 */

sx127x_t *radio_get_ptr(void)
{
    return dev_ptr;
}

void radio_set_ptr(sx127x_t *ptr)
{
    dev_ptr = ptr;
}

RadioEvents_t *radio_get_event_ptr(void)
{
    return RadioEvents;
}

void radio_set_event_ptr(RadioEvents_t *events)
{
    RadioEvents = events;
}

void SX1276Init(RadioEvents_t *events)
{
    RadioEvents = events;
    sx127x_init(dev_ptr);
}

RadioState_t SX1276GetStatus(void)
{
    return (RadioState_t)sx127x_get_state(dev_ptr);
}

void SX1276SetModem(RadioModems_t modem)
{
    sx127x_set_modem(dev_ptr, (uint8_t)modem);
}

void SX1276SetChannel(uint32_t freq)
{
    sx127x_set_channel(dev_ptr, freq);
}

bool SX1276IsChannelFree(RadioModems_t modem, uint32_t freq,
                         int16_t rssiThresh)
{
    return sx127x_is_channel_free(dev_ptr, freq, rssiThresh);
}

uint32_t SX1276Random(void)
{
     return sx127x_random(dev_ptr);
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
    sx127x_set_modem(dev_ptr, modem);
    sx127x_set_freq_hop(dev_ptr, freqHopOn);
    sx127x_set_bandwidth(dev_ptr, bandwidth);
    sx127x_set_coding_rate(dev_ptr, coding_rate);
    sx127x_set_spreading_factor(dev_ptr, spreading_factor);
    sx127x_set_crc(dev_ptr, crcOn);
    sx127x_set_freq_hop(dev_ptr, freqHopOn);
    sx127x_set_hop_period(dev_ptr, hopPeriod);
    sx127x_set_fixed_header_len_mode(dev_ptr, false);
    sx127x_set_iq_invert(dev_ptr, iqInverted);
    // settings.low_datarate_optimize = fixLen;
    // settings.payload_len = payloadLen;sx127x_set_tx_power(dev_ptr, power);
    sx127x_set_tx_power(dev_ptr, SX127X_RADIO_TX_POWER);
    sx127x_set_preamble_length(dev_ptr, preambleLen);
    sx127x_set_rx_single(dev_ptr, !rxContinuous);
    sx127x_set_rx(dev_ptr);
    sx127x_set_tx_timeout(dev_ptr, 3 * 1000 * 1000); /* base units us */
    sx127x_set_rx_timeout(dev_ptr, symbTimeout);
}

void SX1276SetTxConfig(RadioModems_t modem, int8_t power, uint32_t fdev,
                       uint32_t bandwidth, uint32_t spreading_factor,
                       uint8_t coding_rate, uint16_t preambleLen,
                       bool fixLen, bool crcOn, bool freqHopOn,
                       uint8_t hopPeriod, bool iqInverted, uint32_t timeout)
{
    (void) fdev;
    sx127x_set_modem(dev_ptr, modem);
    sx127x_set_freq_hop(dev_ptr, freqHopOn);
    sx127x_set_bandwidth(dev_ptr, bandwidth);
    sx127x_set_coding_rate(dev_ptr, coding_rate);
    sx127x_set_spreading_factor(dev_ptr, spreading_factor);
    sx127x_set_crc(dev_ptr, crcOn);
    sx127x_set_freq_hop(dev_ptr, freqHopOn);
    sx127x_set_hop_period(dev_ptr, hopPeriod);
    sx127x_set_fixed_header_len_mode(dev_ptr, false);
    sx127x_set_iq_invert(dev_ptr, iqInverted);
    // settings.low_datarate_optimize = fixLen;
    // settings.payload_len = 0;
    sx127x_set_tx_power(dev_ptr, power);
    sx127x_set_preamble_length(dev_ptr, preambleLen);
    sx127x_set_rx_single(dev_ptr, false); 
    sx127x_set_tx_timeout(dev_ptr, timeout * 1000); /* base unit us, LoRaMAC ms */
    sx127x_set_rx_timeout(dev_ptr, 10);
}

uint32_t SX1276GetTimeOnAir(RadioModems_t modem, uint8_t pktLen)
{
    return sx127x_get_time_on_air(dev_ptr, pktLen);
}

void SX1276Send(uint8_t *buffer, uint8_t size)
{
    sx127x_send(dev_ptr, buffer, size);
}

void SX1276SetSleep(void)
{
    sx127x_set_sleep(dev_ptr);
}

void SX1276SetStby(void)
{
    sx127x_set_standby(dev_ptr);
}

void SX1276SetRx(uint32_t timeout)
{
    sx127x_set_rx_timeout(dev_ptr, timeout * 1000);
    sx127x_set_rx(dev_ptr);
}


void SX1276StartCad(void)
{
    sx127x_start_cad(dev_ptr);
}

int16_t SX1276ReadRssi(RadioModems_t modem)
{
    sx127x_set_modem(dev_ptr, (uint8_t)modem);
    return sx127x_read_rssi(dev_ptr);
}

void SX1276Write(uint8_t addr, uint8_t data)
{
    sx127x_reg_write(dev_ptr, addr, data);
}

uint8_t SX1276Read(uint8_t addr)
{
    return sx127x_reg_read(dev_ptr, addr);
}

void SX1276WriteBuffer(uint8_t addr, uint8_t *buffer, uint8_t size)
{
    sx127x_reg_write_burst(dev_ptr, addr, buffer, size);
}

void SX1276ReadBuffer(uint8_t addr, uint8_t *buffer, uint8_t size)
{
    sx127x_reg_read_burst(dev_ptr, addr, buffer, size);
}

void SX1276SetMaxPayloadLength(RadioModems_t modem, uint8_t max)
{
    sx127x_set_max_payload_len(dev_ptr, max);
}

bool SX1276CheckRfFrequency(uint32_t frequency)
{
    // Implement check. Currently all frequencies are supported
    return true;
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
    SX1276ReadRssi,
    SX1276Write,
    SX1276Read,
    SX1276WriteBuffer,
    SX1276ReadBuffer,
    SX1276SetMaxPayloadLength
};
