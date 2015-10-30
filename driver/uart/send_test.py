#!  /usr/bin/python3

import struct
from xxd import xxd_bin
from random import randint

def send_uart(name):
    """
    this function try send message to /dev/name
    """
    file_name = "/dev/%s" % name

    uart_fp = open(file_name,"wb",0)

    try:
        # generate random number and output
        #size = randint(0,0x7f)
        size = 10
        msg = [randint(0,255) for i in range(size)]
        b = struct.pack("%dB" % size,*tuple(msg))
        uart_fp.write(b)
        print("has write %s %d num" % (file_name,size))
        xxd_bin(b)
        print()

    finally:
        uart_fp.close()
    return

if __name__ == "__main__":
    send_uart("uart1a")
    #send_uart("uart1b")
    send_uart("uart2a")
    #send_uart("uart2b")
