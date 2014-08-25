#!/bin/bash
make -C ~/linux-raspberrypi/ M=`pwd` modules && sudo ./hello_unload.sh && sudo ./hello_load.sh
