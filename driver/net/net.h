/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 作者        : 张彦升
 版本        : 1.0
 创建日期    : 2014年2月28日 9:07:07
 用途        : 定义net基本结构
 历史修改记录: v1.0    创建
**********************************************************************/

#ifndef _net_h__
#define _net_h__

#define NET_MAJOR                       0       /*预设的net的主设备号，0表示动态申请*/
#define NET_DEVS_NR                     2       /*共有两个设备，一个读取A口，一个读取B口*/
#define NET_MINOR_A                     0       /*A口使用的子设备号*/
#define NET_MINOR_B                     1       /*B口使用的子设备号*/

#define NET_IO_PORT                     0x27f   /*io操作地址*/
#define NET_IO_SELECT_VALUE             0x06    /*选通网络板编号*/
#define NET_IO_RESET_VALUE              0xA5    /*io操作至于空闲状态编号*/
#define NET_IO_CLEAR_MEMORY_PORT        0x234   /*清空ram数据的端口*/
#define NET_IO_WRITE_SEM_PORT           0x23B   /*向下写数据时的信号量端口*/
#define NET_IO_WRITE_DATA_LEN_HIGH_PORT 0x23E   /*写入数据的长度的高位的端口*/
#define NET_IO_WRITE_DATA_LEN_LOW_PORT  0x23D   /*写入数据的长度的地位的端口*/

#define NET_RESET_PORT                  0x232   /*复位网络通信板*/

#define NET_SEND_START_ADDR             0xD1000
#define NET_SEND_END_ADDR               0xD17FF
#define NET_SEND_TOTAL_MEMORY_SIZE      (NET_SEND_END_ADDR - NET_SEND_START_ADDR)

#define NET_RECV_START_ADDR_A           0xD4050     /*网口A读取数据起始位置*/
#define NET_RECV_END_ADDR_A             0xD547F     /*网口A读取数据终止位置*/

#define NET_RECV_START_ADDR_B           0xD5580     /*网口B读取数据起始位置*/
#define NET_RECV_END_ADDR_B             0xD69AF     /*网口B读取数据终止位置*/

#define NET_STATION_BUF_SIZE            0x100       /*一个站场的内存区域大小*/
#define NET_STATION_SIZE                10          /*共有10个站场空间*/

/*!caution,data is not consecutive*/
#define NET_WLOCK_OFF       0                                           /*每个网口写锁的偏移位置*/
#define NET_RLOCK_OFF       (NET_WLOCK_OFF + NET_STATION_SIZE)          /*每个网口读锁的偏移位置*/
#define NET_SEM_OFF         (NET_RLOCK_OFF + NET_STATION_SIZE)          /*每个网口信号量的偏移位置*/
#define NET_BUF_OFF         (NET_SEM_OFF + NET_STATION_SIZE + 2)        /*每个网口数据区的偏移位置*/
#define NET_SHADOW_SEM_OFF  (NET_BUF_OFF + NET_STATION_BUF_SIZE * NET_STATION_SIZE)    /*每个网口影子空间信号量的偏移位置*/
#define NET_SHADOW_OFF      (NET_SHADOW_SEM_OFF + NET_STATION_SIZE + 6) /*每个网口影子空间数据偏移*/

#define NET_DEST_IP_ADDR_A              0xD1800     /*目标地址A口的ip*/
#define NET_DEST_PORT_ADDR_A            0xD1804     /*目标地址A口的端口*/

#define NET_DEST_IP_ADDR_B              0xD1806     /*目标地址B口的ip*/
#define NET_DEST_PORT_ADDR_B            0xD180A     /*目标地址B口的端口*/

#define NET_FALSE                       0x55        /*假标志*/
#define NET_TRUE                        0xAA        /*真标志*/

#define NET_IOC_MAGIC  'k'
#define NET_IOCRESET    _IO(NET_IOC_MAGIC, 0)
/*约定网卡从50开始*/
#define NET_IOC_DEST_IP_A _IOWR(NET_IOC_MAGIC,  50, int)
#define NET_IOC_DEST_PORT_A _IOWR(NET_IOC_MAGIC,  51, int)
#define NET_IOC_DEST_IP_B _IOWR(NET_IOC_MAGIC,  52, int)
#define NET_IOC_DEST_PORT_B _IOWR(NET_IOC_MAGIC,  53, int)
#define NET_IOC_RESET _IOWR(NET_IOC_MAGIC,  54, int)
#define NET_IOC_MAXNR 5

#define NET_DEBUG 1

struct net_station
{
    unsigned char* data_sem;  /*数据信号量*/
    unsigned char* rlock;     /*读锁*/
    unsigned char* wlock;     /*写锁*/

    unsigned char* data_addr;  /*站场数据空间*/

    unsigned char* shadow_sem;/*站场影子空间信号量*/
    unsigned char* shadow;    /*站场影子空间*/
};

struct net_dev
{
    struct cdev cdev;                               /*cdev结构体*/

    struct net_station stations[NET_STATION_SIZE];  /*存放要接受的每个设备维护的站场信息*/
    unsigned char* send_addr;              /*发送帧空间*/
    short b_use;                                    /*是否被使用标志*/
    struct semaphore sem;                           /*信号量*/
};

#endif /*!_net_h__*/

