#!/bin/bash
#
# SCRIPT: dpram.sh
# AUTHOR: zhangys
# DATE  : 20140320
# REV: 1.1.D (Valid are A, B, D, T and P)
# (For Alpha, Beta, Dev, Test and Production)
#
# PLATFORM: Linux only
#
# PURPOSE: this script make easy for load and reload dpram module
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

PROJ_PATH="/root/driver/cpudpram"
DEVICE="dpram"

# every two value corresponding one file,first value is the suffix of device,
# second value is the minor number of the devide
# please notice that the minor of primary and slave are not quivalent.
FILES=" 1a  0
        2a  2
        3a  4
        4a  6
        5a  8
        1b  10
        2b  12
        3b  14
        4b  16
        5b  18
        "

# MAJOR will be extract from /proc/devices
MAJOR=0

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
# desc   : Create device files
# param  : Array which contain of pairs of device name suffix and minor number.
#          like: suffix1 minor1 suffix2 minor2
# return : none
function create_char_device_files ()
{
    [ 0 -eq ${MAJOR} ] && (echo "    MAJOR is ${MAJOR}";return 1)

    local devlist=""
    local file
    while true; do
        if [ $# -lt 2 ]; then break; fi
        file="/dev/${DEVICE}$1"
        mknod $file c $MAJOR $2
        devlist="$devlist $file"
        shift 2
    done

    echo "    mknod ${devlist}"

    return 0
}

# desc   : Remove device files
# param  : Array which contain of pairs of device name suffix and minor number.
#          like: suffix1 minor1 suffix2 minor2
# return : none
function remove_char_device_files ()
{
    local devlist=""
    local file
    while true; do
        if [ $# -lt 2 ]; then break; fi
        file="/dev/${DEVICE}$1"
        devlist="$devlist $file"
        shift 2
    done
    rm -f $devlist
    echo "    rm ${devlist}"

    return 0
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
        MAJOR=`awk "\\$2==\"$DEVICE\" {print \\$1}" /proc/devices`
        remove_char_device_files $FILES || (echo "    remove file failed";return 1)
        create_char_device_files $FILES || (echo "    create file_failed";return 1)
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
    remove_char_device_files $FILES
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

