#!/bin/bash
CURRENT_DIR=`pwd`
cd $1
esptool.py --port $2 --chip ESP32-C6 -b 460800 write_flash @flash_args
cd $CURRENT_DIR
./monitor_in_linux.sh $1 $2