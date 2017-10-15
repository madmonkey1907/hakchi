# Instructions

Clone the repo:

```
git clone git@github.com:madmonkey1907/hakchi.git
cd hakchi
git submodule init
git submodule update
```

## Install the build dependencies

### macOS 10.12

If you don't already have homebrew installed on your Mac:

```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

The actual build dependencies:

```
brew install libusb qt wget upx lzop
brew link qt --force
```

### Ubuntu 16.04

```
sudo apt-get install libusb-1.0-0-dev libqt4-dev upx-ucl cpio lzop wget
```

# Build

```
make
```

# Execute

## Linux

```
. importpath
hakchi-gui
```

## macOS

```
open build/hakchi-gui.app
```
