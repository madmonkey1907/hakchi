all: bin/sunxi-fel bin/mkbootimg bin/unpackbootimg mod/bin/busybox build/hakchi-gui bin/sntool

bin/sunxi-fel: 3rdparty/sunxi-tools/sunxi-fel
	@cp 3rdparty/sunxi-tools/sunxi-fel bin/

3rdparty/sunxi-tools/sunxi-fel: 3rdparty/sunxi-tools/fel.c
	@make -C 3rdparty/sunxi-tools sunxi-fel

bin/mkbootimg: 3rdparty/mkbootimg/mkbootimg
	@cp 3rdparty/mkbootimg/mkbootimg bin/

3rdparty/mkbootimg/mkbootimg: 3rdparty/mkbootimg/mkbootimg.c
	@make -C 3rdparty/mkbootimg

bin/unpackbootimg: 3rdparty/mkbootimg/unpackbootimg
	@cp 3rdparty/mkbootimg/unpackbootimg bin/

3rdparty/mkbootimg/unpackbootimg: 3rdparty/mkbootimg/unpackbootimg.c
	@make -C 3rdparty/mkbootimg

mod/bin/busybox: 3rdparty/busybox.url
	wget --no-use-server-timestamps $(shell cat $<) -O $@ && chmod +x $@ && upx -qq --best $@

build/hakchi-gui: build/Makefile hakchi-gui/src/*
	@make -C build

build/Makefile: hakchi-gui/hakchi-gui.pro
	@mkdir -p build && (cd build; qmake ../$< CONFIG+=release)

bin/sntool: sntool/sntool.cpp
	@g++ -std=gnu++11 -Wall -Wextra $< -o $@
