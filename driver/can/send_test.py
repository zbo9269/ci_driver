#!  /usr/bin/python3
import struct
from xxd import xxd_bin
from random import randint
from fcntl import ioctl

def send_can(file_name):
    """
    this function try send message
    """
    can_fp = open(file_name,"wb",0)

    try:
        size = 13
        msg = [randint(0,255) for i in range(size)]
        msg[0] = 0x88 # 扩展帧
        b = struct.pack("%dB" % size,*tuple(msg))
        can_fp.write(b)
        print("has write to %s %d char" % (file_name,size))
        xxd_bin(b)
        print()

    finally:
        can_fp.close()
    return

if __name__ == "__main__":
    send_can("/dev/cana")


