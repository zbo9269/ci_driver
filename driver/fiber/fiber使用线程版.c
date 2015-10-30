/*********************************************************************
Copyright (C), 2011,  Co.Hengjun, Ltd.

����        : ������
�汾        : 1.0
��������    : 2014��3��13�� 12:59:35
��;        : ���˰������
��ʷ�޸ļ�¼: v1.0    ����
             v1.1   Ϊ����������̣߳�ʹ��workqueue
             v1.2   ȥ��workqueue��ʹ��һ���־�����Ϊ�����źŵĴ洢����һ��
                    ��������Ϊ�������ݵĴ洢
             v1.3   ȥ���־�����������֮�֣���˫��ram��������ע���������ռ�
                    �ķ�ʽά����5���豸���������ݴ���
             v1.4   ���ڴ������ݿռ䲻��ʹ�ã��ٴ�ʹ���̣߳��ð�ʹ���˻���洢
                    ��������
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
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kthread.h> 

#include "fiber.h"

MODULE_AUTHOR("zhangys");
MODULE_LICENSE("Dual BSD/GPL");

#define FIBER_TRUE   0xAA        /*��*/
#define FIBER_FALSE  0x55        /*��*/

struct fiber_recver
{
    unsigned char* recv_addr_a;            /*ioremap֮���ָ��*/
    unsigned char* recv_addr_b;            /*ioremap֮���ָ��*/
    struct work_struct work;               /*��������*/
};

static int fiber_major = FIBER_MAJOR;
static struct fiber_recver fiber_recver;

DECLARE_WAIT_QUEUE_HEAD(fiber_queue);
static struct task_struct * fiber_recv_thread = NULL;
static int b_stop_thread = FIBER_FALSE;

static struct semaphore fiber_dev_sem;      /*����ʹ��һ������ź������������豸�ķ���*/
static struct semaphore fiber_data_sem;
extern struct semaphore ci_io_select_sem;  /*���ź�������֮������ģ��ʹ�ã�����ѡ�ж˿��Ա���в���*/

static struct fiber_dev fiber_dev_a[FIBER_VIRTUAL_SECTIONS];
static struct fiber_dev fiber_dev_b[FIBER_VIRTUAL_SECTIONS];

/*����ʹ�õĻ�������*/
static uint8_t fiber_mbuf_a[FIBER_MBUF_MEM_SIZE] = {0};
static uint8_t fiber_mbuf_b[FIBER_MBUF_MEM_SIZE] = {0};

static uint8_t* fiber_frag_recv_addr_a[FIBER_FRAG_COUNT] = {0};
static uint8_t* fiber_frag_recv_addr_b[FIBER_FRAG_COUNT] = {0};

static int32_t fiber_alloced_frag_id_size = 0;

static unsigned long fiber_recv_status_a = 0;
static unsigned long fiber_recv_status_b = 0;

static volatile int32_t fiber_irq = FIBER_IRQ;

static __iomem uint8_t* send_addr = 0;

/*
 ��������    : ����size��frag id
 ����ֵ      : �ɹ����������frag id����ʼid�����󷵻ظ���
 ����        : 
 ����        : ������
 ����        : 2014��5��20�� 13:35:25
*/
static int32_t fiber_alloc_frag_id(int32_t frag_count)
{
    if (0 >= frag_count)
    {
        printk(KERN_ERR "fiber_alloc_frag_id:size(%#x) <= 0\n",frag_count);
        return -EINVAL;
    }
    if (FIBER_FRAG_COUNT < fiber_alloced_frag_id_size + frag_count)
    {
        printk(KERN_ERR "fiber_alloc_frag_id:none memory remain(%#x)\n",frag_count);
        return -ENOMEM;
    }

    fiber_alloced_frag_id_size += frag_count;

    return fiber_alloced_frag_id_size - frag_count;
}
/*
 ��������    : ͨ��������ڴ�ֵ�Ĵ�С�õ������fragment�ĸ���
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��5��22�� 13:04:23
*/
static int32_t fiber_calc_frag_count(int32_t mem_size)
{
    int32_t frag_count = 0;

    if (0 > mem_size)
    {
        printk(KERN_INFO "fiber_calc_frag_count:mem_size(%#x) < 0\n",mem_size);
        return -1;
    }

    frag_count = mem_size / FIBER_FRAG_MEM_SIZE;

    if (0 == mem_size % FIBER_FRAG_MEM_SIZE)
    {
        return frag_count;
    }
    else
    {
        return frag_count + 1;
    }
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
        udelay(1);  /*�ӳ�1΢��*/
        count ++;
    } while (0x01 == lock && count < 10);

    if (0x01 == inb(port))
    {
        return -EBUSY;
    }

    return 0;
}

/*
��������    : �ȴ�������
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��19�� 14:34:50
*/
static int fiber_wait_unlock(int port)
{
    int lock = 0;
    int count = 0;
    int unlock_value = 0x01;

    /*when read semaphore is 0x01 meaning FPGA has data*/
    do
    {
        lock = inb(port);
        count ++;
    } while (unlock_value != lock && count < 10);

    if (unlock_value != inb(port))
    {
        return -EBUSY;
    }

    return 0;
}
/*
 ��������    : ���λ�Ƿ�����
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��6��10�� 14:01:40
*/
static int fiber_test_status(int start,int size,unsigned long* status)
{
    int i = 0;

    if (0 > start || FIBER_FRAG_COUNT < start)
    {
        return -EINVAL;
    }
    if (0 > size || FIBER_FRAG_COUNT - start < size)
    {
        return -EINVAL;
    }
    /*������е�λ�Ƿ񶼱����ã��������������ж�ȡ����*/
    for (i = 0;i < size;i++)
    {
        /*�����־û�б�������˵��û�����ݣ��򷵻�*/
        if (!test_bit(start + i, status))
        {
            return -EAGAIN;
        }
    }

    return 0;
}
/*
 ��������    : ����start��start+size��λȫ�������
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��6��10�� 14:16:03
*/
static int fiber_clear_status(int start,int size,unsigned long* status)
{
    int i = 0;

    for (i = 0;i < size;i++)
    {
        clear_bit(start + i,status);
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

    if (down_interruptible(&fiber_dev_sem))
    {
        return -ERESTARTSYS;
    }

    len += sprintf(buf + len,"\tb_use"
                             "\tfrag_start_id"
                             "\tfrag_count\n" );

    for (i = 0;i < FIBER_VIRTUAL_SECTIONS;i++)
    {
        dev = &fiber_dev_a[i];
        len += sprintf(buf + len,"fibera%d",i + 1);
        len += sprintf(buf + len,"\t%#x",dev->b_use);
        len += sprintf(buf + len,"\t%#x",dev->frag_start_id);
        len += sprintf(buf + len,"\t%#x",dev->frag_count);
        len += sprintf(buf + len,"\n");

        dev = &fiber_dev_b[i];
        len += sprintf(buf + len,"fiberb%d",i + 1);
        len += sprintf(buf + len,"\t%#x",dev->b_use);
        len += sprintf(buf + len,"\t%#x",dev->frag_start_id);
        len += sprintf(buf + len,"\t%#x",dev->frag_count);
        len += sprintf(buf + len,"\n");
    }

    len += sprintf(buf + len,"fiber_recv_status_a:%#lx\n",fiber_recv_status_a);
    len += sprintf(buf + len,"fiber_recv_status_b:%#lx\n",fiber_recv_status_b);

    up(&fiber_dev_sem);
    *eof = 1;

    return len;
}
static int fiber_proc_recv_buf_a(char *buf, char **start, off_t offset,
                       int count, int *eof, void *data)
{
    if (down_interruptible(&fiber_dev_sem))
    {
        return -ERESTARTSYS;
    }
    /*����linux proc�ļ�ϵͳҳ��С�����ƣ����ﲻ�ÿ�������һ��ҳ������*/
    memcpy(buf,fiber_mbuf_a,FIBER_SEND_TOTAL_MEMORY_SIZE);

    up(&fiber_dev_sem);
    *eof = 1;

    return FIBER_SEND_TOTAL_MEMORY_SIZE;
}

static int fiber_proc_recv_buf_b(char *buf, char **start, off_t offset,
                       int count, int *eof, void *data)
{
    if (down_interruptible(&fiber_dev_sem))
    {
        return -ERESTARTSYS;
    }
    memcpy(buf,fiber_mbuf_b,FIBER_SEND_TOTAL_MEMORY_SIZE);

    up(&fiber_dev_sem);
    *eof = 1;

    return FIBER_SEND_TOTAL_MEMORY_SIZE;
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
    create_proc_read_entry("fiber_recv_buf_a",0,NULL, fiber_proc_recv_buf_a, NULL);
    create_proc_read_entry("fiber_recv_buf_b",0,NULL, fiber_proc_recv_buf_b, NULL);

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
    remove_proc_entry("fiber_recv_buf_a", NULL);
    remove_proc_entry("fiber_recv_buf_b", NULL);
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
    struct fiber_dev* dev = NULL;
    int minor = iminor(inode);
    int ret = 0;

    /*���豸mknod��ʱ���ܴ�������FIBER_DEVS_NR���豸*/
    if (FIBER_DEVS_NR < minor)
    {
        return -ENODEV;
    }

    dev = container_of(inode->i_cdev,struct fiber_dev,cdev);

    if (down_interruptible(&fiber_dev_sem))
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

    /*���豸�ṹ��ָ�븳ֵ���ļ�˽������ָ��*/
    filp->private_data = dev;
    up(&fiber_dev_sem);

    return 0;
fail:
    up(&fiber_dev_sem);
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

    if (down_interruptible(&fiber_dev_sem))
    {
        return -ERESTARTSYS;
    }

    dev->b_use = FIBER_FALSE;

    up(&fiber_dev_sem);

    return 0;
}
/*
 ��������    : ��a�ڶ�ȡ����
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��5��20�� 15:40:37
*/
static ssize_t fiber_read_a(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
    int ret = 0;
    struct fiber_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/
    DEFINE_WAIT(wait);

    if (NULL == dev)
    {
        printk(KERN_ERR "fiber_read_a:dev is NULL\n");

        return -EIO;
    }
    /*��ֹ�жϴ�������ڸ����������������ȡ�˲���ȷ������*/
    if (down_interruptible(&fiber_data_sem))
    {
        return -ERESTARTSYS;
    }
    if (down_interruptible(&fiber_dev_sem))
    {
        up(&fiber_data_sem);
        return -ERESTARTSYS;
    }
    /*˵��û�г�ʼ��*/
    if (0 > dev->frag_count || FIBER_FRAG_COUNT < dev->frag_count)
    {
        printk(KERN_ERR "fiber_read_a:dev->frag_count(%#x) not correct\n", dev->frag_count);
        ret = -ENOMEM;
        goto fail;
    }
    if (dev->frag_count * FIBER_FRAG_MEM_SIZE < size)
    {
        printk(KERN_ERR "fiber_read_a:recv_buf(%#x) < size(%#x)\n",dev->frag_count * FIBER_FRAG_MEM_SIZE,size);
        ret = -EINVAL;
        goto fail;
    }

    while (0 != fiber_test_status(dev->frag_start_id,dev->frag_count,&fiber_recv_status_a))
    {
        if (filp->f_flags & O_NONBLOCK)
        {
            ret = -EAGAIN;
            goto fail;
        }
        prepare_to_wait(&dev->data_queue, &wait, TASK_INTERRUPTIBLE);
        if (signal_pending (current))  /* a signal arrived */
        {
            ret = -ERESTARTSYS; /* tell the fs layer to handle it */
            goto fail;
        }
        /*�ȴ����ˣ��ټ��һ���Ƿ������ݣ����û������*/
        if (0 != fiber_test_status(dev->frag_start_id,dev->frag_count,&fiber_recv_status_a))
        {
            schedule();
        }
        finish_wait(&dev->data_queue, &wait);
    }

    /*at here,we begin copy data to user space*/
    if (copy_to_user(buf, fiber_frag_recv_addr_a[dev->frag_start_id], size))
    {
        printk(KERN_ERR "fiber_read_a: copy_to_user error\n");

        ret = -EIO;
        goto fail;
    }
    rmb();
    /*���������*/
    memset(fiber_frag_recv_addr_a[dev->frag_start_id],0,dev->frag_count * FIBER_FRAG_MEM_SIZE);

    fiber_clear_status(dev->frag_start_id,dev->frag_count,&fiber_recv_status_a);

    up(&fiber_dev_sem);
    up(&fiber_data_sem);
    return size;
fail:
    up(&fiber_dev_sem);
    up(&fiber_data_sem);
    return ret;
}
/*
��������    : ��b�ڶ�ȡ����
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��3�� 13:28:38
*/
static ssize_t fiber_read_b(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
    int ret = 0;
    struct fiber_dev *dev = filp->private_data; /*����豸�ṹ��ָ��*/
    DEFINE_WAIT(wait);

    if (NULL == dev)
    {
        printk(KERN_ERR "fiber_read_b:dev is NULL\n");

        return -EIO;
    }
    if (dev->frag_count * FIBER_FRAG_MEM_SIZE < size)
    {
        printk(KERN_ERR "fiber_read_b:recv_buf(%#x) < size(%#x)\n",dev->frag_count * FIBER_FRAG_MEM_SIZE,size);
        return -EINVAL;
    }

    if (down_interruptible(&fiber_data_sem))
    {
        return -ERESTARTSYS;
    }
    if (down_interruptible(&fiber_dev_sem))
    {
        up(&fiber_data_sem);
        return -ERESTARTSYS;
    }

    while (0 != fiber_test_status(dev->frag_start_id,dev->frag_count,&fiber_recv_status_b))
    {
        if (filp->f_flags & O_NONBLOCK)
        {
            ret = -EAGAIN;
            goto fail;
        }
        prepare_to_wait(&dev->data_queue, &wait, TASK_INTERRUPTIBLE);
        if (signal_pending (current))  /* a signal arrived */
        {
            ret = -ERESTARTSYS; /* tell the fs layer to handle it */
            goto fail;
        }
        /*�ȴ����ˣ��ټ��һ���Ƿ������ݣ����û������*/
        if (0 != fiber_test_status(dev->frag_start_id,dev->frag_count,&fiber_recv_status_b))
        {
            schedule();
        }
        finish_wait(&dev->data_queue, &wait);
    }

    if (copy_to_user(buf, fiber_frag_recv_addr_b[dev->frag_start_id], size))
    {
        printk(KERN_ERR "fiber_read_b: copy_to_user error\n");

        ret = -EIO;
        goto fail;
    }
    rmb();
    /*���������*/
    memset(fiber_frag_recv_addr_b[dev->frag_start_id],0,dev->frag_count * FIBER_FRAG_MEM_SIZE);

    fiber_clear_status(dev->frag_start_id,dev->frag_count,&fiber_recv_status_a);

    up(&fiber_dev_sem);
    up(&fiber_data_sem);
    return size;
fail:
    up(&fiber_dev_sem);
    up(&fiber_data_sem);
    return ret;
}
/*
 ��������    : �ӹ��˷���һ�ε����ݣ��������ݳ���Ϊ4k
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ����
 ����        : 
 ����        : ������
 ����        : 2014��5��20�� 12:53:06
*/
static int32_t fiber_write_once(short frag_id,const __user void* data,int data_size)
{
    struct fiber_head head = {0};
    int32_t ret = 0;

    if (NULL == send_addr)
    {
        printk(KERN_ERR "fiber_write_once:send_addr is NULL \n");
        return -EFAULT;
    }
    ret = fiber_wait_lock(FIBER_IO_WRITE_LOCK_PORT);

    if (0 != ret)
    {
        return ret;
    }

    head.head_flag = 0xAA55;
    head.frame_size = 0xfb0f;
    /*Ӳ��Ҫ��������д��ĳ���Ϊ������֡��־�ͳ��ȵ����ݳ���*/
    head.frag_id = frag_id;

    /*fix me!���ﴫ���֡��С�����ݱ����ܺ���ʵ��������ȷ��*/
    if (FIBER_SEND_TOTAL_MEMORY_SIZE - sizeof(struct fiber_head) < data_size)
    {
        printk(KERN_ERR "fiber_write_once: write data(%#x) too large in once\n",data_size);

        return -EINVAL;
    }
    memcpy(send_addr, &head, sizeof(struct fiber_head));
    /*copy data*/
    if (copy_from_user(send_addr + sizeof(struct fiber_head), data, data_size))
    {
        printk(KERN_ERR "fiber_write_once: copy_from_user error\n");

        return -EIO;
    }

    /*
     * ����˰�д�����ݣ�ע������ķ��ͳ��ȣ���Ϊ���ǵ���Ƶ���ÿ��ʹ��ȫ���ռ���з���
     * ���ݣ������Ҫ����Ҫ�����ȷ�����ݳ��ȣ�������ݳ��Ȳ���ȷ����ᵼ�¹������ݻ��ơ�
     * �����ں���λ�á���ʧ�������Ȼ�����ݳ��ȼȷ�FIBER_SEND_TOTAL_MEMORY_SIZE��Ҳ��
     * FIBER_SEND_TOTAL_MEMORY_SIZE - 4(֡ͷ�ĳ���)��Ӳ��Ҫ��ó������Ϊ��ȥ֡ͷ��־
     * 0x55aa�ĳ��ȣ������������ԭ�����ɹ����������ݶ�����ġ�
     */
    outb(FIBER_SEND_MAX_MEMORY_SIZE >> 8 & 0xff,FIBER_IO_WRITE_DATA_LEN_HIGH_PORT);
    outb(FIBER_SEND_MAX_MEMORY_SIZE & 0xff,FIBER_IO_WRITE_DATA_LEN_LOW_PORT);
    /*reset semaphore and send data*/
    outb(0x01,FIBER_IO_WRITE_LOCK_PORT);

    return 0;
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
    int off = 0;
    int i = 0;
    int frag_count = 0;  /*��Ҫ��Ƭ�Ĵ�С*/
    struct fiber_dev *dev = filp->private_data;

    if (NULL == dev)
    {
        printk(KERN_ERR "fiber_write:filp->private_data is NULL\n");

        return -ENODEV;
    }

    frag_count = fiber_calc_frag_count(size);

    if (down_interruptible(&ci_io_select_sem))
    {
        return -ERESTARTSYS;
    }
    if (down_interruptible(&fiber_dev_sem))
    {
        up(&ci_io_select_sem);
        return -ERESTARTSYS;
    }

    if (frag_count != dev->frag_count)
    {
        printk(KERN_ERR "fiber_write:dev->frag_count(%#x) != frag_count(%#x)\n",dev->frag_count,frag_count);

        ret = -EINVAL;
        goto fail;
    }

    outb(FIBER_IO_SELECT_VALUE,FIBER_IO_PORT);

    off = 0;
    /*�������ݷ�Ƭ֮��ֱ�����ȥ���ȷ���ǰ����������ݣ�����ʣ��Ĳ���*/
    for (i = 0;i < frag_count - 1;i++)
    {
        ret = fiber_write_once(dev->frag_start_id + i,buf + off,FIBER_FRAG_MEM_SIZE);
        if (0 > ret)
        {
            goto fail;
        }
        off += FIBER_FRAG_MEM_SIZE;

        /*
         * �������Ѿ����ͳɹ���Ϊ�˱�����ջ����ӳ٣��������ݳ�ˢ����������һ���ӳ١�
         * �ӳٷ���������Ա�������ֻռ��һ��fragment������Ҳ�����ӳ٣���������CPU��
         * ������ȫ��ȡ4kb��������Ҫ��4 * 1.25ms=5ms��Ϊ�˱�֤������ȷ�����գ����͵�Ƶ��
         * ����Ӧ�ñ�����10ms���ϣ��������ڲ���ϵͳ�ķ�ʱЧ��������ʱ��Ƭ�ķ����������
         * ���ƣ����ǲ���ȷ��׼ȷ���������ӳٶ೤ʱ������öԷ���ȫ�Ľ��յ����ݣ����
         * ֻ��ͨ����ʱ����Լ������жϡ�
         * ͨ�������ж��ӳ���5ms��ʱ������Ч���Ϻã���ֻ��һ֡����Ҫ���͵������ʹ��1ms
         * ���ӳٱ��ܱ�֤���ݲ�����ʧ����Ϊ���û�̬���ں�̬ת�����̵�����Ҫ����ʱ��
         */
        mdelay(15);
    }

    ret = fiber_write_once(dev->frag_start_id + i,buf + off,size - off);
    if (0 > ret)
    {
        goto fail;
    }
    mdelay(1);

    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);
    up(&fiber_dev_sem);
    up(&ci_io_select_sem);

    return size;

fail:
    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);
    up(&fiber_dev_sem);
    up(&ci_io_select_sem);

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
    int ret = 0,i = 0;
    int32_t mem_size = 0;
    struct fiber_dev* dev = NULL;

    dev = container_of(inode->i_cdev,struct fiber_dev,cdev);
    /*
    * extract the type and number bitfields, and don't decode
    * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
    */
    if (_IOC_TYPE(cmd) != FIBER_IOC_MAGIC)
    {
        printk(KERN_ERR "fiber_ioctl:_IOC_TYPE(%d) must be %d\n",_IOC_TYPE(cmd),FIBER_IOC_MAGIC);

        return -ENOTTY;
    }
    if (_IOC_NR(cmd) > FIBER_IOC_MAXNR)
    {
        printk(KERN_ERR "fiber_ioctl:_IOC_NR(%d) must be less than %d\n",_IOC_NR(cmd),FIBER_IOC_MAXNR);
        return -ENOTTY;
    }

    if (down_interruptible(&fiber_dev_sem))
    {
        return -ERESTARTSYS;
    }

    switch (cmd)
    {
    case FIBER_IOC_RESET:
        /*
         * Ϊ��ȫ�����������Ҫ��workqueueֹͣ���ٽ�������Ĳ�������������workqueue����
         * ���򲻺õ��ԣ��������ܻ���ɾ����ֹͣ���
         */
        memset(fiber_mbuf_a,0,FIBER_MBUF_MEM_SIZE);
        memset(fiber_mbuf_b,0,FIBER_MBUF_MEM_SIZE);

        fiber_alloced_frag_id_size = 0;
        fiber_recv_status_a = 0;
        fiber_recv_status_b = 0;

        for (i = 0;i < FIBER_VIRTUAL_SECTIONS;i++)
        {
            fiber_dev_a[i].frag_start_id = -1;
            fiber_dev_a[i].frag_count = -1;

            fiber_dev_b[i].frag_start_id = -1;
            fiber_dev_b[i].frag_count = -1;
        }
        break;
    case FIBER_IOC_ALLOC_MEMORY:
        mem_size = arg;
        if (0 > dev->frag_start_id || 0 >= dev->frag_count)
        {
            dev->frag_count = fiber_calc_frag_count(mem_size);
            dev->frag_start_id = fiber_alloc_frag_id(dev->frag_count);

            dev->peer_fiber_dev->frag_count = dev->frag_count;
            dev->peer_fiber_dev->frag_start_id = dev->frag_start_id;
        }
        if (0 >= dev->frag_count || 0 > dev->frag_start_id)
        {
            dev->frag_count = 0;
            dev->frag_start_id = 0;
            dev->peer_fiber_dev->frag_count = 0;
            dev->peer_fiber_dev->frag_start_id = 0;

            ret = -ENOMEM;
            goto fail;
        }
        break;
    default:
        ret = -EFAULT;
        break;
    }

    up(&fiber_dev_sem);
    return 0;

fail:
    up(&fiber_dev_sem);
    return ret;
}

static const struct file_operations fiber_fops_a =
{
    .owner = THIS_MODULE,
    .read = fiber_read_a,
    .write = fiber_write,
    .ioctl = fiber_ioctl,
    .open = fiber_open,
    .release = fiber_release,
};
static const struct file_operations fiber_fops_b =
{
    .owner = THIS_MODULE,
    .read = fiber_read_b,
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
static int fiber_setup_cdev(struct cdev* dev, int minor,const struct file_operations* f_op)
{
    int err;
    dev_t devno = MKDEV(fiber_major,minor);

    cdev_init(dev, f_op);
    dev->owner = THIS_MODULE;
    dev->ops = f_op;

    err = cdev_add(dev, devno, 1);

    if (err)
    {
        printk(KERN_ERR "Error %d adding fiber %d\n", err, minor);

        return err;
    }

    return 0;
}
/*
��������    : ���Դ�A�ڽ�������
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��19�� 10:57:15
*/
static int fiber_recv_store_ext(uint8_t* recv_addr,
                                uint8_t** frag_address,
                                int32_t lock_port,
                                unsigned long* status)
{
    int off = 0;
    int ret = 0;
    struct fiber_head head = {0};

    if (down_interruptible(&ci_io_select_sem))
    {
        return -ERESTARTSYS;
    }

    if (down_interruptible(&fiber_data_sem))
    {
        up(&fiber_data_sem);
        return -ERESTARTSYS;
    }

    outb(FIBER_IO_SELECT_VALUE,FIBER_IO_PORT);

    /*lock if there has data*/
    ret = fiber_wait_unlock(lock_port);
    if(0 != ret)
    {
        goto fail;
    }

    off = 0;
    memcpy(&head,recv_addr + off,sizeof(struct fiber_head));

    if (0xAA55 != (head.head_flag & 0xffff))
    {
#ifdef FIBER_DEBUG_FRAG_ID
        printk(KERN_INFO "fiber_recv_store_ext:head_flag is not 0xAA55(%#x)\n",head.head_flag & 0xffff);
#endif /*FIBER_DEBUG*/
        ret = -EFAULT;
        goto fail;
    }
    if (0 > head.frag_id || FIBER_FRAG_COUNT <= head.frag_id)
    {
        printk(KERN_INFO "fiber_recv_store_ext:frag_id is not correct(%#x)\n",head.frag_id);
        ret = -EFAULT;
        goto fail;
    }
    off += sizeof(struct fiber_head);

#ifdef FIBER_DEBUG
    printk(KERN_INFO "fiber_recv_store_ext:frag_id is %#x\n",head.frag_id);
#endif /*FIBER_DEBUG*/

    /*
     * �������㣬CPI����Ƶ��Ϊ33MHZ��ÿ����������һ��������Ҫ����30.3ns��ʱ�䣬��һ����
     * ����4kb�����ݣ�������������32λ������Ҫ����1000�β��ܿ�����ɣ�������CPI��������
     * �����ַ�߸��ã����Կ���һ��������Ҫ60.6ns�����Կ���һ��������Ҫ60.6us
     */
    memcpy(frag_address[head.frag_id],recv_addr + off,FIBER_FRAG_MEM_SIZE);
    set_bit(head.frag_id, status);

    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);

    /*
     * �����ﱾ�����Ե���fiber_lock���������������Ա��֪FPGA�����Ѿ�����ȡ�꣬���Ǽ���
     * FPGA�����յ������ݽ���������д����1��������ǵĳ����ڶ�γ��Թ��̵��н���λ���ո�Ϊ
     * ��0��Ϊ�˱�֤���ݲ�����ʧ����������ֻ���Ÿ���һ�θ�λ��ֵ.
     */
    outb(0,lock_port);

    up(&fiber_data_sem);
    up(&ci_io_select_sem);
    return 0;
fail:
    up(&fiber_data_sem);
    up(&ci_io_select_sem);
    return ret;
}
/*
��������    : �����ݶ�ȡ��Ȼ��洢�ڻ�������
            ��������һ����Ҫע��ģ��û��岢Ϊ��֤�ϴ��Ѿ���������ݱ����꣬���Ǳ���ˢ��
            ��ƽ̨����߼��㣬������Ϊ���ֳ�ˢ������ͨ��Ϊ��ͬ���ݣ�����ֻ��֤�������
            ������ʧ����
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��18�� 8:30:35
*/
static int fiber_recv_store(void* arg)
{
    struct fiber_recver* recver = &fiber_recver;
    int ret = 0;
    int i = 0;
    DEFINE_WAIT(wait);

    printk(KERN_INFO "fiber_recv_store:begin recieve\n");

    while (1)
    {
        prepare_to_wait(&fiber_queue, &wait, TASK_INTERRUPTIBLE);
        schedule();
        finish_wait(&fiber_queue, &wait);
#ifdef FIBER_DEBUG_IRQ
        printk(KERN_INFO "fiber_recv_store:fiber_queue waik up\n");
#endif /*FIBER_DEBUG*/
        if (FIBER_TRUE == b_stop_thread)
        {
            break;
        }

        ret = fiber_recv_store_ext(recver->recv_addr_a,
                                   fiber_frag_recv_addr_a,
                                   FIBER_IO_RECV_LOCK_PORT_A,
                                   &fiber_recv_status_a);
        ret = fiber_recv_store_ext(recver->recv_addr_b,
                                   fiber_frag_recv_addr_b,
                                   FIBER_IO_RECV_LOCK_PORT_B,
                                   &fiber_recv_status_b);

        for (i = 0;i < FIBER_VIRTUAL_SECTIONS;i++)
        {
            if (0 != fiber_test_status(fiber_dev_a[i].frag_start_id,
                                       fiber_dev_a[i].frag_count,
                                       &fiber_recv_status_a))
            {
                wake_up_interruptible(&fiber_dev_a[i].data_queue); /* awake any reading process */
            }
            if (0 != fiber_test_status(fiber_dev_b[i].frag_start_id,
                                       fiber_dev_b[i].frag_count,
                                       &fiber_recv_status_b))
            {
                wake_up_interruptible(&fiber_dev_b[i].data_queue); /* awake any reading process */
            }
        }
    }
    do_exit(0);
    return 0;
}
/*
��������    : 
����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
����        : 
����        : ������
����        : 2014��3��18�� 13:54:36
*/
static int __init fiber_init_recver(void)
{
    fiber_recver.recv_addr_a = ioremap(FIBER_RECV_START_ADDR_A,FIBER_RECV_TOTAL_MEMORY_SIZE_A);
    fiber_recver.recv_addr_b = ioremap(FIBER_RECV_START_ADDR_B,FIBER_RECV_TOTAL_MEMORY_SIZE_B);

    if (NULL == fiber_recver.recv_addr_a || NULL == fiber_recver.recv_addr_b)
    {
        return -ENOMEM;
    }

    return 0;
}
/*
 ��������    : ��ʼ�����˸���fragment��ʹ�õĵ�ַ
 ����ֵ      : �ɹ�Ϊ0��ʧ��Ϊ-1
 ����        : 
 ����        : ������
 ����        : 2014��5��20�� 14:15:46
*/
static int __init fiber_frag_addr_init(void)
{
    int32_t i = 0;

    for (i = 0;i < FIBER_FRAG_COUNT;i++)
    {
        fiber_frag_recv_addr_a[i] = fiber_mbuf_a + FIBER_FRAG_MEM_SIZE * i;
        fiber_frag_recv_addr_b[i] = fiber_mbuf_b + FIBER_FRAG_MEM_SIZE * i;
    }

    return 0;
}

static irqreturn_t fiber_interrupt(int irq, void *dev_id)
{
#ifdef FIBER_DEBUG
    printk("fiber_interrupt:interrupt\n");
#endif /*FIBER_DEBUG_IRQ*/
    wake_up_interruptible(&fiber_queue); /* awake any reading process */
    return IRQ_HANDLED;
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
    int i = 0;
    int minor_a = 0;
    int minor_b = 0;
    dev_t devno = 0;

    if (fiber_major)
    {
        devno = MKDEV(fiber_major, 0);
        ret = register_chrdev_region(devno, FIBER_DEVS_NR, "fiber");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, FIBER_DEVS_NR, "fiber");
        fiber_major = MAJOR(devno);
    }
    if (ret < 0)
    {
        printk(KERN_WARNING "fiber: can't get major %d\n", fiber_major);

        return ret;
    }
    fiber_recv_status_a = 0;
    fiber_recv_status_b = 0;

    send_addr = ioremap(FIBER_SEND_START_ADDR,FIBER_SEND_TOTAL_MEMORY_SIZE);

    sema_init(&fiber_dev_sem,1);
    sema_init(&fiber_data_sem,1);

    for (i = 0;i < FIBER_VIRTUAL_SECTIONS;i++)
    {
        minor_a = i * 2;
        minor_b = minor_a + 1;

        fiber_dev_a[i].b_use = FIBER_FALSE;
        fiber_dev_a[i].frag_start_id = -1;
        fiber_dev_a[i].frag_count = -1;
        init_waitqueue_head(&(fiber_dev_a[i].data_queue));
        fiber_dev_a[i].peer_fiber_dev = &fiber_dev_b[i];

        ret = fiber_setup_cdev(&fiber_dev_a[i].cdev,minor_a,&fiber_fops_a);
        if (0 != ret)
        {
            goto fail;
        }

        fiber_dev_b[i].b_use = FIBER_FALSE;
        fiber_dev_b[i].frag_start_id = -1;
        fiber_dev_b[i].frag_count = -1;
        init_waitqueue_head(&(fiber_dev_b[i].data_queue));
        fiber_dev_b[i].peer_fiber_dev = &fiber_dev_a[i];

        ret = fiber_setup_cdev(&fiber_dev_b[i].cdev,minor_b,&fiber_fops_b);
        if (0 != ret)
        {
            goto fail;
        }
    }

    /*init irq*/
    fiber_irq = FIBER_IRQ;
    ret = request_irq(fiber_irq, fiber_interrupt, 0, "fiber", NULL);
    if (ret)
    {
        printk(KERN_ERR "fiber_init: can't get assigned irq %i\n", fiber_irq);

        goto fail;
    }

    ret = fiber_init_recver();
    if (0 != ret)
    {
        goto fail;
    }
    fiber_frag_addr_init();

    b_stop_thread = FIBER_FALSE;
    fiber_recv_thread = kthread_run(fiber_recv_store,NULL,"fiber_recver"); 
#ifdef FIBER_DEBUG
    /*����proc�ӿ�*/
    fiber_create_proc();
#endif /*FIBER_DEBUG*/

    printk(KERN_INFO "fiber init.\n");

    return 0;

fail:
    unregister_chrdev_region(MKDEV(fiber_major, 0), FIBER_DEVS_NR); /*�ͷ��豸��*/
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

    unregister_chrdev_region(MKDEV(fiber_major, 0), FIBER_DEVS_NR); /*�ͷ��豸��*/

    for (i = 0;i < FIBER_VIRTUAL_SECTIONS;i++)
    {
        cdev_del(&fiber_dev_a[i].cdev);
        cdev_del(&fiber_dev_b[i].cdev);
    }

    free_irq(fiber_irq, NULL);

    if (NULL != fiber_recv_thread)
    {
        printk("fiber_exit:stop thread\n");
        b_stop_thread = FIBER_TRUE;
        wake_up_interruptible(&fiber_queue); /* awake any reading process */
        /*
         * note!�����ﲻҪ����kthread_stop(fiber_recv_thread)���߳��˳�,�������˸�
         * ����֮������ٴν����̺߳�������ᵼ�²���bug����ȷ�ķ�ʽΪ�����̺߳����ڲ�
         * ʹ���˳�����
         **/
    }
#ifdef FIBER_DEBUG
    fiber_remove_proc();
#endif /*FIBER_DEBUG*/

    printk(KERN_INFO "fiber exit!\n");

    return;
}

module_init(fiber_init);
module_exit(fiber_exit);

