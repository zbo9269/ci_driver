#!/bin/bash
#
# SCRIPT: led_wdt.sh
# AUTHOR: zhangys
# DATE  : 20140320
# REV: 1.1.D (Valid are A, B, D, T and P)
# (For Alpha, Beta, Dev, Test and Production)
#
# PLATFORM: Linux only
#
# PURPOSE: this script make easy for load and reload led_wdt module
#
# REV LIST  : 1.1.D
# DATE      : 20140320
# BY        : zhangys
# MODIFICATION: create it
#
#
# set -n # Uncomment to check script syntax, without execution.
# # NOTE: Do not forget to put the comment back in or
# # the shell script will not execute!
# set -x # Uncomment to debug this shell script
#
##########################################################
# DEFINE FILES AND VARIABLES HERE
##########################################################

PROJ_PATH="/root/driver/led"

# partport had implemented by kernel coder,we just use modprobe
# to probe,it will work ok.
modprobe parport_pc
mknod /dev/parport0 c 99 0

