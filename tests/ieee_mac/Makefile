APPLICATION += gnrc_lorawan

#BOARD ?= b-l072z-lrwan1
BOARD ?= native

RIOTBASE = $(CURDIR)/../../

USEMODULE += event_callback
USEMODULE += xtimer

DEVELHELP ?= 1

DRIVER ?= sx1276

# use SX1276 by default
#USEMODULE += $(DRIVER)

USEMODULE += hashes
USEMODULE += crypto
CFLAGS += -DCRYPTO_AES -DDEBUG_ASSERT_VERBOSE

INCLUDES += $(CURDIR)

include $(RIOTBASE)/Makefile.include
