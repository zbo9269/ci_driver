#!  /usr/bin/python
# author: zhangys
# date  : 20140310
# ver   : 0.1
#
# this program will test read data from net board

from xxd import xxd_bin

neta_file_name = "/dev/neta"

buf_size = 0x100

# open function use 4096 as its buffer size,we expected it is no buffer
neta_fp = open(neta_file_name,"rb",0)

try:
    buf = neta_fp.read(buf_size)
    xxd_bin(buf)

finally:
    neta_fp.close()


