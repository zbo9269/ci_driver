#!  /usr/bin/python3

import struct
from xxd import xxd_bin
from random import randint

def send_fiber(file_name):
    """
    this function try send message
    """
    fiber_fp = open(file_name,"wb",0)

    try:
        # generate random number and output
        size = randint(0,0x30)
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
    send_fiber("/dev/fiber_persist_a")
    #send_fiber("/dev/fiber_persist_b")
    send_fiber("/dev/fiber_share_a")
    #send_fiber("/dev/fiber_share_b")
