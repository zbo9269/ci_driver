/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 作者        : 张彦升
 版本        : 1.0
 创建日期    : 2014年3月13日 13:00:09
 用途        : 光纤板的驱动程序
 历史修改记录: v1.0    创建
**********************************************************************/

#ifndef _fiber_h__
#define _fiber_h__

#define FIBER_MAJOR                       0       /*预设的fiber的主设备号*/

#define FIBER_PAIR_DEVS_NR                5       /*对应A口和B口共有5对设备*/

#define FIBER_IO_PORT                     0x27f   /*io操作地址*/
#define FIBER_IO_SELECT_VALUE             0x03    /*选通网络板编号*/
#define FIBER_IO_RESET_VALUE              0xAA    /*io操作至于空闲状态编号*/

#define FIBER_IO_WRITE_LOCK_PORT          0x25B   /*向下写数据时的信号量端口*/
#define FIBER_IO_WRITE_DATA_LEN_HIGH_PORT 0x25E   /*写入数据的长度的高位的端口*/
#define FIBER_IO_WRITE_DATA_LEN_LOW_PORT  0x25D   /*写入数据的长度的地位的端口*/

#define FIBER_IO_RECV_LOCK_PORT_A         0x250   /*端口A读时的信号量*/
#define FIBER_IO_RECV_LOCK_PORT_B         0x251   /*端口B读时的信号量*/

#define FIBER_SEND_START_ADDR             0xD3000
#define FIBER_SEND_END_ADDR               0xD3FFF
#define FIBER_SEND_TOTAL_MEMORY_SIZE      (FIBER_SEND_END_ADDR - FIBER_SEND_START_ADDR)

#define FIBER_HEAD_SIZE                   4
#define FIBER_SEND_MAX_MEMORY_SIZE        (FIBER_SEND_TOTAL_MEMORY_SIZE - 2)

#define FIBER_RECV_START_ADDR_A           0xD0035     /*A读取数据起始位置*/
#define FIBER_RECV_END_ADDR_A             0xD1032     /*A读取数据终止位置*/
#define FIBER_RECV_TOTAL_MEMORY_SIZE_A    (FIBER_RECV_END_ADDR_A - FIBER_RECV_START_ADDR_A)

#define FIBER_RECV_START_ADDR_B           0xD2033     /*B读取数据起始位置*/
#define FIBER_RECV_END_ADDR_B             0xD3030     /*B读取数据终止位置*/
#define FIBER_RECV_TOTAL_MEMORY_SIZE_B    (FIBER_RECV_END_ADDR_B - FIBER_RECV_START_ADDR_B)

#define FIBER_IOC_MAGIC 'm'
#define FIBER_IOC_RESET _IO(FIBER_IOC_MAGIC, 0)
#define FIBER_IOC_ALLOC_MEMORY _IOWR(FIBER_IOC_MAGIC, 1, int)
#define FIBER_IOC_CLEAN_BUFFER _IOWR(FIBER_IOC_MAGIC, 2, int)  /*清除缓存*/
#define FIBER_IOC_MAXNR 2

#define FIBER_DEBUG 1

struct fiber_data
{
    uint8_t* data_addr;     /*发送地址*/
    uint32_t* sn_addr;      /*存放sn的地址*/
};
struct fiber_dev
{
    struct cdev cdev;               /*cdev结构体*/
    short b_use;                    /*是否被使用*/
    struct fiber_data send_data;    /*发送地址*/
    struct fiber_data recv_data;    /*发送地址*/
    __iomem uint8_t* recv_addr;     /*接受地址*/
    __iomem uint8_t* send_addr;     /*接受地址*/
    uint8_t* recv_buff;             /*接收数据缓冲区地址*/
    uint8_t* send_buff;             /*接收数据缓冲区地址*/
    int32_t buf_size;               /*发送的数据大小*/
    int32_t send_sn;                /*帧号，作为数据是否被读取的依据*/
    int32_t last_recv_sn;           /*帧号，作为数据是否被读取的依据*/
    short recv_lock_port;

    struct fiber_dev* peer_fiber_dev;   /*与这个口对应的另外一个口的设备*/
};

#endif /*!_fiber_h__*/
