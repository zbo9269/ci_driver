/*********************************************************************
 Copyright (C), 2014,  Co.Hengjun, Ltd.
 
 ����        : ������
 �汾        : 1.0
 ��������    : 2014��8��12�� 13:54:36
 ��;        : can����������
 ��ʷ�޸ļ�¼: v1.0    ����
    v2.0    zhangys ���SJA���ڴ�ļ��
**********************************************************************/

#ifndef _can_h__
#define _can_h__
 
#define CAN_MAJOR               0       /*0 meaning dynamic alloc device id*/
#define CAN_MINOR_A             0x00
#define CAN_MINOR_B             0x01

#define CAN_DEVS_NR             2       /*can���������豸��A�ں�B��*/

#define CAN_IO_PORT             0x27f
#define CAN_IO_SELECT_VALUE     0x04
#define CAN_IO_RESET_VALUE      0xAA

#define CAN_SJA_RESET_PORT      0x242

#define CAN_FRAME_LEN_PORT      0x24a
#define CAN_IO_FRAME_SEND       0x24b

/*
 * 20150701 REQUIREMENT��CANͨ�Ű�ͨ��һ��ʱ���������������Ҫ��λ����
 * SJA1000.pdf˵���ĵ���(Ӣ���ĵ�6.3.5��,Table 5,Page 15)ָ����SJA״̬
 * �Ĵ�����6λ�͵�7λ���жϴ���״̬��Ϊ�ˣ�ͨ���ö˿ڽ��üĴ���ӳ�䵽PC104
 * �Ա��ȡ
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
#define FPG_WRITE_FLAG_OFFS     0 /* FPGA_Aд���ر�־��ַƫ��*/
#define PC104_READ_OFFS CAN_FLAG_LEN   /* PC104��FPGA_A���ر�־��ַƫ�� 0XAAΪ����Ч 0X55Ϊ��Ч*/
#define DATA_FLAG_OFFS (PC104_READ_OFFS + CAN_FLAG_LEN)    /* �������ݿռ��־��ַƫ�� 0X01Ϊ������Ч��־ 0X00Ϊ�ձ�־*/
#define DATA_SPACE_OFFS (DATA_FLAG_OFFS + CAN_FLAG_LEN) /* �������ݿռ��ַƫ�� ÿ�����ص����ݿռ�Ϊ13���ֽ�*/
#define DATAB_FLAG_OFFS (DATA_SPACE_OFFS + CAN_DATA_LEN)     /* ����Ӱ�����ݿռ��־��ַƫ�� 0X01Ϊ������Ч��־ 0X00Ϊ�ձ�־*/
#define DATAB_SPACE_OFFS (DATAB_FLAG_OFFS + CAN_FLAG_LEN)    /* ����Ӱ�����ݿռ��ַƫ�� ÿ�����ص����ݿռ�Ϊ13���ֽ�*/

#define CAN_RECV_START_ADDR_A           0xD7000     /*A��ȡ������ʼλ��*/
#define CAN_RECV_END_ADDR_A             0xDAA1F     /*A��ȡ������ֹλ��*/
#define CAN_RECV_TOTAL_MEMORY_SIZE_A    (CAN_RECV_END_ADDR_A - CAN_RECV_START_ADDR_A)

#define CAN_RECV_START_ADDR_B           0xDb000     /*B��ȡ������ʼλ��*/
#define CAN_RECV_END_ADDR_B             0xDEA1F     /*B��ȡ������ֹλ��*/
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

/*can�豸�ṹ��*/
struct can_dev
{
    struct cdev cdev;                       /*cdev�ṹ��*/
    unsigned char* recv_addr;               /*�������ݵ�ַ*/
    short b_use;                            /*���豸�Ƿ�ʹ�ñ�־*/
};

struct can_pair_devs
{
    struct can_dev can_dev_a;
    struct can_dev can_dev_b;
};

#endif /*!_can_h__*/
