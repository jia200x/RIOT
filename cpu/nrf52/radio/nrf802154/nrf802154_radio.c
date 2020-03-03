#include <string.h>
#include <errno.h>

#include "cpu.h"
#include "luid.h"
#include "mutex.h"

#include "net/ieee802154.h"
#include "periph/timer.h"
#include "nrf802154.h"
#include "net/ieee802154/radio.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static volatile uint8_t _ifs;
static uint8_t rxbuf[IEEE802154_FRAME_LEN_MAX + 3]; /* len PHR + PSDU + LQI */
static uint8_t txbuf[IEEE802154_FRAME_LEN_MAX + 3]; /* len PHR + PSDU + LQI */
static void (*__isr)(void *arg);

#define ED_RSSISCALE        (4U)
#define ED_RSSIOFFS         (-92)

#define RX_COMPLETE         (0x1)
#define TX_COMPLETE         (0x2)
#define LIFS                (40U)
#define SIFS                (12U)
#define SIFS_MAXPKTSIZE     (18U)
#define TIMER_FREQ          (62500UL)
static volatile uint8_t _state;
static mutex_t _txlock;
ieee802154_radio_ops_t nrf802154_ops;

ieee802154_dev_t nrf802154_dev = {
    .driver = &nrf802154_ops
};

static int prepare(ieee802154_dev_t *dev, iolist_t *iolist)
{
    (void)dev;

    DEBUG("[nrf802154] Send a packet\n");

    assert(iolist);

    mutex_lock(&_txlock);

    /* copy packet data into the transmit buffer */
    unsigned int len = 0;
    for (; iolist; iolist = iolist->iol_next) {
        if ((IEEE802154_FCS_LEN + len + iolist->iol_len) > (IEEE802154_FRAME_LEN_MAX)) {
            DEBUG("[nrf802154] send: unable to do so, packet is too large!\n");
            mutex_unlock(&_txlock);
            return -EOVERFLOW;
        }
        /* Check if there is data to copy, prevents undefined behaviour with
         * memcpy when iolist->iol_base == NULL */
        if (iolist->iol_len) {
            memcpy(&txbuf[len + 1], iolist->iol_base, iolist->iol_len);
            len += iolist->iol_len;
        }
    }

    DEBUG("[nrf802154] send: putting %i byte into the ether\n", len);
    /* specify the length of the package. */
    txbuf[0] = len + IEEE802154_FCS_LEN;

    /* set interframe spacing based on packet size */
    _ifs = (len + IEEE802154_FCS_LEN > SIFS_MAXPKTSIZE) ? LIFS
                                                                    : SIFS;

    return len;
}

static int transmit(ieee802154_dev_t *dev)
{
    (void) dev;
    /* trigger the actual transmission */
    NRF_RADIO->TASKS_TXEN = 1;
    while (!(NRF_RADIO->EVENTS_TXREADY)) {};
    DEBUG("[nrf802154] Device state: TXIDLE\n");
    timer_set(NRF802154_TIMER, 0, _ifs);
    return 0;
}

/**
 * @brief   Reset the RXIDLE state
 */
 static void _reset_rx(void)
 {
    if (NRF_RADIO->STATE != RADIO_STATE_STATE_RxIdle) {
        return;
    }

    /* reset RX state and listen for new packets */
    _state &= ~RX_COMPLETE;
    NRF_RADIO->TASKS_START = 1;
 }

static int _read(ieee802154_dev_t *dev, struct iovec *iov, ieee802154_rx_info_t *info, ieee802154_rx_done_cb rx_done)
{
    (void) iov;
    size_t pktlen = (size_t)rxbuf[0] - IEEE802154_FCS_LEN;
    int res = -ENOBUFS;

    if (iov->iov_len < pktlen) {
        DEBUG("[nrf802154] recv: buffer is to small\n");
        return res;
    }
    else {
        DEBUG("[nrf802154] recv: reading packet of length %i\n", pktlen);
        if (info != NULL) {
            ieee802154_rx_info_t *radio_info = info;
            /* Hardware link quality indicator */
            uint8_t hwlqi = rxbuf[pktlen + 1];
            /* Convert to 802.15.4 LQI (page 319 of product spec v1.1) */
            radio_info->lqi = (uint8_t)(hwlqi > UINT8_MAX/ED_RSSISCALE
                                       ? UINT8_MAX
                                       : hwlqi * ED_RSSISCALE);
            /* Calculate RSSI by subtracting the offset from the datasheet.
             * Intentionally using a different calculation than the one from
             * figure 122 of the v1.1 product specification. This appears to
             * match real world performance better */
            radio_info->rssi = (int16_t)hwlqi + ED_RSSIOFFS;
        }
        iov->iov_base = &rxbuf[1];
        iov->iov_len = pktlen;
        res = rx_done(dev, iov, info);
    }

    _reset_rx();

    return res;
}

static int set_channel(ieee802154_dev_t *dev, uint8_t channel, uint8_t page)
{
    (void) dev;
    (void) page;
    NRF_RADIO->FREQUENCY = (channel - 10) * 5;
    return 0;
}

/**
 * @brief   Check whether the channel is clear or not
 * @note    So far only CCA with Energy Detection is supported (CCA_MODE=1).
 */
static bool _channel_is_clear(void)
{
    NRF_RADIO->CCACTRL |= RADIO_CCACTRL_CCAMODE_EdMode;
    NRF_RADIO->EVENTS_CCAIDLE = 0;
    NRF_RADIO->EVENTS_CCABUSY = 0;
    NRF_RADIO->TASKS_CCASTART = 1;

    for(;;) {
        if(NRF_RADIO->EVENTS_CCAIDLE) {
            return true;
        }
        if(NRF_RADIO->EVENTS_CCABUSY) {
            return false;
        }
    }
}

static bool cca(ieee802154_dev_t *dev)
{
    (void) dev;
    return _channel_is_clear();
}

/**
 * @brief   Set CCA threshold value in internal represetion
 */
static void _set_cca_thresh(uint8_t thresh)
{
    NRF_RADIO->CCACTRL &= ~RADIO_CCACTRL_CCAEDTHRES_Msk;
    NRF_RADIO->CCACTRL |= thresh << RADIO_CCACTRL_CCAEDTHRES_Pos;
}

/**
 * @brief   Convert from dBm to the internal representation, when the
 *          radio operates as a IEEE802.15.4 transceiver.
 */
static inline uint8_t _dbm_to_ieee802154_hwval(int8_t dbm)
{
    return ((dbm - ED_RSSIOFFS) / ED_RSSISCALE);
}

static int set_cca_threshold(ieee802154_dev_t *dev, int8_t threshold)
{
    (void) dev;
    _set_cca_thresh(_dbm_to_ieee802154_hwval(threshold));
    return 0;
}

static void _set_txpower(int16_t txpower)
{
    if (txpower > 8) {
        NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos8dBm;
    }
    if (txpower > 1) {
        NRF_RADIO->TXPOWER = (uint32_t)txpower;
    }
    else if (txpower > -1) {
        NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_0dBm;
    }
    else if (txpower > -5) {
        NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg4dBm;
    }
    else if (txpower > -9) {
        NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg8dBm;
    }
    else if (txpower > -13) {
        NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg12dBm;
    }
    else if (txpower > -17) {
        NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg16dBm;
    }
    else if (txpower > -21) {
        NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg20dBm;
    }
    else {
        NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg40dBm;
    }
}

static int set_tx_power(ieee802154_dev_t *dev, int16_t pow)
{
    (void) dev;
    _set_txpower(pow);
    return 0;
}

static void _disable(void)
{
    /* set device into DISABLED state */
    if (NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled) {
        NRF_RADIO->EVENTS_DISABLED = 0;
        NRF_RADIO->TASKS_DISABLE = 1;
        while (!(NRF_RADIO->EVENTS_DISABLED)) {};
        DEBUG("[nrf802154] Device state: DISABLED\n");
    }
}

/**
 * @brief   Set radio into RXIDLE state
 */
static void _enable_rx(void)
{
    DEBUG("[nrf802154] Set device state to RXIDLE\n");
    /* set device into RXIDLE state */
    if (NRF_RADIO->STATE != RADIO_STATE_STATE_RxIdle) {
        _disable();
    }
    NRF_RADIO->PACKETPTR = (uint32_t)rxbuf;
    NRF_RADIO->EVENTS_RXREADY = 0;
    NRF_RADIO->TASKS_RXEN = 1;
    while (!(NRF_RADIO->EVENTS_RXREADY)) {};
    DEBUG("[nrf802154] Device state: RXIDLE\n");
}


static int set_trx_state(ieee802154_dev_t *dev, ieee802154_trx_state_t state)
{
    (void) dev;
    switch(state) {
        case IEEE802154_TRX_STATE_TRX_OFF:
            _disable();
            break;
        case IEEE802154_TRX_STATE_RX_ON:
            _enable_rx();
            break;
        case IEEE802154_TRX_STATE_TX_ON:
            _disable();
            NRF_RADIO->PACKETPTR = (uint32_t)txbuf;
            NRF_RADIO->EVENTS_TXREADY = 0;
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

static int set_flag(ieee802154_dev_t *dev, ieee802154_rf_flags_t flag, bool value)
{
    (void) dev;
    (void) flag;
    (void) value;
    return 0;
}

static bool get_flag(ieee802154_dev_t *dev, ieee802154_rf_flags_t flag)
{
    (void) dev;
    (void) flag;
    return false;
}

uint8_t _poll_events(ieee802154_dev_t *dev)
{
    (void) dev;
    uint8_t flags = 0;
    if (_state & RX_COMPLETE) {
        flags |= IEEE802154_RF_FLAG_RX_DONE;
    }

    if (_state & TX_COMPLETE) {
        flags |= IEEE802154_RF_FLAG_TX_DONE;
        _state &= ~TX_COMPLETE;
    }
    return flags;
}

void nrf802154_init_int(void (*isr)(void *arg))
{
    __isr = isr;
}

static void _timer_cb(void *arg, int chan)
{
    (void)arg;
    (void)chan;
    mutex_unlock(&_txlock);
    timer_stop(NRF802154_TIMER);
}

/**
 * @brief   Set radio into DISABLED state
 */
int nrf802154_init(void)
{
    int result = timer_init(NRF802154_TIMER, TIMER_FREQ, _timer_cb, NULL);
    assert(result >= 0);
    (void)result;
    timer_stop(NRF802154_TIMER);

    /* initialize local variables */
    mutex_init(&_txlock);

    /* reset buffer */
    rxbuf[0] = 0;
    txbuf[0] = 0;
    _state = 0;

    /* power on peripheral */
    NRF_RADIO->POWER = 1;

    /* make sure the radio is disabled/stopped */
    _disable();

    /* we configure it to run in IEEE802.15.4 mode */
    NRF_RADIO->MODE = RADIO_MODE_MODE_Ieee802154_250Kbit;
    /* and set some fitting configuration */
    NRF_RADIO->PCNF0 = ((8 << RADIO_PCNF0_LFLEN_Pos) |
                        (RADIO_PCNF0_PLEN_32bitZero << RADIO_PCNF0_PLEN_Pos) |
                        (RADIO_PCNF0_CRCINC_Include << RADIO_PCNF0_CRCINC_Pos));
    NRF_RADIO->PCNF1 = IEEE802154_FRAME_LEN_MAX;
    /* set start frame delimiter */
    NRF_RADIO->SFD = IEEE802154_SFD;
    /* set MHR filters */
    NRF_RADIO->MHRMATCHCONF = 0;              /* Search Pattern Configuration */
    NRF_RADIO->MHRMATCHMAS = 0xff0007ff;      /* Pattern mask */
    /* configure CRC conform to IEEE802154 */
    NRF_RADIO->CRCCNF = ((RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos) |
                         (RADIO_CRCCNF_SKIPADDR_Ieee802154 << RADIO_CRCCNF_SKIPADDR_Pos));
    NRF_RADIO->CRCPOLY = 0x011021;
    NRF_RADIO->CRCINIT = 0;

    /* Disable the hardware IFS handling  */
    NRF_RADIO->MODECNF0 |= RADIO_MODECNF0_RU_Msk;

    /* set default CCA threshold */
    _set_cca_thresh(CONFIG_NRF802154_CCA_THRESH_DEFAULT);

    /* configure some shortcuts */
    NRF_RADIO->SHORTS = RADIO_SHORTS_RXREADY_START_Msk | RADIO_SHORTS_TXREADY_START_Msk;

    /* enable interrupts */
    NVIC_EnableIRQ(RADIO_IRQn);
    NRF_RADIO->INTENSET = RADIO_INTENSET_END_Msk;

    return 0;
}

void isr_radio(void)
{
    /* Clear flag */
    if (NRF_RADIO->EVENTS_END) {
        NRF_RADIO->EVENTS_END = 0;

        /* did we just send or receive something? */
        uint8_t state = (uint8_t)NRF_RADIO->STATE;
        switch(state) {
            case RADIO_STATE_STATE_RxIdle:
                /* only process packet if event callback is set and CRC is valid */
                if (NRF_RADIO->CRCSTATUS == 1) {
                    _state |= RX_COMPLETE;
                }
                else {
                    _reset_rx();
                }
                break;
            case RADIO_STATE_STATE_Tx:
            case RADIO_STATE_STATE_TxIdle:
            case RADIO_STATE_STATE_TxDisable:
                timer_start(NRF802154_TIMER);
                DEBUG("[nrf802154] TX state: %x\n", (uint8_t)NRF_RADIO->STATE);
                _state |= TX_COMPLETE;
                _enable_rx();
                break;
            default:
                DEBUG("[nrf802154] Unhandled state: %x\n", (uint8_t)NRF_RADIO->STATE);
        }
        if (_state) {
            __isr(&nrf802154_dev);
        }
    }
    else {
        DEBUG("[nrf802154] Unknown interrupt triggered\n");
    }

    cortexm_isr_end();
}


ieee802154_radio_ops_t nrf802154_ops = {
    .prepare = prepare,
    .transmit = transmit,
    .read = _read,
    .cca = cca,
    .set_cca_threshold = set_cca_threshold,
    .set_channel = set_channel,
    .set_tx_power = set_tx_power,
    .set_trx_state = set_trx_state,
    .set_flag = set_flag,
    .get_flag = get_flag,
    .poll_events = _poll_events,
};
