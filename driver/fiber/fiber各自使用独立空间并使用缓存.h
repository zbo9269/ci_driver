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

#define FIBER_SEND_START_ADDR             0xD3000
#define FIBER_SEND_END_ADDR               0xD3FFF
#define FIBER_SEND_TOTAL_MEMORY_SIZE      (FIBER_SEND_END_ADDR - FIBER_SEND_START_ADDR)

#define FIBER_HEAD_SIZE                   4
#define FIBER_SEND_MAX_MEMORY_SIZE        (FIBER_SEND_TOTAL_MEMORY_SIZE - 2)

#define FIBER_RECV_START_ADDR_A           0xD0035     /*A��ȡ������ʼλ��*/
#define FIBER_RECV_END_ADDR_A             0xD1032     /*A��ȡ������ֹλ��*/
#define FIBER_RECV_TOTAL_MEMORY_SIZE_A    (FIBER_RECV_END_ADDR_A - FIBER_RECV_START_ADDR_A)

#define FIBER_RECV_START_ADDR_B           0xD2033     /*B��ȡ������ʼλ��*/
#define FIBER_RECV_END_ADDR_B             0xD3030     /*B��ȡ������ֹλ��*/
#define FIBER_RECV_TOTAL_MEMORY_SIZE_B    (FIBER_RECV_END_ADDR_B - FIBER_RECV_START_ADDR_B)

#define FIBER_IOC_MAGIC 'm'
#define FIBER_IOC_RESET _IO(FIBER_IOC_MAGIC, 0)
#define FIBER_IOC_ALLOC_MEMORY _IOWR(FIBER_IOC_MAGIC, 1, int)
#define FIBER_IOC_CLEAN_BUFFER _IOWR(FIBER_IOC_MAGIC, 2, int)  /*�������*/
#define FIBER_IOC_MAXNR 2

#define FIBER_DEBUG 1

struct fiber_data
{
    uint8_t* data_addr;     /*���͵�ַ*/
    uint32_t* sn_addr;      /*���sn�ĵ�ַ*/
};
struct fiber_dev
{
    struct cdev cdev;               /*cdev�ṹ��*/
    short b_use;                    /*�Ƿ�ʹ��*/
    struct fiber_data send_data;    /*���͵�ַ*/
    struct fiber_data recv_data;    /*���͵�ַ*/
    __iomem uint8_t* recv_addr;     /*���ܵ�ַ*/
    __iomem uint8_t* send_addr;     /*���ܵ�ַ*/
    uint8_t* recv_buff;             /*�������ݻ�������ַ*/
    uint8_t* send_buff;             /*�������ݻ�������ַ*/
    int32_t buf_size;               /*���͵����ݴ�С*/
    int32_t send_sn;                /*֡�ţ���Ϊ�����Ƿ񱻶�ȡ������*/
    int32_t last_recv_sn;           /*֡�ţ���Ϊ�����Ƿ񱻶�ȡ������*/
    short recv_lock_port;

    struct fiber_dev* peer_fiber_dev;   /*������ڶ�Ӧ������һ���ڵ��豸*/
};

#endif /*!_fiber_h__*/
