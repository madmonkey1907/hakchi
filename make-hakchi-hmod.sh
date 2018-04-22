#!/bin/bash -e

rm -rf "./hakchi-hmod/bin/"
rm -rf "./hakchi-hmod/etc/"
rsync -ac "./mod/hakchi/rootfs/" "./hakchi-hmod/"
rsync -ac "./mod/bin/" "./hakchi-hmod/bin/"
rsync -ac "./kernel-hmod/bin/" "./hakchi-hmod/bin/"
makepack "hakchi-hmod"
mv -f "hakchi-hmod.hmod.tgz" "hakchi.hmod"
rm -rf "./hakchi-hmod/bin/"
rm -rf "./hakchi-hmod/etc/"
echo "done"
