# name of your application
APPLICATION = opendsme_example

# If no BOARD is found in the environment, use this default:
BOARD ?= nrf52840dk

# Whitelist nrf802154-based boards.
BOARD_WHITELIST := nrf52dk nrf52840dk nrf52840dongle

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# Uncomment these lines if you want to use platform support from external
# repositories:
#RIOTCPU ?= $(CURDIR)/../../../thirdparty_cpu
#EXTERNAL_BOARD_DIRS ?= $(CURDIR)/../../../thirdparty_boards

# Uncomment this to enable scheduler statistics for ps:
#CFLAGS += -DSCHEDSTATISTICS

# If you want to use native with valgrind, you should recompile native
# with the target all-valgrind instead of all:
# make -B clean all-valgrind

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += ps
USEMODULE += shell
USEMODULE += shell_commands

USEPKG += opendsme
USEMODULE += auto_init_gnrc_netif
USEMODULE += l2util
USEMODULE += gnrc_pktdump
USEMODULE += gnrc_txtsnd

USEMODULE += netdev_default
CFLAGS += -DCONFIG_IEEE802154_CCA_THRESH_DEFAULT=-30

include $(RIOTBASE)/Makefile.include
