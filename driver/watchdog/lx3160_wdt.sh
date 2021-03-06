#!/bin/bash
#
# SCRIPT: lx3160_wdt.sh
# AUTHOR: zhangys
# DATE  : 20140320
# REV: 1.1.D (Valid are A, B, D, T and P)
# (For Alpha, Beta, Dev, Test and Production)
#
# PLATFORM: Linux only
#
# PURPOSE: this script make easy for load and reload lx3160_wdt module
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

PROJ_PATH="/root/driver/watchdog"
DEVICE="lx3160_wdt"

INSMOD=/sbin/insmod; # use /sbin/modprobe if you prefer

##########################################################
# DEFINE FUNCTIONS HERE
##########################################################

# desc   : add any thing you want do before device is load
# param  : 
# return : none
function device_specific_pre_unload ()
{
    true; # fill at will
}
# desc   : add any thing you want do after device is load
# param  : 
# return : none
function device_specific_post_load ()
{
    true; # fill at will
}

# desc   : Load and create files
# param  : none
# return : none
function load_device ()
{
    devpath=${PROJ_PATH}/${DEVICE}.ko

    # check device file whether exist or not
    [ -f ${devpath} ] || (echo "${devpath} not exist"; return 1)

    if ${INSMOD} ${devpath}; then
        device_specific_post_load

        echo "    insmod ${devpath}"
        return 0
    else
        return 1
    fi
}

# desc   : Unload and remove files
# param  : none
# return : none
function unload_device ()
{
    devpath=${PROJ_PATH}/${DEVICE}.ko
    device_specific_pre_unload 
    /sbin/rmmod ${devpath} || return 1
    echo "    rmmod ${devpath}"
}
# desc   : main function for this script will execute
# param  : none
# return : none
function main()
{
    case "$1" in
        start)
            echo  "Loading $DEVICE ..."
            load_device && echo "Load ${DEVICE} success"
            ;;
        stop)
            echo  "Unloading $DEVICE ..."
            unload_device && echo "Unload ${DEVICE} success"
            ;;
        force-reload|restart)
            echo  "Reloading $DEVICE ..."
            unload_device && echo "Unload ${DEVICE} success"
            load_device && echo "Load ${DEVICE} success"
            ;;
        *)
            echo "Usage: $0 {start|stop|restart|force-reload}"
            exit 1
    esac
}

# if not need execute this script,and you don't want remove it 
# from system boot run time script,just comment this
main $*

exit 0

