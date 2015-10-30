#!  /usr/bin/python3
import struct
from xxd import xxd_bin
from random import randint
from fcntl import ioctl

def send_net(file_name):
    """
    this function try send message
    """

    net_fp = open(file_name,"wb",0)
    ip_bin_a = struct.pack("4B",192,168,1,95)
    ip_bin_b = struct.pack("4B",192,168,2,95)
    ip = struct.unpack("1i",ip_bin_a)
    #print("ip:%x port:3000" % ip)
    # set ip a
    ioctl(net_fp.fileno(),0xc0046b32,ip[0])
    # set port a
    ioctl(net_fp.fileno(),0xc0046b33,3000)

    ip = struct.unpack("1i",ip_bin_b)
    # set ip b
    ioctl(net_fp.fileno(),0xc0046b34,ip[0])
    # set port b
    ioctl(net_fp.fileno(),0xc0046b35,3000)

    try:
        # generate random number and output
        size = randint(19,255)
        msg = [randint(0,255) for i in range(size)]
        msg[0] = 0x55
        msg[1] = 0xAA
        msg[10] = 0
        b = struct.pack("%dB" % size,*tuple(msg))
        net_fp.write(b)
        print("has write to %s station:%d %d char" % (file_name,0,size))
        xxd_bin(b)
        print()

    finally:
        net_fp.close()
    return

if __name__ == "__main__":
    send_net("/dev/neta")
    #send_net("/dev/netb")


