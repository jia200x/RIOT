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

DEVELHELP ?= 1

DRIVER ?= sx1276

# use SX1276 by default
USEMODULE += $(DRIVER)

USEMODULE += hashes
USEMODULE += crypto
CFLAGS += -DCRYPTO_AES -DDEBUG_ASSERT_VERBOSE

INCLUDES += $(CURDIR)

include $(RIOTBASE)/Makefile.include
