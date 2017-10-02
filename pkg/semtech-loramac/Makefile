PKG_NAME=semtech-loramac
PKG_URL=git://github.com/Lora-net/LoRaMac-node.git
PKG_VERSION=f42be67be402a40b3586724800771bfe13fb18e6
PKG_BUILDDIR ?= $(BINDIRBASE)/pkg/$(BOARD)/$(PKG_NAME)

.PHONY: all

all: git-download
	$(MAKE) -C $(PKG_BUILDDIR)

include $(RIOTBASE)/pkg/pkg.mk
