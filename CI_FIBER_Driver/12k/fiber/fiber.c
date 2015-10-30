/*********************************************************************
Copyright (C), 2011,  Co.Hengjun, Ltd.

作者        : 张彦升
版本        : 1.0
创建日期    : 2014年3月13日 12:59:35
用途        : 光纤板的驱动
历史修改记录: v1.0    创建
             v1.1    更正光纤先读共享区的数据然后读取持久区的数据读不上来的
                     错误
             v1.3    由于备系校核数据发送会被其它数据冲刷，最后考虑将所有数据统一
                     按照各自占用一个设备的方式设计，该版本当中每次发送和接收
                     会使用全部内存空间
             v1.4    上一版当中使用了缓存，这一版当中去除缓存
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

#define FIBER_TRUE   0xAA        /*真*/
#define FIBER_FALSE  0x55        /*假*/

/*
 * 为了判断每一帧的内容是否已经被读取，我们在设备里面设置了last_sn变量。但是last_sn的
 * 初始化却成了问题，由于sn一直自增会回转，所以last_sn初始化成任何值势必导致第一次工作
 * 无法进行，一种可行的办法是使sn在一定的范围内回转，last_sn初始化成该范围之外的一个值
 * 即可，另外使用FIBER_SN_MAX取模运算，最好使其初始化在2的整数次方内
 */
#define FIBER_SN_MAX 0xf00000

static struct fiber_dev fiber_dev_a[FIBER_PAIR_DEVS_NR];    /*A口设备*/
static struct fiber_dev fiber_dev_b[FIBER_PAIR_DEVS_NR];    /*B口设备*/

static struct semaphore fiber_sem;  /*光纤使用一个大的信号量控制所有设备的访问*/

static int32_t fiber_major = FIBER_MAJOR;

static int32_t fiber_send_allocated_size = 0;
static int32_t fiber_a_recv_allocated_size = 0; /*将头部偏移过去*/
static int32_t fiber_b_recv_allocated_size = 0;

static __iomem uint8_t* send_head_addr = NULL;
static __iomem uint8_t* recv_head_addr_a = NULL;
static __iomem uint8_t* recv_head_addr_b = NULL;

module_param(fiber_major, int, S_IRUGO);
/*
 功能描述    : 内存管理函数
 返回值      : 成功为申请后内存的指针，否则返回NULL
 参数        : @size申请内存的大小
 作者        : 张彦升
 日期        : 2013年12月1日 15:51:39
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
 功能描述    : 负责a口设备接收数据的内存申请
 返回值      : 成功为申请后内存的指针，否则返回NULL
 参数        : @size申请内存的大小
 作者        : 张彦升
 日期        : 2013年12月1日 15:51:39
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
 功能描述    : 负责b口设备接收数据的内存申请
 返回值      : 成功为申请后内存的指针，否则返回NULL
 参数        : @size申请内存的大小
 作者        : 张彦升
 日期        : 2013年12月1日 15:51:39
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
功能描述    : 等待锁被上锁
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月19日 14:34:50
*/
static int fiber_wait_lock(int port)
{
    int lock = 0;
    int count = 0;

    /*
     * 根据协议这里的判断应该是不等于0X01，然而FPGA程序并未确保第一次下发数据的时候该位
     * 为0x00，所以改使用这种逻辑
     */
    do
    {
        lock = inb(port);
        udelay(100);  /*延迟1微妙*/
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
    /*从内存当中取出recv_sn并打印其实意义不大，因为这个值只有在光纤帧头正确的时候才有意义*/

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
 功能描述    : 创建proc接口文件
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月19日 16:27:58
*/
static void fiber_create_proc(void)
{
    create_proc_read_entry("fiber_mem",0,NULL, fiber_proc_mem, NULL);

    return;
}
/*
 功能描述    : 删除proc接口文件
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月19日 16:27:58
*/
static void fiber_remove_proc(void)
{
    /* no problem if it was not registered */
    remove_proc_entry("fiber_mem", NULL);
}

#endif /* FIBER_DEBUG */

/*
功能描述    : open系统调用的实现
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 13:26:23
*/
static int fiber_open(struct inode* inode, struct file* filp)
{
    struct fiber_dev* dev;
    int num = iminor(inode);
    int ret = 0;

    /*该设备mknod的时候不能创建大于FIBER_DEVS_NR个设备*/
    if (FIBER_PAIR_DEVS_NR * 2 < num)
    {
        return -ENODEV;
    }

    dev = container_of(inode->i_cdev,struct fiber_dev,cdev);

    if (down_interruptible(&fiber_sem))
    {
        return -ERESTARTSYS;
    }

    /*我们的目标是允许一次open系统调用，其它所有打开操作全部拒绝*/
    if (FIBER_TRUE == dev->b_use)
    {
        ret = -EBUSY;
        goto fail;
    }

    dev->b_use = FIBER_TRUE;

    up(&fiber_sem);
    /*将设备结构体指针赋值给文件私有数据指针*/
    filp->private_data = dev;

    return 0;
fail:

    up(&fiber_sem);
    return ret;
}
/*
功能描述    : 释放设备
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 13:28:10
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
功能描述    : 读取函数
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 13:28:38
*/
static ssize_t fiber_read(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
    int ret = 0;
    struct fiber_dev *dev = filp->private_data; /*获得设备结构体指针*/

    if (NULL == dev)
    {
        printk(KERN_ERR "fiber_read:dev is NULL\n");

        return -EINVAL;
    }
    /*说明并没有使用ioctl进行过初始化*/
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
        /*判断上次是否有缓存数据，如果有的话则读取并返回*/
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
功能描述    : 写数据函数
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 15:45:19
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
    /*说明并没有使用ioctl进行过初始化*/
    if (0 >= dev->buf_size || NULL == dev->send_sn_addr || NULL == dev->send_addr)
    {
        return -EIO;
    }

    /*检查数据区大小*/
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
    /*拷贝帧号*/
    memcpy(dev->send_sn_addr,&dev->send_sn,sizeof(int32_t));

    if (copy_from_user(dev->send_addr, buf, size))
    {
        printk(KERN_ERR "fiber_write:copy_from_user failed\n");
        ret = -EIO;
        goto fail;
    }

    /*填充头部*/
    send_head_addr[0] = 0x55;
    send_head_addr[1] = 0xaa;/*帧头*/
    send_head_addr[2] = 0x0f;
    send_head_addr[3] = 0xfb;/*数据长度，这里长度为所有长度-4，更改可能会发生数据乱序*/
    /*
     * 向光纤板写入数据，注意这里的发送长度，因为我们的设计当中每次使用全部空间进行发送
     * 数据，因此需要这里要填充正确的数据长度，如果数据长度不正确，则会导致光纤数据回绕、
     * 补充在后续位置、丢失等情况，然而数据长度既非FIBER_SEND_TOTAL_MEMORY_SIZE，也非
     * FIBER_SEND_TOTAL_MEMORY_SIZE - 4(帧头的长度)，硬件要求该长度最大为减去帧头标志
     * 0x55aa的长度，该问题的最终原因是由光纤是流数据而引起的。
     */
    outb((data_len >> 8) & 0xff,FIBER_IO_WRITE_DATA_LEN_HIGH_PORT);
    outb(data_len & 0xff,FIBER_IO_WRITE_DATA_LEN_LOW_PORT);

    /*
     * 在这里按照编程的思路我们应该调用fiber_unlock函数，但是在FPGA实现当中，将锁
     * 解锁之后再返回来查看锁是否真的解锁，没有必要，FPGA会立即将数据下发下去并将该
     * 端口的值改为0x0，所以并没有必要等待
     */
    outb(0x01,FIBER_IO_WRITE_LOCK_PORT);

    outb(FIBER_IO_RESET_VALUE,FIBER_IO_PORT);

    /*一切都成功则将序号增1*/
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
功能描述    : 通过ioctl进行控制
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 15:16:52
*/
static int fiber_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int i = 0;
    int mem_size = 0;
    int minor_devno = MINOR(inode->i_rdev);

    struct fiber_dev *dev = filp->private_data; /*获得设备结构体指针*/

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
            fiber_dev_a[i].last_recv_sn = FIBER_SN_MAX;   /*帧号默认比FIBER_SN_MAX大于或等于即可*/

            fiber_dev_b[i].buf_size = 0;
            fiber_dev_b[i].send_addr = NULL;
            fiber_dev_b[i].recv_addr = NULL;
            fiber_dev_b[i].send_sn_addr = NULL;
            fiber_dev_b[i].recv_sn_addr = NULL;
            fiber_dev_b[i].send_sn = 0;
            fiber_dev_b[i].last_recv_sn = FIBER_SN_MAX;   /*帧号默认比FIBER_SN_MAX大于或等于即可*/
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
            /*发送和接收的空间地址必须一一对应*/
            dev->send_sn_addr = (int32_t*)fiber_send_mem_alloc(sizeof(int32_t));
            dev->send_addr = fiber_send_mem_alloc(mem_size);

            dev->peer_fiber_dev->send_sn_addr = dev->send_sn_addr;
            dev->peer_fiber_dev->send_addr = dev->send_addr;

        }
        /*a口使用偶数子设备，b口使用奇数子设备*/
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
        /*在申请内存时将sn设置为最大值用来标志该设备是否有数据*/
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
功能描述    : 将驱动程序的设备安装到内核内
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 10:24:02
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
功能描述    : 光纤驱动程序初始化函数
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 10:23:31
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

    /*初始化光纤信号量*/
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
    /*申请头部*/
    send_head_addr = fiber_send_mem_alloc(FIBER_HEAD_SIZE);

#ifdef FIBER_DEBUG
    /*创建proc接口*/
    fiber_create_proc();
#endif /*FIBER_DEBUG*/

    printk(KERN_INFO "fiber init.\n");

    return 0;

fail:
    unregister_chrdev_region(MKDEV(fiber_major, 0), FIBER_PAIR_DEVS_NR * 2); /*释放设备号*/

    printk(KERN_ERR "fiber_init:failed\n");

    return ret;
}
/*
功能描述    : 注销函数
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 10:23:45
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

    unregister_chrdev_region(MKDEV(fiber_major, 0), FIBER_PAIR_DEVS_NR * 2); /*释放设备号*/

    printk(KERN_INFO "fiber exit!\n");

    return;
}

module_init(fiber_init);
module_exit(fiber_exit);

