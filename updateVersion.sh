#!/bin/bash -e

#exec 1>/dev/null 2>&1

branch="$(git rev-parse --abbrev-ref HEAD)"
latesttag="$(git describe --tags --abbrev=0)"
shorthash="$(git log --pretty=format:'%h' -n1)"
revcount="$(git log --oneline | wc -l)"

VERSION="[$branch]$latesttag-$revcount($shorthash)"
echo "$VERSION"

cd "./mod/hakchi/rootfs/etc/preinit.d"
sed -i "s#hakchiVersion=.*#hakchiVersion=\"$VERSION\"#" p0000_version
