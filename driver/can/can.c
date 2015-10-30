/*********************************************************************
 Copyright (C), 2014,  Co.Hengjun, Ltd.
 
 ����        : �ξ�̩
 �汾        : 1.0
 ��������    : 2014��8��12�� 14:07:05
 ��;        : 
 ��ʷ�޸ļ�¼: v1.0    ����
    v1.1    2014.08.12 ������
               1.���ݴ���淶Ҫ��Ϊ�������ע�Ͳ����Ĳ�����Ҫ����
               2.���proc�ṹ
    v2.0    2015.03.31  ������
        ���SJA��飬���ڴ���
**********************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>   
#include <asm/io.h>
#include <linux/cdev.h>  
#include <linux/proc_fs.h>

#include "can.h"

MODULE_AUTHOR("Hjt");
MODULE_LICENSE("Dual BSD/GPL");

/*������Ÿõ�ַ�Ƿ�ʹ�ñ�־*/
static unsigned short can_address_mask[EEU_GA_NUM];

static int can_major = CAN_MAJOR;
static int can_minor = 0;
static int can_delay_counter = 10; /*����ӳٴ���*/

static char* send_addr = NULL;

static short read_ga_address = 0;   /*��ȡ���ݵ����ص�ַ*/
static short read_ea_address = 0;   /*��ȡ���ݵĵ��ӵ�Ԫ��ַ*/

static struct can_pair_devs can_pair_dev; /*can�豸�ṹ��*/

module_param(can_major, int, S_IRUGO);

/*
 ��������    : ���豸
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : @inode �豸�ڵ�
              @filp �ļ��豸ָ��
 ����        : �ξ�̩
 ����        : 2014��8��18�� 10:40:54
*/
static int can_open(struct inode* inode, struct file* filp)
{
    struct can_dev* dev;
    int minor = MINOR(inode->i_rdev);

    /*���豸mknod��ʱ���ܴ�������CAN_DEVS_NR���豸*/
    if (minor >= CAN_DEVS_NR)
    {
        return -ENODEV;
    }
    dev = container_of(inode->i_cdev,struct can_dev,cdev);

    /*���ǵ�Ŀ��������һ��openϵͳ���ã��������д򿪲���ȫ���ܾ�*/
    if (dev->b_use)
    {
        return -EBUSY;
    }

    /*���豸�ṹ��ָ�븳ֵ���ļ�˽������ָ��*/
    filp->private_data = dev;

    dev->b_use = 1;

    return 0;
}


/*
 ��������    : �ļ��ر�ʱ���ô˺���
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : @inode �ļ�inode�ڵ�
              @filp �ļ�ָ��
 ����        : �ξ�̩
 ����        : 2014��8��18�� 10:53:38
*/
static int can_release(struct inode* inode, struct file* filp)
{
    struct can_dev* dev = filp->private_data;

    dev->b_use = 0;

    return 0;
}
/*
 ��������    : �ļ���дƫ�����ú���
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : ��
 ����        : �ξ�̩
 ����        : 2014��8��18�� 10:56:05
*/
static loff_t can_llseek(struct file *filp, loff_t offset, int whence)
{
    return -ENOSYS;
}

/*
 ��������    : �豸������
 ����ֵ      : ��ȡ���ַ��Ĵ�С
 ����        : 
 ����        : �ξ�̩
 ����        : 2014��8��18�� 11:00:55
*/
static ssize_t can_read(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
    int ret = 0;
    int k = 0;
    int32_t off_ea = read_ea_address + 1;
    unsigned char* data_flag_addr = NULL;       /*���ض�Ӧ�Ŀռ��Ƿ�������־��ַ*/
    unsigned char* fpga_write_flag_addr = NULL; /*FPGA������˫��ramд���ݵ�ַ��־*/
    unsigned char* pc104_read_flag_addr = NULL; /*pc104���ڴ�˫��ram���ж���ڱ�־*/
    unsigned char* data_space_addr = NULL;      /*���ݿռ��ַ*/

    struct can_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/

    if (CAN_FRAME_LEN < size)
    {
        return -EINVAL;
    }

    data_flag_addr = dev->recv_addr + DATA_FLAG_OFFS + (read_ga_address * EEU_EA_NUM) + off_ea;
    fpga_write_flag_addr = dev->recv_addr + FPG_WRITE_FLAG_OFFS + (read_ga_address * EEU_EA_NUM) + off_ea;
    pc104_read_flag_addr = dev->recv_addr + PC104_READ_OFFS + (read_ga_address * EEU_EA_NUM) + off_ea;
    data_space_addr = dev->recv_addr + DATA_SPACE_OFFS + (( read_ga_address * EEU_EA_NUM) + off_ea) * CAN_FRAME_LEN;

    /*����Ҫ���ĵ�ַ�Ƿ�ʹ��*/
    if (!(can_address_mask[read_ga_address] & (1 << read_ea_address)))
    {
        return -EINVAL;
    }

    outb(CAN_IO_SELECT_VALUE,CAN_IO_PORT);

    /*�²�ѯ�Ƿ������ռ��־�����û�����򷵻�*/
    do 
    {
        ret = ioread8(data_flag_addr);
        udelay(10);
        k++;
    } while (FLAG_TRUE != ret && k < can_delay_counter);

    if (FLAG_TRUE != ret)
    {
        ret = -ETIME;
#if 0
        printk(KERN_INFO "can_read:data_flag_addr:%#x,ret:%#x\n",data_flag_addr,ret);
#endif
        goto fail;
    }

    /*�ٲ�ѯFPGAд���ر�־,0XAAΪд��Ч,˵��FPGA����д������,��ʱ�յȴ�*/
    k = 0;
    do 
    {
        ret = ioread8(fpga_write_flag_addr);
        udelay(10);
        k++;
    } while (FPGA_FLAG_BUSY == ret && k < can_delay_counter);

    if (FPGA_FLAG_BUSY == ret)
    {
        ret = -ETIME;
#if 0
        printk(KERN_INFO "can_read:fpga_write_flag_addr:%#x,ret:%#x\n",fpga_write_flag_addr,ret);
#endif
        goto fail;
    }
    /*��鵽FPGA����д�豸����ر�־��ע��Чʱ���轫PC104��FPGA�豸��״̬����Ч��*/
    iowrite8(FPGA_FLAG_BUSY, pc104_read_flag_addr);

    /*��ȡ�������ݿռ�,ÿ�����ص����ݿռ�Ϊ13���ֽ�*/
    if (__copy_to_user(buf, data_space_addr,size))
    {
        printk(KERN_ERR "can_read: copy_to_user error\n");

        ret = -EIO;
        goto fail;
    }
    rmb();

    /*��PC104��ѯ���˵�ַ�������������ݿռ��־����Ч*/
    iowrite8(FLAG_FALSE, data_flag_addr);
    /*��PC104��ѯ���˵�ַ����ʹPC104��FPGA��־��ע��Ч��*/
    iowrite8(FLAG_FALSE, pc104_read_flag_addr);

    outb(CAN_IO_RESET_VALUE,CAN_IO_PORT);

    return size;
fail:
    outb(CAN_IO_RESET_VALUE,CAN_IO_PORT);

    return ret;
}
/*
 ��������    : �豸д����
 ����ֵ      : д�����ݵĴ�С
 ����        : 
 ����        : �ξ�̩
 ����        : 2014��8��18�� 11:00:23
*/
static ssize_t can_write(struct file *filp, const char __user *buf,size_t size, loff_t *ppos)
{
    if (NULL == send_addr)
    {
        return -EFAULT;
    }

    outb(CAN_IO_SELECT_VALUE,CAN_IO_PORT);

    /*�û��ռ�->�ں˿ռ�*/
    if (copy_from_user(send_addr, buf, size))
    {
        return -EIO;
    }

    /*��CAN_FRAME_LEN_PORT��ַд�뱾�ι����͵�֡��*/
    outb(size / CAN_FRAME_LEN,CAN_FRAME_LEN_PORT);

    /*��CAN_IO_FRAME_SEND��ַд��1����FPGA��ʼ�·�����*/
    outb(FLAG_TRUE,CAN_IO_FRAME_SEND); 

    /*���ѡ��˿�����*/
    outb(CAN_IO_RESET_VALUE,CAN_IO_PORT);

    return size;
}

/*
 ��������    : �������
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : �ξ�̩
 ����        : 2014��8��18�� 11:02:40
*/
static int can_ioctl(struct inode *inodep, struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int address = 0;
    u8 sja_status_flag = 0;
    void __user* argp = (void __user*)arg;

    /* don't even decode wrong cmds: better returning ENOTTY than EFAULT */
    if (_IOC_TYPE(cmd) != CAN_IOC_MAGIC) 
    {
        printk(KERN_ERR "can_ioctl:_IOC_TYPE(%d) must be %d\n",_IOC_TYPE(cmd),CAN_IOC_MAGIC);
        return -ENOTTY;
    }
    if (_IOC_NR(cmd) > CAN_IOC_MAXNR) 
    {
        printk(KERN_ERR "can_ioctl:_IOC_NR(%d) must be less than %d\n",_IOC_NR(cmd),CAN_IOC_MAXNR);
        return -ENOTTY;
    }

    outb(CAN_IO_SELECT_VALUE, CAN_IO_PORT);

    switch (cmd)
    {
    case CAN_IOCRESET:

        outb(0x01, CAN_SJA_RESET_PORT);

        ret = 0;
        break;
    case CAN_IOC_SET_DATAMASK:
        if (NULL == argp)
        {
            printk(KERN_ERR "can_ioctl[arg]:user data_mask is NULL\n");
            ret = -ENOMEM;
        }
        /*�û��ռ�->�ں˿ռ�*/
        if (copy_from_user(can_address_mask, argp, sizeof(can_address_mask)))
        {
            ret = -EIO;
        }
        break;
    case CAN_IOC_SET_DELAY_COUNTER:
        ret = __get_user(can_delay_counter, (int __user *) arg);
        break;
    case CAN_IOC_SET_READ_ADDRESS:
        address = arg;
        /*���ص�ַΪ��16λ�����ӵ�Ԫ��ַΪ��16λ*/
        read_ga_address = (address & 0xffff0000) >> 16;
        read_ea_address = address & 0x0000ffff;
        if (EEU_EA_NUM < read_ea_address || EEU_GA_NUM < read_ga_address)
        {
            ret = -EINVAL;
        }
        else
        {
            ret = 0;
        }
        break;
    case CAN_IOC_GET_SJA_STATUS_FLAG:
        sja_status_flag = inb(CAN_SJA_STATUS_FLAG);
        if (__copy_to_user(argp, &sja_status_flag, sizeof(sja_status_flag)))
        {
            printk(KERN_ERR "can_ioctl: copy sja status flag to user failed\n");

            ret = -EIO;
        }
        else
        {
            ret = 0;
        }
        rmb();
        break;
    default:
        ret = -EINVAL;
        break;
    }

    outb(CAN_IO_RESET_VALUE, CAN_IO_PORT);

    return ret;
}

/*�ļ������ṹ��*/
static const struct file_operations can_fops =
{
    .owner      = THIS_MODULE,
    .llseek     = can_llseek,
    .read       = can_read,
    .write      = can_write,
    .ioctl      = can_ioctl,
    .open       = can_open,
    .release    = can_release,
};

#ifdef CAN_DEBUG
/*
 ��������    : ��ӡ�ڴ���Ϣ
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��3��26�� 13:47:05
*/
static int can_proc_mem(char *buf, char **start, off_t offset,
                       int count, int *eof, void *data)
{
    int i = 0;
    int len = 0;

    len += sprintf(buf + len, "can_mask\n");

    for (i = 0;i < EEU_GA_NUM;i++)
    {
        len += sprintf(buf + len, "%04x ",can_address_mask[i]);
    }

    len += sprintf(buf + len,"\n");

    len += sprintf(buf + len,"can_delay_counter:%d\n",can_delay_counter);

    len += sprintf(buf + len,"read_ga_address:%d\n",read_ga_address);

    len += sprintf(buf + len,"read_ea_address:%d\n",read_ea_address);

    *eof = 1;

    return len;
}
/*
 ��������    : ����proc�ӿ��ļ�
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��3��19�� 16:27:58
*/
static void can_create_proc(void)
{
    create_proc_read_entry("can_mem",0,NULL, can_proc_mem, NULL);

    return;
}
/*
 ��������    : ɾ��proc�ӿ��ļ�
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��3��19�� 16:27:58
*/
static void can_remove_proc(void)
{
    /* no problem if it was not registered */
    remove_proc_entry("can_mem", NULL);
}

#endif /* CAN_DEBUG */
/*
 ��������    : ע�Ტ��ʼ���豸�ṹ
 ����ֵ      : ��
 ����        : @dev �豸�ṹ��ָ��
              @minor ���豸��
 ����        : �ξ�̩
 ����        : 2014��8��18�� 10:38:20
*/
static void can_setup_cdev(struct can_dev *dev, int minor)
{
    int err, devno = MKDEV(can_major, can_minor + minor);

    cdev_init(&dev->cdev, &can_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &can_fops;
    err = cdev_add(&dev->cdev, devno, 1);

    return;
}
/*
 ��������    : canģ���ʼ�����ú���
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : ��
 ����        : �ξ�̩
 ����        : 2014��8��12�� 14:12:56
*/
static int __init can_init(void)
{
    int result;
    dev_t dev = 0;

    if (can_major)
    {
        dev = MKDEV(can_major, can_minor);
        result = register_chrdev_region(dev,CAN_DEVS_NR,"can");
    }
    else
    {
        result = alloc_chrdev_region(&dev,can_minor,CAN_DEVS_NR,"can");
        can_major = MAJOR(dev);
    }

    if (result < 0)
    {
        printk(KERN_WARNING "can: can't get major %d\n", can_major);
        return result;
    }

    send_addr = ioremap(CAN_SEND_START_ADDR,CAN_SEND_TOTAL_MEMORY_SIZE);

#ifdef CAN_DEBUG
    /*����proc�ӿ�*/
    can_create_proc();
#endif /*CAN_DEBUG*/

    can_pair_dev.can_dev_a.b_use = 0;
    can_pair_dev.can_dev_a.recv_addr = ioremap(CAN_RECV_START_ADDR_A,CAN_RECV_TOTAL_MEMORY_SIZE_A); ;

    can_pair_dev.can_dev_b.b_use = 0;
    can_pair_dev.can_dev_b.recv_addr = ioremap(CAN_RECV_START_ADDR_B,CAN_RECV_TOTAL_MEMORY_SIZE_B); ;

    /*can�������豸�ֱ��Ӧ0��1���豸��*/
    can_setup_cdev(&can_pair_dev.can_dev_a, CAN_MINOR_A);

    can_setup_cdev(&can_pair_dev.can_dev_b, CAN_MINOR_B);

    printk(KERN_INFO "can init!\n");

    return 0;
}
/*
 ��������    : canģ���˳����ú���
 ����ֵ      : ��
 ����        : ��
 ����        : �ξ�̩
 ����        : 2014��8��12�� 14:12:10
*/
static void __exit can_exit(void)
{
    cdev_del(&can_pair_dev.can_dev_a.cdev);   /*ע��cdev*/
    cdev_del(&can_pair_dev.can_dev_b.cdev);   /*ע��cdev*/
    unregister_chrdev_region(MKDEV(can_major, can_minor), CAN_DEVS_NR); /*�ͷ��豸��*/

#ifdef CAN_DEBUG
    can_remove_proc();
#endif /*CAN_DEBUG*/

    printk(KERN_INFO "can exit!\n");

    return;
}

module_init(can_init);
module_exit(can_exit);
