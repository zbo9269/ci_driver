#!  /bin/bash

# set -x

function transfer()
{
    echo "transfer to ${1}"
    scp -r /root/driver/can/can.ko root@${1}:/root/driver/can/
}

#transfer 192.168.1.202
transfer 192.168.1.203
#transfer 192.168.1.204
#transfer 192.168.1.205


