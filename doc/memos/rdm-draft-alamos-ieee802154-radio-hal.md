- RDM: rdm-draft-alamos-ieee802154-radio-hal
- Title: The IEEE802.15.4 radio HAL
- Authors: José Álamos
- Status: draft
- Type: Design
- Created: March 2020

## Abstract

This memo describes the proposed Hardware Abstraction Layer for radios
compliant with IEEE802.15.4. The radio HAL provides an
abstraction layer to implement radio agnostic IEEE802.15.4 PHY and MAC.

The following list summarizes the features of the radio HAL:
- Support for IEEE802.15.4 accelerations like Automatic Retransmissions, Auto
  ACK, ACK Timeout, Address Filter, Hardware CSMA-CA, etc.
- `prepare` and `transmit` function to allow time sensitive MAC layers
 (e.g IEEE802.15.4 TSCH mode) to send preloaded data ASAP.
- Support for static and dynamic allocation.
- Discovery of hardware accelerations present in the device.
- Support for (optional) information associated to a TX or RX.
  Examples of these are LQI, RSSI, number of retransmissions, valid CRC.

Although the HAL reuses concepts of our current `netdev` abstraction, there are
some advantages:
- It's optimized for radios compliant with IEEE802.15.4
- It's more lightweight because it only implements the slimmest layer to
  plug a IEEE802.15.4 layer on top.
- It allows the usage of radios without an abstraction layer on top.
  This is useful for writing device specific tests, extremely lightweight
  applications or for using hardware dependent features.
- It provides helpers for the upper layer to implement a PHY or SubMAC layer
  on top.
- The abstraction layer has no transceiver state changes.
  This allow higher layers to have full control of the states.

Throughout the design and API proposed in this document is tentative,
the goal is to describe the desired functionality of the final implementation.

## Status

This document is currently under open discussions. This document is a product of
the [Lower Network Stack rework](https://github.com/RIOT-OS/RIOT/issues/12688)
and aims to describe the architecture of the IEEE802.15.4 radio abstraction
layer.
The content of this document is licensed with a Creative Commons CC-BY-SA license.

## Terminology
This memo uses the [RFC2119](https://www.ietf.org/rfc/rfc2119.txt) terminology
and the following acronyms and definitions:

### Acronyms
- RDM: RIOT Developer Memo
- PIB: Physical Information Base.
- MIB: MAC Information Base.

### Definitions
- SubMAC: Lower layer of the IEEE802.15.4 MAC that provides the
  retransmissions with CSMA-CA logic, address filtering and CRC validation.
- Stand Alone CCA: Single run of the Clear Channel Assessment procedure.
- Continuous CCA: Clear Channel Assessment procedure followed by transmission
  (required by the CSMA-CA algorithm)

# Introduction
This document defines the proposed API for the IEEE802.15.4 radio abstraction
layer. 

The IEEE802.15.4 Radio HAL abstract common functionalities of IEEE802.15.4
compliant radios, so upper layers have a hardware independent layer to control
the radio.

The radio HAL can be used to implement a compliant IEEE802.15.4 MAC on top or
the South Bound API of network stacks that require direct access to the radio
(OpenThread).

```
+-----------------------------+             +-----------------------------+
|                             |             |                             |
|    gnrc_netif South Bound   |             |     OpenThread South Bound  |
|    (gnrc_netif_ieee802154)  |             |         (otPlatRadio)       |
+--------------+--------------+             +--------------+--------------+
               ^                                           ^               
               |                                           |               
               |              Stack dependent              |               
               |                                           |               
 --------------------------------------------------------------------------
               |                                           |               
               |              Stack independent            |
          Link Layer API                                   |
     (MCPS/MLME, SubMAC API)                               | 
               |                                           |               
               |                                           |               
               v                                           |               
+-----------------------------+                            | 
|                             |              Radio HAL API / Event Callback
|   IEEE802.15.4 Link Layer   |                            |
|          (MAC + PHY)        |                            |
+--------------+--------------+                            |
               ^                                           |               
               |                                           |               
 Radio HAL API / Event Callback                            |  
               |                                           |               
               v                                           v               
+-------------------------------------------------------------------------+
|                                                                         |
|                           IEEE802.154 radio HAL                         |
|                                                                         |
+-------------------------------------------------------------------------+
```

# Architecture
The radio HAL is designed to be on top of an autonomous device driver.
Since devices drivers don't have any dependency with the HAL, it's still
possible to use the device without the HAL on top (e.g testing, device specific
features).

The upper layer controls the radio via a defined set of operations (process IRQ,
 set channel, transmit packet) using the radio HAL API.
The radio communicates with the upper layer via an event callback.

Same as `netdev`, the radio HAL requires an upper layer to do the Bottom-Half
processing (a.k.a avoid processing the radio on ISR context).
This allows the usage of different event processing mechanisms
(msg, thread flags, event threads, etc). The Bottom-Half processer should use
the Radio API to request the device driver to process the IRQ.


```
+-----------------------------+            +-----------------------------+
|                             |            |                             |
|         Upper layer         |            |    Bottom-Half processor    |                   
|     (e.g SubMAC, OpenWSN)   |            | (e.g OpenWSN, event_thread) |                   
+--------------+--------------+            +--------------+--------------+                   
       |               ^                           |                 ^
       |               |                           |                 |
       |               |                           |                 |
Radio HAL API     event_callback                   |                 |
       |               |                       Radio HAL API         |
       |               |                       (Process IRQ)         |
       v               |                           |                 |
+--------------+--------------+                    |                 |
|                             |                    |                 |
|     IEEE802.154 radio HAL   |<-------------------+                 |
|                             |                                      |
+--------------+--------------+                                     IRQ
               |                                                     |
               |                                                     |
               |                                                     |
           radio_ops                                                 |
               |                                                     |
               |                                                     |
               v                                                     |
+--------------+--------------+                                      |
|                             |                                      |
|       Device Specific       |                                      |
|      HAL implementation     |                                      |
+-----------------------------+                                      |
               |                                                     |
               |                                                     |
               |                                                     |
        Device Specific API                                          |
               |                                                     |
               |                                                     |
               v                                                     |
+--------------+--------------+                                      |
|                             |                                      |
|        Device Driver        |--------------------------------------+
|                             |
+-----------------------------+
```

## Upper layers
The upper layers are users that requires direct access to the primitive
operations of the radio and/or to the hardware acceleration.

For instance:
- A SubMAC layer can use the radio HAL to implement the PHY layer (data
communication, set/get PHY parameters, perform CCA, etc).
- A network stack that requires direct access to the radio (OpenWSN,
  OpenThread), can use the radio HAL to implement the South Bound API.
- A user that wants to write a very simple application to send and receive
  data between radios (relying on hardware acceleration for retransmissions,
  CSMA-CA, etc).

The upper layer controls the radio via the Radio HAL API. Events from the radio
(packet received, transmission finished) are indicated via an event callback.

## Radio HAL
The Radio HAL implement a set of functionalities to control the operation of
the radio, process the IRQ handler and fetch events from the device (Event
Callback).

The API has a group of common functions and some functions specific to radios
with a certain kind of hardware acceleration.

Examples of common operations are:
- Set the transceiver state
- Prepare a packet in the framebuffer
- Transmit a packet
- Read a packet into a buffer and RX information (LQI, RSSI, etc).
- Check if the device support retransmissions

Examples of specific operations
- Read TX info after retransmission procedure (frame retries, pending bit, etc)
- Set address filter addresses (extended, short, PAN ID)

The API also provides a function to process IRQs from the radio. The Bottom-Half
processor will call this function as soon as possible when an IRQ is requested.

The API provides an Event Callback that should be implemented by the Upper Layer
in order to perform actions (e.g allocate a packet when a packet is received or
start software ACK timers).

The Radio HAL uses the `radio_ops` interface to access the device specific
HAL implementation.

## Bottom-Half processor
The BH processor is a component to offload the IRQ processing to thread context.
The component registers to the device IRQ and uses internal mechanisms to call
the Radio API IRQ handler from a safe context.

Examples of BH processors:
- The `gnrc_netif` thread that waits for a message from the ISR implementation
  of the device driver to call the IRQ handler.
- A thread waiting for thread flags sent from ISR context.

## Device Specific HAL implementation
The Device Specific HAL implementation implements the hardware dependent
component of the Radio API. It implements the `radio_ops` interface so the
Radio HAL can be agnostic to the hardware.

This component uses the Device Driver specific API to implement the operations.

## Device Driver
The Device Driver implements the Hardware Adoption Layer of the device. It
should implement the minimal functionalities to implement the Device Specific
HAL on top, but it could also include device specific functionalities not
exposed but the Radio HAL API (e.g Smart Listening in AT86RF2xx radios).

The Device Driver can be used without the Radio HAL on top, for testing or
device specific applications.

How to implement the Device Driver is out of the scope of this document.

## `radio_ops` interface
The `radio_ops` interface define the south bound API of the Radio HAL. This
interface connects the Radio HAL with the Device Specific HAL implementation.
The interface has a group of functions that are mandatory to all radios and a
group of functions that are dependent of the capabilites of the device or
compile time configurations.

Here's a list of minimal functions that should be implemented by the
Device Specific HAL Implementation.
- `get_cap`: Check if the radio supports a capability. Examples of capabilites
             are ACK timeout, frame retransmissions with CSMA-CA, hardware
             address filter, support for SubGHz band, etc.
- `set_trx_state`: Set the PHY transceiver state (TX mode, RX mode, transceiver
                   off)
- `prepare`: Load a packet (PSDU) in the framebuffer of the device
- `transmit`: Transmit a packet already preloaded with the `prepare` function.
              The transmission uses all hardware accelerations declared by
              the `get_cap` function.
- `read`: Read a received packet into a buffer or drop packet.
- `set_cca_threshold`: Set the CCA Theshold for the first mode of the CCA
                       procedure.
- `set_cca_mode`: Set the CCA mode (ED threshold, Carrier Sense, combination of
                  both)
- `cca`: Perform blocking CCA
- `set_channel`: Set channel number and page
- `set_tx_power`: Set TX power in dBm
- `set_sleep`: Set the device to sleep mode
- `start`: Start the device
- `set_promiscuous`: Set promiscuous mode
- `irq_handler`: Process the IRQ from the radio. The Event Callback is invoked
                 from this function.

The following functions are optional functionalities used to access hardware
acceleration features or allow allocation in network stacks that work with
packet buffers (GNRC, LWIP):
- `len`: Get the length of a received packet, without raising framebuffer
         protections.
- `get_tx_status`: If `get_caps` reports frame retransmissions, this function
                   reads the TX SubMAC status (pending bit, transmission with no
                   ACK or medium busy, etc).
- `set_hw_addr_filter`: If `get_caps` reports hardware address filter, this
                        function writes the IEEE addresses (extended, short,
                        PAN ID) in the AF.
- `set_frame_retries`: If `get_caps` reports frame retransmissions,
                       this function sets the frame retries.
- `set_csma_params`: If `get_caps` reports frame retransmissions, this function
                     set the CSMA-CA params for the exponential back-off.

Note the `radio_ops` has just a few getters. This is done on purpose, because
it's assumed that higher layers will already have a copy of the PIB and MIB,

## Event Callback interface
The Event Callback is called by the IRQ process function when the radio reports
an event (received a packet, a transmission is done, ACK timeout, CRC failed,
etc). This callback should be implemented by the user of the Radio HAL.

An overview of the events:
- TX done: The radio finished the process of sending a packet.
- RX done: Indicates that the radio received a packet.
- ACK timeout: If the radio supports ACK timeout, this indicates that the
                 timer expired.
- CRC fail: Packet was received but CRC failed

Other MAC specific events are not included (e.g TX done with frame pending,
CSMA-CA medium busy or exceeded number of retranmissions). This can be
extracted on TX done event using the Radio HAL API.

## Device Driver API
Besides implementing functions for the HAL, the Device Driver API should
 at least expose the following functionalities:
- Init function: The driver should provide a function to setup the device
  and put it in a state that minimizes power consumption. This is because there
  might be some delay between the device initialization and starting the
  device (e.g setting a network interface up). 
- Start function: This function should set the device in a state where is ready
  to operate (IRQ enabled, transceiver enabled). The PHY settings (channel,
  tx power) should be the defaults. If this function succeeds the transceiver
  should be off.

A Device Driver API can expose functions that are not related to the Radio HAL
but add some value to testing or simple radio applications (e.g getters from
internal registers, device specific functions, etc).

## IRQ
The Device Driver API should provide a mechanism to expose the ISR of the radio,
so it's possible to do Bottom-Half processing without the HAL on top.
The Bottom-Half processor should register to the device ISR in order to do
ISR offloading.

# Implementation details
## Radio HAL API
The Radio HAL API is the North Bound API for accessing the Radio HAL
features.

The API is mostly defined by wrappers of `radio_ops` functions but with
more semantics (e.g `ieee802154_radio_has_frame_retries` instead of
`get_cap(CAP_FRAME_RETRIES)`.

An advantage of wrapping the `radio_ops` functions is the fact that allow
some compile time optimizations.
For instance, we can write the `ieee802154_radio_has_frame_retries` this way to
optimize out code.

```c
bool ieee802154_radio_has_frame_retries(void *dev)
{
    if (IS_ACTIVE(RADIO_REQUIRES_FRAME_RETRIES) &&
        IS_ACTIVE(RADIO_HAS_FRAME_RETRIES)) {
        return dev->get_caps(CAP_FRAME_RETRIES);
    }
    else if (IS_ACTIVE(RADIO_HAS_FRAME_RETRIES)) {
        return true;
    }
    else {
        return false;
    }
}
```
Also, if the API of the `radio_ops` changes, it doesn't break the Radio HAL API.
The full list of functions is available in the Implementation Details section.

### Notes on the Radio API
#### Explicit transceiver states
Following the IEEE802.15.4 PHY definition, there are only 3 transceiver states:

- `TRX_OFF`: The transceiver is off. It cannot receive packets nor perform
             transmissions.
- `TRX_ON`: The transceiver is in a state where the framebuffer can be loaded
            and a packet can be transmitted.
- `RX_ON`: The radio is in a state where it can receive new packets.

This simplify a lot the operation and implementation of the HAL.
The implementation of the HAL is responsible to map other states to the
PHY state. For instance, a radio in sleep mode or with the transceiver disabled
will map to the same `TRX_OFF` state.

Some functions of the radio API require that the transceiver is on a specific
state. This does not only allow the usage of upper layers that require total
control of the radio (OpenWSN) but also ensures that the radio is always in a
well known state since the HAL won't perform any hidden state change. It also
avoid unnecessary state changes.

For instance, functions to prepare and transmit packets require the radio to be
in `TX_ON` state.

The only exception is the `ieee802154_set_sleep` function (that sets the
state to `TRX_OFF`). There might be some flags in the future to allow the radio
to change to a well defined state after certain operations. E.g if the radio
doesn't allow CSMA-CA but provides Auto CCA, the radio should try to send data
as soon as possible. Some radios alreayd include this feature to automatically
change to `TX_ON` after CCA without an explicit call to the function to set
the transceiver state.

#### Prepare and transmit
The Radio HAL defines separates the send procedure in preloading the frame
buffer and triggering the transmissions start. This is required for MAC layers
with timing contraints.

It's expected that a proper "send" function is defined by a higher layer (for
instance a SubMAC)

#### TX and RX info
All information associated to the reception of a packet (LQI, RSSI) as well
as the transmission information (if a device supports frame retransmissions) is
exposed to the user via the Radio HAL API.

However, requesting this information is optional. An application might not be
interested in the RSSI information or the TX status if unconfirmed MAC messages
are sent.

#### Thread Safety

The radio API is designed to be called from a single thread context. Thus,
the API is not thread safe. However, as long as the the API functions are
called sequencially, it would be possible to use any synchronization mechanism
to use the radio in a multi-thread environment.
The API guarantees that the event callback is invoked from the
`ieee802154_radio_irq_handler`, so this should be taken into consideration
when implementing a synchronization mechanism (e.g usage of recursive mutex).

### Implementation

```c
/**
 * @brief Load a packet in the internal framebuffer of the device.
 *
 * @pre the PHY state is @ref IEEE802154_TRX_STATE_TX_ON.
 * @pre the device is not sleeping
 *
 * This function assumes @p pkt is valid and doesn't exceed the maximum PHY
 * length. Also, it should block until is safe to write a packet.
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] pkt the packet to be loaded
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_prepare(ieee802154_dev_t *dev, iolist_t *pkt)
{
    return dev->driver->prepare(dev, pkt);
}


/**
 * @brief Transmit a preloaded packet in the framebuffer.
 *
 * @pre the PHY state is @ref IEEE802154_TRX_STATE_TX_ON.
 * @pre the device is not sleeping
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_transmit(ieee802154_dev_t *dev)
{
    return dev->driver->transmit(dev);
}

/**
 * @brief Read the length of the received packet
 *
 * This function can be used to allocate buffer space for a packet.
 * It doesn't release the framebuffer and MUST be called
 * before the @ref ieee802154_radio_read function, otherwise the behavior is
 * undefined.
 *
 * @pre the device is not sleeping
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_len(ieee802154_dev_t *dev)
{
    return dev->driver->len(dev);
}

/**
 * @brief Read a packet into a buffer of given size
 * 
 * @pre the radio already received a packet (e.g
 *      @ref ieee802154_dev_t::cb with @ref IEEE802154_RADIO_RX_DONE).
 * @pre the device is not sleeping
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[out] buf buffer to write the received packet into. If NULL, the
 *             packet is not copied.
 * @param[in] size size of @p buf
 * @param[in] info information of the received packet (LQI, RSSI). Can be
 *            NULL if this information is not needed.
 *
 * @post the PHY state is @ref IEEE802154_TRX_STATE_RX_ON
 * @post the transceiver can receive more packets (raise frame buffer protection)
 *
 * @return number of bytes written in @p buffer
 * @return 0 if @p buf == NULL
 * @return -ENOBUFS if the packet doesn't fit in @p buffer
 */
static inline int ieee802154_radio_read(ieee802154_dev_t *dev, void *buf,
                                        size_t size, ieee802154_rx_info_t *info)
{
    return dev->driver->read(dev, buf, size, info);
}

/**
 * @brief Perform CCA to check if the channel is clear
 *
 * @pre the PHY state is @ref IEEE802154_TRX_STATE_RX_ON.
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if channel is clear.
 * @return false if channel is busy.
 */
static inline bool ieee802154_radio_cca(ieee802154_dev_t *dev)
{
    return dev->driver->cca(dev);
}

/**
 * @brief Set the threshold for the CCA (first mode)
 *
 * @pre the device is not sleeping
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] threshold the threshold in dBm
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_cca_threshold(ieee802154_dev_t *dev,
                                                     int8_t threshold)
{
    return dev->driver->set_cca_threshold(dev, threshold);
}

/**
 * @brief Set CCA mode
 *
 * @note It's guaranteed that CCA with ED Threshold is supported in all radios.
 *
 * @pre the device is not sleeping
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] mode the CCA mode
 *
 * @return 0 on success
 * @return -ENOTSUP if the CCA mode is not supported
 * @return negative errno on error
 *
 */
static inline int ieee802154_radio_set_cca_mode(ieee802154_dev_t *dev,
                                                ieee802154_cca_mode_t mode)
{
    return dev->driver-set_cca_mode(dev, mode);
}

/**
 * @brief Set IEEE802.15.4 PHY configuration (channel, TX power)
 *
 * @note This functio DOES NOT validate the PHY configurations unless
 * it's specific to the device. The upper layer is responsible of all kind
 * of validations.
 *
 * @pre the device is not sleeping
 * @pre the PHY state is @ref IEEE802154_TRX_STATE_TRX_OFF.
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] conf the PHY configuration
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_config_phy(ieee802154_dev_t *dev,
                                              ieee802154_phy_conf_t *conf)
{
    return dev->driver->config_phy(dev, conf);
}

/**
 * @brief Sets the transceiver PHY state.
 *
 * This function blocks until the state change occurs.
 *
 * @pre the device is not sleeping
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] state the transceiver state to change to
 *
 * @return 0 on success
 * @return negative -EBUSY if the radio is busy
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_trx_state(ieee802154_dev_t *dev,
                                                 ieee802154_trx_state_t state)
{
    return dev->driver->set_trx_state(dev, state);
}

/**
 * @brief Set the sleep state of the radio
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] sleep whether the device should sleep or not
 *
 * @post if @p sleep == true, the transceiver PHY state is
 *        @ref IEEE802154_TRX_STATE_TRX_OFF
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_sleep(ieee802154_dev_t *dev, bool sleep)
{
    return dev->driver->set_sleep(dev, sleep);
}

/**
 * @brief Process the transceiver IRQ
 *
 * @note It's safe to call this function when the transceiver is sleeping. In
 *       that case it does nothing.
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 */
static inline void ieee802154_radio_irq_handler(ieee802154_dev_t *dev)
{
    dev->driver->irq_handler(dev);
}

/**
 * @brief Get the SubMAC TX information
 *
 * @pre the device is not sleeping
 * @pre the device finished the TX procedure
 * @pre the device supports frame retransmissions
 *      (@ref ieee802154_radio_has_frame_retries() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] info the TX info
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_get_tx_status(ieee802154_dev_t *dev,
                                                 ieee802154_tx_info_t *info)
{
    return dev->driver->get_tx_status(dev, info);
}

/**
 * @brief Write IEEE802.15.4 addresses into the hardware address filter
 *
 * @pre the device is not sleeping
 * @pre the device supports address filtering
 *      (@ref ieee802154_radio_has_addr_filter() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] short_addr IEEE802.15.4 short address
 * @param[in] ext_addr IEEE802.15.4 extended address
 * @param[in] pan_id IEEE802.15.4 PAN ID
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_hw_addr_filter(ieee802154_dev_t *dev,
                                                      uint8_t *short_addr,
                                                      uint8_t *ext_addr,
                                                      uint16_t pan_id)
{
    return dev->driver->set_hw_addr_filter(dev, short_addr, ext_addr, pan_id);
}

/**
 * @brief Set the number of retransmissions
 *
 * @pre the device is not sleeping
 * @pre the device supports frame retransmissions
 *      (@ref ieee802154_radio_has_frame_retries() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] retries the number of retransmissions
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_frame_retries(ieee802154_dev_t *dev,
                                                     int retries)
{
    return dev->driver->set_frame_retries(dev, retries);
}

/**
 * @brief Set the CSMA-CA parameters
 *
 * @pre the device is not sleeping
 * @pre the device supports frame retransmissions
 *      (@ref ieee802154_radio_has_frame_retries() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] bd parameters of the exponential backoff
 * @param[in] retries number of CSMA-CA retries. If @p restries < 0,
 *                    retransmissions with CSMA-CA are disabled
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_csma_params(ieee802154_dev_t *dev,
                                                   ieee802154_csma_be_t *bd,
                                                   int8_t retries)
{
    return dev->driver->set_csma_params(dev, bd, retries);
}

/**
 * @brief Set IEEE802.15.4 promiscuous mode
 *
 * @pre the device is not sleeping
 * @pre the device supports address filtering
 *      (@ref ieee802154_radio_has_addr_filter() == true)
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] enable whether the promiscuous mode should be enabled or not
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_set_promiscuous(ieee802154_dev_t *dev,
                                                   bool enable)
{
    return dev->driver->set_promiscuous(dev, enable);
}

/**
 * @brief Start the device
 *
 * This function puts the radio in a state where it can be operated (enable
 * interrupts, etc)
 *
 * @pre the device driver init function was already called
 *
 * @post the transceiver state is @ref IEEE802154_TRX_STATE_TRX_OFF
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return 0 on success
 * @return negative errno on error
 */
static inline int ieee802154_radio_start(ieee802154_dev_t *dev)
{
    return dev->driver->start(dev);
}

/**
 * @brief Check if the device supports ACK timeout
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_ack_timeout(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_ACK_TIMEOUT);
}

/**
 * @brief Check if the device supports frame retransmissions
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_frame_retries(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_FRAME_RETRIES);
}

/**
 * @brief Check if the device supports address filtering
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_addr_filter(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_HW_ADDR_FILTER);
}

/**
 * @brief Check if the device supports Auto ACK
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_auto_ack(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_AUTO_ACK);
}

/**
 * @brief Check if the device supports the IEEE802.15.4 Sub-GHz band
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_sub_ghz(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_SUB_GHZ);
}

/**
 * @brief Check if the device supports the IEEE802.15.4 2.4 GHz band
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 *
 * @return true if the device has support
 * @return false otherwise
 */
static inline bool ieee802154_radio_has_sub_ghz(ieee802154_dev_t *dev)
{
    return dev->driver->get_cap(dev, IEEE802154_CAP_24_GHZ);
}
```

### Examples of usage
#### Set the IEEE802.15.4 channel to 26 and page to 0.
  `ieee802154_radio_set_channel(dev, 26, 0);`

#### Read a packet into a buffer of size 127, and get the RX info (lqi, rssi, etc).
  ```c
  ieee802154_rx_info_t info;
  ieee802154_radio_read(dev, buffer, 127, info);
  ```

#### Check if the radio supports frame retransmissions:
  `ieee802154_radio_has_frame_retries(dev);`

#### Send a packet:
  ```c
  /* Set the transceiver state to TX_ON
  ieee802154_radio_set_trx_state(dev, TX_ON);

  /* Load the packet in the framebuffer
  ieee802154_radio_prepare(dev, pkt);

  /* Trigger TX_START (send the packet) */
  ieee802154_radio_transmit(dev, pkt);
  ```

## Definition of Radio Caps
```c
typedef enum {
    /**
     * @brief the device supports hardware address filter
     */
    IEEE802154_CAP_HW_ADDR_FILTER,
    /**
     * @brief the device support frame retransmissions with CSMA-CA
     */
    IEEE802154_CAP_FRAME_RETRIES,
    /**
     * @brief the device support ACK timeout interrupt
     */
    IEEE802154_CAP_ACK_TIMEOUT,
    /**
     * @brief the device supports Auto ACK
     */
    IEEE802154_CAP_AUTO_ACK,
    /**
     * @brief the device performs CCA when sending
     */
    IEEE802154_CAP_24_GHZ,
    /**
     * @brief the device support the IEEE802.15.4 Sub GHz band
     */
    IEEE802154_CAP_SUB_GHZ,
} ieee802154_rf_caps_t;
```

## The `ieee802154_dev_t` descriptor
The `ieee802154_dev_t` is a HAL descriptor that holds the Radio Ops driver
and the event callbacks for a device instance.

```c
/**
 * @brief the IEEE802.15.4 device descriptor
 */
struct ieee802154_dev {
    /**
     * @brief pointer to the operations of the device
     */
    const ieee802154_radio_ops_t *driver;
    /**
     * @brief the event callback of the device
     */
    ieee802154_cb_t cb;
};
```

The allocation of the HAL descriptor and pointer to the actual device
instance is up to the implementor of the HAL. For instance, the first member
of the device descriptor can be an `ieee802154_dev_t` struct, but other
techniques are also possible.

## Event callback

Here's the definition of the Event Callback and all events.
It's safe to call the radio HAL API from the event callback function.

```c
/**
 * @brief Prototype of the IEEE802.15.4 device event callback
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] status the status 
 */
typedef void (*ieee802154_cb_t)(ieee802154_dev_t *dev,
                                ieee802154_tx_status_t status);

typedef enum {
    /**
     * @brief the transceiver detected a valid SFD
     */
    IEEE802154_RADIO_RX_START,

    /**
     * @brief the transceiver received a packet and there's a packet in the
     *        internal framebuffer.
     *
     * The transceiver is in @ref IEEE802154_TRX_STATE_RX_ON state when
     * this funcion is called, but with framebuffer protection (either
     * dynamic framebuffer protection or disabled RX). Thus, the packet
     * won't be overwritten before calling the @ref ieee802154_radio_read
     * function. However, @ref ieee802154_radio_read MUST be called in
     * order to receive new packets. If there's no interest in the
     * packet, the function can be called with a NULL buffer to drop
     * the packet.
     */
    IEEE802154_RADIO_RX_DONE,

    /**
     * @brief the transceiver finished sending a packet or the
     *        retransmission procedure
     *
     * If the radio supports frame retransmissions the 
     * @ref ieee802154_radio_get_tx_status MAY be called to retrieve useful
     * information (number of retries, frame pending bit, etc). The
     * transceiver is in @ref IEEE802154_TRX_STATE_TX_ON state when this function
     * is called.
     */
    IEEE802154_RADIO_TX_DONE,
    /**
     * @brief the transceiver reports that the ACK timeout expired
     *
     * This event is present only if the radio support ACK timeout.
     */
    IEEE802154_RADIO_ACK_TIMEOUT,
    /**
     * @brief the transceiver received a packet but the CRC check failed
     */
    IEEE802154_RADIO_CRC_FAIL,
} ieee802154_trx_ev_t;
```

## South-Bound API (`radio_ops`)

```c
struct ieee802154_radio_ops {
    /**
     * @brief Load packet in the framebuffer of a radio.
     *
     * This function shouldn't do any checks, so the packet MUST be valid.
     * If the radio is still transmitting, it should block until is safe to
     * write again in the frame buffer
     *
     * @pre the PHY state is @ref IEEE802154_TRX_STATE_TX_ON.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] pkt the packet to be sent with valid length
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*prepare)(ieee802154_dev_t *dev, iolist_t *pkt);

    /**
     * @brief Transmit a preloaded packet
     *
     * @pre the PHY state is @ref IEEE802154_TRX_STATE_TX_ON and the packet
     *      is already in the framebuffer.
     *
     * @post the PHY state is @ref IEEE802154_TRX_STATE_TX_ON.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*transmit)(ieee802154_dev_t *dev);

    /**
     * @brief Get the lenght of the received packet.
     *
     * This function can use SRAM, a reg value or similar to read the packet
     * length.
     *
     * @pre the radio already received a packet (e.g
     *      @ref ieee802154_dev_t::cb with @ref IEEE802154_RADIO_RX_DONE).
     * @pre the device is not sleeping
     *
     * @post the frame buffer is still protected against new packet arrivals.
     *      
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*len)(ieee802154_dev_t *dev);

    /**
     * @brief Read a packet from the internal framebuffer of the radio.
     *
     * This function should try to write the received packet into @p buf and
     * put the radio in a state where it can receive more packets.
     *
     * @pre the radio already received a packet (e.g
     *      @ref ieee802154_dev_t::cb with @ref IEEE802154_RADIO_RX_DONE).
     * @pre the device is not sleeping
     *
     * @post the PHY state is @ref IEEE802154_TRX_STATE_RX_ON and the radio is
     *       in a state where it can receive more packets, regardless of the
     *       return value.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[out] buf buffer to write the received packet into. If NULL, the
     *             packet is not copied.
     * @param[in] size size of @p buf
     * @param[in] info information of the received packet (LQI, RSSI). Can be
     *            NULL if this information is not needed.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*read)(ieee802154_dev_t *dev, void *buf, size_t size,
                ieee802154_rx_info_t *info);

    /**
     * @brief Perform Stand-Alone Clear Channel Assessment
     *
     * This function performs blocking CCA to check if the channel is clear.
     * @pre the PHY state is @ref IEEE802154_TRX_STATE_RX_ON.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @return true if channel is clear.
     * @return false if channel is busy.
     */
    bool (*cca)(ieee802154_dev_t *dev);

    /**
     * @brief Set the threshold for the Energy Detection (first mode of CCA)
     *
     * @pre the device is not sleeping
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] threshold the threshold in dBm.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_cca_threshold)(ieee802154_dev_t *dev, int8_t threshold);

    /**
     * @brief Set CCA mode
     *
     * All radios MUST at least implement the first CCA mode (ED Threshold).
     *
     * @pre the device is not sleeping
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] mode the CCA mode
     *
     * @return 0 on success
     * @return -ENOTSUP if the mode is not supported
     * @return negative errno on error
     */
    int (*set_cca_mode)(ieee802154_dev_t *dev, ieee802154_cca_mode_t mode);

    /**
     * @brief Set IEEE802.15.4 PHY configuration (channel, TX power)
     *
     * This function SHOULD NOT validate the PHY configurations unless
     * it's specific to the device. The upper layer is responsible of all kind
     * of validations.
     *
     * @pre the device is not sleeping
     * @pre the PHY state is @ref IEEE802154_TRX_STATE_TRX_OFF.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] conf the PHY configuration
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*config_phy)(ieee802154_dev_t *dev, ieee802154_phy_conf_t *conf);

    /**
     * @brief Set the transceiver PHY state
     *
     * @pre the device is not sleeping
     *
     * @note the implementation MUST block until the state change occurs.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] state the new state
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_trx_state)(ieee802154_dev_t *dev, ieee802154_trx_state_t state);

    /**
     * @brief Set the sleep state of the device (sleep or awake)
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] sleep whether the device should sleep or not.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_sleep)(ieee802154_dev_t *dev, bool sleep);

    /**
     * @brief Get a cap from the radio
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param cap cap to be checked
     *
     * @return true if the radio supports the cap
     * @return false otherwise
     */
    bool (*get_cap)(ieee802154_dev_t *dev, ieee802154_rf_caps_t cap);

    /**
     * @brief Process radio IRQ.
     *
     * This function calls the @ref ieee802154_cb_t::cb function with all
     * the corresponding events.
     *
     * @note if the device is sleeping, this function should do nothing
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     */
    void (*irq_handler)(ieee802154_dev_t *dev);

    /**
     * @brief Start the device
     *
     * @pre the init function of the radio succeeded.
     *
     * This function puts the radio in a state where it can be operated. It
     * should enable interrupts and set the transceiver state to
     * @ref IEEE802154_TRX_STATE_TRX_OFF
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*start)(ieee802154_dev_t *dev);

    /**
     * @brief Set IEEE802.15.4 promiscuous mode
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       hardware address filtering.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] enable whether the promiscuous mode should be enabled or not.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_promiscuous)(ieee802154_dev_t *dev, bool enable);

    /**
     * @brief Get the SubMAC TX information (number of retransmissions,
     *        pending bit, status, etc).
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       frame retransmissions
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[out] info the TX information
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*get_tx_status)(ieee802154_dev_t *dev, ieee802154_tx_info_t *info);

    /**
     * @brief Set IEEE802.15.4 addresses in hardware address filter
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       hardware address filtering.
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] short_addr the IEEE802.15.4 short address
     * @param[in] ext_addr the IEEE802.15.4 extended address
     * @param[in] pan_id the IEEE802.15.4 PAN ID
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_hw_addr_filter)(ieee802154_dev_t *dev, uint8_t *short_addr,
                              uint8_t *ext_addr, uint16_t pan_id);

    /**
     * @brief Set number of frame retransmissions
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       frame retransmissions
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] retries the number of retransmissions
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_frame_retries)(ieee802154_dev_t *dev, int retries);

    /**
     * @brief 
     *
     * @pre the device is not sleeping
     *
     * @note this function pointer can be NULL if the device doesn't support
     *       frame retransmissions
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] bd parameters of the exponential backoff
     * @param[in] retries number of CSMA-CA retries
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_csma_params)(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd,
                           int8_t retries);
};
```

These functions should be implemented with device specific validations only.
Everything that's not device specific (valid channels, address length, etc)
should be checked by higher layers. This avoids redundant checks.

Although this pattern can be implemented with switch-case, there some evidence
suggesting that for this case the vtable approach can generate more efficient
instructions and it's easier to write. If a switch-case approach is desired,
it's possible to change the South-Bound API without affecting the radio API.
See the Appendix to see the comparison betweem the switch-case and vtable
approach.

# Annex
## Compile Time Configurations and Optimizations
Here is a list of tentative Compile Time configurations

### Radio HAL configurations
- `IEEE802154_RADIO_REPORT_BAD_CRC`: If enabled, radios will report packets with
  bad CRC. This is useful for statistics.
- `IEEE802154_RADIO_OPS_ENABLE_LEN`: Enable the radio ops`len` function.
  For applications with static input buffers, this function is not needed.

### Device specific configurations
- `<RADIO>_DISABLE_AUTO_ACK`: Disable the Auto ACK feature (if supported).
  The `get_cap` ops function should not report `CAP_AUTO_ACK` if enabled.
- `<RADIO>_DISABLE_FRAME_RETRANS`: Disable frame retransmissions with CSMA-CA
  (if supported).  The `get_cap` ops function should not report
  `CAP_FRAME_RETRIES` if enabled.
- `<RADIO>_DISABLE_HW_ADDR_FILTER`: Disable the Hardware Address filter
  (if supported). The `get_cap` ops function should not report
  `CAP_HW_ADDRESS_FILTER` if enabled.


## Measurements and comparison to netdev
### Footprint
TBD
### Speed
TBD

## Feature Matrix
TBD
## Vtables vs FP
TBD
