#!/bin/sh
checkFile(){
  [ -f "$1" ] && return 0
  echo "Not Found: $1"
  return 1
}

scriptPath="$(dirname "$0")"
cd "$scriptPath"
scriptPath="$(pwd)"
sd="$scriptPath/sd"
sdTemp="$sd/temp"
sdImg="$sd.img"
sdFs="$sdTemp/data.fat32"
squashTemp="$sdTemp/squash"
updateTemp="$scriptPath/kernel-hmod/sd"
squashFile="$sdTemp/squash.hsqs"
fsRoot="$squashTemp/fsroot"


if [ "$1" = "" ]; then
  make sd
  exit $?
fi

checkFile "$sd/boot0.bin" || exit 2
checkFile "$sd/uboot.bin" || exit 3

rm -r "kernel.img" "$sd/out" "sd.img.xz" "$updateTemp"
export PATH="$PATH:$scriptPath/bin:$scriptPath/build"
./make-kernel-hmod.sh norsync || exit 4
originalCmdLine="$(cat "kernel/kernel.img-cmdline")"
sed -i -E 's#root=/dev/nandb decrypt#root=/dev/loop2 waitfor=/dev/mmcblk0p1 sdboot#' kernel/kernel.img-cmdline
sed -i -E 's# hakchi-(clover)?shell##' kernel/kernel.img-cmdline
makeimg ./kernel notx noshell || exit 5
cat "kernel/kernel.img-cmdline"
echo "$originalCmdLine" > "kernel/kernel.img-cmdline"

echo $sdTemp
[ -d "$sdTemp" ] && (rm -r "$sdTemp" || exit 6)
mkdir -p "$sdTemp" "$updateTemp" "$squashTemp" "$fsRoot" || exit 7
dd if=/dev/zero "of=$sdImg" bs=1M count=128 || exit 8
dd if=/dev/zero "of=$sdFs" bs=1M count=96 || exit 9

sfdisk "$sdImg" <<EOT
32M,,06
EOT

[ "$?" != "0" ] && exit 10

mkfs.vfat -F 16 -s 16 "$sdFs" || exit 11
mkdir -p "$fsRoot/hakchi/rootfs/bin" || exit 12
rsync -ac "$scriptPath/mod/hakchi/rootfs/" "$fsRoot/hakchi/rootfs" || exit 13
rsync -ac "$scriptPath/mod/bin/" "$fsRoot/hakchi/rootfs/bin" || exit 14
mkdir -p "$fsRoot/hakchi/rootfs/lib/modules/"


rsync -ac "$scriptPath/data/modules/" "$fsRoot/hakchi/rootfs/lib/modules/" || exit 14
rsync -ac --links \
  "$scriptPath/kernel-hmod/bin" \
  "$scriptPath/kernel-hmod/etc" \
  "$scriptPath/kernel-hmod/var" \
  "$fsRoot/hakchi/rootfs/" || exit 14
busyboxlist="$(cd "kernel/initramfs/bin" && qemu-arm-static -L ".." "./busybox" --list)" || echo 14
rm "$fsRoot/hakchi/rootfs/bin/sh"
(cd "$fsRoot/hakchi/rootfs/bin/" && (echo "$busyboxlist" | xargs -rn1 ln -s "busybox")) || exit 14

if [ -d "mod/hakchi/transfer" ]; then
  mkdir -p "$fsRoot/hakchi/transfer" || exit 15
  rsync -ac "mod/hakchi/transfer/" "$fsRoot/hakchi/transfer/" || exit 16
fi

rsync -ac --links "$sd/fs/" "$fsRoot/" || exit 17
chmod -R a+rw "$fsRoot" || exit 18
chmod -R a+x "$fsRoot/hakchi/rootfs/bin" "$fsRoot/hakchi/rootfs/etc/init.d" || exit 19
mkdir -p "$squashTemp/hakchi/" || exit 20
find "$fsRoot"

cp "3rdparty/util-linux-2.31.1/sfdisk.static" "$squashTemp/sfdisk" || exit 21
chmod a+x "$squashTemp/sfdisk" || exit 24

rsync -ac --links "$sd/squash/" "$squashTemp/" || exit 25
mksquashfs "$squashTemp" "$squashFile" -all-root || exit 26

printf "hakchi\n%s\n" "$(cat "$scriptPath/kernel-hmod/var/version")" | dd "of=$sdImg" conv=notrunc || exit 28
dd "if=$sdFs" "of=$sdImg" bs=1M seek=32 conv=notrunc || exit 29
dd "if=$sd/boot0.bin" "of=$sdImg" bs=1K seek=8 conv=notrunc || exit 30
dd "if=$sd/uboot.bin" "of=$sdImg" bs=1K seek=19096 conv=notrunc || exit 31
dd "if=$scriptPath/kernel.img" "of=$sdImg" bs=1K seek=20480 conv=notrunc || exit 32
dd "if=$squashFile" "of=$sdImg" bs=1K seek=40 conv=notrunc || exit 33
dd "if=$sdFs" "of=$sdImg" bs=1M seek=32 conv=notrunc || exit 34

cp "$sd/boot0.bin" "$sd/uboot.bin" "$scriptPath/kernel.img" "$squashFile" "$updateTemp/" || exit 35

cd "$scriptPath"
./make-kernel-hmod.sh norsync

mkdir -p "$sd/out/" || exit 36
mv "$sdImg" "$sd/out/sd.img" || exit 37
#rm -r "kernel" "kernel.img" || exit 39
#rm -r "$sdTemp" || exit 40
