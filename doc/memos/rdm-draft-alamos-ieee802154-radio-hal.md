- RDM: rdm-draft-alamos-ieee802154-radio-hal
- Title: The IEEE802.15.4 radio HAL
- Authors: José Álamos
- Status: draft
- Type: Design
- Created: March 2020

# 1. Abstract

This memo describes the proposed Hardware Abstraction Layer for radios
compliant with the IEEE802.15.4 standard. The work follows a technology specific
approach to provide a well known layer for implementing agnostic IEEE802.15.4
PHY and MAC or integrating network stacks that require direct access
to the radio (OpenWSN)

The work presented in this document addresses the problems of using `netdev`
as a Hardware Abstraction Layer:
- It's too generic to be used as a HAL, considering the semantics
  of radio devices are technology specific and usually well defined.
- It includes PHY and MAC components that are not in the scope of a Radio HAL.
  This pulls much more code than needed and makes harder to implement and
  maintain a device driver.
- The interface doesn't expose primitive operations (set transceiver state) and
  hardcode MAC layer specific functionalities (internal state changes, etc).

# 2. Status

This document is currently under open discussions. This document is a product of
the [Lower Network Stack rework](https://github.com/RIOT-OS/RIOT/issues/12688)
and aims to describe the architecture of the IEEE802.15.4 radio abstraction
layer.
The content of this document is licensed with a Creative Commons CC-BY-SA license.

## 2.1 Terminology
This memo uses the [RFC2119](https://www.ietf.org/rfc/rfc2119.txt) terminology
and the following acronyms and definitions:

## 2.2 Acronyms
- RDM: RIOT Developer Memo
- PIB: Physical Information Base.
- MIB: MAC Information Base.

## 2.3 Definitions
- SubMAC: Lower layer of the IEEE802.15.4 MAC that provides the
  retransmissions with CSMA-CA logic, address filtering and CRC validation.
- Stand Alone CCA: Single run of the Clear Channel Assessment procedure.
- Continuous CCA: Clear Channel Assessment procedure followed by transmission
  (required by the CSMA-CA algorithm)
- Caps: Short word for capabilities. In this context, capabilites are the
        the features (hardware acceleration) present in a radio device.
- Ops: Short word for operations. In this context, operations are a set of
       instructions to control the radio device.

# 3. Introduction
This document defines a Hardware Abstraction Layer for IEEE802.15.4 compliant
radios.

The IEEE802.15.4 Radio HAL abstract common functionalities of IEEE802.15.4
compliant radios such as loading packets, transmitting, configuraring PHY
parameters, etc. This abstraction is required for upper layers that require a
hardware independent layer to drive radio devices (802.15.4 MAC, network stacks,
test applications, etc).

In the current RIOT lower network stack architecture radios are driven by the
`netdev` interface, which includes components of the 802.15.4 MAC/PHY such as
transmission of Physical Service Data Unit packets (PSDU), retransmissions with
CSMA-CA and ACK handling. The latter, only available if the hardware supports
these capabilities.

Other components of the 802.15.4 MAC are present in the GNRC Netif
implementation of the 802.15.4 Link Layer (`gnrc_netif_ieee802154`). These
components prepare and parse 802.15.4 frames in order to send and receive data.
However, these components don't include mandatory features of the 802.15.4 MAC
(commissioning, security, channel scanning, etc).

There are some evident problems of this strategy:
- The 802.15.4 MAC components of `gnrc_netif_ieee802154` are GNRC specific and
  cannot be reused in other network stacks that require a 802.15.4 MAC.
- The `netdev` interface exposes (Sub)MAC semantics to the upper layer. This is
  not good for users that require direct access to the radio (OpenWSN,
  OpenThread, 802.15.4 PHY, etc).

Ideally, these components should be separated in:
1. 802.15.4 Radio HAL: hardware agnostic interface to drive radio devices
2. 802.15.4 MAC: full link layer including PHY definition
3. Network Stack interface (netif): controls the 802.15.4 MAC layer to send
   and receive packets.

The 802.15.4 MAC and netif are out of the scope of this document, but they are
mentioned to give more context for the proposed work.

The following picture compares the current RIOT lower network architecture with
the approach described above:

```
         OLD             |                        NEW             
         ===             |                        ===             
                         |                                     
+---------------------+  |  +---------------------+   +---------------------+
|                     |  |  |                     |   |                     |
|  GNRC Network Stack |  |  |  GNRC Network Stack |   |                     |
|                     |  |  |                     |   |                     |
+---------------------+  |  +---------------------+   |                     |
          ^              |            ^               |                     |
          |              |            |               |                     |
     gnrc_netapi         |       gnrc_netapi          | OpenThread, OpenWSN |
          |              |            |               |                     |
          v              |            v               |                     |
+---------------------+  |  +---------------------+   |                     |
|                     |  |  |                     |   |                     |
|     GNRC Netif      |  |  |     GNRC Netif      |   |                     |
|                     |  |  |                     |   |                     |
+---------------------+  |  +---------------------+   +---------------------+
          ^              |            ^                         ^
          |              |            |                         |
   gnrc_netif_ops_t      |     gnrc_netif_ops_t                 |
          |              |            |                         |
          v              |            v                         |
+---------------------+  |  +---------------------+             |              
|                     |  |  |                     |             |              
|gnrc_netif_ieee802154|  |  |gnrc_netif_ieee802154|             |              
|                     |  |  |                     |             |              
+---------------------+  |  +---------------------+             |              
          ^              |            ^                         |              
          |              |            |                         |              
    netdev_driver_t      |     802.15.4 MAC API           Radio HAL API
          |              |            |                         |              
          v              |            v                         |              
+---------------------+  |  +---------------------+             |              
|                     |  |  |                     |             |              
|       netdev        |  |  |    802.15.4 MAC     |             |              
|                     |  |  |                     |             |              
+---------------------+  |  +---------------------+             |              
          ^              |            ^                         |              
          |              |            |                         |              
          |              |      Radio HAL API                   |
          |              |            |                         |              
          |              |            v                         v              
          |              |  +-----------------------------------------------+           
          |              |  |                                               |           
   netdev_driver_t       |  |               802.15.4 Radio HAL              |
          |              |  |                                               |           
          |              |  +-----------------------------------------------+           
          |              |                         ^                                               
          |              |                         |                                               
          |              |                   Device Driver API                                       
          |              |                         |                                               
          v              |                         v                                               
+---------------------+  |  +-----------------------------------------------+
|                     |  |  |                                               |
|    Device Driver    |  |  |                  Device Driver                |
|                     |  |  |                                               |
+---------------------+  |  +-----------------------------------------------+
```
As seen, the 802.15.4 Radio HAL is more specific than `netdev` and doesn't
include any 802.15.4 MAC components. 

# 4. Architecture
```
+-----------------------------------------------------------------------------+
|                                                                             |
|                               Upper layer                                   |
|                                                                             |
+-----------------------------------------------------------------------------+
      ^
      |
      |
      |
  Radio HAL API                                  +----------------------------+
      |                         Radio HAL API    |                            |
      |                   +----------------------|    Bottom-Half processor   |
      |                   |     (Process IRQ)    |                            |
      |                   |                      +----------------------------+
      |                   |                                   ^
      |                   |                                   |
      v                   v                                  IRQ
+-----------------------------+                               |
|                             |               HW independent  |
|   IEEE802.15.4 Radio HAL    |------------------------------------------------
|                             |               HW dependent    |
+-----------------------------+                               |
                |                                             |
       Device Specific API                                    | 
                |                                             |
                v                                             |
+-----------------------------+                               |
|                             |                               |
|       Device Driver         |-------------------------------+
|       implementation        |
+-----------------------------+
```

As shown in the above figure, the IEEE802.15.4 Radio HAL is a central component
that provides any upper layer a technology dependent and unified access to the
hardware specific device driver, by implementing the Radio HAL API.  Since
devices drivers do not depend on the Radio HAL, it is still possible to use a
the raw driver of a specific device, for testing purposes or accessing device
specific features.

Similar to the preceding approach based on `netdev` , the Radio HAL requires an
upper layer to take over the Bottom-Half processing which means, offloading the
ISR to thread context.  This allows for different event processing mechanisms
such as `msg`, `thread flags`, `event threads`, etc. The Bottom-Half processor
should use the Radio HAL API to process the IRQ.

## 4.1 Upper layer
Upper layers are users that requires direct access to the primitive operations
of a radio and its hardware acceleration features, if available.

Examples for Upper Layers:
- A MAC layer can use the Radio HAL to implement parts of a PHY layer (data
  communication, set/get parameters, perform CCA, etc.) .
- A network stack that requires direct access to the radio (OpenWSN,
  OpenThread) can use the Radio HAL to implement the integration code.
- A developer who implements a simple application to send and receive data
  between 802.15.4 radios (relying on hardware accelerated MAC features, if
available).

The upper layer accesses the radio using the Radio HAL API. Events that are
triggered by the device (packet received, transmission finished) are indicated
by an event notification mechanism, described below.

## 4.2 Bottom-Half processor
The Bottom-Half (BH) processor is a component to offload the IRQ processing to
thread context.  The component registers an IRQ handler during initialization
which is executed when the device triggers and interrupt. This handler uses
internal mechanisms to call the Radio API IRQ handler from a safe context.

The BH processor can be implemented dependent or independent of the network
stack. A network stack independent solution is preferred in order to reuse
functionality between different network stacks.

## 4.3 Radio HAL

The Radio HAL is defined by the Radio HAL API which consists of three main
components: Radio Operations, Event Notification, and the Device Specific
IEEE802.15.4 HAL implementation.

The Radio HAL Implementation provides a set of functionalities to control the
operation of the device, to process the IRQ handler and to receive event
notifications from the device.

### 4.3.1 Radio Operations
The Radio Operations (`radio_ops`) interface exposes operations that are common
to control 802.15.4 devices, to request their hardware capabilities information
(i.e., MAC acceleration hardware) and to process the radio IRQ.

The interface defines a collection of mandatory functions:
- Set the transceiver state
- Set the PHY configuration (channel, tx power, etc)
- Load and transmit a frame
- Get device capabilities
- Process IRQ

The interface provides a collection of optional functions that may or may not
be implemented, dependent on the hardware acceleration features of a device.
These functions include:
- Read the number of retransmissions
- Set address filter addresses (extended, short, PAN ID)
- Set CSMA-CA backoff parameters.

The full list of functions can be found in the Interface Definition section.

### 4.3.2 Event Notification
The Radio HAL provides an Event Notification mechanism to inform the upper
layer about an event (a packet was received, a transmission finished, etc).

The upper layer can subscribe to these events to perform different actions. A
an example, a MAC layer would subscribe to the RX done event to allocate the
received packet. The TX done event is commonly used to release resources or
update statistics.

The full list of events and implications are defined in the Interface
Definition section.

### 4.3.3 Device Specific IEEE802.15.4 HAL implementation
The Device Specific IEEE802.15.4 HAL implementation is part of the IEEE802.15.4
Radio HAL component in the above figure. It implements the hardware dependent
part of the IEEE802.15.4 Radio HAL by wrapping the `radio_ops` interface around
the device specific code by using the Device Driver API which grants access to
all device operations.

## 4.4 Device Driver
The Device Driver implements the Hardware Adoption Layer of the device. It
provides minimal functionalities needed by the Radio Operations,
and additionally a mechanism to expose the ISR of the radio, so the
Bottom-Half processor can offload the ISR.

The function set of the Device Driver can include device specific features that
are not exposed but the Radio HAL API (e.g., Smart Listening with AT86RF2xx
radios).  The Device Driver is an independent component and it can be used
without the Radio HAL on top, for testing purposes or device specific
applications.

# 5 Interface definition and implementation details
## 5.1.1 Init vs Start
In order to implement the 802.15.4 on top of a device driver, the driver
requires to separate the the initialization process of the device in 2
stages:
- Init: This stage sets up the device driver (analogue to the current
  `xxx_setup` functions) and puts it in a state that minimizes power
  consumption.
- Start: This state is intended to be triggered during network interface
  initialization (e.g `ifconfig up`) to put the device in a state where is
  ready to operate (enable IRQ lines, turn on the transceiver, etc).

Explicitly separating the Init and Start process is more efficient in terms
of power consumption, because a network stack might take some time to set an
interface up.

The Radio HAL "start" function should be mapped to the Start stage proposed
above.

## 5.1.2 Explicit transceiver states
Following the IEEE802.15.4 PHY definition, there are only 3 transceiver states:

```c
typedef enum {
    IEEE802154_TRX_STATE_TRX_OFF, /**< the transceiver state if off */
    IEEE802154_TRX_STATE_RX_ON, /**< the transceiver is ready to receive packets */
    IEEE802154_TRX_STATE_TX_ON, /**< the transceiver is ready to send packets */
} ieee802154_trx_state_t;
```

The implementation of the HAL is responsible to map other states to the
PHY state. For instance, a radio in sleep mode or with the transceiver disabled
will map to the same `TRX_OFF` state.  This simplify a lot the operation and
implementation of the HAL.

Some functions of the radio API require that the transceiver is on a specific
state. This does not only allow the usage of upper layers that require total
control of the radio (OpenWSN) but also ensures that the radio is always in a
well known state since the HAL won't perform any hidden state change. It also
avoid unnecessary state changes.

For instance, functions to prepare and transmit packets require the radio to be
in `TX_ON` state.

The only exception is the "sleep" function (that sets the state to `TRX_OFF`).
There might be some flags in the future to allow the radio to change to a well
defined state after certain operations. E.g if the radio doesn't allow CSMA-CA
but provides Auto CCA, the radio should try to send data as soon as possible.
Some radios alreayd include this feature to automatically change to `TX_ON`
after CCA without an explicit call to the function to set the transceiver state.

## 5.3 Prepare and transmit
The Radio HAL defines separates the send procedure in preloading the frame
buffer and triggering the transmissions start. This is required for MAC layers
with timing contraints. (E.g TSCH mode of 802.15.4 MAC)

It's expected that a proper "send" function is defined by a higher layer (for
instance a 802.15.4 MAC or a helper function)

## 5.4 TX and RX info
All information associated to the reception of a packet (LQI, RSSI) as well
as the transmission information (if a device supports frame retransmissions) is
exposed to the user via the Radio HAL API.

However, requesting this information is optional. An application might not be
interested in the RSSI information or the TX status if unconfirmed MAC messages
are sent.

## 5.5 Thread Safety

The radio API is designed to be called from a single thread context. Thus,
the API is not thread safe. However, as long as the the API functions are
called sequencially, it would be possible to use any synchronization mechanism
to use the radio in a multi-thread environment.
The API guarantees that the event callback is invoked from the
process IRQ function of the Radio HAL, so this should be taken into
consideration when implementing a synchronization mechanism for a multi-thread
environment (e.g usage of recursive mutex).

## 5.6 802.15.4 Radio HAL definition

### 5.6.1 Radio Ops interface definition
The Radio Ops interface is implemented using vtables (function pointers)
Although this pattern can be implemented with switch-case, there some evidence
suggesting that for this case the vtable approach can generate more efficient
instructions and it's easier to write. 

See the Appendix to see the comparison betweem the switch-case and vtable
approach.

These functions should be implemented with device specific validations only.
Everything that's not device specific (valid channels, address length, etc)
should be checked by higher layers. This avoids redundant checks.

Note the Radio Ops interface has just a few getters. This is done on purpose,
because it's assumed that higher layers will already have a copy of the PIB and
MIB.

#### 5.6.1.1 Send/Receive related functions
```c
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
     * @return number of bytes written in @p buffer (0 if @p buf == NULL)
     * @return -ENOBUFS if the packet doesn't fit in @p
     */
    int (*read)(ieee802154_dev_t *dev, void *buf, size_t size,
                ieee802154_rx_info_t *info);
```

#### 5.6.1.2 PHY related functions
```c
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

```

#### 5.6.1.3 Device Management related functions
```c
    /**
     * @brief Set the sleep state of the device (sleep or awake)
     *
     * @param[in] dev IEEE802.15.4 device descriptor
     * @param[in] sleep whether the device should sleep or not.
     *
     * @post if @p sleep == true, the transceiver PHY state is
     *        @ref IEEE802154_TRX_STATE_TRX_OFF
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_sleep)(ieee802154_dev_t *dev, bool sleep);

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
```

#### 5.6.1.4 Caps related and optional functions
```c
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
     * @param[in] retries number of CSMA-CA retries. If @p retries < 0, 
     *                    retransmissions with CSMA-CA MUST be disabled.
     *
     * @return 0 on success
     * @return negative errno on error
     */
    int (*set_csma_params)(ieee802154_dev_t *dev, ieee802154_csma_be_t *bd,
                           int8_t retries);
```

### 5.6.2 Event Notification definition

The Event Notification mechanism is defined with a function callback that is
supposed to be implemented by the upper layer.

The callback signature, the events and their expected behavior are available
in the following block:

```c
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

/**
 * @brief Prototype of the IEEE802.15.4 device event callback
 *
 * @param[in] dev IEEE802.15.4 device descriptor
 * @param[in] status the status 
 */
typedef void (*ieee802154_cb_t)(ieee802154_dev_t *dev,
                                ieee802154_tx_status_t status);
```

Other MAC specific events are not included (e.g TX done with frame pending,
CSMA-CA medium busy or exceeded number of retranmissions). This can be
extracted on TX done event using the Radio HAL API.

It's safe to call the radio HAL API from the event callback function.

### 5.6.3 Radio capabilities
The following list defines the basic capabilities available in common
IEEE802.15.4 compliants radios. More caps can be added to support other
hardware features.

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

### 5.6.4 802.15.4 Radio Hal Device Descriptor
The 802.15.4 Device Descriptor simply holds the Radio Ops driver and the Event
Notification for an instance of a device.
It has the following structure:
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
of the internal device descriptor can be an `ieee802154_dev_t` struct, but other
techniques are also possible.

# 6 Measurements and comparison to netdev

## Footprint
TBD
## Speed
TBD

# Annex

## Feature Matrix
TBD
## Vtables vs FP
TBD
