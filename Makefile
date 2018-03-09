all: bin/sunxi-fel bin/mkbootimg bin/unpackbootimg mod/bin/busybox build/hakchi-gui bin/sntool

ifndef SYSTEMROOT
ifeq ($(shell uname), Darwin)
build/hakchi-gui: build/macdylibbundler
endif
endif

.PHONY: all clean patch unpatch submodule

clean: unpatch
	@cd bin && rm -rf sunxi-fel mkbootimg unpackbootimg sntool
	@rm -rf build/
	@$(MAKE) -C 3rdparty/sunxi-tools clean
	@$(MAKE) -C 3rdparty/mkbootimg clean

patch:
	@cd 3rdparty/sunxi-tools && if git diff --quiet; then git apply ../sunxi-tools.diff; fi

unpatch:
	@cd 3rdparty/sunxi-tools && git checkout .

submodule:
	@git submodule update --init

bin/sunxi-fel: 3rdparty/sunxi-tools/sunxi-fel
	@cp $< $@

3rdparty/sunxi-tools/sunxi-fel: 3rdparty/sunxi-tools/fel.c
	@$(MAKE) -C $(<D) $(@F)

bin/mkbootimg: 3rdparty/mkbootimg/mkbootimg
	@cp $< $@

3rdparty/mkbootimg/mkbootimg: 3rdparty/mkbootimg/mkbootimg.c
	@$(MAKE) -C $(<D)

bin/unpackbootimg: 3rdparty/mkbootimg/unpackbootimg
	@cp $< $@

3rdparty/mkbootimg/unpackbootimg: 3rdparty/mkbootimg/unpackbootimg.c
	@$(MAKE) -C $(<D)

mod/bin/busybox: 3rdparty/busybox.url
	@if [ ! -x $@ ]; then mkdir -p $(@D); if wget --no-use-server-timestamps $(shell cat $<) -O $@; then chmod +x $@ && upx -qq --best $@; else rm -rf $@; fi; else touch $@; fi

build/hakchi-gui: build/Makefile hakchi-gui/src/* patch
	@$(MAKE) -C build
ifndef SYSTEMROOT
ifeq ($(shell uname), Darwin)
	@sh fix_mac_app_bundle.sh
endif
endif

build/Makefile: hakchi-gui/hakchi-gui.pro
	@mkdir -p build && (cd build; qmake ../$< CONFIG+=release)

build/macdylibbundler: 3rdparty/macdylibbundler/*
	@$(MAKE) -C $(<D)

bin/sntool: sntool/sntool.cpp
	@$(CROSS_COMPILE)g++ -I3rdparty/sunxi-tools -I3rdparty/mkbootimg -std=gnu++11 -Wall -Wextra $< -o $@
