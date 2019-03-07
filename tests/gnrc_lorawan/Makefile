APPLICATION += gnrc_lorawan

BOARD ?= b-l072z-lrwan1

RIOTBASE = $(CURDIR)/../../

USEMODULE += od
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += xtimer
USEMODULE += random
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += gnrc_lorawan
USEMODULE += gnrc_pktbuf_cmd
USEMODULE += gnrc_netapi_callbacks
USEMODULE += netdev_layer
USEMODULE += gnrc_neterr
USEMODULE += gnrc_netreg
USEMODULE += hashes
USEMODULE += crypto

# Turn on developer helpers
DEVELHELP ?= 1

DRIVER ?= sx1276

# use SX1276 by default
USEMODULE += $(DRIVER)

# Required for the cipher module */
CFLAGS += -DCRYPTO_AES

########################### COMPILE TIME CONFIGURATION ########################
# NOTE: The following options can be configured on runtime as well using
# `ifconfig`

# OTAA compile time configuration keys
CFLAGS += -DLORAMAC_APP_KEY_DEFAULT=\{0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\}
CFLAGS += -DLORAMAC_APP_EUI_DEFAULT=\{0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\}
CFLAGS += -DLORAMAC_DEV_EUI_DEFAULT=\{0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\}

# Uncomment and replace with proper keys for joining with ABP
#CFLAGS += -DLORAMAC_DEV_ADDR_DEFAULT=\{0x00\,0x00\,0x00\,0x00\}
#CFLAGS += -DLORAMAC_NWK_SKEY_DEFAULT=\{0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\}
#CFLAGS += -DLORAMAC_APP_SKEY_DEFAULT=\{0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\,0x00\}

# Comment/uncomment as necessary
CFLAGS += -DLORAMAC_DEFAULT_JOIN_PROCEDURE=LORAMAC_JOIN_OTAA
#CFLAGS += -DLORAMAC_DEFAULT_JOIN_PROCEDURE=LORAMAC_JOIN_ABP

# Set to the highest DR for the EU868 in order to maximize the throughput
CFLAGS += -DLORAMAC_DEFAULT_DR=LORAMAC_DR_5

# Set the default RX2 datarate to DR3 (used by The Things Network)
CFLAGS += -DLORAMAC_DEFAULT_RX2_DR=LORAMAC_DR_3

# Set default messages to unconfirmable
CFLAGS += -DLORAMAC_DEFAULT_TX_MODE=LORAMAC_TX_UNCNF

include $(RIOTBASE)/Makefile.include
