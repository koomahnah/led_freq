#!/bin/sh
module="led_freq"
/sbin/insmod ./${module}.ko $* || exit 1
rm -f /dev/${module}0

major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)
mknod /dev/${module}0 c $major 0

#chgrp staff /dev/${module}[0-1]
chmod 664 /dev/${module}0
