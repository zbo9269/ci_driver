/*********************************************************************
 Copyright (C), 2014,  Co.Hengjun, Ltd.
 
 作者        : 张彦升
 版本        : 1.0
 创建日期    : 2014年8月12日 13:54:36
 用途        : can板驱动程序
 历史修改记录: v1.0    创建
    v2.0    zhangys 添加SJA及内存的检测
**********************************************************************/

#ifndef _can_h__
#define _can_h__
 
#define CAN_MAJOR               0       /*0 meaning dynamic alloc device id*/
#define CAN_MINOR_A             0x00
#define CAN_MINOR_B             0x01

#define CAN_DEVS_NR             2       /*can共有两个设备，A口和B口*/

#define CAN_IO_PORT             0x27f
#define CAN_IO_SELECT_VALUE     0x04
#define CAN_IO_RESET_VALUE      0xAA

#define CAN_SJA_RESET_PORT      0x242

#define CAN_FRAME_LEN_PORT      0x24a
#define CAN_IO_FRAME_SEND       0x24b

/*
 * 20150701 REQUIREMENT：CAN通信板通信一段时间后会死掉，因此需要复位操作
 * SJA1000.pdf说明文档中(英文文档6.3.5节,Table 5,Page 15)指出，SJA状态
 * 寄存器第6位和第7位可判断错误状态，为此，通过该端口将该寄存器映射到PC104
 * 以便读取
*/
#define CAN_SJA_STATUS_FLAG      0x245

#define FPGA_FLAG_BUSY          0xaa
#define FPGA_FLAG_FREE          0x55

#define FLAG_TRUE               0x01
#define FLAG_FALSE              0x00

#define CAN_FRAME_LEN           0x0d
#define EEU_GA_NUM              31
#define EEU_EA_NUM              16
#define EEU_FRAME_LEN           13
#define EEU_GA_DATA_LEN         (EEU_EA_NUM * EEU_FRAME_LEN)
#define CAN_FLAG_LEN            (EEU_GA_NUM * EEU_EA_NUM)
#define CAN_DATA_LEN            0x1930
#define FPG_WRITE_FLAG_OFFS     0 /* FPGA_A写网关标志地址偏移*/
#define PC104_READ_OFFS CAN_FLAG_LEN   /* PC104读FPGA_A网关标志地址偏移 0XAA为读有效 0X55为无效*/
#define DATA_FLAG_OFFS (PC104_READ_OFFS + CAN_FLAG_LEN)    /* 网关数据空间标志地址偏移 0X01为有数有效标志 0X00为空标志*/
#define DATA_SPACE_OFFS (DATA_FLAG_OFFS + CAN_FLAG_LEN) /* 网关数据空间地址偏移 每个网关的数据空间为13个字节*/
#define DATAB_FLAG_OFFS (DATA_SPACE_OFFS + CAN_DATA_LEN)     /* 网关影子数据空间标志地址偏移 0X01为有数有效标志 0X00为空标志*/
#define DATAB_SPACE_OFFS (DATAB_FLAG_OFFS + CAN_FLAG_LEN)    /* 网关影子数据空间地址偏移 每个网关的数据空间为13个字节*/

#define CAN_RECV_START_ADDR_A           0xD7000     /*A读取数据起始位置*/
#define CAN_RECV_END_ADDR_A             0xDAA1F     /*A读取数据终止位置*/
#define CAN_RECV_TOTAL_MEMORY_SIZE_A    (CAN_RECV_END_ADDR_A - CAN_RECV_START_ADDR_A)

#define CAN_RECV_START_ADDR_B           0xDb000     /*B读取数据起始位置*/
#define CAN_RECV_END_ADDR_B             0xDEA1F     /*B读取数据终止位置*/
#define CAN_RECV_TOTAL_MEMORY_SIZE_B    (CAN_RECV_END_ADDR_B - CAN_RECV_START_ADDR_B)

#define CAN_SEND_START_ADDR             0xD2000
#define CAN_SEND_END_ADDR               0xD20CF
#define CAN_SEND_TOTAL_MEMORY_SIZE      (CAN_SEND_END_ADDR - CAN_SEND_START_ADDR)

#define CAN_IOC_MAGIC       'e'
#define CAN_IOCRESET        _IO(CAN_IOC_MAGIC, 0)
#define CAN_IOC_SET_DATAMASK _IOW(CAN_IOC_MAGIC, 1, int*)
#define CAN_IOC_SET_DELAY_COUNTER _IOW(CAN_IOC_MAGIC, 2, int)
#define CAN_IOC_SET_READ_ADDRESS _IOW(CAN_IOC_MAGIC, 3, int)
#define CAN_IOC_GET_SJA_STATUS_FLAG _IOW(CAN_IOC_MAGIC, 4, int)
#define CAN_IOC_MAXNR 4

#define CAN_DEBUG 1

/*can设备结构体*/
struct can_dev
{
    struct cdev cdev;                       /*cdev结构体*/
    unsigned char* recv_addr;               /*接收数据地址*/
    short b_use;                            /*该设备是否被使用标志*/
};

struct can_pair_devs
{
    struct can_dev can_dev_a;
    struct can_dev can_dev_b;
};

#endif /*!_can_h__*/
