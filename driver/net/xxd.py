#!  /usr/bin/python3
# author: zhangys
# date  : 20140310
# ver   : 1.0
#
# this module is very convenience for debug binary data,especially network
# data,it is print data format like xxd
 
import string
 
__all__ = [ "xxd_bin" ,"xxd_str" ,"line_len"]
 
__version__ = "1.0.0"
 
line_len = 16
 
"""
@binarys bytes
return none
print @binarys as xxd like style
"""
def xxd_bin(binarys):
    counter = 0
    for i in range(0,len(binarys),line_len):
        line = binarys[i:i + line_len]
        # turn byte to hex str
        buf2 = ['%02x' % i for i in line]
        print( '{0}: {1:<39}  {2}'.format(('%07x' % (counter * 16)),
        ' '.join([''.join(buf2[i:i + 2]) for i in range(0, len(buf2), 2)]),
        ''.join([chr(b) if chr(b) in string.printable[:-5] else '.' for b in line])))
 
        counter += 1
 
    return
 
"""
@buf str
return none
print @buf as xxd like style
"""
def xxd_str(buf):
    counter = 0
    for i in range(0,len(buf),line_len):
        line = buf[i:i + line_len]
        buf2 = ['%02x' % ord(i) for i in line]
 
        print( '{0}: {1:<39}  {2}'.format(('%07x' % (counter * 16)),
        ' '.join([''.join(buf2[i:i + 2]) for i in range(0, len(buf2), 2)]),
        ''.join([c if c in string.printable[:-5] else '.' for c in line])))
 
        counter += 1
    return
 
if __name__ == "__main__":
    s = "asdfhjjl;;p009888hhnncbbbc"
    xxd_bin(bytes(s,"ASCii"))
    xxd_str(s)

