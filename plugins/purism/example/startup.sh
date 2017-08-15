#/bin/sh

# do something with the old firmware
sha1sum /boot/purism-librem15v3.bin

# run a random tool
./random-tool

# this is the deliverable
cp /boot/purism-librem15v3.bin firmware.bin
