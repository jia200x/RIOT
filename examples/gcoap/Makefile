# Default Makefile, for host native GNRC-based networking

# name of your application
APPLICATION = gcoap_example

# If no BOARD is found in the environment, use this default:
BOARD ?= nrf52840dk

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
#USEMODULE += netdev_default

# use GNRC by default
LWIP ?= 0

ifeq (0,$(LWIP))
  USEMODULE += auto_init_gnrc_netif
  # Specify the mandatory networking modules
  USEMODULE += gnrc_ipv6_default
  # Additional networking modules that can be dropped if not needed
  USEMODULE += gnrc_icmpv6_echo
  USEPKG += opendsme
USEMODULE += l2util
USEMODULE += event_thread_lowest
USEMODULE += iolist
USEMODULE += sx1276
else
  USEMODULE += lwip_ipv6
  USEMODULE += lwip_netdev
endif
#CFLAGS += -DCONFIG_IEEE802154_DSME_SUPERFRAME_ORDER=10
#CFLAGS += -DCONFIG_IEEE802154_DSME_MULTISUPERFRAME_ORDER=10
#CFLAGS += -DCONFIG_IEEE802154_DSME_BEACON_ORDER=10
CFLAGS += -DCONFIG_IEEE802154_DEFAULT_CSMA_CA_MIN_BE=3
CFLAGS += -DCONFIG_IEEE802154_DEFAULT_CSMA_CA_MAX_BE=5
CFLAGS += -DCONFIG_IEEE802154_CCA_THRESH_DEFAULT=-40
CFLAGS += -DCONFIG_IEEE802154_DSME_GTS_EXPIRATION=255
export PKG_SOURCE_LOCAL_OPENDSME=/home/jialamos/Development/opendsme

#CFLAGS += -DSTATISTICS_MONITOR_LATENESS

# RIOT Configs
CFLAGS += -DDEBUG_ASSERT_VERBOSE

CFLAGS += -DISR_STACKSIZE=1024
USEMODULE += netdev_ieee802154

USEMODULE += gcoap

# Required by gcoap example
USEMODULE += od
USEMODULE += fmt
USEMODULE += netutils
# Add also the shell, some shell commands
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += ethos
# include UHCP client
USEMODULE += gnrc_uhcpc

# ethos baudrate can be configured from make command
ETHOS_BAUDRATE ?= 115200
CFLAGS += -DETHOS_BAUDRATE=$(ETHOS_BAUDRATE)

STATIC_ROUTES ?= 1

ifeq (1,$(USE_DHCPV6))
  FLAGS_EXTRAS=--use-dhcpv6
endif

# Configure terminal parameters
TERMDEPS += host-tools
TERMPROG ?= sudo sh $(RIOTTOOLS)/ethos/start_network.sh
TERMFLAGS ?= $(FLAGS_EXTRAS) $(PORT) $(TAP) $(IPV6_PREFIX) $(ETHOS_BAUDRATE)

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Instead of simulating an Ethernet connection, we can also simulate
# an IEEE 802.15.4 radio using ZEP
USE_ZEP ?= 0

# set the ZEP port for native
ZEP_PORT_BASE ?= 17754
ifeq (1,$(USE_ZEP))
  TERMFLAGS += -z [::1]:$(ZEP_PORT_BASE)
  USEMODULE += socket_zep

  ifneq (,$(ZEP_MAC))
    TERMFLAGS += --eui64=$(ZEP_MAC)
  endif
endif
#
# Ethos/native TAP interface and UHCP prefix can be configured from make command
TAP ?= tap0
ifeq (1,$(USE_DHCPV6))
  # with DHCPv6 the 64-bit delegated prefixes are generated from a shorter
  # configured prefix.
  IPV6_PREFIX ?= 2001:db8::/32
else
  # UHCP advertises the prefix as is.
  IPV6_PREFIX ?= 2001:db8::/64
endif

include $(RIOTBASE)/Makefile.include

# For now this goes after the inclusion of Makefile.include so Kconfig symbols
# are available. Only set configuration via CFLAGS if Kconfig is not being used
# for this module.
ifndef CONFIG_KCONFIG_MODULE_GCOAP
## Uncomment to redefine port, for example use 61616 for RFC 6282 UDP compression.
#GCOAP_PORT = 5683
#CFLAGS += -DCONFIG_GCOAP_PORT=$(GCOAP_PORT)

## Uncomment to redefine request token length, max 8.
#GCOAP_TOKENLEN = 2
#CFLAGS += -DCONFIG_GCOAP_TOKENLEN=$(GCOAP_TOKENLEN)

# Increase from default for confirmable block2 follow-on requests
GCOAP_RESEND_BUFS_MAX ?= 2
CFLAGS += -DCONFIG_GCOAP_RESEND_BUFS_MAX=$(GCOAP_RESEND_BUFS_MAX)
endif

.PHONY: host-tools

host-tools:
	$(Q)env -u CC -u CFLAGS $(MAKE) -C $(RIOTTOOLS)

# define native specific targets to only run UHCP daemon when required
ifneq (,$(filter native,$(BOARD)))
ifneq (1,$(USE_DHCPV6))
.PHONY: uhcpd-daemon

uhcpd-daemon: host-tools
	$(RIOTTOOLS)/uhcpd/bin/uhcpd $(TAP) $(IPV6_PREFIX) &
endif
endif
