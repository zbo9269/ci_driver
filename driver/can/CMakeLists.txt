PROJECT(can)

SET(can_src
    can.c
    can.h
    Makefile
    can.sh
    )

ADD_EXECUTABLE(can ${can_src} )

