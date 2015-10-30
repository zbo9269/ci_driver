#!  /usr/bin/python3
# author: zhangys
# date  : 20140310
# ver   : 0.1
#
# this program will test read data from can board

from xxd import xxd_bin

cana_file_name = "/dev/cana"

buf_size = 0x2000

# open function use 4096 as its buffer size,we expected it is no buffer
cana_fp = open(cana_file_name,"rb",0)

try:
    buf = cana_fp.read(buf_size)
    xxd_bin(buf)

finally:
    cana_fp.close()


