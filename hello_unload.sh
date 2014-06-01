#!/bin/sh
/sbin/rmmod hello.ko || exit 1
rm -f /dev/hello[0-1]
