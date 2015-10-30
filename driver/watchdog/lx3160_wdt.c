/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 作者        : 张彦升
 版本        : 1.0
 创建日期    : 2014年4月24日 10:25:26
 用途        : LX3160看门狗驱动
 历史修改记录: v1.0    创建
**********************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/ioport.h>
#include <linux/watchdog.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include "lx3160_wdt.h"

MODULE_AUTHOR("zhangys");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Hardware Watchdog Device Driver for LX3160 I/O");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);

static unsigned long    wdt_status = 0;
static int pba          = 0; /*port base address*/
static int max_units    = WDT_MAXUNITS;
static int exclusive    = DEFAULT_EXCLUSIVE;
static int timeout      = DEFAULT_TIMEOUT;
static bool nowayout    = DEFAULT_NOWAYOUT;

module_param(exclusive, int, 0);
MODULE_PARM_DESC(exclusive, "Watchdog exclusive device open, default="
                 __MODULE_STRING(DEFAULT_EXCLUSIVE));
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds, default="
                 __MODULE_STRING(DEFAULT_TIMEOUT));
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started, default="
                 __MODULE_STRING(WATCHDOG_NOWAYOUT));

/* Superio Chip */

static inline void lx3160_superio_enter(void)
{
    outb(0x55,REG);
}

static inline void lx3160_superio_exit(void)
{
    outb(0xaa,REG);
}

static inline int lx3160_superio_inb(int reg)
{
    outb(reg, REG);
    return inb(VAL);
}

static inline void lx3160_superio_outb(int val, int reg)
{
    outb(reg, REG);
    outb(val, VAL);
}
/*
 功能描述    : 从reg,reg+1当中读取一个字
 返回值      : 读取的一字内容
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月24日 11:25:07
*/
static inline short lx3160_superio_inw(int reg)
{
    short val = 0;

    /*使用大字头序*/
    outb(reg++, REG);
    val = inb(VAL) << 8;
    outb(reg, REG);
    val |= inb(VAL);

    return val;
}
/*
 功能描述    : 向reg,reg+1写入一个字
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月24日 12:35:41
*/
static inline void lx3160_superio_outw(int val, int reg)
{
    outb(reg++, REG);
    outb(val >> 8, VAL);
    outb(reg, REG);
    outb(val, VAL);
}
/*
 功能描述    : 更新时间
 返回值      : 无
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月24日 14:00:16
*/
static void lx3160_wdt_update_timeout(void)
{
    outb(timeout,pba + TIME_OUT_REG_OFF);
}
/*
 功能描述    : 调用该函数使看门狗不超时
 返回值      : none
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月25日 8:11:05
*/
static void lx3160_wdt_keepalive(void)
{
    lx3160_superio_enter();
    lx3160_wdt_update_timeout();
    lx3160_superio_exit();

    set_bit(WDTS_KEEPALIVE, &wdt_status);
}
/*
 功能描述    : 启动看门狗
 返回值      : 成功为0，失败为非0
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月25日 8:10:17
*/
static int lx3160_wdt_start(void)
{
    lx3160_superio_enter();

    /*copy from senbo lx3160 demo,I don't know what mean it is.*/
    outb(0x0e,pba + 0x47);

    /*we only support mode in seconds*/
    outb(MODE_SECOND,pba + MODE_REG_OFF);

    lx3160_wdt_update_timeout();

    lx3160_superio_exit();

    return 0;
}
/*
 功能描述    : 停止看门狗
 返回值      : 成功为0，失败为非0
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月24日 13:22:32
*/
static int lx3160_wdt_stop(void)
{
    lx3160_superio_enter();

    /*from senbo technical support,to stop watchdog,just set time to zero.*/
    outb(0x00,pba + TIME_OUT_REG_OFF);

    lx3160_superio_exit();

    return 0;
}
/*
 功能描述    : 设置新的时间间隔，该函数只可能被ioctl执行
 返回值      : 成功为0，失败为-1
 参数        : @t 以秒为单位的时间
 作者        : 张彦升
 日期        : 2014年4月24日 14:06:25
*/
static int lx3160_wdt_set_timeout(int t)
{
    if (t < 1 || t > max_units)
    {
        return -EINVAL;
    }

    timeout = t;

    if (test_bit(WDTS_TIMER_RUN, &wdt_status))
    {
        lx3160_superio_enter();

        lx3160_wdt_update_timeout();
        lx3160_superio_exit();
    }

    return 0;
}

/*
 功能描述    : 得到看门狗的状态信息
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月24日 14:07:27
*/
static int lx3160_wdt_get_status(int *status)
{
    *status = 0;

    if (test_and_clear_bit(WDTS_KEEPALIVE, &wdt_status))
    {
        *status |= WDIOF_KEEPALIVEPING;
    }
    if (test_bit(WDTS_EXPECTED, &wdt_status))
    {
        *status |= WDIOF_MAGICCLOSE;
    }

    return 0;
}

/*
 功能描述    : 打开看门狗，打开之后便会有定时，所以需要开始喂狗
 返回值      : 成功为0，失败为相应返回值
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月24日 14:07:57
*/
static int lx3160_wdt_open(struct inode *inode, struct file *file)
{
    int ret;

    if (exclusive && test_and_set_bit(WDTS_DEV_OPEN, &wdt_status))
    {
        return -EBUSY;
    }
    if (!test_and_set_bit(WDTS_TIMER_RUN, &wdt_status))
    {
        if (nowayout && !test_and_set_bit(WDTS_LOCKED, &wdt_status))
        {
            __module_get(THIS_MODULE);
        }

        ret = lx3160_wdt_start();
        if (ret)
        {
            clear_bit(WDTS_LOCKED, &wdt_status);
            clear_bit(WDTS_TIMER_RUN, &wdt_status);
            clear_bit(WDTS_DEV_OPEN, &wdt_status);
            return ret;
        }
    }

    return nonseekable_open(inode, file);
}
/*
 功能描述    : 关闭看门狗，如果关闭不成功则将其置为WDTS_EXPECTED状态
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月24日 14:09:00
*/
static int lx3160_wdt_release(struct inode *inode, struct file *file)
{
    if (test_bit(WDTS_TIMER_RUN, &wdt_status))
    {
        if (test_and_clear_bit(WDTS_EXPECTED, &wdt_status))
        {
            int ret = lx3160_wdt_stop();
            if (ret)
            {
                /*
                * Stop failed. Just keep the watchdog alive
                * and hope nothing bad happens.
                */
                set_bit(WDTS_EXPECTED, &wdt_status);
                lx3160_wdt_keepalive();
                return ret;
            }
            clear_bit(WDTS_TIMER_RUN, &wdt_status);
        }
        else
        {
            lx3160_wdt_keepalive();
            pr_crit("unexpected close, not stopping watchdog!\n");
        }
    }
    clear_bit(WDTS_DEV_OPEN, &wdt_status);

    return 0;
}
/*
 功能描述    : 喂狗函数，写入任何数据都代表喂狗
 返回值      : 成功为0，失败为-1
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月24日 14:09:58
*/
static ssize_t lx3160_wdt_write(struct file *file, const char __user *buf,
                         size_t count, loff_t *ppos)
{
    size_t ofs = 0;
    char c = 0;

    if (count)
    {
        clear_bit(WDTS_EXPECTED, &wdt_status);
        lx3160_wdt_keepalive();
    }
    if (!nowayout)
    {
        /* note: just in case someone wrote the magic character long ago */
        for (ofs = 0; ofs != count; ofs++)
        {
            c = 0;
            if (get_user(c, buf + ofs))
            {
                return -EFAULT;
            }
            if (c == WD_MAGIC)
            {
                set_bit(WDTS_EXPECTED, &wdt_status);
            }
        }
    }

    return count;
}

static const struct watchdog_info ident =
{
    .options = WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING,
    .firmware_version =    1,
    .identity = WATCHDOG_NAME,
};

/*
 功能描述    : 对看门狗进行配置
 返回值      : 成功为0，失败为相应错误值
 参数        : 
 作者        : 张彦升
 日期        : 2014年4月24日 14:21:18
*/
static long lx3160_wdt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0, status = 0, new_options = 0, new_timeout = 0;
    union
    {
        struct watchdog_info __user *ident;
        int __user *i;
    } uarg;

    uarg.i = (int __user *)arg;

    switch (cmd)
    {
    case WDIOC_GETSUPPORT:
        ret = copy_to_user(uarg.ident, &ident, sizeof(ident));
        if (ret)
        {
            return -EFAULT;
        }
        
        return 0;

    case WDIOC_GETSTATUS:
        ret = lx3160_wdt_get_status(&status);
        if (ret)
        {
            return ret;
        }
        return put_user(status, uarg.i);

    case WDIOC_GETBOOTSTATUS:
        return put_user(0, uarg.i);

    case WDIOC_KEEPALIVE:
        lx3160_wdt_keepalive();
        return 0;

    case WDIOC_SETOPTIONS:
        if (get_user(new_options, uarg.i))
        {
            return -EFAULT;
        }

        switch (new_options)
        {
        case WDIOS_DISABLECARD:
            if (test_bit(WDTS_TIMER_RUN, &wdt_status))
            {
                ret = lx3160_wdt_stop();
                if (ret)
                {
                    return ret;
                }
            }
            clear_bit(WDTS_TIMER_RUN, &wdt_status);
            return 0;

        case WDIOS_ENABLECARD:
            if (!test_and_set_bit(WDTS_TIMER_RUN, &wdt_status))
            {
                ret = lx3160_wdt_start();
                if (ret)
                {
                    clear_bit(WDTS_TIMER_RUN, &wdt_status);
                    return ret;
                }
            }
            return 0;

        default:
            return -EFAULT;
        }
        break;

    case WDIOC_SETTIMEOUT:
        if (get_user(new_timeout, uarg.i))
        {
            return -EFAULT;
        }
        ret = lx3160_wdt_set_timeout(new_timeout);
        /*default return timeout of new,let user check whether set ok*/
    case WDIOC_GETTIMEOUT:
        if (put_user(timeout, uarg.i))
        {
            return -EFAULT;
        }
        return ret;

    default:
        return -ENOTTY;
    }
}

static int lx3160_wdt_notify_sys(struct notifier_block *this, unsigned long code,
                          void *unused)
{
    if (code == SYS_DOWN || code == SYS_HALT)
    {
        lx3160_wdt_stop();
    }
    printk(KERN_INFO "watchdog notify...\n");

    return NOTIFY_DONE;
}

static const struct file_operations lx3160_wdt_fops =
{
    .owner          = THIS_MODULE,
    .llseek         = no_llseek,
    .open           = lx3160_wdt_open,
    .release        = lx3160_wdt_release,
    .write          = lx3160_wdt_write,
    .unlocked_ioctl = lx3160_wdt_ioctl,
};

static struct miscdevice lx3160_wdt_miscdev =
{
    .minor          = WATCHDOG_MINOR,
    .name           = "watchdog",
    .fops           = &lx3160_wdt_fops,
};

static struct notifier_block lx3160_wdt_notifier =
{
    .notifier_call  = lx3160_wdt_notify_sys,
};


static int __init lx3160_wdt_init(void)
{
    int ret = 0;
    wdt_status = 0;

    lx3160_superio_enter();

    /*copy from senbo lx3160 demo program,actually I don't know what's mean*/
    lx3160_superio_outb(0x0a, 0x07);
    lx3160_superio_outb(0x01, 0xf0);

    pba = lx3160_superio_inw(BASEREG);

    lx3160_superio_exit();

    if (!pba)
    {
        printk(KERN_ERR "port base address is zero\n");

        return 0;
    }

    ret = register_reboot_notifier(&lx3160_wdt_notifier);
    if (ret)
    {
        pr_err("Cannot register reboot notifier (err=%d)\n", ret);
        goto err_out_quit;
    }

    ret = misc_register(&lx3160_wdt_miscdev);
    if (ret)
    {
        pr_err("Cannot register miscdev on minor=%d (err=%d)\n", lx3160_wdt_miscdev.minor, ret);
        goto err_out_reboot;
    }

    return 0;

err_out_reboot:
    unregister_reboot_notifier(&lx3160_wdt_notifier);

err_out_quit:
    return ret;
}

static void __exit lx3160_wdt_exit(void)
{
    misc_deregister(&lx3160_wdt_miscdev);
    unregister_reboot_notifier(&lx3160_wdt_notifier);

    return;
}

module_init(lx3160_wdt_init);
module_exit(lx3160_wdt_exit);

