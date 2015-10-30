#!/usr/bin/python3
# author: zhangys
# date  : 20140310
# ver   : 0.1
#
# To use this program,please do things below
# 1.change you eth to static which address is 192.168.1.3
# 2.make sure you are in the subnet of 192.168.1.1

import socketserver
from xxd import xxd_bin

class MyUDPHandler(socketserver.BaseRequestHandler):
    def handle(self):
        data = self.request[0]
        print("%s wrote:" % self.client_address[0])
        xxd_bin(data)

if __name__ == "__main__":
    #HOST, PORT = "127.0.0.1", 3000
    HOST, PORT = "192.168.1.101", 3005
    server = socketserver.UDPServer((HOST, PORT), MyUDPHandler)
    print("listening..")
    server.serve_forever()

