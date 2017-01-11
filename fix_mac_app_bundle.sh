#!/bin/bash

BINARY=build/hakchi-gui.app
FRAMEW_FOLDER=$BINARY/Contents/Frameworks/
DYLIBBUNDLER=3rdparty/macdylibbundler/dylibbundler

$DYLIBBUNDLER -x $BINARY/Contents/MacOS/hakchi-gui -b -cd -d $BINARY/Contents/libs

if [ ! -d "$FRAMEW_FOLDER" ]; then
    mkdir $FRAMEW_FOLDER
fi

cp -rf bin $BINARY/Contents/MacOS/bin
cp -rf udev $BINARY/Contents/MacOS/udev
cp -rf mod $BINARY/Contents/MacOS/mod
cp -rf 3rdparty $BINARY/Contents/MacOS/3rdparty

curl -o icon.png http://www.iconeasy.com/icon/png/Game/Nes/Nes%20Console.png

replace_icon(){
    droplet=$1
    icon=$2
    if [[ $icon =~ ^https?:// ]]; then
        curl -sLo /tmp/icon $icon
        icon=/tmp/icon
    fi
    rm -rf $droplet$'/Icon\r'
    sips -i $icon >/dev/null
    DeRez -only icns $icon > /tmp/icns.rsrc
    Rez -append /tmp/icns.rsrc -o $droplet$'/Icon\r'
    SetFile -a C $droplet
    SetFile -a V $droplet$'/Icon\r'
}

replace_icon $BINARY icon.png

# clean up.
rm icon.png
rm /tmp/icns.rsrc

plutil -replace CFBundleGetInfoString -string "© madmonkey1907" $BINARY/Contents/Info.plist

chmod +x $BINARY/Contents/MacOS/hakchi-gui
chmod +x $BINARY/Contents/MacOS/bin/*

mv $BINARY "build/Hakchi GUI.app"

exit 0