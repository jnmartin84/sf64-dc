#!/bin/sh
#sh-elf-objcopy -O binary sf64.elf sf64.bin
#mkdcdisc -f sf64.ico -f sf64.bin -d music -d sf_data -e loader.elf -o sf64.cdi -N
mkdcdisc -f sf64.ico -d music -d sf_data -e sf64.elf -o sf64.cdi -N
/Applications/Flycast.app/Contents/MacOS/Flycast sf64.cdi
