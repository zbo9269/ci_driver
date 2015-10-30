/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 ����        : ������
 �汾        : 1.0
 ��������    : 2014��3��21�� 8:40:49
 ��;        : ���ڵ���������
 ��ʷ�޸ļ�¼: v1.0    ����
**********************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>

#include "uart.h"

MODULE_AUTHOR("zhangys");
MODULE_LICENSE("Dual BSD/GPL");

static struct uart_dev uart1_dev_a;    /*1��a���豸*/
static struct uart_dev uart1_dev_b;    /*1��b���豸*/
static struct uart_dev uart2_dev_a;    /*2��a���豸*/
static struct uart_dev uart2_dev_b;    /*2��b���豸*/

static int uart_major = UART_MAJOR;

module_param(uart_major, int, S_IRUGO);

/*
��������    : openϵͳ���õ�ʵ��
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 13:26:23
*/
static int uart_open(struct inode* inode, struct file* filp)
{
    struct uart_dev* dev;
    int num = MINOR(inode->i_rdev);

    /*���豸mknod��ʱ���ܴ�������DPRAM_DEVS_NR���豸*/
    if (UART_DEVS_NR < num)
    {
        return -ENODEV;
    }

    dev = container_of(inode->i_cdev,struct uart_dev,cdev);

    /*���ǵ�Ŀ��������һ��openϵͳ���ã��������д򿪲���ȫ���ܾ�*/
    if (UART_TRUE == dev->b_use)
    {
        return -EBUSY;
    }

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    dev->b_use = UART_TRUE;

    /*���豸�ṹ��ָ�븳ֵ���ļ�˽������ָ��*/
    filp->private_data = dev;

    up(&dev->sem);

    return 0;

}
/*
��������    : �ͷ��豸
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 13:28:10
*/
static int uart_release(struct inode* inode, struct file *filp)
{
    struct uart_dev* dev = filp->private_data;

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    dev->b_use = UART_FALSE;

    up(&dev->sem);

    return 0;
}
/*
��������    : ��ȡ����
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 13:28:38
*/
static ssize_t uart_read(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
    int count = 0;
    int ret = 0;
    unsigned char data[UART1_SEND_TOTAL_MEMORY_SIZE];
    /*TODO����2�ſڲ�������*/

    struct uart_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/

    if (NULL == dev)
    {
        printk(KERN_ERR "uart_read:dev is NULL\n");

        return -ENXIO;
    }
    if (UART1_SEND_TOTAL_MEMORY_SIZE < size)
    {
        printk(KERN_ERR "uart_read:UART1_SEND_TOTAL_MEMORY_SIZE(%d) < size(%d)\n",
            UART1_SEND_TOTAL_MEMORY_SIZE,size);

        return -ENOMEM;
    }

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    outb(UART_IO_SELECT_VALUE,UART_IO_PORT);

    /*has data,read it,until sem is 0x00*/
    count = 0;
    while (0x01 == inb(dev->recv_port))
    {
        data[count++] = ioread8(dev->recv_addr);
        if (count >= size)
        {
            break;
        }
    }
    if (count == 0)
    {
        ret = -EAGAIN;
        goto fail;
    }

    if (copy_to_user(buf, data, count))
    {
        printk(KERN_ERR "uart_read: copy_to_user error\n");

        ret = -ENXIO;
        goto fail;
    }
    up(&dev->sem);

    outb(UART_IO_RESET_VALUE,UART_IO_PORT);

    return count;
fail:
    outb(UART_IO_RESET_VALUE,UART_IO_PORT);
    up(&dev->sem);

    return ret;
}

/*
��������    : д���ݺ���
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 15:45:19
*/
static ssize_t uart_write(struct file *filp, const char __user *buf,size_t size, loff_t *ppos)
{
    int ret = 0;
    int count = 0;
    int write_sem = 0;
    struct uart_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/

    if (dev->send_max_len < size)
    {
        printk(KERN_ERR "uart_write:too many data,%#X(size) > %#X(send_max_len)\n",
            size,dev->send_max_len);

        return -ENOMEM;
    }
    if (NULL == dev)
    {
        printk(KERN_ERR "uart_write:dev is NULL\n");

        return -EFAULT;
    }
    if (NULL == dev->send_addr)
    {
        printk(KERN_ERR "uart_write:dev->send_addr is NULL\n");

        return -EFAULT;
    }

    do
    {
        write_sem = inb(dev->send_port);
        udelay(1);  /*�ӳ�1΢��*/
        count ++;
    } while (0x01 == write_sem && count < 10);

    if (10 == count)
    {
        printk(KERN_ERR "uart_write:try get semaphore of write exceeded 10 times\n");

        ret = -EBUSY;
        goto fail;
    }

    outb(UART_IO_SELECT_VALUE,UART_IO_PORT);

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    /*copy data*/
    if (copy_from_user(dev->send_addr, buf, size))
    {
        printk(KERN_ERR "uart_write: copy_from_user error\n");

        ret = -EIO;
        goto fail;
    }
    /*set data length*/
    outb(size & 0xff,dev->send_len_port);

    /*reset semaphore and send data*/
    outb(0x01,dev->send_port);
    outb(UART_IO_RESET_VALUE,UART_IO_PORT);

    up(&dev->sem);

    return size;

fail:
    /*reset semaphore*/
    outb(0x01,dev->send_port);
    outb(UART_IO_RESET_VALUE,UART_IO_PORT);

    up(&dev->sem);
    return ret;
}
/*
��������    : ͨ��ioctl���п���
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 15:16:52
*/
static int uart_ioctl(struct inode *inodep, struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct uart_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/
    int32_t ret = 0;

    if (NULL == dev)
    {
        return -EFAULT;
    }
    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != UART_IOC_MAGIC)
    {
        printk(KERN_ERR "uart_ioctl:_IOC_TYPE(%d) must be %d\n",_IOC_TYPE(cmd),UART_IOC_MAGIC);

        return -ENOTTY;
    }

    outb(UART_IO_SELECT_VALUE,UART_IO_PORT);

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }
    switch (cmd)
    {
    case UART_IOC_CLEAN_MEMORY:
        while (0x01 == inb(dev->recv_port))
        {
            /*��ֹ�������Ż�*/
            ret = ioread8(dev->recv_addr);
        }
        ret = 0;
        break;
    default:
        ret = -ENOSYS;
    }

    up(&dev->sem);
    outb(UART_IO_RESET_VALUE,UART_IO_PORT);

    return ret;
}

static const struct file_operations uart_fops =
{
    .owner = THIS_MODULE,
    .read = uart_read,
    .write = uart_write,
    .ioctl = uart_ioctl,
    .open = uart_open,
    .release = uart_release,
};

#ifdef UART_DEBUG /* use proc only if debugging */

static int uart_proc_mem(char *buf, char **start, off_t offset,
                       int count, int *eof, void *data)
{
    int len = 0;

    len += sprintf(buf + len,"\t\t"
                             "recv_addr\t"
                             "send_addr\t"
                             "recv_port\t"
                             "b_use\n"
                             );

    /*print uart1_dev_a*/
    if (down_interruptible(&uart1_dev_a.sem))
    {
        return -ERESTARTSYS;
    }
    len += sprintf(buf + len,"uart1_dev_a\t%#x\t %#x\t %#x\t\t %#x\t\n",
        (int)uart1_dev_a.recv_addr,
        (int)uart1_dev_a.send_addr,
        (int)uart1_dev_a.recv_port,
        (int)uart1_dev_a.b_use);

    up(&uart1_dev_a.sem);

    /*print uart1_dev_b*/
    if (down_interruptible(&uart1_dev_b.sem))
    {
        return -ERESTARTSYS;
    }
    len += sprintf(buf + len,"uart1_dev_b\t%#x\t %#x\t %#x\t\t %#x\t\n",
        (int)uart1_dev_b.recv_addr,
        (int)uart1_dev_b.send_addr,
        (int)uart1_dev_b.recv_port,
        (int)uart1_dev_b.b_use);

    up(&uart1_dev_b.sem);

    /*print uart2_dev_a*/
    if (down_interruptible(&uart2_dev_a.sem))
    {
        return -ERESTARTSYS;
    }
    len += sprintf(buf + len,"uart2_dev_a\t%#x\t %#x\t %#x\t\t %#x\t\n",
        (int)uart2_dev_a.recv_addr,
        (int)uart2_dev_a.send_addr,
        (int)uart2_dev_a.recv_port,
        (int)uart2_dev_a.b_use);

    up(&uart2_dev_a.sem);

    /*print uart2_dev_b*/
    if (down_interruptible(&uart2_dev_b.sem))
    {
        return -ERESTARTSYS;
    }
    len += sprintf(buf + len,"uart2_dev_b\t%#x\t %#x\t %#x\t\t %#x\t\n",
        (int)uart2_dev_b.recv_addr,
        (int)uart2_dev_b.send_addr,
        (int)uart2_dev_b.recv_port,
        (int)uart2_dev_b.b_use);

    up(&uart2_dev_b.sem);

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
static void uart_create_proc(void)
{
    create_proc_read_entry("uart_mem",0,NULL, uart_proc_mem, NULL);

    return;
}
/*
 ��������    : ɾ��proc�ӿ��ļ�
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��3��19�� 16:27:58
*/
static void uart_remove_proc(void)
{
    /* no problem if it was not registered */
    remove_proc_entry("uart_mem", NULL);
}

#endif /* UART_DEBUG */
/*
��������    : ������������豸��װ���ں���
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 10:24:02
*/
static int uart_setup_cdev(struct cdev* dev, int minor)
{
    int err;
    dev_t devno = MKDEV(uart_major,minor);

    cdev_init(dev, &uart_fops);
    dev->owner = THIS_MODULE;
    dev->ops = &uart_fops;

    err = cdev_add(dev, devno, 1);

    if (err)
    {
        printk(KERN_ERR "Error %d adding uart %d\n", err, minor);

        return err;
    }

    return 0;
}

/*
 ��������    : Ӳ�����
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��3��21�� 10:24:46
*/
static int __init uart_hardware_check(void)
{
    return 0;
}
/*
��������    : ʼ������
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 10:23:31
*/
static int __init uart_init(void)
{
    int ret;
    dev_t devno = 0;

    if (uart_major)
    {
        devno = MKDEV(uart_major, 0);
        ret = register_chrdev_region(devno, UART_DEVS_NR, "uart");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, UART_DEVS_NR, "uart");
        uart_major = MAJOR(devno);
    }

    if (ret < 0)
    {
        printk(KERN_WARNING "uart: can't get major %d\n", uart_major);

        return ret;
    }

    /*���Ӳ��*/
    if (0 != uart_hardware_check())
    {
        goto fail;
    }

    /*��ʼ��1�ŵ�A�˿�*/
    uart1_dev_a.b_use = UART_FALSE;
    uart1_dev_a.send_addr = ioremap(UART1_SEND_START_ADDR,UART1_SEND_TOTAL_MEMORY_SIZE);
    uart1_dev_a.recv_addr = ioremap(UART1_RECV_FIFO_ADDR_A,1);
    uart1_dev_a.recv_port = UART1_RECV_FIFO_PORT_A;
    uart1_dev_a.send_port = UART1_IO_WRITE_SEM_PORT;
    uart1_dev_a.send_len_port = UART1_IO_WRITE_SEM_PORT;
    uart1_dev_a.send_max_len = UART1_SEND_TOTAL_MEMORY_SIZE;
    sema_init(&uart1_dev_a.sem,1);
    uart_setup_cdev(&uart1_dev_a.cdev, UART1_MINOR_A);

    /*��ʼ��1�ŵ�B�˿�*/
    uart1_dev_b.b_use = UART_FALSE;
    uart1_dev_b.send_addr = ioremap(UART1_SEND_START_ADDR,UART1_SEND_TOTAL_MEMORY_SIZE);
    uart1_dev_b.recv_addr = ioremap(UART1_RECV_FIFO_ADDR_B,1);
    uart1_dev_b.recv_port = UART1_RECV_FIFO_PORT_B;
    uart1_dev_b.send_port = UART1_IO_WRITE_SEM_PORT;
    uart1_dev_b.send_len_port = UART1_IO_WRITE_SEM_PORT;
    uart1_dev_b.send_max_len = UART1_SEND_TOTAL_MEMORY_SIZE;
    sema_init(&uart1_dev_b.sem,1);
    uart_setup_cdev(&uart1_dev_b.cdev, UART1_MINOR_B);

    /*��ʼ��2�ŵ�A�˿�*/
    uart2_dev_a.b_use = UART_FALSE;
    uart2_dev_a.send_addr = ioremap(UART2_SEND_START_ADDR,UART2_SEND_TOTAL_MEMORY_SIZE);
    uart2_dev_a.recv_addr = ioremap(UART2_RECV_FIFO_ADDR_A,1);
    uart2_dev_a.recv_port = UART2_RECV_FIFO_PORT_A;
    uart2_dev_a.send_port = UART2_IO_WRITE_SEM_PORT;
    uart2_dev_a.send_len_port = UART2_IO_WRITE_SEM_PORT;
    uart2_dev_a.send_max_len = UART2_SEND_TOTAL_MEMORY_SIZE;
    sema_init(&uart2_dev_a.sem,1);
    uart_setup_cdev(&uart2_dev_a.cdev, UART2_MINOR_A);

    /*��ʼ��2�ŵ�B�˿�*/
    uart2_dev_b.b_use = UART_FALSE;
    uart2_dev_b.send_addr = ioremap(UART2_SEND_START_ADDR,UART2_SEND_TOTAL_MEMORY_SIZE);
    uart2_dev_b.recv_addr = ioremap(UART2_RECV_FIFO_ADDR_B,1);
    uart2_dev_b.recv_port = UART2_RECV_FIFO_PORT_B;
    uart2_dev_b.send_port = UART2_IO_WRITE_SEM_PORT;
    uart2_dev_b.send_len_port = UART2_IO_WRITE_SEM_PORT;
    uart2_dev_b.send_max_len = UART2_SEND_TOTAL_MEMORY_SIZE;
    sema_init(&uart2_dev_b.sem,1);
    uart_setup_cdev(&uart2_dev_b.cdev, UART2_MINOR_B);

#ifdef UART_DEBUG
    /*����proc�ӿ�*/
    uart_create_proc();
#endif /*UART_DEBUG*/
    printk(KERN_INFO "uart init.\n");

    return 0;

fail:
    unregister_chrdev_region(MKDEV(uart_major, 0), UART_DEVS_NR); /*�ͷ��豸��*/

    return ret;
}
/*
��������    : ������������ע������
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 10:23:45
*/
static void __exit uart_exit(void)
{
    /*�����豸*/
    cdev_del(&uart1_dev_a.cdev);
    cdev_del(&uart1_dev_b.cdev);
    cdev_del(&uart2_dev_a.cdev);
    cdev_del(&uart2_dev_b.cdev);

    unregister_chrdev_region(MKDEV(uart_major, 0), UART_DEVS_NR); /*�ͷ��豸��*/

    /*���շ��͵�ַ�ռ�*/
    iounmap(uart1_dev_a.send_addr);
    iounmap(uart1_dev_b.send_addr);
    iounmap(uart2_dev_a.send_addr);
    iounmap(uart2_dev_b.send_addr);

    /*���ս��ܵ�ַ�ռ�*/
    iounmap(uart1_dev_a.recv_addr);
    iounmap(uart1_dev_b.recv_addr);
    iounmap(uart2_dev_a.recv_addr);
    iounmap(uart2_dev_b.recv_addr);

#ifdef UART_DEBUG
    uart_remove_proc();
#endif /*UART_DEBUG*/

    printk(KERN_INFO "uart exit!\n");

    return;
}

module_init(uart_init);
module_exit(uart_exit);
