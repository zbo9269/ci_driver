#!  /usr/bin/python3
# author:zhangys
# date  :20140320
# ver   :1.0
#

import fcntl

def start_workqueue():
    """
    start workqueue
    """
    fiber_fp = open("/dev/fibera1","rb",0)
    fcntl.ioctl(fiber_fp.fileno(),0xc0046d01)
    print("started")
    fiber_fp.close()

    return

if __name__ == "__main__":
    start_workqueue()
