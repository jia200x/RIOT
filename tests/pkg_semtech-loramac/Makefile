APPLICATION = pkg_semtech-loramac
include ../Makefile.tests_common

BOARD ?= b-l072z-lrwan1

RIOTBASE ?= $(CURDIR)/../..

CFLAGS += -DUSE_BAND_868
CFLAGS += -DCRYPTO_AES
CFLAGS += -DDEVELHELP
CFLAGS += -DDEBUG_ASSERT_VERBOSE
CFLAGS += -DREGION_EU868

FEATURES_REQUIRED += periph_spi
FEATURES_REQUIRED +=periph_gpio

USEPKG += semtech-loramac

DRIVER ?= sx1276

# include the selected driver
USEMODULE += $(DRIVER)
USEMODULE += od
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += xtimer
USEMODULE += loramac_contrib
USEMODULE += loramac_mac
USEMODULE += loramac_mac_region
USEMODULE += loramac_crypto
USEMODULE += random

include $(RIOTBASE)/Makefile.include
