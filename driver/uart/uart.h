/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 ����        : ������
 �汾        : 1.0
 ��������    : 2014��3��21�� 8:42:04
 ��;        : uart��������
 ��ʷ�޸ļ�¼: v1.0    ����
**********************************************************************/
#ifndef _uart_h__
#define _uart_h__

#define UART_MAJOR                       0       /*Ԥ���uart�����豸�ţ���Ϊ0��ʾ��̬*/
#define UART_DEVS_NR                     4       /*���������豸��һ����ȡA�ڣ�һ����ȡB��*/
#define UART1_MINOR_A                    0       /*1��A��ʹ�õ����豸��*/
#define UART1_MINOR_B                    1       /*1��B��ʹ�õ����豸��*/
#define UART2_MINOR_A                    2       /*2��A��ʹ�õ����豸��*/
#define UART2_MINOR_B                    3       /*2��B��ʹ�õ����豸��*/

#define UART_IO_PORT                     0x27f   /*io������ַ*/
#define UART_IO_SELECT_VALUE             0x01    /*ѡͨ�������*/
#define UART_IO_RESET_VALUE              0xA5    /*io�������ڿ���״̬���*/

#define UART1_IO_WRITE_DATA_LEN_PORT     0x27c   /*1�ſ�д�����ݳ��ȵĶ˿�*/
#define UART1_IO_WRITE_SEM_PORT          0x27B   /*1�ſ�����д����ʱ���ź����˿�*/

#define UART2_IO_WRITE_DATA_LEN_PORT     0x27e   /*2�ſ�д�����ݳ��ȵĶ˿�*/
#define UART2_IO_WRITE_SEM_PORT          0x27d   /*2�ſ�����д����ʱ���ź����˿�*/

/*����1�ŷ��͵�ַ*/
#define UART1_SEND_START_ADDR            0xD0000
#define UART1_SEND_END_ADDR              0xD007F

/*����2�ŷ��͵�ַ*/
#define UART2_SEND_START_ADDR            0xD0080
#define UART2_SEND_END_ADDR              0xD1080

#define UART1_SEND_TOTAL_MEMORY_SIZE     (UART1_SEND_END_ADDR - UART1_SEND_START_ADDR)
#define UART2_SEND_TOTAL_MEMORY_SIZE     (UART2_SEND_END_ADDR - UART2_SEND_START_ADDR)

/*��������ʱFIFO�ı�־*/
#define UART_FIFO_HAS_DATA               0x01        /*FIFO������־*/
#define UART_FIFO_NO_DATA                0x00        /*FIFOû������־*/

/*����1��������ַ���˿�*/
#define UART1_RECV_FIFO_ADDR_A           0xD0007     /*1��������ȡFIFO��ַ*/
#define UART1_RECV_FIFO_PORT_A           0x279       /*1��������ȡFIFO�˿�*/
#define UART1_RECV_FIFO_ADDR_B           0xD000A     /*2��������ȡFIFO��ַ*/
#define UART1_RECV_FIFO_PORT_B           0x277       /*2��������ȡFIFO�˿�*/

/*����2��������ַ���˿�*/
#define UART2_RECV_FIFO_ADDR_A           0xD0017     /*1��������ȡFIFO��ַ*/
#define UART2_RECV_FIFO_PORT_A           0x269       /*1��������ȡFIFO�˿�*/
#define UART2_RECV_FIFO_ADDR_B           0xD001A     /*2��������ȡFIFO��ַ*/
#define UART2_RECV_FIFO_PORT_B           0x267       /*2��������ȡFIFO�˿�*/

#define UART_FALSE                       0x55        /*�ٱ�־*/
#define UART_TRUE                        0xAA        /*���־*/

#define UART_IOC_MAGIC  'k'
#define UART_IOCRESET    _IO(UART_IOC_MAGIC, 0)
#define UART_IOC_CLEAN_MEMORY _IOWR(UART_IOC_MAGIC,  60, int)
#define UART_IOC_MAXNR 1

#define UART_DEBUG 1

struct uart_dev
{
    struct cdev cdev;                      /*cdev�ṹ��*/

    unsigned char* recv_addr;              /*����֡�ռ�*/
    unsigned char* send_addr;              /*����֡�ռ�*/
    short recv_port;                       /*���ܶ˿�*/
    short send_port;                       /*���ݷ��Ͷ˿�*/
    short send_len_port;                   /*���ݳ��ȷ��Ͷ˿�*/
    short send_max_len;                    /*���·������ݵ���󳤶�*/
    short b_use;                           /*��ʹ�ñ�־*/
    struct semaphore sem;                  /*�ź���*/
};

#endif /*!_uart_h__*/

