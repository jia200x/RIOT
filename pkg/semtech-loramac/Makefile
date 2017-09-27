PKG_NAME=semtech-loramac
PKG_URL=git://github.com/Lora-net/LoRaMac-node.git
PKG_VERSION=263d611b4f82da117301027c6feb1bc4443e6efe
PKG_BUILDDIR ?= $(BINDIRBASE)/pkg/$(BOARD)/$(PKG_NAME)

.PHONY: all

all: git-download
	$(MAKE) -C $(PKG_BUILDDIR)

include $(RIOTBASE)/pkg/pkg.mk
