#/bin/sh
appstream-util validate-relax com.Purism.Librem15v3.metainfo.xml
tar -cf firmware.tar startup.sh random-tool
gcab --create --nopath Purism-Librem15v3-1.2.3.cab firmware.tar com.Purism.Librem15v3.metainfo.xml
