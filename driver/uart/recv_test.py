#!  /usr/bin/python3

import struct
import fcntl
from xxd import xxd_bin

def recv_uart(name):
    """
    this function try recv message from /dev/name
    """
    file_name = "/dev/%s" % name

    uart_fp = open(file_name,"rb",0)

    try:
        b = uart_fp.read(0x7F)
        if b:
            print("has read %d char from %s" % (len(b),file_name))
            xxd_bin(b)
        else:
            print("none data read from %s" % file_name)
        print()

    finally:
        uart_fp.close()
    return

if __name__ == "__main__":
    recv_uart("uart1a")
    recv_uart("uart1b")
    recv_uart("uart2a")
    recv_uart("uart2b")
