#!/bin/bash
set -e
cd $( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )

if [ ! -f ../../../../samples/locator_tag/build/locator_tag/zephyr/zephyr.signed.bin ]; then
    echo "The '../../../../samples/locator_tag/build/locator_tag/zephyr/zephyr.signed.bin' file"
    echo "does not exists. Prepare an application image as described"
    echo "in the documentation."
    exit 1
fi

mkdir -p build
cp ../../../../samples/locator_tag/build/locator_tag/zephyr/zephyr.signed.bin build/

python3 ../../../ncsfmntools SuperBinary    \
	SuperBinary.plist                   \
	--out-plist build/SuperBinary.plist \
	--out-uarp build/SuperBinary.uarp   \
	--metadata build/MetaData.plist     \
	--payloads-dir build                \
	--release-notes ../ReleaseNotes
