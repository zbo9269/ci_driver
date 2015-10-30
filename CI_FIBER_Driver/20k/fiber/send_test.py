#!  /usr/bin/python3

import struct
from xxd import xxd_bin
from random import randint
from fcntl import ioctl

def reset_memory(file_name):
    """
    reset memory
    """
    FIBER_IOCRESET = 0x6d00

    fiber_fp = open(file_name,"wb",0)
    ioctl(fiber_fp.fileno(),FIBER_IOCRESET)

    fiber_fp.close()

    return

def alloc_memory(file_name,size):
    """
    use ioctl alloc memory for fp
    """
    FIBER_IOC_ALLOC_MEMORY = 0xc0046d01

    fiber_fp = open(file_name,"wb",0)
    ioctl(fiber_fp.fileno(),FIBER_IOC_ALLOC_MEMORY,size)

    fiber_fp.close()

    return

def send_data(file_name,size):
    """
    this function try send message
    """
    fiber_fp = open(file_name,"wb",0)

    try:
        # generate random number and output
        # size = randint(0,0xfff)
        #size = 0xff7
        #size = 0xff
        msg = [randint(0,255) for i in range(size)]
        b = struct.pack("%dB" % size,*tuple(msg))
        xxd_bin(b)
        fiber_fp.write(b)
        print("has write %s %d num" % (file_name,size))
        print()

    finally:
        fiber_fp.close()
    return

if __name__ == "__main__":
    a1_size = 0xfa0 
    a2_size = 0xfa0 
    a3_size = 0xfa0 
    a4_size = 0xfa0 
    a5_size = 0xfa0 

"""
    reset_memory("/dev/fibera1")

    alloc_memory("/dev/fibera1",a1_size)
    alloc_memory("/dev/fibera2",a2_size)
    alloc_memory("/dev/fibera3",a3_size)
    alloc_memory("/dev/fibera4",a4_size)
    alloc_memory("/dev/fibera5",a5_size)

    alloc_memory("/dev/fiberb1",a1_size)
    alloc_memory("/dev/fiberb2",a2_size)
    alloc_memory("/dev/fiberb3",a3_size)
    alloc_memory("/dev/fiberb4",a4_size)
    alloc_memory("/dev/fiberb5",a5_size)
"""

send_data("/dev/fibera1",a1_size)
send_data("/dev/fibera2",a2_size)
send_data("/dev/fibera3",a3_size)
send_data("/dev/fibera4",a4_size)
send_data("/dev/fibera5",a5_size)

"""
send_data("/dev/fiberb1",a1_size)
send_data("/dev/fiberb2",a2_size) 
send_data("/dev/fiberb3",a3_size)
send_data("/dev/fiberb4",a4_size)
send_data("/dev/fiberb5",a5_size)
  
"""
