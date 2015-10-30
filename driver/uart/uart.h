/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 作者        : 张彦升
 版本        : 1.0
 创建日期    : 2014年3月21日 8:42:04
 用途        : uart驱动程序
 历史修改记录: v1.0    创建
**********************************************************************/
#ifndef _uart_h__
#define _uart_h__

#define UART_MAJOR                       0       /*预设的uart的主设备号，若为0表示动态*/
#define UART_DEVS_NR                     4       /*共有两个设备，一个读取A口，一个读取B口*/
#define UART1_MINOR_A                    0       /*1号A口使用的子设备号*/
#define UART1_MINOR_B                    1       /*1号B口使用的子设备号*/
#define UART2_MINOR_A                    2       /*2号A口使用的子设备号*/
#define UART2_MINOR_B                    3       /*2号B口使用的子设备号*/

#define UART_IO_PORT                     0x27f   /*io操作地址*/
#define UART_IO_SELECT_VALUE             0x01    /*选通网络板编号*/
#define UART_IO_RESET_VALUE              0xA5    /*io操作至于空闲状态编号*/

#define UART1_IO_WRITE_DATA_LEN_PORT     0x27c   /*1号口写入数据长度的端口*/
#define UART1_IO_WRITE_SEM_PORT          0x27B   /*1号口向下写数据时的信号量端口*/

#define UART2_IO_WRITE_DATA_LEN_PORT     0x27e   /*2号口写入数据长度的端口*/
#define UART2_IO_WRITE_SEM_PORT          0x27d   /*2号口向下写数据时的信号量端口*/

/*串口1号发送地址*/
#define UART1_SEND_START_ADDR            0xD0000
#define UART1_SEND_END_ADDR              0xD007F

/*串口2号发送地址*/
#define UART2_SEND_START_ADDR            0xD0080
#define UART2_SEND_END_ADDR              0xD1080

#define UART1_SEND_TOTAL_MEMORY_SIZE     (UART1_SEND_END_ADDR - UART1_SEND_START_ADDR)
#define UART2_SEND_TOTAL_MEMORY_SIZE     (UART2_SEND_END_ADDR - UART2_SEND_START_ADDR)

/*串口收数时FIFO的标志*/
#define UART_FIFO_HAS_DATA               0x01        /*FIFO有数标志*/
#define UART_FIFO_NO_DATA                0x00        /*FIFO没有数标志*/

/*串口1号收数地址及端口*/
#define UART1_RECV_FIFO_ADDR_A           0xD0007     /*1号数据收取FIFO地址*/
#define UART1_RECV_FIFO_PORT_A           0x279       /*1号数据收取FIFO端口*/
#define UART1_RECV_FIFO_ADDR_B           0xD000A     /*2号数据收取FIFO地址*/
#define UART1_RECV_FIFO_PORT_B           0x277       /*2号数据收取FIFO端口*/

/*串口2号收数地址及端口*/
#define UART2_RECV_FIFO_ADDR_A           0xD0017     /*1号数据收取FIFO地址*/
#define UART2_RECV_FIFO_PORT_A           0x269       /*1号数据收取FIFO端口*/
#define UART2_RECV_FIFO_ADDR_B           0xD001A     /*2号数据收取FIFO地址*/
#define UART2_RECV_FIFO_PORT_B           0x267       /*2号数据收取FIFO端口*/

#define UART_FALSE                       0x55        /*假标志*/
#define UART_TRUE                        0xAA        /*真标志*/

#define UART_IOC_MAGIC  'k'
#define UART_IOCRESET    _IO(UART_IOC_MAGIC, 0)
#define UART_IOC_CLEAN_MEMORY _IOWR(UART_IOC_MAGIC,  60, int)
#define UART_IOC_MAXNR 1

#define UART_DEBUG 1

struct uart_dev
{
    struct cdev cdev;                      /*cdev结构体*/

    unsigned char* recv_addr;              /*发送帧空间*/
    unsigned char* send_addr;              /*收数帧空间*/
    short recv_port;                       /*接受端口*/
    short send_port;                       /*数据发送端口*/
    short send_len_port;                   /*数据长度发送端口*/
    short send_max_len;                    /*向下发送数据的最大长度*/
    short b_use;                           /*否被使用标志*/
    struct semaphore sem;                  /*信号量*/
};

#endif /*!_uart_h__*/

