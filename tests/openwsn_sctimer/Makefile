APPLICATION = openwsn_sctimer
include ../Makefile.tests_common

USEPKG += openwsn
USEMODULE += at86rf2xx

CFLAGS += -DTHREAD_STACKSIZE_MAIN=10240
CFLAGS += -DTHREAD_EXTRA_STACKSIZE_PRINTF=4096
CFLAGS += -DDEVELHELP

WERROR = 0

include $(RIOTBASE)/Makefile.include