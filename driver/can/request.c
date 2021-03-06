﻿/*********************************************************************
 Copyright (C), 2014,  Co.Hengjun, Ltd.
 
 作者        : 张彦升
 版本        : 1.0
 创建日期    : 2014年8月18日 9:00:40
 用途        : 请求数据
 历史修改记录: v1.0    创建
**********************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#define CAN_MEM_SIZE    0x3A20  /*单个设备全局内存最大14880字节*/
#define CAN_SEND_SIZE   0xD0    /*发送内存最大208字节*/

#define CAN_IOC_MAGIC       'e'
#define CAN_IOCRESET        _IO(CAN_IOC_MAGIC, 0)
#define CAN_IOC_SET_DATAMASK _IOW(CAN_IOC_MAGIC, 1, int*)
#define CAN_IOC_SET_DELAY_COUNTER _IOW(CAN_IOC_MAGIC, 2, int)
#define CAN_IOC_SET_READ_ADDRESS _IOW(CAN_IOC_MAGIC, 3, int)
#define CAN_IOC_MAXNR 3

static int32_t can_fd = 0;
static short eeu_fsn = 0;

#pragma pack(1)

typedef struct _EeuTag
{
    uint32_t dt : 2;                     /*数据类型*/
    uint32_t ga : 6;                     /*网关地址*/             
    uint32_t fsn : 12;                   /*帧序号*/
    uint32_t ea : 4;                     /*电子单元地址*/
    uint32_t et : 4;                     /*电子单元类型*/
    uint32_t dir : 1;                    /*方向标志*/
    uint32_t preserve : 3;               /*预留位*/
}EeuTag;

/*电子单元通信帧共1 + 4 + 4 + 2 + 2 = 13位*/
typedef struct _EeuFrame                 /*联锁机与电子模块协议帧*/
{
    uint8_t head;                        /*帧头*/
    union 
    {
        EeuTag tag;                      /*CAN数据ID解析*/
        uint32_t frame_id;
    }id;                                 /*帧ID*/
    uint32_t state;                      /*状态数据*/
    uint16_t crc;                        /*crc校验码*/
    uint16_t preserve;                   /*最后两位为预留位*/
}EeuFrame;

const uint8_t crc_tbl_high[256]={
    0x78,0xF1,0x6A,0xE3,0x5C,0xD5,0x4E,0xC7,
    0x30,0xB9,0x22,0xAB,0x14,0x9D,0x06,0x8F,
    0xF9,0x70,0xEB,0x62,0xDD,0x54,0xCF,0x46,
    0xB1,0x38,0xA3,0x2A,0x95,0x1C,0x87,0x0E,
    0x7A,0xF3,0x68,0xE1,0x5E,0xD7,0x4C,0xC5,
    0x32,0xBB,0x20,0xA9,0x16,0x9F,0x04,0x8D,
    0xFB,0x72,0xE9,0x60,0xDF,0x56,0xCD,0x44,
    0xB3,0x3A,0xA1,0x28,0x97,0x1E,0x85,0x0C,
    0x7C,0xF5,0x6E,0xE7,0x58,0xD1,0x4A,0xC3,
    0x34,0xBD,0x26,0xAF,0x10,0x99,0x02,0x8B,
    0xFD,0x74,0xEF,0x66,0xD9,0x50,0xCB,0x42,
    0xB5,0x3C,0xA7,0x2E,0x91,0x18,0x83,0x0A,
    0x7E,0xF7,0x6C,0xE5,0x5A,0xD3,0x48,0xC1,
    0x36,0xBF,0x24,0xAD,0x12,0x9B,0x00,0x89,
    0xFF,0x76,0xED,0x64,0xDB,0x52,0xC9,0x40,
    0xB7,0x3E,0xA5,0x2C,0x93,0x1A,0x81,0x08,
    0x70,0xF9,0x62,0xEB,0x54,0xDD,0x46,0xCF,
    0x38,0xB1,0x2A,0xA3,0x1C,0x95,0x0E,0x87,
    0xF1,0x78,0xE3,0x6A,0xD5,0x5C,0xC7,0x4E,
    0xB9,0x30,0xAB,0x22,0x9D,0x14,0x8F,0x06,
    0x72,0xFB,0x60,0xE9,0x56,0xDF,0x44,0xCD,
    0x3A,0xB3,0x28,0xA1,0x1E,0x97,0x0C,0x85,
    0xF3,0x7A,0xE1,0x68,0xD7,0x5E,0xC5,0x4C,
    0xBB,0x32,0xA9,0x20,0x9F,0x16,0x8D,0x04,
    0x74,0xFD,0x66,0xEF,0x50,0xD9,0x42,0xCB,
    0x3C,0xB5,0x2E,0xA7,0x18,0x91,0x0A,0x83,
    0xF5,0x7C,0xE7,0x6E,0xD1,0x58,0xC3,0x4A,
    0xBD,0x34,0xAF,0x26,0x99,0x10,0x8B,0x02,
    0x76,0xFF,0x64,0xED,0x52,0xDB,0x40,0xC9,
    0x3E,0xB7,0x2C,0xA5,0x1A,0x93,0x08,0x81,
    0xF7,0x7E,0xE5,0x6C,0xD3,0x5A,0xC1,0x48,
    0xBF,0x36,0xAD,0x24,0x9B,0x12,0x89,0x00
};

const uint8_t crc_tbl_low[256]={
    0xF0,0xE1,0xD3,0xC2,0xB6,0xA7,0x95,0x84,
    0x7C,0x6D,0x5F,0x4E,0x3A,0x2B,0x19,0x08,
    0xE0,0xF1,0xC3,0xD2,0xA6,0xB7,0x85,0x94,
    0x6C,0x7D,0x4F,0x5E,0x2A,0x3B,0x09,0x18,
    0xD1,0xC0,0xF2,0xE3,0x97,0x86,0xB4,0xA5,
    0x5D,0x4C,0x7E,0x6F,0x1B,0x0A,0x38,0x29,
    0xC1,0xD0,0xE2,0xF3,0x87,0x96,0xA4,0xB5,
    0x4D,0x5C,0x6E,0x7F,0x0B,0x1A,0x28,0x39,
    0xB2,0xA3,0x91,0x80,0xF4,0xE5,0xD7,0xC6,
    0x3E,0x2F,0x1D,0x0C,0x78,0x69,0x5B,0x4A,
    0xA2,0xB3,0x81,0x90,0xE4,0xF5,0xC7,0xD6,
    0x2E,0x3F,0x0D,0x1C,0x68,0x79,0x4B,0x5A,
    0x93,0x82,0xB0,0xA1,0xD5,0xC4,0xF6,0xE7,
    0x1F,0x0E,0x3C,0x2D,0x59,0x48,0x7A,0x6B,
    0x83,0x92,0xA0,0xB1,0xC5,0xD4,0xE6,0xF7,
    0x0F,0x1E,0x2C,0x3D,0x49,0x58,0x6A,0x7B,
    0x74,0x65,0x57,0x46,0x32,0x23,0x11,0x00,
    0xF8,0xE9,0xDB,0xCA,0xBE,0xAF,0x9D,0x8C,
    0x64,0x75,0x47,0x56,0x22,0x33,0x01,0x10,
    0xE8,0xF9,0xCB,0xDA,0xAE,0xBF,0x8D,0x9C,
    0x55,0x44,0x76,0x67,0x13,0x02,0x30,0x21,
    0xD9,0xC8,0xFA,0xEB,0x9F,0x8E,0xBC,0xAD,
    0x45,0x54,0x66,0x77,0x03,0x12,0x20,0x31,
    0xC9,0xD8,0xEA,0xFB,0x8F,0x9E,0xAC,0xBD,
    0x36,0x27,0x15,0x04,0x70,0x61,0x53,0x42,
    0xBA,0xAB,0x99,0x88,0xFC,0xED,0xDF,0xCE,
    0x26,0x37,0x05,0x14,0x60,0x71,0x43,0x52,
    0xAA,0xBB,0x89,0x98,0xEC,0xFD,0xCF,0xDE,
    0x17,0x06,0x34,0x25,0x51,0x40,0x72,0x63,
    0x9B,0x8A,0xB8,0xA9,0xDD,0xCC,0xFE,0xEF,
    0x07,0x16,0x24,0x35,0x41,0x50,0x62,0x73,
    0x8B,0x9A,0xA8,0xB9,0xCD,0xDC,0xEE,0xFF
};
/*
 功能描述    : 计算16位的crc
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 何境泰
 日期        : 2014年7月28日 13:42:26
*/
uint16_t CIAlgorithm_Crc16(const void* data, uint32_t size)
{
    uint32_t i = 0;
    uint16_t  acc = 0;
    uint8_t  a = 0, h = 0, l = 0;
    const uint8_t* p_data = (uint8_t*)data;

    /*查表发计算CRC码*/
    for( i = 0;  i < size; i++ )
    {
        a = ((uint8_t)(acc >> 8 ) ^ ( *p_data));
        h = crc_tbl_high[ (a & 0x00ff) ];
        l = (uint8_t)( acc & 0x00ff );
        h = h ^ l;
        l = crc_tbl_low[ (a & 0x00ff) ];
        acc = (((uint16_t)h) << 8 ) | (((uint16_t)l) & 0x00ff);

        p_data++; 
    }
    return acc;
}
/*
 功能描述    : 使用16进制方式打印数据，该函数不允许使用strcat等长度未知的函数
 返回值      : 
 参数        : @buf以char数组保存的数据
              @len数据的长度
 作者        : 张彦升
 日期        : 2014年3月5日 12:45:05
*/
void hex_dump(const void* p_data, int len)
{
    char str[80] = {0}, octet[10] = {0};
    const char* buf = (const char*)p_data;
    int ofs = 0, i = 0, l = 0;

    for (ofs = 0; ofs < len; ofs += 16)
    {
        memset(str,0,80);

        sprintf( str, "%07x: ", ofs );

        for (i = 0; i < 16; i++)
        {
            if ((i + ofs) < len)
            {
                sprintf( octet, "%02x ", buf[ofs + i] & 0xff );
            }
            else
            {
                strncpy( octet, "   ",3);
            }

            strncat(str,octet,3);
        }
        strncat(str,"  ",2);
        l = strlen(str);

        for (i = 0; (i < 16) && ((i + ofs) < len); i++)
        {
            str[l++] = isprint( buf[ofs + i] ) ? buf[ofs + i] : '.';
        }

        str[l] = '\0';
        printf( "%s", str );
    }
    printf("\n");
}
/*
功能描述    : 32位数字大小端字节互转
返回值      : 转换成功后的新数值
参数        : @val 要转换的值
作者        : 何境泰
日期        : 2014年5月23日 1:04:47
*/
uint32_t CI_Swap32(uint32_t val)
{
    return ((((val) & 0xff000000) >> 24) |
        (((val) & 0x00ff0000) >>  8) |
        (((val) & 0x0000ff00) <<  8) |
        (((val) & 0x000000ff) << 24));
}
/*
 功能描述    : 向电子单元发送请求命令帧
 返回值      : 无
              @command             命令数据
              @et                  电子单元类型
              @ga                  网关地址
              @dt                  数据类型
              @ea                  电子单元地址
 作者       : 何境泰
 日期       : 2013年12月13日 13:22:38A
*/
static void eeu_assemble_send_frame(EeuFrame* frame,
                                    uint8_t et,
                                    uint8_t ga,
                                    uint8_t ea,
                                    uint8_t dt,
                                    uint32_t command)
{
    uint32_t temp_id = 0;

    frame->head = 0X86;

    /*将预留位清空，避免计算出错误的crc码*/
    frame->id.tag.preserve = 0;
    frame->id.tag.dir = 0;        /*由联锁机向电子单元的数据类型为0*/
    frame->id.tag.fsn = eeu_fsn;
    frame->id.tag.et = et;
    frame->id.tag.ga = ga;
    frame->id.tag.ea = ea;
    frame->id.tag.dt = dt;

    frame->state = command;

    temp_id = frame->id.frame_id;
    frame->id.frame_id = CI_Swap32(temp_id);

    /*计算crc暂时不包含头部，注意，这里计算crc时id域为未偏移之前大小端转换后的数据*/
    frame->crc = CIAlgorithm_Crc16(&frame->id.tag, 8);

    /*进行fpga的偏移和大小端变化，ID域向左偏移3位,以后如果修改fpga发送偏移问题,可以去掉*/
    temp_id=((temp_id << 3) & 0xfffffff8);
    frame->id.frame_id = CI_Swap32(temp_id);
    frame->preserve = 0;

    /*帧序号按照4096来回转的*/
    eeu_fsn = (eeu_fsn + 1) % 4096;

    return;
}
/*
功能描述    : 发送多帧数据
返回值      : 
参数        : @send_buff    发送数据缓存区
             @size         发送数据大小(最大不超过15* 13)
作者        : 何境泰
日期        : 2014年5月4日 9:03:37
*/
static int32_t eeu_send_data(const void* send_data, int32_t size)
{
    int32_t ret = 0;

    /*发送电子模块数据*/
    ret = write(can_fd, send_data, size);
    if (0 >= ret)
    {
        perror("电子模块A口发送数据失败!");
    }

    return 0;
}
/*
 功能描述    : 向电子单元发送请求帧
 返回值      : 成功为0，失败为-1
 参数        : 无
 作者        : 张彦升
 日期        : 2014年8月18日 9:04:26
*/
static int32_t CIEeu_RequestStatus(void)
{
    static EeuFrame eeu_frame;
    int32_t ret = 0;
    int len = 0;

    eeu_assemble_send_frame(&eeu_frame,0,0,0,1,0x11111111);
    eeu_send_data(&eeu_frame,sizeof(eeu_frame));
    hex_dump(&eeu_frame,sizeof(eeu_frame));

    return 0;
}

int main()
{
    int fd = 0;
    int ret = 0;
    int i = 0;
    unsigned short null_mask[31] = {0};

    can_fd = open("/dev/cana",O_RDWR);
    if(-1 == fd)
    {
        perror("can't open /dev/cana");
        return -1;
    }

    while(1)
    {
        CIEeu_RequestStatus();
        usleep(400000);
    }

    return 0;
}
