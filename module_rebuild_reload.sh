#!/bin/bash
make all || exit 1
sudo ./module_unload.sh 2> /dev/null
sudo ./module_load.sh
