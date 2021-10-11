# name of your application
APPLICATION = opendsme_example

# If no BOARD is found in the environment, use this default:
BOARD ?= native

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

# Features required
FEATURES_REQUIRED += cpp # basic C++ support
FEATURES_REQUIRED += libstdcpp # libstdc++ support (for #include <cstdio>)

# If you want to add some extra flags when compile c++ files, add these flags
# to CXXEXFLAGS variable
CXXEXFLAGS += -Wno-unused-parameter -Wno-pedantic -Wno-deprecated-copy -Wno-missing-field-initializers -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-unused-variable -Wno-reorder -Wno-address -Wno-sign-compare -Wno-unused-function -Wno-error
USEPKG += opendsme
USEMODULE += ztimer_msec
USEMODULE += gnrc_pktbuf
USEMODULE += event_thread
USEMODULE += cpp11-compat
USEMODULE += netdev_default
USEMODULE += ieee802154
USEMODULE += od
USEMODULE += luid
USEMODULE += shell
USEMODULE += l2util
USEMODULE += gnrc_pktdump
USEMODULE += gnrc
CFLAGS += -DEVENT_THREAD_MEDIUM_STACKSIZE=4096
CFLAGS += -DEVENT_THREAD_MEDIUM_PRIO=EVENT_THREAD_HIGHEST_PRIO
CFLAGS += -DCONFIG_IEEE802154_AUTO_ACK_DISABLE=1
CFLAGS += -DDEBUG_ASSERT_VERBOSE

export PKG_SOURCE_LOCAL_OPENDSME = /home/jialamos/Development/opendsme

include $(RIOTBASE)/Makefile.include
