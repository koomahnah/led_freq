#!/bin/sh
module="led_freq"
/sbin/rmmod ${module}.ko || exit 1
rm -f /dev/${module}0
