PKG_NAME=gnrc_lorawan
PKG_SOURCE_LOCAL=$(RIOTBASE)/../gnrc_lorawan
PKG_LICENSE=MIT

.PHONY: all ..cmake_version_supported

all: git-download
	"$(MAKE)" -C $(PKG_BUILDDIR)/src

include $(RIOTBASE)/pkg/pkg.mk
