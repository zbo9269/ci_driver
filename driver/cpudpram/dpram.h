/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 作者        : 张彦升
 版本        : 1.0
 创建日期    : 2014年1月18日 14:52:07
 用途        : 双CPU之间双口ram的驱动程序
 历史修改记录: v1.0    创建
    v2.0    张彦升  增加检查双口ram是否工作正常逻辑
**********************************************************************/
#ifndef _dpram_h__
#define _dpram_h__

#include <linux/mutex.h>
#include <linux/cdev.h>

#define DPRAM_MAJOR         0       /*0 meaning dynamic alloc device id*/

#define DPRAM_PAIR_DEV_NR   10       /*this driver only support 10 pair of devs*/
#define DPRAM_DEVS_NR (DPRAM_PAIR_DEV_NR * 2)

#define DPRAM_IO_PORT           0x27f
#define DPRAM_IO_SELECT_VALUE   0x09
#define DPRAM_IO_RESET_VALUE    0xA5
#define DPRAM_FALSE             0x55
#define DPRAM_TRUE              0xAA
#define DPRAM_PENDING_DATA      0x44    /*挂起状态*/

#define DPRAM_START_ADDR        0xd0081
#define DPRAM_END_ADDR          0xdffff
#define DPRAM_TOTAL_MEMORY_SIZE (DPRAM_END_ADDR - DPRAM_START_ADDR)

#define DPRAM_IOC_MAGIC  'k'
#define DPRAM_IOCRESET    _IO(DPRAM_IOC_MAGIC, 0)
#define DPRAM_IOC_CLEAN_MEMORY _IOWR(DPRAM_IOC_MAGIC,  1, int)
#define DPRAM_IOC_ALLOC_MEMORY _IOWR(DPRAM_IOC_MAGIC,  2, int)
#define DPRAM_IOC_CHECK_MEMORY _IOWR(DPRAM_IOC_MAGIC,  3, int)
#define DPRAM_IOC_MAXNR 3

#define DPRAM_DEBUG 1

struct dpram_dev
{
    struct cdev cdev;	            /*Char device structure*/
    struct dpram_dev* peer_dev;     /*peer of primary or slave*/
    __iomem uint8_t* data_addr;     /*发送数据的地址*/
    __iomem uint8_t* rw_lock;       /*读写锁地址*/
    __iomem uint8_t* rw_sem;        /*读写信号量，确保读写不在同时被进行*/
    int buf_size;                   /*发送的数据大小*/
    short b_use;                    /*该设备是否被使用标志*/
    struct semaphore sem;           /*结构体保护信号量*/
};

struct dpram_pair_devs
{
    struct dpram_dev dpram_dev_a;
    struct dpram_dev dpram_dev_b;
};

#endif /*_dpram_h__*/
