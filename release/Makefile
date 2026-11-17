# SPDX-License-Identifier: ISC
#
# Builds the kaiHEN release zip:
#   bootstrap.bin, hbmenu.nro, ro/main, hen/main,
#   game/main                    nx-hbloader
#   programs/ounceshell/ounceshell.nro  OunceShell, the VitaShell port
#   licenses/                    the licenses of everything above
#
# The Dockerfile runs this with RELEASE=1 and sets the sources and commits:
#   make -f release/Makefile RELEASE_ZIP=FILE \
#       HBLOADER_URL=... HBLOADER_COMMIT=... HBMENU_URL=... HBMENU_COMMIT=... \
#       OUNCESHELL_URL=... OUNCESHELL_COMMIT=...
DEVKITPRO ?= /opt/devkitpro
RELEASE_ZIP ?= build/kaiHEN.zip
DEPS := build/release
HBLOADER := $(DEPS)/nx-hbloader
HBMENU := $(DEPS)/nx-hbmenu
OUNCESHELL := $(DEPS)/ounceshell
STAGE := $(DEPS)/zip
LICENSES := $(STAGE)/licenses
# devkitPro's Switch portlibs that hbmenu and OunceShell link, that
# ship a license file.
PORTLIB_LICENSES := $(DEVKITPRO)/portlibs/switch/licenses
PORTLIBS := bzip2 freetype libarchive libconfig libjpeg-turbo libpng libzstd \
	sdl2_image sdl2_ttf zlib

# Checks out one commit of a repository into a folder.
#   $(call checkout,DIR,URL,COMMIT)
define checkout
rm -rf $(1)
git init -q $(1)
git -C $(1) fetch -q --depth=1 $(2) $(3)
git -C $(1) checkout -q FETCH_HEAD
endef

# Copies the licenses of kaiHEN, the programs, and the libraries linked into
# them. The portlibs without a license file (physfs, harfbuzz, SDL 2, libwebp,
# expat, lz4, liblzma, libvorbis, libogg, Oniguruma) have theirs in
# release/licenses, from upstream at the installed versions.
define copy_licenses
	mkdir -p $(LICENSES)
	cp LICENSE $(LICENSES)/kaiHEN-GPL-2.0.txt
	cp libkaihen/LICENSE $(LICENSES)/libkaihen-ISC.txt
	cp $(HBLOADER)/LICENSE.md $(LICENSES)/nx-hbloader-ISC.md
	cp $(HBMENU)/LICENSE.md $(LICENSES)/nx-hbmenu-ISC.md
	cp $(HBMENU)/switch2/third_party/nanosvg/README.md $(LICENSES)/nanosvg-zlib.md
	cp $(OUNCESHELL)/LICENSE $(LICENSES)/ounceshell-GPL-3.0.txt
	cp $(OUNCESHELL)/switch2/third_party/ftpd/LICENSE $(LICENSES)/ftpd-Unlicense.txt
	cp $(OUNCESHELL)/switch2/resources/NotoSans-OFL.txt $(LICENSES)/notosans-OFL.txt
	cp $(DEVKITPRO)/libounce/licenses/* $(LICENSES)/
	cp -r $(DEVKITPRO)/portlibs/switch2/share/licenses/ouncevk $(LICENSES)/
	for lib in $(PORTLIBS); do \
	    cp -r $(PORTLIB_LICENSES)/switch-$$lib $(LICENSES)/$$lib; \
	done
	cp release/licenses/* $(LICENSES)/
endef

all: $(RELEASE_ZIP)

kaihen:
	$(MAKE) -f Makefile

$(HBLOADER)/build/clean/main:
	$(call checkout,$(HBLOADER),$(HBLOADER_URL),$(HBLOADER_COMMIT))
	$(MAKE) -C $(HBLOADER)/switch2 -f Makefile.clean \
	    KAIHEN_INCLUDE=$(CURDIR)/libkaihen/include

$(HBMENU)/build/nro/nro/hbmenu.nro:
	$(call checkout,$(HBMENU),$(HBMENU_URL),$(HBMENU_COMMIT))
	$(MAKE) -C $(HBMENU) -f switch2/Makefile

$(OUNCESHELL)/build/switch2/ounceshell.nro:
	$(call checkout,$(OUNCESHELL),$(OUNCESHELL_URL),$(OUNCESHELL_COMMIT))
	$(MAKE) -C $(OUNCESHELL) -f switch2/Makefile

$(RELEASE_ZIP): kaihen $(HBLOADER)/build/clean/main \
        $(HBMENU)/build/nro/nro/hbmenu.nro $(OUNCESHELL)/build/switch2/ounceshell.nro
	rm -rf $(STAGE)
	mkdir -p $(STAGE)/ro $(STAGE)/hen $(STAGE)/game $(STAGE)/programs/ounceshell
	cp bootstrap/build/bootstrap.bin $(STAGE)/bootstrap.bin
	cp $(HBMENU)/build/nro/nro/hbmenu.nro $(STAGE)/hbmenu.nro
	cp ro/build/main $(STAGE)/ro/main
	cp hen/build/main $(STAGE)/hen/main
	cp $(HBLOADER)/build/clean/main $(STAGE)/game/main
	cp $(OUNCESHELL)/build/switch2/ounceshell.nro $(STAGE)/programs/ounceshell/
	$(copy_licenses)
	mkdir -p $(dir $(abspath $@))
	rm -f $@
	cd $(STAGE) && zip -q -r -X $(abspath $@) .

.PHONY: all kaihen
