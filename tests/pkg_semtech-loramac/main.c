/*
 * Copyright (C) Inria Chile 2016
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Test for raw IPv6 connections
 *
 * @author      Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * This test application tests the gnrc_conn_ip module. If you select protocol 58 you can also
 * test if gnrc is able to deal with multiple subscribers to ICMPv6 (gnrc_icmpv6 and this
 * application).
 *
 * @}
 */

#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "shell_commands.h"
#include "thread.h"
#include "xtimer.h"
// #include "lpm.h"
#include "periph/rtc.h"

#include "common.h"
#include "board.h"

#include "sx127x_registers.h"

#include "LoRaMac.h"
#include "loramac/board_definitions.h" 
#include "Comissioning.h"

/*!
 * Thread Variables and packet count
 */

static sx1276_t sx1276;

LoRaMacPrimitives_t LoRaMacPrimitives;
LoRaMacCallback_t LoRaMacCallbacks;
MibRequestConfirm_t mibReq;

uint32_t count = 0;

/*!
 * Unique Devices IDs register set ( STM32L1xxx )
 */
#define         ID1                                 ( 0x1FF800D0 )
#define         ID2                                 ( 0x1FF800D4 )
#define         ID3                                 ( 0x1FF800E4 )

/*!
 * Defines the application data transmission duty cycle. 5s, value in [ms].
 */
#define APP_TX_DUTYCYCLE                            10000

/*!
 * Defines a random delay for application data transmission duty cycle. 1s,
 * value in [ms].
 */
#define APP_TX_DUTYCYCLE_RND                        1000

/*!
 * Default datarate
 */
#define LORAWAN_DEFAULT_DATARATE                    DR_1

/*!
 * LoRaWAN confirmed messages
 */
#define LORAWAN_CONFIRMED_MSG_ON                    false

/*!
 * LoRaWAN Adaptive Data Rate
 *
 * \remark Please note that when ADR is enabled the end-device should be static
 */
#define LORAWAN_ADR_ON                              0

#if defined( USE_BAND_868 )

#include "LoRaMacTest.h"

/*!
 * LoRaWAN ETSI duty cycle control enable/disable
 *
 * \remark Please note that ETSI mandates duty cycled transmissions. Use only for test purposes
 */
#define LORAWAN_DUTYCYCLE_ON                        true

#define USE_SEMTECH_DEFAULT_CHANNEL_LINEUP          1

#if( USE_SEMTECH_DEFAULT_CHANNEL_LINEUP == 1 ) 

#define LC4                { 867100000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC5                { 867300000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC6                { 867500000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC7                { 867700000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC8                { 867900000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC9                { 868800000, { ( ( DR_7 << 4 ) | DR_7 ) }, 2 }
#define LC10               { 868300000, { ( ( DR_6 << 4 ) | DR_6 ) }, 1 }

#endif

#endif

/*!
 * LoRaWAN application port
 */
#define LORAWAN_APP_PORT                            1

/*!
 * User application data buffer size
 */
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )

#define LORAWAN_APP_DATA_SIZE                       16

#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )

#define LORAWAN_APP_DATA_SIZE                       53

#endif

static uint8_t DevEui[] = LORAWAN_DEVICE_EUI;
static uint8_t AppEui[] = LORAWAN_APPLICATION_EUI;
static uint8_t AppKey[] = LORAWAN_APPLICATION_KEY;

#if( OVER_THE_AIR_ACTIVATION == 0 )

static uint8_t NwkSKey[] = LORAWAN_NWKSKEY;
static uint8_t AppSKey[] = LORAWAN_APPSKEY;

/*!
 * Device address
 */
static uint32_t DevAddr = LORAWAN_DEVICE_ADDRESS;

#endif

/*!
 * Application port
 */
static uint8_t AppPort = LORAWAN_APP_PORT;

/*!
 * User application data size
 */
static uint8_t AppDataSize = LORAWAN_APP_DATA_SIZE;

/*!
 * User application data buffer size
 */
#define LORAWAN_APP_DATA_MAX_SIZE                           64

/*!
 * User application data
 */
static uint8_t AppData[LORAWAN_APP_DATA_MAX_SIZE];

/*!
 * Indicates if the node is sending confirmed or unconfirmed messages
 */
static uint8_t IsTxConfirmed = LORAWAN_CONFIRMED_MSG_ON;

/*!
 * Defines the application data transmission duty cycle
 */
static uint32_t TxDutyCycleTime;

/*!
 * Timer to handle the application data transmission duty cycle
 */
static TimerEvent_t TxNextPacketTimer;

/*!
 * Specifies the state of the application LED
 */
static bool AppLedStateOn = false;

/*!
 * Timer to handle the state of LED1
 */
static TimerEvent_t Led1Timer;

/*!
 * Timer to handle the state of LED2
 */
static TimerEvent_t Led2Timer;

/*!
 * Indicates if a new packet can be sent
 */
static bool NextTx = true;

/*!
 * Device states
 */
static enum eDevicState
{
    DEVICE_STATE_INIT,
    DEVICE_STATE_JOIN,
    DEVICE_STATE_SEND,
    DEVICE_STATE_CYCLE,
    DEVICE_STATE_SLEEP
}DeviceState;

/*!
 * LoRaWAN compliance tests support data
 */
struct ComplianceTest_s
{
    bool Running;
    uint8_t State;
    bool IsTxConfirmed;
    uint8_t AppPort;
    uint8_t AppDataSize;
    uint8_t *AppDataBuffer;
    uint16_t DownLinkCounter;
    bool LinkCheck;
    uint8_t DemodMargin;
    uint8_t NbGateways;
}ComplianceTest;

/*!
*   Thread for MAC iteration Handling
*/
void *MAC_thread_handler(void *arg);


uint32_t BoardGetRandomSeed( void )
{
    return ( ( *( uint32_t* )ID1 ) ^ ( *( uint32_t* )ID2 ) ^ ( *( uint32_t* )ID3 ) );
}

void BoardGetUniqueId( uint8_t *id )
{
    printf("ID1 %lu ", (unsigned long) ( *( uint32_t* )ID1 ));
    printf("ID2 %lu ", (unsigned long) ( *( uint32_t* )ID2 ));
    printf("ID3 %lu \n", (unsigned long) ( *( uint32_t* )ID3 ));


    id[7] = ( ( *( uint32_t* )ID1 )+ ( *( uint32_t* )ID3 ) ) >> 24;
    id[6] = ( ( *( uint32_t* )ID1 )+ ( *( uint32_t* )ID3 ) ) >> 16;
    id[5] = ( ( *( uint32_t* )ID1 )+ ( *( uint32_t* )ID3 ) ) >> 8;
    id[4] = ( ( *( uint32_t* )ID1 )+ ( *( uint32_t* )ID3 ) );
    id[3] = ( ( *( uint32_t* )ID2 ) ) >> 24;
    id[2] = ( ( *( uint32_t* )ID2 ) ) >> 16;
    id[1] = ( ( *( uint32_t* )ID2 ) ) >> 8;
    id[0] = ( ( *( uint32_t* )ID2 ) );
}

uint8_t BoardGetBatteryLevel ( void ){
    return 50;
}

/*!
 * \brief   Prepares the payload of the frame
 */
static void PrepareTxFrame( uint8_t port )
{
    switch( port )
    {
    case 1:
        {
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
            AppData[0] = 'T';
            AppData[1] = 'E';
            AppData[2] = 'S';
            AppData[3] = 'T';
#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
            AppData[0] = '\\';
            AppData[1] = '!';            
            AppData[2] = '#';
            AppData[3] = '3';
            AppData[4] = '#';
            AppData[5] = 'T';
            AppData[6] = '/';
            AppData[7] = '2';
            AppData[8] = '2';
            AppData[9] = '.';
            AppData[10] = '0';
            AppData[11] = '0';
            AppData[12] = '0';
            AppData[13] = '0';
            AppData[14] = '0';
            AppData[15] = '0';
#endif
        }

    case 2:
        {
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
            AppData[0] = 'T';
            AppData[1] = 'E';
            AppData[2] = 'S';
            AppData[3] = 'T';
#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
            puts("Appsend");
            AppData[0] = '\\';
            AppData[1] = '!';            
            AppData[2] = '#';
            AppData[3] = '3';
            AppData[4] = '#';
            AppData[5] = 'T';
            AppData[6] = '/';
            AppData[7] = '2';
            AppData[8] = '2';
            AppData[9] = '.';
            AppData[10] = '0';
            AppData[11] = '0';
            AppData[12] = '0';
            AppData[13] = '0';
            AppData[14] = '0';
            AppData[15] = '0';
#endif
        }
        break;
    case 224:
        if( ComplianceTest.LinkCheck == true )
        {
            ComplianceTest.LinkCheck = false;
            AppDataSize = 3;
            AppData[0] = 5;
            AppData[1] = ComplianceTest.DemodMargin;
            AppData[2] = ComplianceTest.NbGateways;
            ComplianceTest.State = 1;
        }
        else
        {
            switch( ComplianceTest.State )
            {
            case 4:
                ComplianceTest.State = 1;
                break;
            case 1:
                AppDataSize = 2;
                AppData[0] = ComplianceTest.DownLinkCounter >> 8;
                AppData[1] = ComplianceTest.DownLinkCounter;
                break;
            }
        }
        break;
    default:
        break;
    }
}

/*!
 * \brief   Prepares the payload of the frame
 *
 * \retval  [0: frame could be send, 1: error]
 */
static bool SendFrame( void )
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;
    
    if( LoRaMacQueryTxPossible( AppDataSize, &txInfo ) != LORAMAC_STATUS_OK )
    {
        // Send empty frame in order to flush MAC commands
        mcpsReq.Type = MCPS_UNCONFIRMED;
        mcpsReq.Req.Unconfirmed.fBuffer = NULL;
        mcpsReq.Req.Unconfirmed.fBufferSize = 0;
        mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
    }
    else
    {
        if( IsTxConfirmed == false )
        {
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = AppPort;
            mcpsReq.Req.Unconfirmed.fBuffer = AppData;
            mcpsReq.Req.Unconfirmed.fBufferSize = AppDataSize;
            mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
        else
        {
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = AppPort;
            mcpsReq.Req.Confirmed.fBuffer = AppData;
            mcpsReq.Req.Confirmed.fBufferSize = AppDataSize;
            mcpsReq.Req.Confirmed.NbTrials = 8;
            mcpsReq.Req.Confirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
    }
    if( LoRaMacMcpsRequest( &mcpsReq ) == LORAMAC_STATUS_OK )
    {
        return false;
    }
    return true;
}

/*!
 * \brief Function executed on TxNextPacket Timeout event
 */
static void OnTxNextPacketTimerEvent( void )
{
    puts("Next Event");
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;

    TimerStop( &TxNextPacketTimer );

    mibReq.Type = MIB_NETWORK_JOINED;
    status = LoRaMacMibGetRequestConfirm( &mibReq );

    if( status == LORAMAC_STATUS_OK )
    {
        if( mibReq.Param.IsNetworkJoined == true )
        {
            DeviceState = DEVICE_STATE_SEND;
            NextTx = true;
        }
        else
        {
            DeviceState = DEVICE_STATE_JOIN;
        }
    }
}

/*!
 * \brief Function executed on Led 1 Timeout event
 */
static void OnLed1TimerEvent( void )
{
    TimerStop( &Led1Timer );
    LED0_TOGGLE;
    // Switch LED 1 OFF
}

/*!
 * \brief Function executed on Led 2 Timeout event
 */
static void OnLed2TimerEvent( void )
{
    TimerStop( &Led2Timer );
    // Switch LED 2 OFF
}

/*!
 * \brief   MCPS-Confirm event function
 *
 * \param   [IN] mcpsConfirm - Pointer to the confirm structure,
 *               containing confirm attributes.
 */
static void McpsConfirm( McpsConfirm_t *mcpsConfirm )
{
    if( mcpsConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK )
    {
        switch( mcpsConfirm->McpsRequest )
        {
            case MCPS_UNCONFIRMED:
            {
                // Check Datarate
                // Check TxPower
                break;
            }
            case MCPS_CONFIRMED:
            {
                // Check Datarate
                // Check TxPower
                // Check AckReceived
                // Check NbTrials
                break;
            }
            case MCPS_PROPRIETARY:
            {
                break;
            }
            default:
                break;
        }

        // Switch LED 1 ON
        TimerStart( &Led1Timer );
    }
    NextTx = true;
}

/*!
 * \brief   MCPS-Indication event function
 *
 * \param   [IN] mcpsIndication - Pointer to the indication structure,
 *               containing indication attributes.
 */
static void McpsIndication( McpsIndication_t *mcpsIndication )
{
    if( mcpsIndication->Status != LORAMAC_EVENT_INFO_STATUS_OK )
    {
        return;
    }

    switch( mcpsIndication->McpsIndication )
    {
        case MCPS_UNCONFIRMED:
        {
            break;
        }
        case MCPS_CONFIRMED:
        {
            break;
        }
        case MCPS_PROPRIETARY:
        {
            break;
        }
        case MCPS_MULTICAST:
        {
            break;
        }
        default:
            break;
    }

    // Check Multicast
    // Check Port
    // Check Datarate
    // Check FramePending
    // Check Buffer
    // Check BufferSize
    // Check Rssi
    // Check Snr
    // Check RxSlot

    if( ComplianceTest.Running == true )
    {
        ComplianceTest.DownLinkCounter++;
    }

    if( mcpsIndication->RxData == true )
    {
        switch( mcpsIndication->Port )
        {
        case 1:
            if( mcpsIndication->BufferSize == 1 )
            {
                AppLedStateOn = mcpsIndication->Buffer[0] & 0x01;
                gpio_write( LED0_PIN, ( ( AppLedStateOn & 0x01 ) != 0 ) ? 0 : 1 );
            }
            break;
        case 2:
            if( mcpsIndication->BufferSize == 1 )
            {
                AppLedStateOn = mcpsIndication->Buffer[0] & 0x01;
                gpio_write( LED0_PIN, ( ( AppLedStateOn & 0x01 ) != 0 ) ? 0 : 1 );
            }
            break;
        case 224:
            if( ComplianceTest.Running == false )
            {
                // Check compliance test enable command (i)
                if( ( mcpsIndication->BufferSize == 4 ) &&
                    ( mcpsIndication->Buffer[0] == 0x01 ) &&
                    ( mcpsIndication->Buffer[1] == 0x01 ) &&
                    ( mcpsIndication->Buffer[2] == 0x01 ) &&
                    ( mcpsIndication->Buffer[3] == 0x01 ) )
                {
                    IsTxConfirmed = false;
                    AppPort = 224;
                    AppDataSize = 2;
                    ComplianceTest.DownLinkCounter = 0;
                    ComplianceTest.LinkCheck = false;
                    ComplianceTest.DemodMargin = 0;
                    ComplianceTest.NbGateways = 0;
                    ComplianceTest.Running = true;
                    ComplianceTest.State = 1;
                    
                    MibRequestConfirm_t mibReq;
                    mibReq.Type = MIB_ADR;
                    mibReq.Param.AdrEnable = true;
                    LoRaMacMibSetRequestConfirm( &mibReq );

#if defined( USE_BAND_868 )
                    LoRaMacTestSetDutyCycleOn( false );
#endif
                }
            }
            else
            {
                ComplianceTest.State = mcpsIndication->Buffer[0];
                switch( ComplianceTest.State )
                {
                case 0: // Check compliance test disable command (ii)
                    IsTxConfirmed = LORAWAN_CONFIRMED_MSG_ON;
                    AppPort = LORAWAN_APP_PORT;
                    AppDataSize = LORAWAN_APP_DATA_SIZE;
                    ComplianceTest.DownLinkCounter = 0;
                    ComplianceTest.Running = false;
                    
                    MibRequestConfirm_t mibReq;
                    mibReq.Type = MIB_ADR;
                    mibReq.Param.AdrEnable = LORAWAN_ADR_ON;
                    LoRaMacMibSetRequestConfirm( &mibReq );
#if defined( USE_BAND_868 )
                    LoRaMacTestSetDutyCycleOn( LORAWAN_DUTYCYCLE_ON );
#endif
                    break;
                case 1: // (iii, iv)
                    AppDataSize = 2;
                    break;
                case 2: // Enable confirmed messages (v)
                    IsTxConfirmed = true;
                    ComplianceTest.State = 1;
                    break;
                case 3:  // Disable confirmed messages (vi)
                    IsTxConfirmed = false;
                    ComplianceTest.State = 1;
                    break;
                case 4: // (vii)
                    AppDataSize = mcpsIndication->BufferSize;

                    AppData[0] = 4;
                    for( uint8_t i = 1; i < AppDataSize; i++ )
                    {
                        AppData[i] = mcpsIndication->Buffer[i] + 1;
                    }
                    break;
                case 5: // (viii)
                    {
                        MlmeReq_t mlmeReq;
                        mlmeReq.Type = MLME_LINK_CHECK;
                        LoRaMacMlmeRequest( &mlmeReq );
                    }
                    break;
                case 6: // (ix)
                    {
                        MlmeReq_t mlmeReq;

                        mlmeReq.Type = MLME_JOIN;

                        mlmeReq.Req.Join.DevEui = DevEui;
                        mlmeReq.Req.Join.AppEui = AppEui;
                        mlmeReq.Req.Join.AppKey = AppKey;

                        LoRaMacMlmeRequest( &mlmeReq );
                        DeviceState = DEVICE_STATE_SLEEP;
                    }
                    break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

    // Switch LED 2 ON for each received downlink
    TimerStart( &Led2Timer );
}

/*!
 * \brief   MLME-Confirm event function
 *
 * \param   [IN] mlmeConfirm - Pointer to the confirm structure,
 *               containing confirm attributes.
 */
static void MlmeConfirm( MlmeConfirm_t *mlmeConfirm )
{
    if( mlmeConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK )
    {
        switch( mlmeConfirm->MlmeRequest )
        {
            case MLME_JOIN:
            {
                // Status is OK, node has joined the network
                DeviceState = DEVICE_STATE_SEND;
                NextTx = true;
                break;
            }
            case MLME_LINK_CHECK:
            {
                // Check DemodMargin
                // Check NbGateways
                if( ComplianceTest.Running == true )
                {
                    ComplianceTest.LinkCheck = true;
                    ComplianceTest.DemodMargin = mlmeConfirm->DemodMargin;
                    ComplianceTest.NbGateways = mlmeConfirm->NbGateways;
                }
                break;
            }
            default:
                break;
        }
    }
    NextTx = true;
}

void init_configs(void)
{
    sx1276_lora_settings_t settings;

    settings.bandwidth = SX1276_BW_125_KHZ;
    settings.coderate = SX1276_CR_4_5;
    settings.datarate = SX1276_SF10;
    settings.crc_on = true;
    settings.freq_hop_on = false;
    settings.hop_period = 0;
    settings.implicit_header = false;
    settings.iq_inverted = false;
    settings.low_datarate_optimize = false;
    settings.payload_len = 0;
    settings.power = 14;
    settings.preamble_len = LORA_PREAMBLE_LENGTH;
    settings.rx_continuous = true;
    settings.tx_timeout = 1000 * 1000 * 30; // 30 sec
    settings.rx_timeout = LORA_SYMBOL_TIMEOUT;

    sx1276_configure_lora(&sx1276, &settings);

    sx1276_set_channel(&sx1276, RF_FREQUENCY);
}

void event_handler_thread(void *arg, sx1276_event_type_t event_type)
{
    sx1276_rx_packet_t *packet = (sx1276_rx_packet_t *) &sx1276._internal.last_packet;
    RadioEvents_t *events = radio_get_event_ptr();
    switch (event_type) {

        case SX1276_TX_DONE:
            //puts("sx1276: TX done");
            printf("TX done, COUNT : %lu \r\n",count);
            count++;
            events->TxDone();
            break;

        case SX1276_TX_TIMEOUT:
            puts("sx1276: TX timeout");
            events->TxTimeout();
            break;

        case SX1276_RX_DONE:
            puts("sx1276: RX Done");
            events->RxDone(packet->content, packet->size, packet->rssi_value, packet-> snr_value);
            break;

        case SX1276_RX_TIMEOUT:
            puts("sx1276: RX timeout");
            events->RxTimeout();
            break;

        case SX1276_RX_ERROR_CRC:
            puts("sx1276: RX CRC_ERROR");
            events->RxError();
            break;

        case SX1276_FHSS_CHANGE_CHANNEL:
            events->FhssChangeChannel(sx1276._internal.last_channel);
            break;

        case SX1276_CAD_DONE:
            events->CadDone(sx1276._internal.is_last_cad_success);
            break;

        default:
            break;
    }
}

void init_radio(void)
{
    sx1276_lora_settings_t settings_lora;
    sx1276_settings_t settings;

    sx1276.nss_pin = SX1276_SPI_NSS;
    sx1276.spi = SX1276_SPI;

    sx1276.dio0_pin = SX1276_DIO0;
    sx1276.dio1_pin = SX1276_DIO1;
    sx1276.dio2_pin = SX1276_DIO2;
    sx1276.dio3_pin = SX1276_DIO3;

    sx1276.dio4_pin = (gpio_t) NULL;
    sx1276.dio5_pin = (gpio_t) NULL;
    sx1276.reset_pin = (gpio_t) SX1276_RESET;

    //settings.channel = RF_FREQUENCY;
    settings.modem = SX1276_MODEM_LORA;
    settings.state = SX1276_RF_IDLE;
    settings.lora = settings_lora;

    sx1276.settings = settings;
    sx1276.sx1276_event_cb = event_handler_thread;

    /* Launch initialization of driver and device */
    puts("init_radio: initializing driver...");

    puts("init_radio: sx1276 initialization done");
}

int lora_setup(int argc, char **argv) 
{
    if (argc < 4) {
        return -1;
    }

    int bw = atoi(argv[1]);
    int sf = atoi(argv[2]);
    int cr = atoi(argv[3]);

    sx1276_lora_bandwidth_t lora_bw;

    switch (bw) {
    case 125:
        lora_bw = SX1276_BW_125_KHZ;
        break;

    case 250:
        lora_bw = SX1276_BW_250_KHZ;
        break;

    case 500:
        lora_bw = SX1276_BW_500_KHZ;
        break;

    default:
        puts("lora_setup invalid bandwidth value passed");
        return -1;
    }

    sx1276_lora_spreading_factor_t lora_sf;
    if (sf < 7 || sf > 12) {
        puts("lora_setup: invalid spreading factor value passed");
        return -1;
    }

    lora_sf = (sx1276_lora_spreading_factor_t) sf;

    sx1276_lora_coding_rate_t lora_cr;
    if (cr < 5 || cr > 8) {
        puts("lora_setup: invalid coding rate value passed");
        return -1;
    }

    lora_cr = (sx1276_lora_coding_rate_t) (cr - 5);

    sx1276_configure_lora_bw(&sx1276, lora_bw);
    sx1276_configure_lora_sf(&sx1276, lora_sf);
    sx1276_configure_lora_cr(&sx1276, lora_cr);

    puts("lora_setup: configuration is set");

    return 0;
}

int random(int argc, char **argv)
{
    printf("random: number from sx1276: %u\n", (unsigned int) sx1276_random(&sx1276));
    init_configs();

    return 0;
}

int regs_set(int argc, char **argv)
{
    if (argc <= 2) {
        puts("usage: set <num> <value>");
        return -1;
    }

    long num, val;

    // Register number in hex
    if (strstr(argv[1], "0x") != NULL) {
        num = strtol(argv[1], NULL, 16);
    }
    else {
        num = atoi(argv[1]);
    }

    // Register value in hex
    if (strstr(argv[2], "0x") != NULL) {
        val = strtol(argv[2], NULL, 16);
    }
    else {
        val = atoi(argv[2]);
    }

    sx1276_reg_write(&sx1276, (uint8_t) num, (uint8_t) val);

    return 0;

}

int regs(int argc, char **argv)
{
    if (argc <= 1) {
        puts("usage: get <all | allinline | regnum>");
        return -1;
    }

    if (strcmp(argv[1], "all") == 0) {
        puts("- listing all registers -");
        uint16_t i = 0;
        uint8_t reg = 0, data = 0;
        uint8_t j = 0;

        /* Listing registers map*/
        puts("Reg   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
        for (i = 0; i <= 7; i++) {
            printf("0x%02X ", i << 4);

            for (j = 0; j <= 15; j++, reg++) {
                data = sx1276_reg_read(&sx1276, reg);

                printf("%02X ", data);
            }

            puts("");
        }

        puts("-done-");

        return 0;
    }
    if (strcmp(argv[1], "allinline") == 0) {
        puts("- listing all registers in one line -");
        uint16_t reg;
        uint8_t data = 0;

        /* Listing registers map*/
        for (reg = 0; reg < 256; reg++) {
            data = sx1276_reg_read(&sx1276, (uint8_t) reg);

            printf("%02X ", data);
        }

        puts("-done-");

        return 0;
    }
    else {
        long int num = 0;

        /* Register number in hex */
        if (strstr(argv[1], "0x") != NULL) {
            num = strtol(argv[1], NULL, 16);
        }
        else {
            num = atoi(argv[1]);
        }

        if (num >= 0 && num <= 255) {
            printf("[regs] 0x%02X = 0x%02X\n", (uint8_t) num, sx1276_reg_read(&sx1276, (uint8_t) num));
        }
        else {
            puts("regs: invalid register number specified");
            return -1;
        }
    }

    return 0;
}

int tx_test(int argc, char **argv)
{
    if (argc <= 1) {
        puts("tx_test: payload is not specified");
        return -1;
    }

    printf("tx_test: sending \"%s\" payload (%d bytes)\n", argv[1], strlen(argv[1]) + 1);

    sx1276_send(&sx1276, (uint8_t *) argv[1], strlen(argv[1]) + 1);

    xtimer_usleep(10000); /* wait for the chip */

    puts("tx_test: sended");

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "random", "Get random number from sx1276", random },
    { "get", "<all | num> - gets value of registers of sx1276, all or by specified number from 0 to 255", regs },
    { "set", "<num> <value> - sets value of register with specified number", regs_set },
    { "tx_test", "<payload> Send test payload string", tx_test },
    { "lora_setup", "<BW (125, 250, 512)> <SF (7..12)> <CR 4/(5,6,7,8)> - sets up LoRa modulation settings", lora_setup},

    { NULL, NULL, NULL }
};

int main(void)
{

    radio_set_ptr(&sx1276);
    xtimer_init();
    init_radio();

    DeviceState = DEVICE_STATE_INIT;

    #ifdef NZ32_SC151
    BoardGetUniqueId( DevEui );

    printf("BoardID: ");

    for(uint8_t i = 0; i < 8; i++)
    {
        printf("%02x ",*(DevEui + i));
    }

    printf("\n");
    #endif

        /* start the shell */
    // puts("Initialization successful - starting the shell now");
    // char line_buf[SHELL_DEFAULT_BUFSIZE];
    // shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    // puts("Start Loop");

while(1)
    {
        switch( DeviceState )
        {
            case DEVICE_STATE_INIT:
            {
                LoRaMacPrimitives.MacMcpsConfirm = McpsConfirm;
                LoRaMacPrimitives.MacMcpsIndication = McpsIndication;
                LoRaMacPrimitives.MacMlmeConfirm = MlmeConfirm;
                LoRaMacCallbacks.GetBatteryLevel = BoardGetBatteryLevel;
                LoRaMacInitialization( &LoRaMacPrimitives, &LoRaMacCallbacks );

                TimerInit( &TxNextPacketTimer, OnTxNextPacketTimerEvent );

                TimerInit( &Led1Timer, OnLed1TimerEvent );
                TimerSetValue( &Led1Timer, 25 );

                TimerInit( &Led2Timer, OnLed2TimerEvent );
                TimerSetValue( &Led2Timer, 25 );

                mibReq.Type = MIB_ADR;
                mibReq.Param.AdrEnable = LORAWAN_ADR_ON;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_PUBLIC_NETWORK;
                mibReq.Param.EnablePublicNetwork = LORAWAN_PUBLIC_NETWORK;
                LoRaMacMibSetRequestConfirm( &mibReq );

#if defined( USE_BAND_868 )
                LoRaMacTestSetDutyCycleOn( LORAWAN_DUTYCYCLE_ON );

#if( USE_SEMTECH_DEFAULT_CHANNEL_LINEUP == 1 ) 
               LoRaMacChannelAdd( 3, ( ChannelParams_t )LC4 );
               LoRaMacChannelAdd( 4, ( ChannelParams_t )LC5 );
               LoRaMacChannelAdd( 5, ( ChannelParams_t )LC6 );
               LoRaMacChannelAdd( 6, ( ChannelParams_t )LC7 );
               LoRaMacChannelAdd( 7, ( ChannelParams_t )LC8 );
               LoRaMacChannelAdd( 8, ( ChannelParams_t )LC9 );
               LoRaMacChannelAdd( 9, ( ChannelParams_t )LC10 );
#endif

#endif
                DeviceState = DEVICE_STATE_JOIN;
                puts("INIT");
                break;
            }
            case DEVICE_STATE_JOIN:
            {
#if( OVER_THE_AIR_ACTIVATION != 0 )
                puts("OA");
                MlmeReq_t mlmeReq;
                // Initialize LoRaMac device unique ID

                #ifdef NZ32_SC151
                BoardGetUniqueId( DevEui );
                #endif

                mlmeReq.Type = MLME_JOIN;

                mlmeReq.Req.Join.DevEui = DevEui;
                mlmeReq.Req.Join.AppEui = AppEui;
                mlmeReq.Req.Join.AppKey = AppKey;

                if( NextTx == true )
                {
                    LoRaMacMlmeRequest( &mlmeReq );
                }
                DeviceState = DEVICE_STATE_SLEEP;

#else
                puts("ACP");
                // Choose a random device address if not already defined in Comissioning.h
                if( DevAddr == 0 )
                {
                    // Random seed initialization
                    srand1( BoardGetRandomSeed( ) );
                    //srand1( 4);
                    // Choose a random device address
                    DevAddr = randr( 0, 0x01FFFFFF );
                }

                mibReq.Type = MIB_NET_ID;
                mibReq.Param.NetID = LORAWAN_NETWORK_ID;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_DEV_ADDR;
                mibReq.Param.DevAddr = DevAddr;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_NWK_SKEY;
                mibReq.Param.NwkSKey = NwkSKey;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_APP_SKEY;
                mibReq.Param.AppSKey = AppSKey;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_NETWORK_JOINED;
                mibReq.Param.IsNetworkJoined = true;
                LoRaMacMibSetRequestConfirm( &mibReq );

                DeviceState = DEVICE_STATE_SEND;

                if(mibReq.Param.IsNetworkJoined)
                    puts("JOINED");
#endif
                break;
            }
            case DEVICE_STATE_SEND:
            {
                if( NextTx == true )
                {
                    PrepareTxFrame( AppPort );
                    NextTx = SendFrame( );
                }
                if( ComplianceTest.Running == true )
                {
                    // Schedule next packet transmission
                    TxDutyCycleTime = APP_TX_DUTYCYCLE; // 5000 ms
                }
                else
                {
                    // Schedule next packet transmission
                    TxDutyCycleTime = APP_TX_DUTYCYCLE + randr( -APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND );
                }
                DeviceState = DEVICE_STATE_CYCLE;
                break;
            }
            case DEVICE_STATE_CYCLE:
            {
                DeviceState = DEVICE_STATE_SLEEP;

                // Schedule next packet transmission
                TimerSetValue( &TxNextPacketTimer, TxDutyCycleTime );
                TimerStart( &TxNextPacketTimer );
                break;
            }
            case DEVICE_STATE_SLEEP:
            {
                // Wake up through events
                TimerLowPowerHandler( );
                break;
            }
            default:
            {
                DeviceState = DEVICE_STATE_INIT;
                break;
            }
        }
    }

    return 0;
}


