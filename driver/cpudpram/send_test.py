#!  /usr/bin/python3
import struct
from xxd import xxd_bin
from random import randint
from fcntl import ioctl

def send_dpram(file_name,size):
    """
    this function try send message
    """
    dpram_fp = open(file_name,"wb",0)

    try:
        # generate random number and output
        #size = randint(5,10)
        msg = [randint(0,255) for i in range(size)]
        b = struct.pack("%dB" % size,*tuple(msg))
        dpram_fp.write(b)
        print("has write to %s %d char" % (file_name,size))
        xxd_bin(b)
        print()
    except Exception as e:
        print("write ",file_name,e)
        print()
        dpram_fp.close()
    finally:
        dpram_fp.close()
    return

if __name__ == "__main__":
    send_dpram("/dev/dpram1a",10)


