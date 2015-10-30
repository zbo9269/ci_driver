/*********************************************************************
Copyright (C), 2011,  Co.Hengjun, Ltd.

作者        : 张彦升
版本        : 1.0
创建日期    : 2014年3月13日 12:59:35
用途        : 光纤板的驱动
历史修改记录: v1.0    创建
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

#include "watchdog.h"

MODULE_AUTHOR("zhangys");
MODULE_LICENSE("Dual BSD/GPL");

/*
功能描述    : 该demo程序为senbo技术人员所给
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 10:23:31
*/
static int __init watchdog_init(void)
{
    int pba,pba_h,pba_l; /*primary base i/o address*/
    int count = 0;

    outb(0x55,0x2e);
    outb(0x07,0x2e);
    outb(0x0a,0x2f);
    outb(0xf0,0x2e);
    outb(0x01,0x2f);

    outb(0x60,0x2e);
    pba_h = inb(0x2f);
    outb(0x61,0x2e);
    pba_l = inb(0x2f);
    pba = pba_h*0x100+pba_l;
    outb(0xaa,0x2e);

    outb(0x0e,pba+0x47);

    /*change mode minutes*/
    outb(128,pba+0x65);
    /*do in one minutes*/
    outb(0x1,pba+0x66);
    while(count < 2)
    {
        outb(0x1,pba+0x66);
        ssleep(1);
        count ++;
    }
    /*outb(0x0,pba+0x66);*/

    printk(KERN_INFO "watchdog init\n");

    return 0;
}
/*
功能描述    : 注销函数
返回值      : 成功为0，失败为-1
参数        : 
作者        : 张彦升
日期        : 2014年3月3日 10:23:45
*/
static void __exit watchdog_exit(void)
{
    return;
}

module_init(watchdog_init);
module_exit(watchdog_exit);

