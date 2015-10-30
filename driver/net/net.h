/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 ����        : ������
 �汾        : 1.0
 ��������    : 2014��2��28�� 9:07:07
 ��;        : ����net�����ṹ
 ��ʷ�޸ļ�¼: v1.0    ����
**********************************************************************/

#ifndef _net_h__
#define _net_h__

#define NET_MAJOR                       0       /*Ԥ���net�����豸�ţ�0��ʾ��̬����*/
#define NET_DEVS_NR                     2       /*���������豸��һ����ȡA�ڣ�һ����ȡB��*/
#define NET_MINOR_A                     0       /*A��ʹ�õ����豸��*/
#define NET_MINOR_B                     1       /*B��ʹ�õ����豸��*/

#define NET_IO_PORT                     0x27f   /*io������ַ*/
#define NET_IO_SELECT_VALUE             0x06    /*ѡͨ�������*/
#define NET_IO_RESET_VALUE              0xA5    /*io�������ڿ���״̬���*/
#define NET_IO_CLEAR_MEMORY_PORT        0x234   /*���ram���ݵĶ˿�*/
#define NET_IO_WRITE_SEM_PORT           0x23B   /*����д����ʱ���ź����˿�*/
#define NET_IO_WRITE_DATA_LEN_HIGH_PORT 0x23E   /*д�����ݵĳ��ȵĸ�λ�Ķ˿�*/
#define NET_IO_WRITE_DATA_LEN_LOW_PORT  0x23D   /*д�����ݵĳ��ȵĵ�λ�Ķ˿�*/

#define NET_RESET_PORT                  0x232   /*��λ����ͨ�Ű�*/

#define NET_SEND_START_ADDR             0xD1000
#define NET_SEND_END_ADDR               0xD17FF
#define NET_SEND_TOTAL_MEMORY_SIZE      (NET_SEND_END_ADDR - NET_SEND_START_ADDR)

#define NET_RECV_START_ADDR_A           0xD4050     /*����A��ȡ������ʼλ��*/
#define NET_RECV_END_ADDR_A             0xD547F     /*����A��ȡ������ֹλ��*/

#define NET_RECV_START_ADDR_B           0xD5580     /*����B��ȡ������ʼλ��*/
#define NET_RECV_END_ADDR_B             0xD69AF     /*����B��ȡ������ֹλ��*/

#define NET_STATION_BUF_SIZE            0x100       /*һ��վ�����ڴ������С*/
#define NET_STATION_SIZE                10          /*����10��վ���ռ�*/

/*!caution,data is not consecutive*/
#define NET_WLOCK_OFF       0                                           /*ÿ������д����ƫ��λ��*/
#define NET_RLOCK_OFF       (NET_WLOCK_OFF + NET_STATION_SIZE)          /*ÿ�����ڶ�����ƫ��λ��*/
#define NET_SEM_OFF         (NET_RLOCK_OFF + NET_STATION_SIZE)          /*ÿ�������ź�����ƫ��λ��*/
#define NET_BUF_OFF         (NET_SEM_OFF + NET_STATION_SIZE + 2)        /*ÿ��������������ƫ��λ��*/
#define NET_SHADOW_SEM_OFF  (NET_BUF_OFF + NET_STATION_BUF_SIZE * NET_STATION_SIZE)    /*ÿ������Ӱ�ӿռ��ź�����ƫ��λ��*/
#define NET_SHADOW_OFF      (NET_SHADOW_SEM_OFF + NET_STATION_SIZE + 6) /*ÿ������Ӱ�ӿռ�����ƫ��*/

#define NET_DEST_IP_ADDR_A              0xD1800     /*Ŀ���ַA�ڵ�ip*/
#define NET_DEST_PORT_ADDR_A            0xD1804     /*Ŀ���ַA�ڵĶ˿�*/

#define NET_DEST_IP_ADDR_B              0xD1806     /*Ŀ���ַB�ڵ�ip*/
#define NET_DEST_PORT_ADDR_B            0xD180A     /*Ŀ���ַB�ڵĶ˿�*/

#define NET_FALSE                       0x55        /*�ٱ�־*/
#define NET_TRUE                        0xAA        /*���־*/

#define NET_IOC_MAGIC  'k'
#define NET_IOCRESET    _IO(NET_IOC_MAGIC, 0)
/*Լ��������50��ʼ*/
#define NET_IOC_DEST_IP_A _IOWR(NET_IOC_MAGIC,  50, int)
#define NET_IOC_DEST_PORT_A _IOWR(NET_IOC_MAGIC,  51, int)
#define NET_IOC_DEST_IP_B _IOWR(NET_IOC_MAGIC,  52, int)
#define NET_IOC_DEST_PORT_B _IOWR(NET_IOC_MAGIC,  53, int)
#define NET_IOC_RESET _IOWR(NET_IOC_MAGIC,  54, int)
#define NET_IOC_MAXNR 5

#define NET_DEBUG 1

struct net_station
{
    unsigned char* data_sem;  /*�����ź���*/
    unsigned char* rlock;     /*����*/
    unsigned char* wlock;     /*д��*/

    unsigned char* data_addr;  /*վ�����ݿռ�*/

    unsigned char* shadow_sem;/*վ��Ӱ�ӿռ��ź���*/
    unsigned char* shadow;    /*վ��Ӱ�ӿռ�*/
};

struct net_dev
{
    struct cdev cdev;                               /*cdev�ṹ��*/

    struct net_station stations[NET_STATION_SIZE];  /*���Ҫ���ܵ�ÿ���豸ά����վ����Ϣ*/
    unsigned char* send_addr;              /*����֡�ռ�*/
    short b_use;                                    /*�Ƿ�ʹ�ñ�־*/
    struct semaphore sem;                           /*�ź���*/
};

#endif /*!_net_h__*/

