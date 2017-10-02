APPLICATION = loramac

FEATURES_REQUIRED = periph_spi periph_gpio

BOARD ?= samr21-xpro

CFLAGS += -DUSE_BAND_915_HYBRID
CFLAGS += -DCRYPTO_AES
CFLAGS += -DDEVELHELP
CFLAGS += -DDEBUG_ASSERT_VERBOSE

RIOTBASE ?= $(CURDIR)/../..

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
