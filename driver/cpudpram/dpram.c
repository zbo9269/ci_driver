/*********************************************************************
Copyright (C), 2011,  Co.Hengjun, Ltd.

作者        : 张彦升
版本        : 1.0
创建日期    : 2014年1月18日 14:25:13
用途        : 双CPU之间进行通信所用的双口RAM的驱动程序。
历史修改记录: v1.0    创建
             v1.2    上一版本当中对锁并没有理解透彻，在程序当中使用marker
             作为变量的名称，而该语义并不是很明确，在该版本当中统一使用锁
             作为变量的名称。该模型当中，write函数负责解锁，而read函数负责
             上锁，这种实现方式将加锁与解锁放在了不同的步骤当中，需谨慎处理。
             v1.3   20140712 张彦升
                    添加rw_sem，使写操作不再考虑rw_lock，意味着写操作会冲刷
             掉上次写的数据
**********************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/mm.h>       /*for mmap*/

#include "dpram.h"

MODULE_AUTHOR("zhangys");
MODULE_LICENSE("Dual BSD/GPL");

static struct dpram_pair_devs pair_devs[DPRAM_PAIR_DEV_NR];
const static int total_size = DPRAM_TOTAL_MEMORY_SIZE;
static int alloced_size = 0;

static int dpram_major = DPRAM_MAJOR;

module_param(dpram_major, int, S_IRUGO);

/*
 功能描述    : dpram内存管理函数，负责申请内存
 返回值      : 成功为申请后内存的指针，否则返回NULL
 参数        : @size申请内存的大小
 作者        : 张彦升
 日期        : 2013年12月1日 15:51:39
*/
static unsigned char* dpram_mem_alloc(int size)
{
    unsigned char* ptr = NULL;

    if (size > total_size - alloced_size)
    {
        printk(KERN_ERR "dpram_mem_alloc:request memory %#x,but only remain %#x"
            "[total_size(%#x) - allocated_size(%#x)]\n",
            size, total_size - alloced_size,total_size,alloced_size);

        return NULL;
    }

    ptr = ioremap(DPRAM_START_ADDR + alloced_size,size);
    if(NULL == ptr)
    {
        printk("dpram_mem_alloc:ioremap error\n");
    }

    alloced_size += size;

    return ptr;
}
/*
功能描述    : 为锁加锁，加锁实质上是将锁置为真
返回值      : 成功为0，失败为-ETIME
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 14:38:27
*/
static int dpram_lock(__iomem uint8_t* lock)
{
    uint8_t ret = 0;
    int count = 0;

    do 
    {
        iowrite8(DPRAM_TRUE,lock);
        /* be sure to flush this to the card */
        wmb();
        ret = ioread8(lock);
        count ++;
    } while (DPRAM_TRUE != ret && count < 10);

    if (DPRAM_TRUE != ret)
    {
#ifdef DPRAM_DEBUG_LOCK
        printk(KERN_INFO "dpram_lock:%#x:%#x,count:%#x\n",(int)lock,ret,count);
#endif /*DPRAM_DEBUG_LOCK*/
        return -ETIME;
    }
    else
    {
        return 0;
    }
}
/*
功能描述    : 为锁解锁，实质上是将锁置为假
返回值      : 成功为0，失败为-ETIME
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 14:38:53
*/
static int dpram_unlock(__iomem uint8_t* lock)
{
    uint8_t ret = 0;
    int count = 0;

    /*
     * 根据经验，大多数情况下只需要一次就可以顺利将结果写入，偶尔会需要两次，如果 
     */
    do
    {
        iowrite8(DPRAM_FALSE,lock);
        /* be sure to flush this to the card */
        wmb();
        ret = ioread8(lock);
        rmb();
        count ++;
    } while (DPRAM_FALSE != ret && count < 10);

    if (DPRAM_FALSE != ret)
    {
#ifdef DPRAM_DEBUG_LOCK
        printk(KERN_INFO "dpram_unlock:%#x:%#x,count:%#x\n",(int)lock,ret,count);
#endif /*DPRAM_DEBUG_LOCK*/
        return -ETIME;
    }
    else
    {
        return 0;
    }
}
/*
功能描述    : 等待锁被上锁
返回值      : 成功为0，失败为-ETIME
参数        : @lock 锁
             @wait_times 等待的次数
作者        : 张彦升
日期        : 2014年3月3日 14:14:00
*/
static int dpram_wait_lock(__iomem uint8_t* lock)
{
    int i = 0;

    while (DPRAM_TRUE != ioread8(lock) && i < 10)
    {
        rmb();
        /*
         * 注意，等待锁的时间与写锁尝试的延迟时间务必不能相等，且等待锁的时间一定要长于
         * 写锁的时间，若两者的时间相等，则使得两者读写冲突的概率大大增加，这导致尝试次
         * 数大大增加，另外，这里延迟了1ms，最大延迟了10ms，该延迟时间较长，且不释放
         * CPU资源，作为一个问题遗留来更正
         */
        udelay(10); /*延迟1*/
        i++;
    }
    /*是超时*/
    if (DPRAM_TRUE != ioread8(lock))
    {
#ifdef DPRAM_DEBUG_LOCK
        printk(KERN_INFO "dpram_wait_lock:%#x:%#x\n",(int)lock,ioread8(lock));
#endif /*DPRAM_DEBUG_LOCK*/
        return -ETIME;
    }
    else
    {
        return 0;
    }
}
/*
功能描述    : 等待锁没有被占用，延迟函数为微妙级函数
返回值      : 成功为0，失败为-ETIME
参数        : @lock 锁
             @wait_times 等待的次数
作者        : 张彦升
日期        : 2014年3月3日 14:14:00
*/
static int dpram_wait_unlock(__iomem uint8_t* lock)
{
    int i = 0;

    while (DPRAM_FALSE != ioread8(lock) && i < 10)
    {
        /* don't have any cached variables */
        rmb();
        /*
         * 注意，等待锁的时间与写锁尝试的延迟时间务必不能相等，且等待锁的时间一定要长于
         * 写锁的时间，若两者的时间相等，则使得两者读写冲突的概率大大增加，这导致尝试次
         * 数大大增加，另外，这里延迟了1ms，最大延迟了10ms，该延迟时间较长，且不释放
         * CPU资源，作为一个问题遗留来更正
         */
        udelay(10); /*延迟1毫秒*/
        i++;
    }
    /*是超时*/
    if (DPRAM_FALSE != ioread8(lock))
    {
#ifdef DPRAM_DEBUG_LOCK
        printk(KERN_INFO "dpram_wait_unlock:%#x:%#x\n",(int)lock,ioread8(lock));
#endif /*DPRAM_DEBUG_LOCK*/
        return -ETIME;
    }
    else
    {
        return 0;
    }
}
/*
 功能描述    : 重新分配内存时，需要调用
              at last,we decide not reclean all memory
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2013年12月1日 20:24:32
*/
static void dpram_reset_mem(void)
{
    alloced_size = 0;

    return;
}

#ifdef DPRAM_DEBUG
/*
 功能描述    : 打印内存信息
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月26日 13:47:05
*/
static int dpram_proc_mem(char *buf, char **start, off_t offset,
                       int count, int *eof, void *data)
{
    int len = 0;
    int i = 0;
    struct dpram_dev* dev = NULL;

    len += sprintf(buf + len, "\t"
                            "self_data_addr\t"
                            "self_rw_lock\t"
                            "self_rw_sem\t"
                            "self_buf_size\t"
                            "peer_data_addr\t"
                            "peer_rw_lock\t"
                            "peer_rw_sem\t"
                            "peer_buf_size\n");
    for (i = 0;i < DPRAM_PAIR_DEV_NR;i++)
    {
        /*print self*/
        dev = &pair_devs[i].dpram_dev_a;
        if (down_interruptible(&dev->sem))
        {
            return -ERESTARTSYS;
        }
        len += sprintf(buf + len,"dpram%d\t%#x\t%#x\t%#x\t%#x\t\t",i + 1,
                                (int)dev->data_addr,
                                (int)dev->rw_lock,
                                (int)dev->rw_sem,
                                (int)dev->buf_size );

        up(&dev->sem);
        /*print peer*/
        dev = &pair_devs[i].dpram_dev_b;
        if (down_interruptible(&dev->sem))
        {
            return -ERESTARTSYS;
        }
        len += sprintf(buf + len,"%#x\t%#x\t%#x\t%#x\n",
                                (int)dev->data_addr,
                                (int)dev->rw_lock,
                                (int)dev->rw_sem,
                                (int)dev->buf_size);

        up(&dev->sem);
    }

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
static void dpram_create_proc(void)
{
    create_proc_read_entry("dpram_mem",0,NULL, dpram_proc_mem, NULL);

    return;
}
/*
 功能描述    : 删除proc接口文件
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月19日 16:27:58
*/
static void dpram_remove_proc(void)
{
    /* no problem if it was not registered */
    remove_proc_entry("dpram_mem", NULL);
}

#endif /* DPRAM_DEBUG */
/*
 功能描述    : 系统调用open的实现
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年2月5日 22:09:24
*/
static int dpram_open(struct inode *inode, struct file *filp)
{
    int minor = MINOR(inode->i_rdev);
    struct dpram_dev *dev;

    /*该设备mknod的时候不能创建大于DPRAM_DEVS_NR个设备*/
    if (minor > DPRAM_DEVS_NR)
    {
        return -ENODEV;
    }
    dev = container_of(inode->i_cdev, struct dpram_dev, cdev);

    /*我们的目标是允许一次open系统调用，其它所有打开操作全部拒绝*/
    if (DPRAM_TRUE == dev->b_use)
    {
        return -EBUSY;
    }

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }
    filp->private_data = dev;

    dev->b_use = DPRAM_TRUE;

    up(&dev->sem);

    return 0;
}
/*
 功能描述    : 释放函数
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月25日 15:02:51
*/
static int dpram_release(struct inode *inode, struct file *filp)
{
    struct dpram_dev* dev = filp->private_data;

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    dev->b_use = DPRAM_FALSE;

    up(&dev->sem);

    return 0;
}
/*
 功能描述    : 读取函数
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年2月11日 11:08:11
*/
static ssize_t dpram_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;

    struct dpram_dev *self_dev = filp->private_data;
    struct dpram_dev *dev = NULL;

    if (NULL == self_dev)
    {
        printk(KERN_ERR "dpram_read:filp->private_data is NULL\n");
        return -EFAULT;
    }

    dev = self_dev->peer_dev;

    if (NULL == dev)
    {
        printk(KERN_ERR "dpram_read:dev(dpram_dev) is NULL\n");
        return -EINVAL;
    }

    if (NULL == dev->data_addr)
    {
        printk(KERN_ERR "dpram_read:dpram send_addr is NULL\n");
        return -EINVAL;
    }
    if (dev->buf_size < size)
    {
        printk(KERN_ERR "dpram_read:dev->send_size(%d) < size(%d)\n",dev->buf_size,size);

        return -ENOMEM;
    }

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    outb(DPRAM_IO_SELECT_VALUE,DPRAM_IO_PORT);

    /*对read的工作是等待锁被解锁，然后做相应的工作再上锁，它只负责上锁*/
    ret = dpram_wait_unlock(dev->rw_lock);

    if (0 != ret)
    {
        goto fail;
    }

    ret = dpram_wait_unlock(dev->rw_sem);
    if (0 != ret)
    {
        goto fail;
    }

    /*上锁，防止写操作同时进行*/
    ret = dpram_lock(dev->rw_sem);
    if (0 != ret)
    {
        goto fail;
    }

    if (__copy_to_user(buf, dev->data_addr, size))
    {
        printk(KERN_ERR "dpram_read: copy_to_user error\n");

        ret = -EIO;
        goto fail;
    }
    rmb();

    /*每次读取完成务必将数据清零，避免读取脏数据*/
    memset_io(dev->data_addr,0,size);

    /*执行完动作开始加锁*/
    ret = dpram_lock(dev->rw_lock);
    /*note，这里可以不goto fail，可以让其下次继续尝试*/

    /*使用完之后释放*/
    ret = dpram_unlock(dev->rw_sem);
    if (0 != ret)
    {
        goto fail;
    }

    outb(DPRAM_IO_RESET_VALUE,DPRAM_IO_PORT);
    up(&dev->sem);
    return size;
fail:

    dpram_unlock(dev->rw_sem);
    outb(DPRAM_IO_RESET_VALUE,DPRAM_IO_PORT);
    up(&dev->sem);
    return ret;
}
/*
 功能描述    : 写操作
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月25日 15:07:39
*/
static ssize_t dpram_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;
    struct dpram_dev* dev = filp->private_data;

    if (NULL == dev)
    {
        printk(KERN_ERR "dpram_write:filp->private_date is NULL\n");
        return -EFAULT;
    }

    if (NULL == dev->data_addr)
    {
        printk(KERN_ERR "dpram_write:dpram send_addr is NULL\n");

        return -EFAULT;
    }

    if (size > dev->buf_size)
    {
        printk(KERN_ERR "dpram_write:too many data,%d(size) > %d(buf_size)\n",size,dev->buf_size);
        return -EFAULT;
    }
    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }
    outb(DPRAM_IO_SELECT_VALUE,DPRAM_IO_PORT);
    /*
     * 如果有数据则返回错误，注意这里一个细节，在read的时候使用的是 != 做判断，而在write
     * 的时候使用了 == 做判断，也就是说在write的时候即使状态为PENDING状态，也会进行写操作,
     * 而在read的时候状态在NO_DATA和PENDING_DATA的时候都表明没有数据可以读。
     */
    ret = dpram_wait_unlock(dev->rw_sem);
    if (0 != ret)
    {
        goto fail;
    }

    /*上锁，防止读操作同时进行*/
    ret = dpram_lock(dev->rw_sem);
    if (0 != ret)
    {
        goto fail;
    }

#if 0
    printk(KERN_INFO ":dpram_write:size:%#x:begin\n",size);
#endif
    if (__copy_from_user(dev->data_addr, buf, size))
    {
        printk(KERN_ERR "dpram_write:copy_from_user failed\n");
        ret = -EIO;
        goto fail;
    }
#if 0
    printk(KERN_INFO ":dpram_write:size:%#x:end\n",size);
#endif

    wmb();

    /*写动作只负责解锁*/
    ret = dpram_unlock(dev->rw_lock);
    if (0 != ret)
    {
        /*原理上不会出现这种错误*/
        goto fail;
    }

    /*解锁，说明可以进行下次获取数据*/
    ret = dpram_unlock(dev->rw_sem);
    if (0 != ret)
    {
        goto fail;
    }

    outb(DPRAM_IO_RESET_VALUE,DPRAM_IO_PORT);
    up(&dev->sem);

    return size;
fail:
    dpram_unlock(dev->rw_sem);

    outb(DPRAM_IO_RESET_VALUE,DPRAM_IO_PORT);

    up(&dev->sem);

    return ret;
}
/*
 * 检查内存空间是否正常
 */
static int32_t dpram_check_mem(void)
{
    u8* ptr = NULL;
    size_t test_size = 0xfff;
    size_t i = 0;
    u8 test_result = 0xff;

    outb(DPRAM_IO_SELECT_VALUE,DPRAM_IO_PORT);

    ptr = ioremap(DPRAM_START_ADDR,test_size);

    if(NULL == ptr)
    {
        printk(KERN_ERR "dpram_check_mem:ioremap error\n");
        return -1;
    }
    for(i = 0;i < test_size;i++)
    {
        test_result &= *ptr++;
    }

    outb(DPRAM_IO_RESET_VALUE,DPRAM_IO_PORT);
    /*当内存全为0说明有硬件未初始化成功*/
    if(0xff == test_result)
    {
        return -ENOMEM;
    }
    return 0;
}
/*
 功能描述    : 在ioctl系统调用当中设置每个buffer的大小
 返回值      : 根据不同的参数返回不同的值
 参数        : 
 作者        : 张彦升
 日期        : 2013年12月1日 14:08:17
*/
static int dpram_ioctl(struct inode *inode, struct file *filp,
                       unsigned int cmd, unsigned long arg)
{
    int mem_size = 0;
    int i = 0;
    int ret = 0;
    struct dpram_dev* dev = filp->private_data;
    int minor_devno = MINOR(inode->i_rdev);

    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != DPRAM_IOC_MAGIC)
    {
        printk(KERN_ERR "dpram_ioctl:_IOC_TYPE(%d) must be %d\n",_IOC_TYPE(cmd),DPRAM_IOC_MAGIC);

        return -ENOTTY;
    }
    if (_IOC_NR(cmd) > DPRAM_IOC_MAXNR)
    {
        printk(KERN_ERR "dpram_ioctl:_IOC_NR(%d) must be less than %d\n",_IOC_NR(cmd),DPRAM_IOC_MAXNR);
        return -ENOTTY;
    }

    if (NULL == dev)
    {
        printk(KERN_ERR "dpram_ioctl:filp->private_data is NULL\n");

        return -EFAULT;
    }

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    outb(DPRAM_IO_SELECT_VALUE,DPRAM_IO_PORT);

    switch (cmd)
    {
    case DPRAM_IOCRESET:
        for (i = 0;i < DPRAM_PAIR_DEV_NR;i++)
        {
            pair_devs[i].dpram_dev_a.buf_size = 0;
            pair_devs[i].dpram_dev_a.data_addr = NULL;
            pair_devs[i].dpram_dev_a.rw_lock = NULL;
            pair_devs[i].dpram_dev_a.rw_sem = NULL;

            pair_devs[i].dpram_dev_b.buf_size = 0;
            pair_devs[i].dpram_dev_b.data_addr = NULL;
            pair_devs[i].dpram_dev_b.rw_lock = NULL;
            pair_devs[i].dpram_dev_b.rw_sem = NULL;
        }
        dpram_reset_mem();

        break;
    case DPRAM_IOC_CLEAN_MEMORY:

        if (NULL == dev->peer_dev->rw_lock && NULL == dev->peer_dev->rw_sem)
        {
            ret = -EFAULT;
            goto fail;
        }
        /*避免读写时的竞争*/
        ret = dpram_wait_unlock(dev->peer_dev->rw_sem);
        if (0 != ret)
        {
            goto fail;
        }

        ret = dpram_lock(dev->peer_dev->rw_sem);
        if (0 != ret)
        {
            goto fail;
        }

        /*将有数标志清掉*/
        ret = dpram_lock(dev->peer_dev->rw_lock);
        if (0 != ret)
        {
            goto fail;
        }
        dpram_unlock(dev->peer_dev->rw_sem);
        break;
    case DPRAM_IOC_ALLOC_MEMORY:
        if (NULL != dev->data_addr)
        {
            printk(KERN_ERR "dpram_ioctl:dev->send_addr is not NULL,not support realloc\n");

            ret = -ENOMEM;
            goto fail;
        }
        if (NULL != dev->rw_lock)
        {
            printk(KERN_ERR "dpram_ioctl:something be wrong.dev->write_sem is not NULL\n");
            ret = -ENOMEM;
            goto fail;
        }
        mem_size = arg;
        if (0 >= mem_size)
        {
            printk(KERN_ERR "dpram_ioctl:alloc size must great than 0\n");
            ret = -EINVAL;
            goto fail;
        }

        if (minor_devno % 2 == 0)
        {
            /*先为自己申请后为对方申请*/
            dev->data_addr = dpram_mem_alloc(mem_size);

            if (NULL == dev->data_addr)
            {
                ret = -ENOMEM;
                goto fail;
            }

            dev->peer_dev->data_addr = dpram_mem_alloc(mem_size);

            if (NULL == dev->peer_dev->data_addr)
            {
                ret = -ENOMEM;
                goto fail;
            }
            /*
             * alloc one bit for data_marker.
             * why for every user program could use the same address as the data_marker_ptr?
             * because the alloc function alloc memory use sequential method,not random.
             * this driver could only use by ci,this program make sure every port CPU execute
             * same instruction.
             */
            dev->rw_lock = dpram_mem_alloc(1);
            if (NULL == dev->rw_lock)
            {
                ret = -ENOMEM;
                goto fail;
            }

            /*写入DPRAM_TRUE表明无数据，读返回无数据，写能写成功*/
            ret = dpram_lock(dev->rw_lock);
            if (0 != ret)
            {
                goto fail;
            }

            dev->peer_dev->rw_lock = dpram_mem_alloc(1);
            if (NULL == dev->peer_dev->rw_lock)
            {
                ret = -ENOMEM;
                goto fail;
            }
            /*alloc rw_sem*/
            dev->rw_sem = dpram_mem_alloc(1);
            if (NULL == dev->rw_sem)
            {
                ret = -ENOMEM;
                goto fail;
            }
            ret = dpram_unlock(dev->rw_sem);
            if (0 != ret)
            {
                goto fail;
            }
            dev->peer_dev->rw_sem = dpram_mem_alloc(1);
            if (NULL == dev->peer_dev->rw_sem)
            {
                ret = -ENOMEM;
                goto fail;
            }
        }
        else
        {
            /*先申请对方再申请自己*/
            dev->peer_dev->data_addr = dpram_mem_alloc(mem_size);

            if (NULL == dev->peer_dev->data_addr)
            {
                ret = -ENOMEM;
                goto fail;
            }

            dev->data_addr = dpram_mem_alloc(mem_size);

            if (NULL == dev->data_addr)
            {
                ret = -ENOMEM;
                goto fail;
            }
            dev->peer_dev->rw_lock = dpram_mem_alloc(1);
            if (NULL == dev->peer_dev->rw_lock)
            {
                ret = -ENOMEM;
                goto fail;
            }
            /*
             * alloc one bit for data_marker.
             * why for every user program could use the same address as the data_marker_ptr?
             * because the alloc function alloc memory use sequential method,not random.
             * this driver could only use by ci,this program make sure every port CPU execute
             * same instruction.
             */
            dev->rw_lock = dpram_mem_alloc(1);
            if (NULL == dev->rw_lock)
            {
                ret = -ENOMEM;
                goto fail;
            }

            ret = dpram_lock(dev->rw_lock);
            if (0 != ret)
            {
                goto fail;
            }
            /*alloc rw_sem先申请对方再申请自己的*/
            dev->peer_dev->rw_sem = dpram_mem_alloc(1);
            if (NULL == dev->peer_dev->rw_sem)
            {
                ret = -ENOMEM;
                goto fail;
            }
            dev->rw_sem = dpram_mem_alloc(1);
            if (NULL == dev->rw_sem)
            {
                ret = -ENOMEM;
                goto fail;
            }
            ret = dpram_unlock(dev->rw_sem);
            if (0 != ret)
            {
                goto fail;
            }
        }

        dev->buf_size = arg;
        dev->peer_dev->buf_size = arg;
        break;
    case DPRAM_IOC_CHECK_MEMORY:
        ret = dpram_check_mem();
        if(0 > ret)
        {
            goto fail;
        }
        break;
    default:
        goto fail;
    }

    outb(DPRAM_IO_RESET_VALUE,DPRAM_IO_PORT);
    up(&dev->sem);

    return 0;
fail:
    dpram_unlock(dev->peer_dev->rw_sem);
    outb(DPRAM_IO_RESET_VALUE,DPRAM_IO_PORT);
    up(&dev->sem);

    return ret;
}

void dpram_vma_open(struct vm_area_struct *vma)
{
}

void dpram_vma_close(struct vm_area_struct *vma)
{
}

struct vm_operations_struct dpram_vm_ops = {
    .open  = dpram_vma_open,
    .close = dpram_vma_close,
};

/*
 功能描述    : 对双口ram的内存进行映射的函数
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年5月6日 20:06:18
*/
static int dpram_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    vma->vm_pgoff = DPRAM_START_ADDR >> PAGE_SHIFT;

    if (0x10000 < size)
    {
        printk(KERN_ERR "dpram_mmap:size to large %#x < %#lx\n", 0x10000, size);

        return -EINVAL;
    }
    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot))
    {
        return -EAGAIN;
    }

    vma->vm_ops = &dpram_vm_ops;
    vma->vm_flags |= VM_RESERVED;
    vma->vm_private_data = filp->private_data;
    dpram_vma_open(vma);

    return 0;
}

static const struct file_operations dpram_fops = {
    .owner   = THIS_MODULE,
    .open    = dpram_open,
    .write   = dpram_write,
    .read    = dpram_read,
    .mmap    = dpram_mmap,
    .ioctl   = dpram_ioctl,
    .release = dpram_release,
};
/*
 功能描述    : 向内核当中添加
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月25日 15:07:02
*/
static int dpram_setup_cdev(struct cdev *dev, int minor)
{
    int ret = 0;
    dev_t devno = MKDEV(dpram_major, minor);

    cdev_init(dev, &dpram_fops);
    dev->owner = THIS_MODULE;
    dev->ops = &dpram_fops;
    ret = cdev_add (dev, devno, 1);
    /* Fail gracefully if need be */
    if (ret)
    {
        printk(KERN_NOTICE "Error %d adding dpram %d", ret, minor);
        return -1;
    }

    return 0;
}
/*
 功能描述    : 初始化一对设备
 返回值      : 成功为0，失败返回相应的错误值
 参数        : 
 作者        : 张彦升
 日期        : 2013年12月1日 14:44:33
*/
static int __init dpram_pair_dev_init(struct dpram_pair_devs* pair_devs,
                                      int minor_a,
                                      int minor_b)
{
    pair_devs->dpram_dev_a.peer_dev = &pair_devs->dpram_dev_b;
    pair_devs->dpram_dev_b.peer_dev = &pair_devs->dpram_dev_a;
    pair_devs->dpram_dev_a.data_addr = NULL;
    pair_devs->dpram_dev_b.data_addr = NULL;
    pair_devs->dpram_dev_a.buf_size = 0;
    pair_devs->dpram_dev_b.buf_size = 0;
    pair_devs->dpram_dev_a.rw_lock = NULL;
    pair_devs->dpram_dev_b.rw_lock = NULL;
    pair_devs->dpram_dev_a.rw_sem = NULL;
    pair_devs->dpram_dev_b.rw_sem = NULL;

    sema_init(&pair_devs->dpram_dev_a.sem,1);
    sema_init(&pair_devs->dpram_dev_b.sem,1);

    pair_devs->dpram_dev_a.b_use = DPRAM_FALSE;
    pair_devs->dpram_dev_b.b_use = DPRAM_FALSE;

    dpram_setup_cdev(&pair_devs->dpram_dev_a.cdev,minor_a);
    dpram_setup_cdev(&pair_devs->dpram_dev_b.cdev,minor_b);

    return 0;
}
/*
 功能描述    : 模块初始化函数
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年1月21日 11:22:01
*/
static int __init dpram_init(void)
{
    int i = 0;
    int minor_a = 0;
    int minor_b = 0;

    int ret;
    dev_t devno = 0;

    if (dpram_major)
    {
        devno = MKDEV(dpram_major, 0);
        ret = register_chrdev_region(devno, DPRAM_DEVS_NR, "dpram");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, DPRAM_DEVS_NR, "dpram");
        dpram_major = MAJOR(devno);
    }

    if (ret < 0)
    {
        printk(KERN_WARNING "dpram: can't get major %d\n", dpram_major);

        return ret;
    }

    for (i = 0;i < DPRAM_PAIR_DEV_NR;i++)
    {
        minor_a = i * 2;
        minor_b = minor_a + 1;

        ret = dpram_pair_dev_init(&pair_devs[i],minor_a,minor_b);

        if (-1 == ret)
        {
            return -EFAULT;
        }
    }

#ifdef DPRAM_DEBUG
    /*创建proc接口*/
    dpram_create_proc();
#endif /*DPRAM_DEBUG*/
    /*暂时没添加清空内存逻辑*/
    /*dpram_mem_clear(0,DPRAM_TOTAL_MEMORY_SIZE);*/

    printk(KERN_INFO "dpram init.\n");

    return 0;
}
/*
 功能描述    : 模块退出函数
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年1月21日 11:22:09
*/
static void __exit dpram_exit(void)
{
    int i = 0;

    for (i = 0;i < DPRAM_PAIR_DEV_NR;i++)
    {
        cdev_del(&pair_devs[i].dpram_dev_a.cdev);
        cdev_del(&pair_devs[i].dpram_dev_b.cdev);
    }
    unregister_chrdev_region(MKDEV(dpram_major, 0), DPRAM_DEVS_NR);

#ifdef DPRAM_DEBUG
    dpram_remove_proc();
#endif /*DPRAM_DEBUG*/

    printk(KERN_INFO "dpram exit\n");
}

module_init(dpram_init);
module_exit(dpram_exit);
