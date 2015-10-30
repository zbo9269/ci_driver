/*********************************************************************
Copyright (C), 2011,  Co.Hengjun, Ltd.

作者        : 张彦升
版本        : 1.0
创建日期    : 2014年2月28日 9:07:46
用途        : 控显机通信板的驱动程序
网口驱动共注册两个设备供平台软件使用，两个设备分别从两个口当中读取和写入数据。
历史修改记录: v1.0    何境泰 创建
v1.1    张彦升 修改
**********************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>

#include "net.h"

MODULE_AUTHOR("zhangys");
MODULE_LICENSE("Dual BSD/GPL");

#define NET_LOCK_USE   0xAA         /*锁被使用标志*/
#define NET_LOCK_UNUSE  0x55        /*锁未被使用标志*/

static struct net_dev net_dev_a;    /*a口设备*/
static struct net_dev net_dev_b;    /*b口设备*/

static int net_major = NET_MAJOR;

module_param(net_major, int, S_IRUGO);

/*
功能描述    : 检查net_station是否为空
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 13:45:00
*/
static int net_station_check(struct net_dev* dev)
{
    int i = 0;
    struct net_station* net_station = NULL;

    for (i = 0;i < NET_STATION_SIZE;i++)
    {
        net_station = &dev->stations[i];
        if (NULL == net_station)
        {
            printk(KERN_ERR "net_read:dev->stations[%d] is NULL\n",i);

            return -ENOMEM;
        }
        if (NULL == net_station->data_sem)
        {
            printk(KERN_ERR "net_read:dev->stations[%d].data_sem is NULL\n",i);

            return -ENOMEM;
        }
        if (NULL ==net_station->rlock)
        {
            printk(KERN_ERR "net_read:dev->stations[%d].wlock is NULL\n",i);

            return -ENOMEM;
        }
        if (NULL == net_station->wlock)
        {
            printk(KERN_ERR "net_read:dev->stations[%d].rlock is NULL\n",i);

            return -ENOMEM;
        }
        if (NULL == net_station->data_addr)
        {
            printk(KERN_ERR "net_read:dev->stations[%d].data_buf is NULL\n",i);

            return -ENOMEM;
        }
        if (NULL == net_station->shadow_sem)
        {
            printk(KERN_ERR "net_read:dev->stations[%d].shadow_sem is NULL\n",i);

            return -ENOMEM;
        }
        if (NULL == net_station->shadow)
        {
            printk(KERN_ERR "net_read:dev->stations[%d].shadow is NULL\n",i);

            return -ENOMEM;
        }
    }

    return 0;
}
/*
功能描述    : 为锁加锁
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 14:38:27
*/
static int net_lock(unsigned char* lock)
{
    int count = 0;

    do 
    {
        iowrite8(NET_LOCK_USE,lock);
        udelay(1);
        count ++;
    } while (NET_LOCK_USE != ioread8(lock) && count < 10);

    if (NET_LOCK_USE != ioread8(lock))
    {
        return -1;
    }
    else
    {
        return 0;
    }
}
/*
功能描述    : 为锁解锁
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 14:38:53
*/
static int net_unlock(unsigned char* lock)
{
    int count = 0;

    do 
    {
        iowrite8(NET_LOCK_UNUSE,lock);
        udelay(1);
        count ++;
    } while (NET_LOCK_UNUSE != ioread8(lock) && count < 10);

    if (NET_LOCK_UNUSE != ioread8(lock))
    {
        return -1;
    }
    else
    {
        return 0;
    }
}
/*
功能描述    : 等待锁，延迟函数为微妙级函数
返回值      : 成功为0，失败为-1
参数        : @lock 锁
@condition 锁为真的条件
@wait_times 等待的次数
作者        : 张彦升
日期        : 2014年3月3日 14:14:00
*/
static int net_wait_unlock(unsigned char* lock)
{
    int i = 0;

    while (NET_LOCK_USE == ioread8(lock) && i < 10)
    {
        udelay(1); /*延迟wait_times微秒*/
        i++;
    }
    if (NET_LOCK_USE == ioread8(lock))
    {
        return -1;
    }
    else
    {
        return 0;
    }
}
/*
功能描述    : open系统调用的实现
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 13:26:23
*/
static int net_open(struct inode* inode, struct file* filp)
{
    struct net_dev* dev;
    int num = MINOR(inode->i_rdev);

    /*该设备mknod的时候不能创建大于NET_DEVS_NR个设备*/
    if (NET_DEVS_NR < num)
    {
        return -ENODEV;
    }

    dev = container_of(inode->i_cdev,struct net_dev,cdev);

    /*我们的目标是允许一次open系统调用，其它所有打开操作全部拒绝*/
    if (NET_TRUE == dev->b_use)
    {
        return -EBUSY;
    }

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    dev->b_use = NET_TRUE;

    up(&dev->sem);
    /*将设备结构体指针赋值给文件私有数据指针*/
    filp->private_data = dev;

    return 0;

}
/*
功能描述    : 释放设备
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 13:28:10
*/
static int net_release(struct inode* inode, struct file *filp)
{
    struct net_dev* dev = filp->private_data;

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    dev->b_use = NET_FALSE;

    up(&dev->sem);

    return 0;
}
/*
功能描述    : 读取函数
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 13:28:38
*/
static ssize_t net_read(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
    int ret = 0;
    int i = 0;
    int b_read_data = 0;    /*标志是否得到了数据*/
    struct net_station* net_station = NULL;

    struct net_dev *dev = filp->private_data; /*获得设备结构体指针*/

    if (NULL == dev)
    {
        printk(KERN_ERR "net_read:dev is NULL\n");

        return -ENXIO;
    }
    if (NET_STATION_BUF_SIZE < size)
    {
        printk(KERN_ERR "net_read:NET_STATION_BUF_SIZE(%d) < size(%d)\n",NET_STATION_BUF_SIZE,size);

        return -ENOMEM;
    }
    /*check it*/
    ret = net_station_check(dev);

    if (0 != ret)
    {
        return ret;
    }

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    outb(NET_IO_SELECT_VALUE,NET_IO_PORT);

    /*loop look up,if has data,read it,otherwise return error*/
    for (i = 0;i < NET_STATION_SIZE;i++)
    {
        net_station = &dev->stations[i];

        /*get the semaphore*/
        if (0 == ioread8(net_station->data_sem))
        {
            continue;
        }
        /*FPGA is writing*/
        ret = net_wait_unlock(net_station->wlock);
        if (0 != ret)
        {
            printk(KERN_ERR "net_read:has wait rlock exceeded 10ms\n");
            /*
            * 这种错误是严重错误，由于硬件设计，当执行到这里的时候net_station->data_sem
            * 已经被置为了0，但是我们尝试等待了10微妙，仍然没有得到读锁，这表明硬件发生了
            * 延迟，如果我们继续循环下去将会导致这一帧被丢失，如果不继续，一直等待将有可能
            * 导致联锁周期超时，甚至导致联锁机重启。
            */
            ret = -ETIME;
            goto fail;
        }
        /*lock the read lock*/
        ret = net_lock(net_station->rlock);
        if (-1 == ret)
        {
            printk(KERN_ERR "net_read:has lock rlock failed\n");
            /*!important error,should not be occur*/
            ret = -ETIME;
            goto fail;
        }

        /*now read data*/
        if (copy_to_user(buf, net_station->data_addr, size))
        {
            printk(KERN_ERR "net_read: copy_to_user error\n");

            ret = -EIO;
            goto fail;
        }
        rmb();
        /*clear it,avoid read dirty data*/
        memset_io(net_station->data_addr,0,size);

        /*unlock the read lock*/
        ret = net_unlock(net_station->rlock);
        if (-1 == ret)
        {
            printk(KERN_ERR "net_read:has unlock rlock failed\n");
            /*!important error,should not be occur*/
            ret = -ETIME;
            goto fail;
        }

        iowrite8(0x00,net_station->data_sem);
        /*
        * if execute at here,break directly,station data which have not read 
        * leave to next syscall read
        */
        b_read_data = 1;
        printk(KERN_INFO "net_read:read from data_buf %d\n",i);

        break;
    }

    outb(NET_IO_RESET_VALUE,NET_IO_PORT);

    if (1 == b_read_data)
    {
        goto success;
    }

    /*below try read from shadow*/
    outb(NET_IO_SELECT_VALUE,NET_IO_PORT);

    for (i = 0;i < NET_STATION_SIZE;i++)
    {
        net_station = &dev->stations[i];

        /*get the semaphore*/
        if (0 == ioread8(net_station->shadow_sem))
        {
            continue;
        }
        /*now read data*/
        if (copy_to_user(buf, net_station->shadow, size))
        {
            printk(KERN_ERR "net_read: shadow copy_to_user error\n");

            ret = -EIO;
            goto fail;
        }
        else
        {
            /*clear it,avoid red dirty data*/
            memset_io(net_station->shadow,0,size);
        }

        rmb();

        iowrite8(0x00,net_station->shadow_sem);

        b_read_data = 1;
        printk(KERN_INFO "net_read:read from shadow %d\n",i);
        break;
    }

    outb(NET_IO_RESET_VALUE,NET_IO_PORT);

    if (1 == b_read_data)
    {
        goto success;
    }
    else
    {
        ret = -EAGAIN;
        goto fail;
    }
success:
    up(&dev->sem);
    return size;

fail:
    up(&dev->sem);

    return ret;
}

/*
功能描述    : 写数据函数
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 15:45:19
*/
static ssize_t net_write(struct file *filp, const char __user *buf,size_t size, loff_t *ppos)
{
    int ret = 0;
    int count = 0;
    int write_sem = 0;
    struct net_dev *dev = filp->private_data; /*获得设备结构体指针*/

    if (NET_SEND_TOTAL_MEMORY_SIZE < size)
    {
        printk(KERN_ERR "net_write:too many data,%#X(size) > %#X(NET_SEND_TOTAL_MEMORY_SIZE)\n",
            size,NET_SEND_TOTAL_MEMORY_SIZE);

        return -ENOMEM;
    }
    if (NULL == dev)
    {
        printk(KERN_ERR "net_write:filp->private_data is NULL\n");

        return -EFAULT;
    }
    if (NULL == dev->send_addr)
    {
        printk(KERN_ERR "net_write:dev->send_buf is NULL\n");

        return -EFAULT;
    }

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    outb(NET_IO_SELECT_VALUE,NET_IO_PORT);

    do
    {
        write_sem = inb(NET_IO_WRITE_SEM_PORT);
        udelay(1);  /*延迟1微妙*/
        count ++;
    } while (0x01 == write_sem && count < 10);

    if (10 == count)
    {
        printk(KERN_ERR "net_write:try get semaphore of write exceeded 10 times\n");

        ret = -EBUSY;
        goto fail;
    }

    /*copy data*/
    if (copy_from_user(dev->send_addr, buf, size))
    {
        printk(KERN_ERR "net_write: copy_from_user error\n");

        ret = -EIO;
        goto fail;
    }
    /*set data length*/
    outb(size >> 8 & 0xff,NET_IO_WRITE_DATA_LEN_HIGH_PORT);
    outb(size & 0xff,NET_IO_WRITE_DATA_LEN_LOW_PORT);

    /*这里注意，将选通端口重置操作务必放在最后进行，否则有可能无法下发数据*/
    outb(0x01,NET_IO_WRITE_SEM_PORT);
    /*reset semaphore*/
    outb(NET_IO_RESET_VALUE,NET_IO_PORT);

    up(&dev->sem);
    return size;

fail:
    /*reset semaphore*/
    outb(0x01,NET_IO_WRITE_SEM_PORT);
    outb(NET_IO_RESET_VALUE,NET_IO_PORT);

    up(&dev->sem);

    return ret;
}
/*
功能描述    : 通过ioctl进行控制
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 15:16:52
*/
static int net_ioctl(struct inode *inodep, struct file *filp, unsigned int cmd, unsigned long arg)
{
    short port = 0;
    int ip = 0;
    unsigned char* buf = NULL;
    unsigned char* addr = NULL;
    int ret = 0;

    struct net_dev *dev = filp->private_data; /*获得设备结构体指针*/

    if (NULL == dev)
    {
        printk(KERN_ERR "net_ioctl:filp->private_data is NULL\n");

        return -EFAULT;
    }
    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != NET_IOC_MAGIC)
    {
        printk(KERN_ERR "net_ioctl:_IOC_TYPE(%d) must be %d\n",_IOC_TYPE(cmd),NET_IOC_MAGIC);

        return -ENOTTY;
    }

    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS;
    }

    outb(NET_IO_SELECT_VALUE,NET_IO_PORT);

    switch (cmd)
    {
    case NET_IOCRESET:
        ret = -ENOSYS;
        break;
    case NET_IOC_DEST_IP_A:
        /*传输ip地址时按照字符顺序传递，比如192.168.1.3，则192在前168.1.3依次在后*/
        ip = arg;
        addr = ioremap(NET_DEST_IP_ADDR_A,4);
        buf = (unsigned char*)&ip;
#if 0
        printk("net_ioctl:ip_a %#x,%#x,%#x,%#x\n",
            buf[0] & 0xff,
            buf[1] & 0xff,
            buf[2] & 0xff,
            buf[3] & 0xff
            );
#endif
        iowrite32(arg,addr);
        iounmap(addr);
        ret = 0;
        break;
    case NET_IOC_DEST_PORT_A:
        /*端口使用的是大头字节，所以需要转换*/
        buf = (unsigned char*)&port;
        buf[0] = (arg & 0xff00) >> 8;
        buf[1] = arg & 0xff;

        addr = ioremap(NET_DEST_PORT_ADDR_A,2);
        iowrite16(port,addr);
#if 0
        printk("net_ioctl:port_a %#x,%#x\n",
            buf[0] & 0xff,
            buf[1] & 0xff
            );
#endif
        iounmap(addr);
        ret = 0;
        break;
    case NET_IOC_DEST_IP_B:
        /*传输ip地址时按照字符顺序传递，比如192.168.1.3，则192在前168.1.3依次在后*/
        ip = arg;
        addr = ioremap(NET_DEST_IP_ADDR_B,4);
        buf = (unsigned char*)&ip;
#if 0
        printk("net_ioctl:ip_b %#x,%#x,%#x,%#x\n",
            buf[0] & 0xff,
            buf[1] & 0xff,
            buf[2] & 0xff,
            buf[3] & 0xff
            );
#endif
        iowrite32(arg,addr);
        iounmap(addr);
        ret = 0;
        break;
    case NET_IOC_DEST_PORT_B:
        /*端口使用的是大头字节，所以需要转换*/
        buf = (unsigned char*)&port;
        buf[0] = (arg & 0xff00) >> 8;
        buf[1] = arg & 0xff;

        addr = ioremap(NET_DEST_PORT_ADDR_B,2);
        iowrite16(port,addr);
#if 0
        printk("net_ioctl:port_b %#x,%#x\n",
            buf[0] & 0xff,
            buf[1] & 0xff
            );
#endif
        iounmap(addr);
        ret = 0;
        break;
    case NET_IOC_RESET:
        outb(0x01, NET_RESET_PORT);
        break;
    default:
        ret = -EFAULT;
        break;
    }

    outb(NET_IO_RESET_VALUE,NET_IO_PORT);

    up(&dev->sem);

    return ret;
}

static const struct file_operations net_fops =
{
    .owner = THIS_MODULE,
    .read = net_read,
    .write = net_write,
    .ioctl = net_ioctl,
    .open = net_open,
    .release = net_release,
};

#ifdef NET_DEBUG
/*
 功能描述    : A口的内存信息
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月24日 14:41:50
*/
static int net_proc_mem_a(char *buf, char **start, off_t offset,
                       int count, int *eof, void *data)
{
    struct net_station* net_station;
    int i = 0;
    int len = 0;

    if (down_interruptible(&net_dev_a.sem))
    {
        return -ERESTARTSYS;
    }
    len += sprintf(buf + len,"data_sem\t wlock\t\t rlock\t\t data_buf\t shadow_sem\t shadow\n");

    for (i = 0;i < NET_STATION_SIZE;i++)
    {
        net_station = &net_dev_a.stations[i];

        len += sprintf(buf + len,"%#x\t %#x\t %#x\t %#x\t %#x\t %#x\n",
            (int)net_station->data_sem,
            (int)net_station->rlock,
            (int)net_station->wlock,
            (int)net_station->data_addr,
            (int)net_station->shadow_sem,
            (int)net_station->shadow);
    }

    len += sprintf(buf + len,"\nsend_buf:%#x\n",(int)net_dev_a.send_addr);

    *eof = 1;

    up(&net_dev_a.sem);

    return len;
}
/*
 功能描述    : B口的内存信息
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月24日 14:42:07
*/
static int net_proc_mem_b(char *buf, char **start, off_t offset,
                       int count, int *eof, void *data)
{
    struct net_station* net_station;
    int i = 0;
    int len = 0;

    if (down_interruptible(&net_dev_b.sem))
    {
        return -ERESTARTSYS;
    }

    len += sprintf(buf+len,"data_sem\t wlock\t\t rlock\t\t data_buf\t shadow_sem\t shadow\n");

    for (i = 0;i < NET_STATION_SIZE;i++)
    {
        net_station = &net_dev_b.stations[i];

        len += sprintf(buf+len,"%#x\t %#x\t %#x\t %#x\t %#x\t %#x\n",
            (int)net_station->data_sem,
            (int)net_station->rlock,
            (int)net_station->wlock,
            (int)net_station->data_addr,
            (int)net_station->shadow_sem,
            (int)net_station->shadow);
    }

    len += sprintf(buf+len,"\nsend_buf:%#x\n",(int)net_dev_b.send_addr);

    *eof = 1;
    up(&net_dev_b.sem);

    return len;
}
/*
 功能描述    : 创建proc接口文件
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月19日 16:27:58
*/
static void net_create_proc(void)
{
    create_proc_read_entry("neta",0,NULL, net_proc_mem_a, NULL);
    create_proc_read_entry("netb",0,NULL, net_proc_mem_b, NULL);

    return;
}
/*
 功能描述    : 删除proc接口文件
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月19日 16:27:58
*/
static void net_remove_proc(void)
{
    /* no problem if it was not registered */
    remove_proc_entry("neta", NULL);
    remove_proc_entry("netb", NULL);
}

#endif /* NET_DEBUG */

/*
功能描述    : 将驱动程序的设备安装到内核内
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 10:24:02
*/
static int net_setup_cdev(struct cdev* dev, int minor)
{
    int err;
    dev_t devno = MKDEV(net_major,minor);

    cdev_init(dev, &net_fops);
    dev->owner = THIS_MODULE;
    dev->ops = &net_fops;

    err = cdev_add(dev, devno, 1);

    if (err)
    {
        printk(KERN_ERR "Error %d adding net %d\n", err, minor);

        return err;
    }

    return 0;
}
/*
功能描述    : 初始化网卡A口
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 10:24:51
*/
static int net_dev_init(struct net_dev* net_dev,
                        int phy_recv_start_addr,
                        int phy_recv_end_addr,
                        int minor )
{
    int ret = 0;
    int i = 0;
    struct net_station* net_station;
    int total_size = phy_recv_start_addr + NET_SHADOW_OFF + NET_STATION_SIZE * NET_STATION_BUF_SIZE;

    if (phy_recv_end_addr + 1 < total_size)
    {
        printk(KERN_ERR "net_dev_nit:buf size error.%#x < %#x\n",phy_recv_end_addr,total_size);

        return -ENOMEM;
    }
    /*below init station*/
    /*
    * 双口ram的内存由低地址向高地址依次分布了写锁、读锁、数据信号量、数据区、影子信号量、
    * 影子数据区，每个占的区域可根据预定义值计算出。
    */
    for (i = 0;i < NET_STATION_SIZE;i++)
    {
        net_station = &net_dev->stations[i];

        net_station->rlock = ioremap(phy_recv_start_addr + NET_WLOCK_OFF + i,1);

        if (NULL == net_station->rlock)
        {
            printk(KERN_ERR "net_dev_init:ioremap net_dev.station[%d].wlock error\n",i);

            return -ENOMEM;
        }

        net_station->wlock = ioremap(phy_recv_start_addr + NET_RLOCK_OFF + i,1); 

        if (NULL == net_station->wlock)
        {
            printk(KERN_ERR "net_dev_nit:ioremap net_dev.station[%d].rlock error\n",i);

            return -ENOMEM;
        }

        net_station->data_sem = ioremap(phy_recv_start_addr + NET_SEM_OFF + i,1);

        if (NULL == net_station->data_sem)
        {
            printk(KERN_ERR "net_dev_nit:ioremap net_dev.station[%d].data_sem error\n",i);

            return -ENOMEM;
        }

        net_station->data_addr = ioremap(phy_recv_start_addr + NET_BUF_OFF + i * NET_STATION_BUF_SIZE,NET_STATION_BUF_SIZE);

        if (NULL == net_station->data_addr)
        {
            printk(KERN_ERR "net_dev_nit:ioremap net_dev.station[%d].data_buf error\n",i);

            return -ENOMEM;
        }

        net_station->shadow_sem = ioremap(phy_recv_start_addr + NET_SHADOW_SEM_OFF + i,1);

        if (NULL == net_station->shadow_sem)
        {
            printk(KERN_ERR "net_dev_nit:ioremap net_dev.station[%d].shadow_sem error\n",i);

            return -ENOMEM;
        }

        net_station->shadow = ioremap(phy_recv_start_addr + NET_SHADOW_OFF + i * NET_STATION_BUF_SIZE,NET_STATION_BUF_SIZE);

        if (NULL == net_station->shadow)
        {
            printk(KERN_ERR "net_dev_nit:ioremap net_dev.station[%d].shadow error\n",i);

            return -ENOMEM;
        }
    }
    /*init send_addr*/
    net_dev->send_addr = ioremap(NET_SEND_START_ADDR,NET_SEND_TOTAL_MEMORY_SIZE);

    if (NULL == net_dev->send_addr)
    {
        printk(KERN_ERR "net_dev_nit:net_dev->send_buf ioremap error\n");
        return -ENOMEM;
    }
    /*init semaphore*/
    sema_init(&net_dev->sem,1);
    /*init b_use*/
    net_dev->b_use = NET_FALSE;

    ret = net_setup_cdev(&net_dev->cdev, minor);

    if (0 != ret)
    {
        cdev_del(&net_dev->cdev);

        return ret;
    }

    return 0;
}
/*
 功能描述    : 释放设备号
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月24日 15:21:25
*/
static void net_dev_release(struct net_dev* dev)
{
    int i = 0;
    struct net_station* net_station = NULL;

    for (i = 0;i < NET_STATION_SIZE;i++)
    {
        net_station = &dev->stations[i];
        iounmap(net_station->data_sem);
        iounmap(net_station->rlock);
        iounmap(net_station->wlock);
        iounmap(net_station->data_addr);
        iounmap(net_station->shadow_sem);
        iounmap(net_station->shadow);
    }

    iounmap(dev->send_addr);

    cdev_del(&dev->cdev);

    return ;
}
/*
功能描述    : 网卡驱动程序初始化函数
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 10:23:31
*/
static int __init net_init(void)
{
    int ret;
    dev_t devno = 0;

    if (net_major)
    {
        devno = MKDEV(net_major, 0);
        ret = register_chrdev_region(devno, NET_DEVS_NR, "net");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, NET_DEVS_NR, "net");
        net_major = MAJOR(devno);
    }

    if (ret < 0)
    {
        printk(KERN_WARNING "net: can't get major %d\n", net_major);

        return ret;
    }

    ret = net_dev_init(&net_dev_a,NET_RECV_START_ADDR_A,NET_RECV_END_ADDR_A,NET_MINOR_A);

    if (0 != ret)
    {
        goto fail;
    }

    ret = net_dev_init(&net_dev_b,NET_RECV_START_ADDR_B,NET_RECV_END_ADDR_B,NET_MINOR_B);

    if (0 != ret)
    {
        goto fail;
    }

    outb(0x01,NET_IO_CLEAR_MEMORY_PORT);

#ifdef NET_DEBUG
    /*创建proc接口*/
    net_create_proc();
#endif /*NET_DEBUG*/

    printk(KERN_INFO "net init.\n");

    return 0;

fail:
    unregister_chrdev_region(MKDEV(net_major, 0), NET_DEVS_NR); /*释放设备号*/

    return ret;
}
/*
功能描述    : 网卡驱动程序注销函数
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 10:23:45
*/
static void __exit net_exit(void)
{
    net_dev_release(&net_dev_a);
    net_dev_release(&net_dev_b);

    unregister_chrdev_region(MKDEV(net_major, 0), NET_DEVS_NR); /*释放设备号*/

#ifdef NET_DEBUG
    net_remove_proc();
#endif /*NET_DEBUG*/

    printk(KERN_INFO "net exit!\n");

    return;
}

module_init(net_init);
module_exit(net_exit);
