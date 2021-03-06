#!  /usr/bin/python3

import struct
import fcntl
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

def recv_data(file_name,size):
    """
    this function try recv message
    """
    fiber_fp = open(file_name,"rb",0)

    try:
        #size = 0xff7
        #size = 0xff
        b = fiber_fp.read(size)
        if b:
            print("has read %d num from %s" % (len(b),file_name))
            xxd_bin(b)
        else:
            print("none data read from %s" % file_name)
        print()

    finally:
        fiber_fp.close()
    return
	
	
	
def recive_all(size):
    recv_data("/dev/fibera1",size)
    recv_data("/dev/fibera2",size)
    recv_data("/dev/fibera3",size)
    recv_data("/dev/fibera4",size)
    recv_data("/dev/fibera5",size)

    recv_data("/dev/fiberb1",size)
    recv_data("/dev/fiberb2",size)
    recv_data("/dev/fiberb3",size)
    recv_data("/dev/fiberb4",size)
    recv_data("/dev/fiberb5",size)
    return
	
def alloc_all(size):
    reset_memory("/dev/fibera1")

    alloc_memory("/dev/fibera1",size)
    alloc_memory("/dev/fibera2",size)
    alloc_memory("/dev/fibera3",size)
    alloc_memory("/dev/fibera4",size)
    alloc_memory("/dev/fibera5",size)

    alloc_memory("/dev/fiberb1",size)
    alloc_memory("/dev/fiberb2",size)
    alloc_memory("/dev/fiberb3",size)
    alloc_memory("/dev/fiberb4",size)
    alloc_memory("/dev/fiberb5",size)
    return
	
def usge_err():
    print ("usge err! \n if you want alloc memory please run me with ****\"./send_test.py alloc\"*** and \
	if you want recive data please run me with ****\"./recive_test.py \"*** \
    all other is err!")

if __name__ == "__main__":
    a1_size = 0xfa0 
    a2_size = 0xfa0 
    a3_size = 0xfa0 
    a4_size = 0xfa0 
    a5_size = 0xfa0 
	
    if len(sys.argv) == 2 :
        if sys.argv[1] == "alloc":
            alloc_all(a1_size)
            print ("alloc")
        else :
            usge_err();
    elif len(sys.argv) == 1:
        recive_all(a1_size)
        print ("send")
    else:
	    usge_err()
