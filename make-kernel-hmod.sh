#!/bin/sh -e
makeimg kernel notx
ksize="$(stat -c%s "kernel.img")"
kmaxsize="$((4*1024*1024))"
ksizestr="$(($kmaxsize-$ksize)) ($((($kmaxsize-$ksize)/1024))k)"
[ $ksize -le $kmaxsize ] || (echo "too big! $ksizestr" 1>&2; exit 1)
echo "$ksizestr"
rsync -ac "kernel.img" "kernel-hmod/boot/boot.img"
rsync -ac "mod/hakchi/rootfs/etc/preinit.d/b0050_boot" "kernel-hmod/etc/preinit.d/b0050_boot"
mkdir -p "kernel-hmod/lib/modules"
rsync -ac "data/modules/" "kernel-hmod/lib/modules/" --delete

bootVersion="v1.0.0"
eval "$(grep -F "bootVersion=" "mod/hakchi/init")"
#hakchiVersion="$(./updateVersion.sh -s)"
eval "$(grep -F "hakchiVersion=" "mod/hakchi/rootfs/etc/preinit.d/b0000_defines")"
kernelVersion="$(ls "kernel-hmod/lib/modules")"
set | grep -F "Version" | sort > "kernel-hmod/var/version"

rm -f hakchi-v*.hmod
makepack "kernel-hmod/"
mv "kernel-hmod.hmod" "hakchi-$hakchiVersion.hmod"
rsync -avc "hakchi-$hakchiVersion.hmod" "hakchi:/var/www/hakchi/"
rsync -avc "hakchi-$hakchiVersion.hmod" "snes:/tmp/boot/"
rsync -avc "kernel.img" "snes:/tmp/boot/"
echo "done"
