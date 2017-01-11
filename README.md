# Instructions

Clone the repo:

```
git clone git@github.com:madmonkey1907/hakchi.git
cd hakchi
git submodule init
git submodule update
```

## Install the build dependencies

### Ubuntu 16.04


```
sudo apt-get install libusb-1.0-0-dev libqt4-dev libudev-dev upx-ucl
```


### macOS

```
brew install libusb qt wget upx
brew link qt --force
```

# Build

```
patch ./3rdparty/sunxi-tools/fel_lib.c < ./3rdparty/sunxi-tools.diff
make
```
