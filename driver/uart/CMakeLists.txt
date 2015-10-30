PROJECT(uart)

SET(uart_src
    uart.c
    uart.h
    Makefile
    uart.sh
    )

ADD_EXECUTABLE(uart ${uart_src} )

