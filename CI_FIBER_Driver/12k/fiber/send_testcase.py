#!  /usr/bin/python3

import sys
from xxd import xxd_bin

def send_data(dev_name,testcase_name):
    dev_fd = open(dev_name,"wb",0)
    testcase_fd = open(testcase_name,"rb")
    try:
        # read max data
        data = testcase_fd.read(0x1010)
        ret = dev_fd.write(data)
        print("write %d bytes to %s" % (len(data),dev_name))
        xxd_bin(data)
    finally:
        dev_fd.close()
        testcase_fd.close()
    return

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage:./send_testcase.py ./testcase/*.dat")
        exit()
    testcase_name = sys.argv[1]
    send_data("/dev/fiber_persist_a",testcase_name)
