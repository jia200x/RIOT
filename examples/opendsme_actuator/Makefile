# name of your application
APPLICATION = opendsme_example

# If no BOARD is found in the environment, use this default:
BOARD ?= b-l072z-lrwan1

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
USEMODULE += gnrc_pktbuf_malloc
#USEMODULE += event_thread
USEMODULE += cpp11-compat
USEMODULE += netdev_default
USEMODULE += ieee802154
USEMODULE += od
USEMODULE += luid
USEMODULE += shell
USEMODULE += l2util
USEMODULE += gnrc
USEMODULE += checksum
USEMODULE += gnrc_netif
USEMODULE += gnrc_netif_events
#USEMODULE += shell_commands
#USEMODULE += heap_cmd

ifeq (1,$(STDIO_NULL))
USEMODULE += stdio_null
endif

COORD ?= 1
CFLAGS += -DCOORD=$(COORD)

#CFLAGS += -DEVENT_THREAD_MEDIUM_STACKSIZE=2000
CFLAGS += -DTHREAD_STACKSIZE_MAIN=700
CFLAGS += -DGNRC_PKTDUMP_STACKSIZE=700
CFLAGS += -DEVENT_THREAD_MEDIUM_PRIO=EVENT_THREAD_HIGHEST_PRIO
CFLAGS += -DCONFIG_IEEE802154_AUTO_ACK_DISABLE=1
CFLAGS += -DCONFIG_GNRC_PKTBUF_SIZE=1024
#CFLAGS += -DDEBUG_ASSERT_VERBOSE
USEMODULE += gnrc_netapi_callbacks

CFLAGS += -DNDEBUG -DLOG_LEVEL=LOG_NONE

CFLAGS += -DMAXTHREADS=4 -DSCHED_PRIO_LEVELS=4
CFLAGS += -DSHELL_NO_ECHO=1
CFLAGS += -DSHELL_NO_PROMPT=1
CFLAGS += -DAUTO_INIT_LED0=1

#CFLAGS += -DPM_BLOCKER_INITIAL=0x00000100

CFLAGS += -DCONFIG_OPENDSME_USE_CAP=0
CFLAGS += -DCONFIG_DSME_PLATFORM_STATIC_GTS=1

CFLAGS += -DCONFIG_DSME_PLATFORM_ACK_REQ=1
CFLAGS += -DCONFIG_OPENDSME_PAYLOAD_LENGTH=16
CFLAGS += -DUSE_CAD=1
CFLAGS += -DCAP_REDUCTION=0
CFLAGS += -DCONFIG_DSME_PLATFORM_SF_PER_MSF=1 

export PKG_SOURCE_LOCAL_OPENDSME = /home/jialamos/Development/opendsme

include $(RIOTBASE)/Makefile.include
