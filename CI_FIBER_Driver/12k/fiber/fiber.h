/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 ����        : ������
 �汾        : 1.0
 ��������    : 2014��3��13�� 13:00:09
 ��;        : ���˰����������
 ��ʷ�޸ļ�¼: v1.0    ����
**********************************************************************/

#ifndef _fiber_h__
#define _fiber_h__

#define FIBER_MAJOR                       0       /*Ԥ���fiber�����豸��*/

#define FIBER_PAIR_DEVS_NR                5       /*��ӦA�ں�B�ڹ���5���豸*/

#define FIBER_IO_PORT                     0x27f   /*io������ַ*/
#define FIBER_IO_SELECT_VALUE             0x03    /*ѡͨ�������*/
#define FIBER_IO_RESET_VALUE              0xAA    /*io�������ڿ���״̬���*/

#define FIBER_IO_WRITE_LOCK_PORT          0x25B   /*����д����ʱ���ź����˿�*/
#define FIBER_IO_WRITE_DATA_LEN_HIGH_PORT 0x25E   /*д�����ݵĳ��ȵĸ�λ�Ķ˿�*/
#define FIBER_IO_WRITE_DATA_LEN_LOW_PORT  0x25D   /*д�����ݵĳ��ȵĵ�λ�Ķ˿�*/

#define FIBER_IO_RECV_LOCK_PORT_A         0x250   /*�˿�A��ʱ���ź���*/
#define FIBER_IO_RECV_LOCK_PORT_B         0x251   /*�˿�B��ʱ���ź���*/

#define FIBER_SEND_START_ADDR             0xD7000
#define FIBER_SEND_END_ADDR               0xD9FFF
#define FIBER_SEND_TOTAL_MEMORY_SIZE      (FIBER_SEND_END_ADDR - FIBER_SEND_START_ADDR)

#define FIBER_HEAD_SIZE                   4
#define FIBER_SEND_MAX_MEMORY_SIZE        (FIBER_SEND_TOTAL_MEMORY_SIZE - FIBER_HEAD_SIZE)

#define FIBER_RECV_START_ADDR_A           0xD0035     /*A��ȡ������ʼλ��*/
#define FIBER_RECV_END_ADDR_A             0xD3032     /*A��ȡ������ֹλ��*/
#define FIBER_RECV_TOTAL_MEMORY_SIZE_A    (FIBER_RECV_END_ADDR_A - FIBER_RECV_START_ADDR_A)

#define FIBER_RECV_START_ADDR_B           0xD3035     /*B��ȡ������ʼλ��*/
#define FIBER_RECV_END_ADDR_B             0xD6032     /*B��ȡ������ֹλ��*/
#define FIBER_RECV_TOTAL_MEMORY_SIZE_B    (FIBER_RECV_END_ADDR_B - FIBER_RECV_START_ADDR_B)

#define FIBER_IOC_MAGIC 'm'
#define FIBER_IOC_RESET _IO(FIBER_IOC_MAGIC, 0)
#define FIBER_IOC_ALLOC_MEMORY _IOWR(FIBER_IOC_MAGIC, 1, int)
#define FIBER_IOC_CLEAN_BUFFER _IOWR(FIBER_IOC_MAGIC, 2, int)  /*�������*/
#define FIBER_IOC_MAXNR 2

#define FIBER_DEBUG 1

struct fiber_dev
{
    struct cdev cdev;               /*cdev�ṹ��*/
    short b_use;                    /*�Ƿ�ʹ��*/
    __iomem uint8_t* send_addr;     /*���͵�ַ*/
    __iomem uint8_t* recv_addr;     /*���յ�ַ*/
    __iomem int32_t* send_sn_addr;  /*����֡�ŵĵ�ַ*/
    __iomem int32_t* recv_sn_addr;  /*����֡�ŵĵ�ַ*/
    int32_t buf_size;               /*���͵����ݴ�С*/
    int32_t send_sn;                /*֡�ţ���Ϊ�����Ƿ񱻶�ȡ������*/
    int32_t last_recv_sn;           /*֡�ţ���Ϊ�����Ƿ񱻶�ȡ������*/
    __iomem uint8_t* recv_head_addr;/*����֡ͷ��ַ*/

    struct fiber_dev* peer_fiber_dev;   /*������ڶ�Ӧ������һ���ڵ��豸*/
};

#endif /*!_fiber_h__*/
