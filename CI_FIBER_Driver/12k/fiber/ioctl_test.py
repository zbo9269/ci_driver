#!  /usr/bin/python3
# test fiber ioctl system call

import struct
from xxd import xxd_bin
from random import randint
from fcntl import ioctl

def ioctl_test(file_name):
    FIBER_IOC_ALLOC_MEMORY = 0xc0046b3c
    FIBER_IOCRESET = 0x6b00
    fiber_fp = open(file_name,"wb",0)
    # fiber memeory is 0xfff
    for mem_size in range(0xfff):
        ioctl(fiber_fp.fileno(),FIBER_IOCRESET)
        ioctl(fiber_fp.fileno(),FIBER_IOC_ALLOC_MEMORY,mem_size)
    return

if __name__ == "__main__":
    ioctl_test("/dev/fibera1")
    #ioctl_test("/dev/fibera2")
    #ioctl_test("/dev/fibera3")
    #ioctl_test("/dev/fibera4")
    #ioctl_test("/dev/fibera5")
    #ioctl_test("/dev/fiberb1")
    #ioctl_test("/dev/fiberb2")
    #ioctl_test("/dev/fiberb3")
    #ioctl_test("/dev/fiberb4")
    #ioctl_test("/dev/fiberb5")
