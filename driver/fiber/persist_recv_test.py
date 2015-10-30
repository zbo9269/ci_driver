#!  /usr/bin/python3

import struct
import fcntl
from xxd import xxd_bin

def recv_fiber(file_name):
    """
    this function try recv message
    """
    fiber_fp = open(file_name,"rb",0)
    print("has open",file_name);

    try:
        b = fiber_fp.read(0xFFF)
        if b:
            print("has read %d num from %s" % (len(b),file_name))
            xxd_bin(b)
        else:
            print("none data read from %s" % file_name)
        print()

    finally:
        fiber_fp.close()
    return

if __name__ == "__main__":
    recv_fiber("/dev/fiber_share_a")
    #recv_fiber("/dev/fiber_share_b")
    recv_fiber("/dev/fiber_persist_a")
    #recv_fiber("/dev/fiber_persist_b")
