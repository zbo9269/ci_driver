#!  /usr/bin/python3
import struct
from xxd import xxd_bin
from random import randint
from fcntl import ioctl

def read_dpram(file_name,size):
    """
    this function try send message
    """
    dpram_fp = open(file_name,"rb",0)

    try:
        data = dpram_fp.read(size)
        print("has read %d char form %s" % (len(data),file_name))
        xxd_bin(data)
        print()
    except Exception as e:
        print("read ",file_name,e)
        print()
        dpram_fp.close()
    finally:
        dpram_fp.close()
    return

if __name__ == "__main__":
    read_dpram("/dev/dpram1a",10)


