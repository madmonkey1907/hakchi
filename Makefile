all: bin/sunxi-fel bin/mkbootimg bin/unpackbootimg mod/bin/busybox build/macdylibbundler build/hakchi-gui bin/sntool

clean:
	@rm -rf bin/sunxi-fel bin/mkbootimg bin/unpackbootimg bin/sntool
	@rm -rf build/
	@make -C 3rdparty/sunxi-tools clean
	@make -C 3rdparty/mkbootimg clean

bin/sunxi-fel: 3rdparty/sunxi-tools/sunxi-fel
	@cp $< $@

3rdparty/sunxi-tools/sunxi-fel: 3rdparty/sunxi-tools/fel.c
	@make -C 3rdparty/sunxi-tools sunxi-fel

bin/mkbootimg: 3rdparty/mkbootimg/mkbootimg
	@cp $< $@

3rdparty/mkbootimg/mkbootimg: 3rdparty/mkbootimg/mkbootimg.c
	@make -C 3rdparty/mkbootimg

bin/unpackbootimg: 3rdparty/mkbootimg/unpackbootimg
	@cp $< $@

3rdparty/mkbootimg/unpackbootimg: 3rdparty/mkbootimg/unpackbootimg.c
	@make -C 3rdparty/mkbootimg

mod/bin/busybox: 3rdparty/busybox.url
	@if [ ! -f $@ ]; then wget --no-use-server-timestamps $(shell cat $<) -O $@ && chmod +x $@ && upx -qq --best $@; else touch $@; fi

build/hakchi-gui: build/Makefile hakchi-gui/src/*
	@make -C build
ifndef SYSTEMROOT
ifeq ($(shell uname), Darwin)
	sh fix_mac_app_bundle.sh
endif
endif

build/macdylibbundler: 3rdparty/macdylibbundler/*
ifndef SYSTEMROOT
ifeq ($(shell uname), Darwin)
	@make -C 3rdparty/macdylibbundler/
endif
endif

build/Makefile: hakchi-gui/hakchi-gui.pro
	@mkdir -p build && (cd build; qmake ../$< CONFIG+=release)

bin/sntool: sntool/sntool.cpp
	@g++ -I3rdparty/sunxi-tools -std=gnu++11 -Wall -Wextra $< -o $@