PROJECT(dpram)

SET(dpram_src
    dpram.c
    dpram.h
    Makefile
    xxd.py
    )

ADD_EXECUTABLE(dpram ${dpram_src} )

