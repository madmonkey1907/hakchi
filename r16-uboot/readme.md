1. get r16-uboot-fc3061df4dbd4153819b2d2f141d82b88fea51cf.tar.gz from http://data.nintendo.co.jp/oss/NintendoEntertainmentSystemNESClassicEdition_OSS.zip
2. apply patch
3. ./build.sh
4. use sntool check -f to set checksum in fes1.bin
5. use sntool split/join to cut script.bin from stock uboot and paste into freshly compiled one
