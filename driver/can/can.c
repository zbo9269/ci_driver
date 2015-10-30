/*********************************************************************
 Copyright (C), 2014,  Co.Hengjun, Ltd.
 
 作者        : 何境泰
 版本        : 1.0
 创建日期    : 2014年8月12日 14:07:05
 用途        : 
 历史修改记录: v1.0    创建
    v1.1    2014.08.12 张彦升
               1.根据代码规范要求为代码添加注释并更改不符合要求项
               2.添加proc结构
    v2.0    2015.03.31  张彦升
        添加SJA检查，与内存检查
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

/*用来存放该地址是否被使用标志*/
static unsigned short can_address_mask[EEU_GA_NUM];

static int can_major = CAN_MAJOR;
static int can_minor = 0;
static int can_delay_counter = 10; /*最大延迟次数*/

static char* send_addr = NULL;

static short read_ga_address = 0;   /*读取数据的网关地址*/
static short read_ea_address = 0;   /*读取数据的电子单元地址*/

static struct can_pair_devs can_pair_dev; /*can设备结构体*/

module_param(can_major, int, S_IRUGO);

/*
 功能描述    : 打开设备
 返回值      : 成功为0，失败为-1
 参数        : @inode 设备节点
              @filp 文件设备指针
 作者        : 何境泰
 日期        : 2014年8月18日 10:40:54
*/
static int can_open(struct inode* inode, struct file* filp)
{
    struct can_dev* dev;
    int minor = MINOR(inode->i_rdev);

    /*该设备mknod的时候不能创建大于CAN_DEVS_NR个设备*/
    if (minor >= CAN_DEVS_NR)
    {
        return -ENODEV;
    }
    dev = container_of(inode->i_cdev,struct can_dev,cdev);

    /*我们的目标是允许一次open系统调用，其它所有打开操作全部拒绝*/
    if (dev->b_use)
    {
        return -EBUSY;
    }

    /*将设备结构体指针赋值给文件私有数据指针*/
    filp->private_data = dev;

    dev->b_use = 1;

    return 0;
}


/*
 功能描述    : 文件关闭时调用此函数
 返回值      : 成功为0，失败为-1
 参数        : @inode 文件inode节点
              @filp 文件指针
 作者        : 何境泰
 日期        : 2014年8月18日 10:53:38
*/
static int can_release(struct inode* inode, struct file* filp)
{
    struct can_dev* dev = filp->private_data;

    dev->b_use = 0;

    return 0;
}
/*
 功能描述    : 文件读写偏移设置函数
 返回值      : 成功为0，失败为-1
 参数        : 无
 作者        : 何境泰
 日期        : 2014年8月18日 10:56:05
*/
static loff_t can_llseek(struct file *filp, loff_t offset, int whence)
{
    return -ENOSYS;
}

/*
 功能描述    : 设备读函数
 返回值      : 读取的字符的大小
 参数        : 
 作者        : 何境泰
 日期        : 2014年8月18日 11:00:55
*/
static ssize_t can_read(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
    int ret = 0;
    int k = 0;
    int32_t off_ea = read_ea_address + 1;
    unsigned char* data_flag_addr = NULL;       /*网关对应的空间是否有数标志地址*/
    unsigned char* fpga_write_flag_addr = NULL; /*FPGA正在向双口ram写数据地址标志*/
    unsigned char* pc104_read_flag_addr = NULL; /*pc104正在从双口ram当中读书节标志*/
    unsigned char* data_space_addr = NULL;      /*数据空间地址*/

    struct can_dev *dev = filp->private_data; /*获得设备结构体指针*/

    if (CAN_FRAME_LEN < size)
    {
        return -EINVAL;
    }

    data_flag_addr = dev->recv_addr + DATA_FLAG_OFFS + (read_ga_address * EEU_EA_NUM) + off_ea;
    fpga_write_flag_addr = dev->recv_addr + FPG_WRITE_FLAG_OFFS + (read_ga_address * EEU_EA_NUM) + off_ea;
    pc104_read_flag_addr = dev->recv_addr + PC104_READ_OFFS + (read_ga_address * EEU_EA_NUM) + off_ea;
    data_space_addr = dev->recv_addr + DATA_SPACE_OFFS + (( read_ga_address * EEU_EA_NUM) + off_ea) * CAN_FRAME_LEN;

    /*检验要读的地址是否被使用*/
    if (!(can_address_mask[read_ga_address] & (1 << read_ea_address)))
    {
        return -EINVAL;
    }

    outb(CAN_IO_SELECT_VALUE,CAN_IO_PORT);

    /*下查询是否有数空间标志，如果没有数则返回*/
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

    /*再查询FPGA写网关标志,0XAA为写有效,说明FPGA正在写数据区,这时空等待*/
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
    /*检查到FPGA正在写设备的相关标志标注无效时，需将PC104读FPGA设备的状态置有效。*/
    iowrite8(FPGA_FLAG_BUSY, pc104_read_flag_addr);

    /*读取网关数据空间,每个网关的数据空间为13个字节*/
    if (__copy_to_user(buf, data_space_addr,size))
    {
        printk(KERN_ERR "can_read: copy_to_user error\n");

        ret = -EIO;
        goto fail;
    }
    rmb();

    /*当PC104查询过此地址。将此网关数据空间标志置无效*/
    iowrite8(FLAG_FALSE, data_flag_addr);
    /*当PC104查询过此地址。再使PC104读FPGA标志标注无效。*/
    iowrite8(FLAG_FALSE, pc104_read_flag_addr);

    outb(CAN_IO_RESET_VALUE,CAN_IO_PORT);

    return size;
fail:
    outb(CAN_IO_RESET_VALUE,CAN_IO_PORT);

    return ret;
}
/*
 功能描述    : 设备写函数
 返回值      : 写入数据的大小
 参数        : 
 作者        : 何境泰
 日期        : 2014年8月18日 11:00:23
*/
static ssize_t can_write(struct file *filp, const char __user *buf,size_t size, loff_t *ppos)
{
    if (NULL == send_addr)
    {
        return -EFAULT;
    }

    outb(CAN_IO_SELECT_VALUE,CAN_IO_PORT);

    /*用户空间->内核空间*/
    if (copy_from_user(send_addr, buf, size))
    {
        return -EIO;
    }

    /*向CAN_FRAME_LEN_PORT地址写入本次共发送的帧数*/
    outb(size / CAN_FRAME_LEN,CAN_FRAME_LEN_PORT);

    /*向CAN_IO_FRAME_SEND地址写入1代表FPGA开始下发数据*/
    outb(FLAG_TRUE,CAN_IO_FRAME_SEND); 

    /*最后将选择端口重置*/
    outb(CAN_IO_RESET_VALUE,CAN_IO_PORT);

    return size;
}

/*
 功能描述    : 杂项控制
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 何境泰
 日期        : 2014年8月18日 11:02:40
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
        /*用户空间->内核空间*/
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
        /*网关地址为高16位，电子单元地址为低16位*/
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

/*文件操作结构体*/
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
 功能描述    : 打印内存信息
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月26日 13:47:05
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
 功能描述    : 创建proc接口文件
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月19日 16:27:58
*/
static void can_create_proc(void)
{
    create_proc_read_entry("can_mem",0,NULL, can_proc_mem, NULL);

    return;
}
/*
 功能描述    : 删除proc接口文件
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年3月19日 16:27:58
*/
static void can_remove_proc(void)
{
    /* no problem if it was not registered */
    remove_proc_entry("can_mem", NULL);
}

#endif /* CAN_DEBUG */
/*
 功能描述    : 注册并初始化设备结构
 返回值      : 无
 参数        : @dev 设备结构体指针
              @minor 子设备号
 作者        : 何境泰
 日期        : 2014年8月18日 10:38:20
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
 功能描述    : can模块初始化调用函数
 返回值      : 成功为0，失败为-1
 参数        : 无
 作者        : 何境泰
 日期        : 2014年8月12日 14:12:56
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
    /*创建proc接口*/
    can_create_proc();
#endif /*CAN_DEBUG*/

    can_pair_dev.can_dev_a.b_use = 0;
    can_pair_dev.can_dev_a.recv_addr = ioremap(CAN_RECV_START_ADDR_A,CAN_RECV_TOTAL_MEMORY_SIZE_A); ;

    can_pair_dev.can_dev_b.b_use = 0;
    can_pair_dev.can_dev_b.recv_addr = ioremap(CAN_RECV_START_ADDR_B,CAN_RECV_TOTAL_MEMORY_SIZE_B); ;

    /*can的两个设备分别对应0和1子设备号*/
    can_setup_cdev(&can_pair_dev.can_dev_a, CAN_MINOR_A);

    can_setup_cdev(&can_pair_dev.can_dev_b, CAN_MINOR_B);

    printk(KERN_INFO "can init!\n");

    return 0;
}
/*
 功能描述    : can模块退出调用函数
 返回值      : 无
 参数        : 无
 作者        : 何境泰
 日期        : 2014年8月12日 14:12:10
*/
static void __exit can_exit(void)
{
    cdev_del(&can_pair_dev.can_dev_a.cdev);   /*注销cdev*/
    cdev_del(&can_pair_dev.can_dev_b.cdev);   /*注销cdev*/
    unregister_chrdev_region(MKDEV(can_major, can_minor), CAN_DEVS_NR); /*释放设备号*/

#ifdef CAN_DEBUG
    can_remove_proc();
#endif /*CAN_DEBUG*/

    printk(KERN_INFO "can exit!\n");

    return;
}

module_init(can_init);
module_exit(can_exit);
