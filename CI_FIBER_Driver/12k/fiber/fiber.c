/*********************************************************************
Copyright (C), 2011,  Co.Hengjun, Ltd.

����        : ������
�汾        : 1.0
��������    : 2014��3��13�� 12:59:35
��;        : ���˰������
��ʷ�޸ļ�¼: v1.0    ����
             v1.1    ���������ȶ�������������Ȼ���ȡ�־��������ݶ���������
                     ����
             v1.3    ���ڱ�ϵУ�����ݷ��ͻᱻ�������ݳ�ˢ������ǽ���������ͳһ
                     ���ո���ռ��һ���豸�ķ�ʽ��ƣ��ð汾����ÿ�η��ͺͽ���
                     ��ʹ��ȫ���ڴ�ռ�
             v1.4    ��һ�浱��ʹ���˻��棬��һ�浱��ȥ������
**********************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <linux/proc_fs.h>

#include "fiber.h"

MODULE_AUTHOR("zhangys");
MODULE_LICENSE("Dual BSD/GPL");

#define FIBER_TRUE   0xAA        /*��*/
#define FIBER_FALSE  0x55        /*��*/

/*
 * Ϊ���ж�ÿһ֡�������Ƿ��Ѿ�����ȡ���������豸����������last_sn����������last_sn��
 * ��ʼ��ȴ�������⣬����snһֱ�������ת������last_sn��ʼ�����κ�ֵ�Ʊص��µ�һ�ι���
 * �޷����У�һ�ֿ��еİ취��ʹsn��һ���ķ�Χ�ڻ�ת��last_sn��ʼ���ɸ÷�Χ֮���һ��ֵ
 * ���ɣ�����ʹ��FIBER_SN_MAXȡģ���㣬���ʹ���ʼ����2�������η���
 */
#define FIBER_SN_MAX 0xf00000

static struct fiber_dev fiber_dev_a[FIBER_PAIR_DEVS_NR];    /*A���豸*/
static struct fiber_dev fiber_dev_b[FIBER_PAIR_DEVS_NR];    /*B���豸*/

static struct semaphore fiber_sem;  /*����ʹ��һ������ź������������豸�ķ���*/

static int32_t fiber_major = FIBER_MAJOR;

static int32_t fiber_send_allocated_size = 0;
static int32_t fiber_a_recv_allocated_size = 0; /*��ͷ��ƫ�ƹ�ȥ*/
static int32_t fiber_b_recv_allocated_size = 0;

static __iomem uint8_t* send_head_addr = NULL;
static __iomem uint8_t* recv_head_addr_a = NULL;
static __iomem uint8_t* recv_head_addr_b = NULL;

module_param(fiber_major, int, S_IRUGO);
/*
 ��������    : �ڴ������
 ����ֵ      : �ɹ�Ϊ������ڴ��ָ�룬���򷵻�NULL
 ����        : @size�����ڴ�Ĵ�С
 ����        : ������
 ����        : 2013��12��1�� 15:51:39
*/
static unsigned char* fiber_send_mem_alloc(int32_t size)
{
    const static int32_t total_size = FIBER_SEND_MAX_MEMORY_SIZE;
    unsigned char* ptr = NULL;

    if (size > total_size - fiber_send_allocated_size)
    {
        printk(KERN_ERR "fiber_mem_alloc:request memory %#x,but only remain %#x"
            "[total_size(%#x) - allocated_size(%#x)]\n",
            size, total_size - fiber_send_allocated_size,total_size,fiber_send_allocated_size);

        return NULL;
    }

    ptr = ioremap(FIBER_SEND_START_ADDR + fiber_send_allocated_size,size);
    if(NULL == ptr)
    {
        printk("fiber_mem_alloc:ioremap error\n");
    }

    fiber_send_allocated_size += size;

    return ptr;
}
/*
 ��������    : ����a���豸�������ݵ��ڴ�����
 ����ֵ      : �ɹ�Ϊ������ڴ��ָ�룬���򷵻�NULL
 ����        : @size�����ڴ�Ĵ�С
 ����        : ������
 ����        : 2013��12��1�� 15:51:39
*/
static uint8_t* fiber_recv_a_mem_alloc(int size)
{
    uint8_t* ptr = NULL;
    static int32_t total_size = FIBER_RECV_TOTAL_MEMORY_SIZE_A;

    if (size > total_size - fiber_a_recv_allocated_size)
    {
        printk(KERN_ERR "fiber_mem_a_alloc:request memory %#x,but only remain %#x\n",
            size, total_size - fiber_a_recv_allocated_size);

        return NULL;
    }

    ptr = ioremap(FIBER_RECV_START_ADDR_A + fiber_a_recv_allocated_size,size);

    fiber_a_recv_allocated_size += size;

    return ptr;
}
/*
 ��������    : ����b���豸�������ݵ��ڴ�����
 ����ֵ      : �ɹ�Ϊ������ڴ��ָ�룬���򷵻�NULL
 ����        : @size�����ڴ�Ĵ�С
 ����        : ������
 ����        : 2013��12��1�� 15:51:39
*/
static uint8_t* fiber_recv_b_mem_alloc(int size)
{
    uint8_t* ptr = NULL;
    static int32_t total_size = FIBER_RECV_TOTAL_MEMORY_SIZE_B;

    if (size > total_size - fiber_b_recv_allocated_size)
    {
        printk(KERN_ERR "fiber_mem_b_alloc:request memory %#x,but only remain %#x\n",
            size, total_size - fiber_b_recv_allocated_size);

        return NULL;
    }

    ptr = ioremap(FIBER_RECV_START_ADDR_B + fiber_b_recv_allocated_size,size);

    fiber_b_recv_allocated_size += size;

    return ptr;
}
/*
��������    : �ȴ���������
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��19�� 14:34:50
*/
static int fiber_wait_lock(int port)
{
    int lock = 0;
    int count = 0;

    /*
     * ����Э��������ж�Ӧ���ǲ�����0X01��Ȼ��FPGA����δȷ����һ���·����ݵ�ʱ���λ
     * Ϊ0x00�����Ը�ʹ�������߼�
     */
    do
    {
        lock = inb(port);
        udelay(100);  /*�ӳ�1΢��*/
        count ++;
    } while (0x01 == lock && count < 10);

    if (0x01 == inb(port))
    {
        return -EBUSY;
    }

    return 0;
}

#ifdef FIBER_DEBUG /* use proc only if debugging */

static int fiber_proc_mem(char *buf, char **start, off_t offset,
                       int count, int *eof, void *data)
{
    int len = 0;
    int i = 0;
    struct fiber_dev* dev = NULL;

    if (down_interruptible(&fiber_sem))
    {
        return -ERESTARTSYS;
    }

    len += sprintf(buf + len,"\t"
                            " buf_size"
                            " send_sn_addr"
                            " send_addr"
                            " recv_sn_addr"
                            " recv_addr"
                            " recv_sn"
                            " send_sn"
                            " last_recv_sn\n" );
    /*���ڴ浱��ȡ��recv_sn����ӡ��ʵ���岻����Ϊ���ֵֻ���ڹ���֡ͷ��ȷ��ʱ���������*/

    for (i = 0;i < FIBER_PAIR_DEVS_NR;i++)
    {
        dev = &fiber_dev_a[i];
        len += sprintf(buf + len,"fibera%d",i + 1);
        len += sprintf(buf + len," %#x",dev->buf_size);
        len += sprintf(buf + len," %#x",(uint32_t)dev->send_sn_addr);
        len += sprintf(buf + len," %#x",(uint32_t)dev->send_addr);
        len += sprintf(buf + len," %#x",(uint32_t)dev->recv_sn_addr);
        len += sprintf(buf + len," %#x",(uint32_t)dev->recv_addr);
        if(NULL == dev->recv_sn_addr)
        {
            len += sprintf(buf + len," %#x",0);
        }
        else
        {
            len += sprintf(buf + len," %#x",(int32_t)(*dev->recv_sn_addr));
        }
        len += sprintf(buf + len," %#x",dev->send_sn);
        len += sprintf(buf + len," %#x",dev->last_recv_sn);
        len += sprintf(buf + len,"\n");
    }
    for (i = 0;i < FIBER_PAIR_DEVS_NR;i++)
    {
        dev = &fiber_dev_b[i];
        len += sprintf(buf + len,"fiberb%d",i + 1);
        len += sprintf(buf + len," %#x",dev->buf_size);
        len += sprintf(buf + len," %#x",(uint32_t)dev->send_sn_addr);
        len += sprintf(buf + len," %#x",(uint32_t)dev->send_addr);
        len += sprintf(buf + len," %#x",(uint32_t)dev->recv_sn_addr);
        len += sprintf(buf + len," %#x",(uint32_t)dev->recv_addr);
        if(NULL == dev->recv_sn_addr)
        {
            len += sprintf(buf + len," %#x",0);
        }
        else
        {
            len += sprintf(buf + len," %#x",(int32_t)(*dev->recv_sn_addr));
        }
        len += sprintf(buf + len," %#x",dev->send_sn);
        len += sprintf(buf + len," %#x",dev->last_recv_sn);
        len += sprintf(buf + len,"\n");
    }

    up(&fiber_sem);
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
static void fiber_create_proc(void)
{
    create_proc_read_entry("fiber_mem",0,NULL, fiber_proc_mem, NULL);

    return;
}
/*
 ��������    : ɾ��proc�ӿ��ļ�
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��3��19�� 16:27:58
*/
static void fiber_remove_proc(void)
{
    /* no problem if it was not registered */
    remove_proc_entry("fiber_mem", NULL);
}

#endif /* FIBER_DEBUG */

/*
��������    : openϵͳ���õ�ʵ��
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 13:26:23
*/
static int fiber_open(struct inode* inode, struct file* filp)
{
    struct fiber_dev* dev;
    int num = iminor(inode);
    int ret = 0;

    /*���豸mknod��ʱ���ܴ�������FIBER_DEVS_NR���豸*/
    if (FIBER_PAIR_DEVS_NR * 2 < num)
    {
        return -ENODEV;
    }

    dev = container_of(inode->i_cdev,struct fiber_dev,cdev);

    if (down_interruptible(&fiber_sem))
    {
        return -ERESTARTSYS;
    }

    /*���ǵ�Ŀ��������һ��openϵͳ���ã��������д򿪲���ȫ���ܾ�*/
    if (FIBER_TRUE == dev->b_use)
    {
        ret = -EBUSY;
        goto fail;
    }

    dev->b_use = FIBER_TRUE;

    up(&fiber_sem);
    /*���豸�ṹ��ָ�븳ֵ���ļ�˽������ָ��*/
    filp->private_data = dev;

    return 0;
fail:

    up(&fiber_sem);
    return ret;
}
/*
��������    : �ͷ��豸
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 13:28:10
*/
static int fiber_release(struct inode* inode, struct file *filp)
{
    struct fiber_dev* dev = filp->private_data;

    if (down_interruptible(&fiber_sem))
    {
        return -ERESTARTSYS;
    }

    dev->b_use = FIBER_FALSE;

    up(&fiber_sem);

    return 0;
}
/*
��������    : ��ȡ����
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 13:28:38
*/
static ssize_t fiber_read(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
    int ret = 0;
    struct fiber_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/

    if (NULL == dev)
    {
        printk(KERN_ERR "fiber_read:dev is NULL\n");

        return -EINVAL;
    }
    /*˵����û��ʹ��ioctl���й���ʼ��*/
    if (0 == dev->buf_size)
    {
        return -EINVAL;
    }
    if (dev->buf_size < size)
    {
        printk(KERN_ERR "fiber_read:buf_size(%#x) < size(%#x)",dev->buf_size,size );

        return -EINVAL;
    }

    if (down_interruptible(&fiber_sem))
    {
        return -ERESTARTSYS;
    }

    outb(FIBER_IO_SELECT_VALUE,FIBER_IO_PORT);

    if (0x55 != dev->recv_head_addr[0] || 0xaa != dev->recv_head_addr[1])
    {
        /*printk(KERN_INFO "fiber_read:recv_head_addr:%#x-%#x",dev->recv_head_addr[0],dev->recv_head_addr[1]);*/
        ret = -EAGAIN;
        goto fail;
    }
    if(NULL == dev->recv_sn_addr)
    {
        printk(KERN_DEBUG "fiber:memory not alloc\n");
        ret = -EAGAIN;
        goto fail;
    }
        /*�ж��ϴ��Ƿ��л������ݣ�����еĻ����ȡ������*/
    if (FIBER_SN_MAX > *dev->recv_sn_addr &&
        *dev->recv_sn_addr != dev->last_recv_sn)
    {
        /*
        printk(KERN_INFO "fiber_read:recv.sn:%#x last_recv_sn:%#x",*dev->recv_sn_addr,dev->last_recv_sn);
        */
        if (copy_to_user(buf, dev->recv_addr,size))
        {
            printk(KERN_ERR "fiber_read: copy_to_user error\n");

            ret = -EIO;
            goto fail;
        }
        rmb();
    }
    else
    {
        /*printk(KERN_INFO "fiber_read:recv.sn:%#x(%#x)",(uint32_t)dev->recv_sn_addr,*dev->recv_sn_addr);*/
        ret = -EAGAIN;
        goto fail;
    }

    dev->last_recv_sn = *dev->recv_sn_addr;

    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);
    up(&fiber_sem);

    return size;

fail:
    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);
    up(&fiber_sem);

    return ret;
}
/*
��������    : д���ݺ���
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 15:45:19
*/
static ssize_t fiber_write(struct file *filp, const char __user *buf,size_t size, loff_t *ppos)
{
    int ret = 0;
    struct fiber_dev *dev = filp->private_data;
    static int32_t data_len = FIBER_SEND_TOTAL_MEMORY_SIZE - 2;

    if (NULL == dev)
    {
        printk(KERN_ERR "fiber_write:filp->private_data is NULL\n");

        return -ENODEV;
    }
    /*˵����û��ʹ��ioctl���й���ʼ��*/
    if (0 >= dev->buf_size || NULL == dev->send_sn_addr || NULL == dev->send_addr)
    {
        return -EIO;
    }

    /*�����������С*/
    if (dev->buf_size < size)
    {
        printk(KERN_ERR "fiber_write:size(%#x)>buf_size(%#x)\n",size,dev->buf_size);

        return -EINVAL;
    }

    if (down_interruptible(&fiber_sem))
    {
        return -ERESTARTSYS;
    }

    outb(FIBER_IO_SELECT_VALUE,FIBER_IO_PORT);

    ret = fiber_wait_lock(FIBER_IO_WRITE_LOCK_PORT);

    if (0 != ret)
    {
        goto fail;
    }
    /*����֡��*/
    memcpy(dev->send_sn_addr,&dev->send_sn,sizeof(int32_t));

    if (copy_from_user(dev->send_addr, buf, size))
    {
        printk(KERN_ERR "fiber_write:copy_from_user failed\n");
        ret = -EIO;
        goto fail;
    }

    /*���ͷ��*/
    send_head_addr[0] = 0x55;
    send_head_addr[1] = 0xaa;/*֡ͷ*/
    send_head_addr[2] = 0x0f;
    send_head_addr[3] = 0xfb;/*���ݳ��ȣ����ﳤ��Ϊ���г���-4�����Ŀ��ܻᷢ����������*/
    /*
     * ����˰�д�����ݣ�ע������ķ��ͳ��ȣ���Ϊ���ǵ���Ƶ���ÿ��ʹ��ȫ���ռ���з���
     * ���ݣ������Ҫ����Ҫ�����ȷ�����ݳ��ȣ�������ݳ��Ȳ���ȷ����ᵼ�¹������ݻ��ơ�
     * �����ں���λ�á���ʧ�������Ȼ�����ݳ��ȼȷ�FIBER_SEND_TOTAL_MEMORY_SIZE��Ҳ��
     * FIBER_SEND_TOTAL_MEMORY_SIZE - 4(֡ͷ�ĳ���)��Ӳ��Ҫ��ó������Ϊ��ȥ֡ͷ��־
     * 0x55aa�ĳ��ȣ������������ԭ�����ɹ����������ݶ�����ġ�
     */
    outb((data_len >> 8) & 0xff,FIBER_IO_WRITE_DATA_LEN_HIGH_PORT);
    outb(data_len & 0xff,FIBER_IO_WRITE_DATA_LEN_LOW_PORT);

    /*
     * �����ﰴ�ձ�̵�˼·����Ӧ�õ���fiber_unlock������������FPGAʵ�ֵ��У�����
     * ����֮���ٷ������鿴���Ƿ���Ľ�����û�б�Ҫ��FPGA�������������·���ȥ������
     * �˿ڵ�ֵ��Ϊ0x0�����Բ�û�б�Ҫ�ȴ�
     */
    outb(0x01,FIBER_IO_WRITE_LOCK_PORT);

    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);

    /*һ�ж��ɹ��������1*/
    dev->send_sn = (dev->send_sn + 1) % FIBER_SN_MAX ;

    up(&fiber_sem);
    return size;

fail:
    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);
    /*reset semaphore*/

    up(&fiber_sem);
    return ret;
}
/*
��������    : ͨ��ioctl���п���
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 15:16:52
*/
static int fiber_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int i = 0;
    int mem_size = 0;
    int minor_devno = MINOR(inode->i_rdev);

    struct fiber_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/

    if (NULL == dev)
    {
        printk(KERN_ERR "fiber_ioctl:filp->private_data is NULL\n");

        return -EFAULT;
    }
    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != FIBER_IOC_MAGIC)
    {
        return -ENOTTY;
    }

    if (down_interruptible(&fiber_sem))
    {
        return -ERESTARTSYS;
    }

    outb(FIBER_IO_SELECT_VALUE,FIBER_IO_PORT);

    switch (cmd)
    {
    case FIBER_IOC_RESET:
        for (i = 0;i < FIBER_PAIR_DEVS_NR;i++)
        {
            fiber_dev_a[i].buf_size = 0;
            fiber_dev_a[i].send_addr = NULL;
            fiber_dev_a[i].recv_addr = NULL;
            fiber_dev_a[i].send_sn_addr = NULL;
            fiber_dev_a[i].recv_sn_addr = NULL;
            fiber_dev_a[i].send_sn = 0;
            fiber_dev_a[i].last_recv_sn = FIBER_SN_MAX;   /*֡��Ĭ�ϱ�FIBER_SN_MAX���ڻ���ڼ���*/

            fiber_dev_b[i].buf_size = 0;
            fiber_dev_b[i].send_addr = NULL;
            fiber_dev_b[i].recv_addr = NULL;
            fiber_dev_b[i].send_sn_addr = NULL;
            fiber_dev_b[i].recv_sn_addr = NULL;
            fiber_dev_b[i].send_sn = 0;
            fiber_dev_b[i].last_recv_sn = FIBER_SN_MAX;   /*֡��Ĭ�ϱ�FIBER_SN_MAX���ڻ���ڼ���*/
        }
        fiber_send_allocated_size = FIBER_HEAD_SIZE;
        fiber_a_recv_allocated_size = FIBER_HEAD_SIZE;
        fiber_b_recv_allocated_size = FIBER_HEAD_SIZE;
        break;
    case FIBER_IOC_CLEAN_BUFFER:
        break;
    case FIBER_IOC_ALLOC_MEMORY:
        mem_size = arg;
        if (0 >= mem_size)
        {
            printk(KERN_ERR "fiber_ioctl:alloc size must great than 0\n");
            ret = -EINVAL;
            goto fail;
        }

        if (NULL == dev->send_addr || 0 == dev->buf_size)
        {
            /*���ͺͽ��յĿռ��ַ����һһ��Ӧ*/
            dev->send_sn_addr = (int32_t*)fiber_send_mem_alloc(sizeof(int32_t));
            dev->send_addr = fiber_send_mem_alloc(mem_size);

            dev->peer_fiber_dev->send_sn_addr = dev->send_sn_addr;
            dev->peer_fiber_dev->send_addr = dev->send_addr;

        }
        /*a��ʹ��ż�����豸��b��ʹ���������豸*/
        if (minor_devno % 2 == 0)
        {
            dev->recv_sn_addr = (int32_t*)fiber_recv_a_mem_alloc(sizeof(int32_t));
            dev->recv_addr = fiber_recv_a_mem_alloc(mem_size);
        }
        else
        {
            dev->recv_sn_addr = (int32_t*)fiber_recv_b_mem_alloc(sizeof(int32_t));
            dev->recv_addr = fiber_recv_b_mem_alloc(mem_size);
        }
        /*�������ڴ�ʱ��sn����Ϊ���ֵ������־���豸�Ƿ�������*/
        *dev->recv_sn_addr = FIBER_SN_MAX;
        *dev->send_sn_addr = FIBER_SN_MAX;
        dev->buf_size = mem_size;
        dev->peer_fiber_dev->buf_size = mem_size;

        dev->last_recv_sn = *dev->recv_sn_addr;

        if(NULL == dev->send_sn_addr
                || NULL == dev->send_addr 
                || NULL == dev->recv_sn_addr 
                || NULL == dev->recv_addr)
        {
            ret = -EIO;
            goto fail;
        }

        break;
    default:
        ret = -EFAULT;
        goto fail;
    }

    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);
    up(&fiber_sem);

    return 0;
fail:
    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);
    up(&fiber_sem);

    return ret;
}

static const struct file_operations fiber_fops =
{
    .owner = THIS_MODULE,
    .read = fiber_read,
    .write = fiber_write,
    .ioctl = fiber_ioctl,
    .open = fiber_open,
    .release = fiber_release,
};
/*
��������    : ������������豸��װ���ں���
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 10:24:02
*/
static int fiber_setup_cdev(struct cdev* dev, int minor)
{
    int err;
    dev_t devno = MKDEV(fiber_major,minor);

    cdev_init(dev, &fiber_fops);
    dev->owner = THIS_MODULE;
    dev->ops = &fiber_fops;

    err = cdev_add(dev, devno, 1);

    if (err)
    {
        printk(KERN_ERR "Error %d adding fiber %d\n", err, minor);

        return err;
    }

    return 0;
}
/*
��������    : �������������ʼ������
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 10:23:31
*/
static int __init fiber_init(void)
{
    int ret;
    dev_t devno = 0;
    int i = 0;
    int minor_a = 0;
    int minor_b = 0;

    if (fiber_major)
    {
        devno = MKDEV(fiber_major, 0);
        ret = register_chrdev_region(devno, FIBER_PAIR_DEVS_NR * 2, "fiber");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, FIBER_PAIR_DEVS_NR * 2, "fiber");
        fiber_major = MAJOR(devno);
    }

    if (ret < 0)
    {
        printk(KERN_WARNING "fiber: can't get major %d\n", fiber_major);

        return ret;
    }

    recv_head_addr_a = fiber_recv_a_mem_alloc(FIBER_HEAD_SIZE);
    recv_head_addr_b = fiber_recv_b_mem_alloc(FIBER_HEAD_SIZE);

    /*��ʼ�������ź���*/
    sema_init(&fiber_sem,1);

    for (i = 0;i < FIBER_PAIR_DEVS_NR;i++)
    {
        minor_a = i * 2;
        minor_b = minor_a + 1;

        fiber_dev_a[i].buf_size = 0;
        fiber_dev_a[i].send_addr = NULL;
        fiber_dev_a[i].recv_addr = NULL;
        fiber_dev_a[i].peer_fiber_dev = &fiber_dev_b[i];
        fiber_dev_a[i].buf_size = 0;
        fiber_dev_a[i].last_recv_sn = FIBER_SN_MAX;
        fiber_dev_a[i].recv_head_addr = recv_head_addr_a;

        fiber_dev_b[i].buf_size = 0;
        fiber_dev_b[i].send_addr = NULL;
        fiber_dev_b[i].recv_addr = NULL;
        fiber_dev_b[i].peer_fiber_dev = &fiber_dev_a[i];
        fiber_dev_b[i].buf_size = 0;
        fiber_dev_b[i].last_recv_sn = FIBER_SN_MAX;
        fiber_dev_b[i].recv_head_addr = recv_head_addr_b;

        ret = fiber_setup_cdev(&fiber_dev_a[i].cdev,minor_a);
        if (0 > ret)
        {
            ret = -EFAULT;
            goto fail;
        }
        ret = fiber_setup_cdev(&fiber_dev_b[i].cdev,minor_b);
        if (0 > ret)
        {
            ret = -EFAULT;
            goto fail;
        }
    }
    /*����ͷ��*/
    send_head_addr = fiber_send_mem_alloc(FIBER_HEAD_SIZE);

#ifdef FIBER_DEBUG
    /*����proc�ӿ�*/
    fiber_create_proc();
#endif /*FIBER_DEBUG*/

    printk(KERN_INFO "fiber init.\n");

    return 0;

fail:
    unregister_chrdev_region(MKDEV(fiber_major, 0), FIBER_PAIR_DEVS_NR * 2); /*�ͷ��豸��*/

    printk(KERN_ERR "fiber_init:failed\n");

    return ret;
}
/*
��������    : ע������
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 10:23:45
*/
static void __exit fiber_exit(void)
{
    int i = 0;

    for (i = 0;i < FIBER_PAIR_DEVS_NR;i++)
    {
        cdev_del(&fiber_dev_a[i].cdev);
        cdev_del(&fiber_dev_b[i].cdev);
    }

#ifdef FIBER_DEBUG
    fiber_remove_proc();
#endif /*FIBER_DEBUG*/

    unregister_chrdev_region(MKDEV(fiber_major, 0), FIBER_PAIR_DEVS_NR * 2); /*�ͷ��豸��*/

    printk(KERN_INFO "fiber exit!\n");

    return;
}

module_init(fiber_init);
module_exit(fiber_exit);

