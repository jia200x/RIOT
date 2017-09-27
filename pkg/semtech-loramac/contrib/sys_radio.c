#include "loramac/board_definitions.h"
#include "sx127x.h"

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
    return (RadioState_t)sx127x_get_status(dev_ptr);
}

void SX1276SetModem(RadioModems_t modem)
{
    sx127x_set_modem(dev_ptr, (sx1276_radio_modems_t) modem);
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

uint32_t SX1276Random( void )
{
     return sx127x_random(dev_ptr);
}

void SX1276SetRxConfig( RadioModems_t modem, uint32_t bandwidth,
                         uint32_t datarate, uint8_t coderate,
                         uint32_t bandwidthAfc, uint16_t preambleLen,
                         uint16_t symbTimeout, bool fixLen,
                         uint8_t payloadLen,
                         bool crcOn, bool freqHopOn, uint8_t hopPeriod,
                         bool iqInverted, bool rxContinuous )
{
    sx127x_lora_settings_t settings;
    dev_ptr->settings.modem = modem;
    (void) bandwidthAfc;
    settings.bandwidth = bandwidth + 7;
    settings.coderate = coderate;
    settings.datarate = datarate;
    settings.crc_on = crcOn;
    settings.freq_hop_on = freqHopOn;
    settings.hop_period = hopPeriod;
    settings.implicit_header = false;
    settings.iq_inverted = iqInverted;
    settings.low_datarate_optimize = fixLen;
    settings.payload_len = payloadLen;
    settings.power = 14;
    settings.preamble_len = preambleLen;
    settings.rx_continuous = rxContinuous;
    settings.tx_timeout = 3 * 1000 * 1000; // base units us
    settings.rx_timeout = symbTimeout;
    sx1276_configure_lora(dev_ptr, &settings);
}

void SX1276SetTxConfig(RadioModems_t modem, int8_t power, uint32_t fdev,
                       uint32_t bandwidth, uint32_t datarate,
                       uint8_t coderate, uint16_t preambleLen,
                       bool fixLen, bool crcOn, bool freqHopOn,
                       uint8_t hopPeriod, bool iqInverted, uint32_t timeout)
{
    sx127x_lora_settings_t settings;
    (void) fdev;
    dev_ptr->settings.modem = modem;
    settings.bandwidth = bandwidth + 7;
    settings.coderate = coderate;
    settings.datarate = datarate;
    settings.crc_on = crcOn;
    settings.freq_hop_on = freqHopOn;
    settings.hop_period = hopPeriod;
    settings.implicit_header = false;
    settings.iq_inverted = iqInverted;
    settings.low_datarate_optimize = fixLen;
    settings.payload_len = 0;
    settings.power = power;
    settings.preamble_len = preambleLen;
    settings.rx_continuous = true;
    settings.tx_timeout = timeout * 1000; // base unit us, LoRaMAC ms
    settings.rx_timeout = 10;
    sx127x_configure_lora(dev_ptr, &settings);
}

uint32_t SX1276GetTimeOnAir(RadioModems_t modem, uint8_t pktLen)
{
    return sx1276_get_time_on_air(dev_ptr, (sx1276_radio_modems_t) modem,
                                  pktLen);
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
    sx127x_set_rx(dev_ptr, timeout * 1000); // base unit us, LoRaMAC ms
}


void SX1276StartCad(void)
{
    sx127x_start_cad(dev_ptr);
}

int16_t SX1276ReadRssi(RadioModems_t modem)
{
    dev_ptr->settings.modem = modem;
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
    sx127x_set_max_payload_len(dev_ptr, (sx1276_radio_modems_t) modem, max);
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
