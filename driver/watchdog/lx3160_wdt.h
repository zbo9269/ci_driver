/*********************************************************************
 Copyright (C), 2011,  Co.Hengjun, Ltd.

 作者        : 张彦升
 版本        : 1.0
 创建日期    : 2014年3月13日 13:00:09
 用途        : lx3160看门狗驱动程序
 历史修改记录: v1.0    创建
**********************************************************************/

#ifndef _watchdog_h__
#define _watchdog_h__

#define WATCHDOG_VERSION        "1.0"
#define WATCHDOG_NAME           "lx3160 WDT"
#define DRIVER_VERSION          WATCHDOG_NAME " driver, v" WATCHDOG_VERSION "\n"
#define WD_MAGIC                'V'

/* Defaults for Module Parameter */
#define DEFAULT_EXCLUSIVE       1
#define DEFAULT_TIMEOUT         60      /*seconds*/
#define DEFAULT_NOWAYOUT        WATCHDOG_NOWAYOUT

#define WDT_MAXUNITS    0xff

/* IO Ports */
#define REG             0x2e    /*super I/O reg of select register*/
#define VAL             0x2f    /*super I/O reg of value you can read from REG*/

/* Configuration Registers and Functions */
#define LDNREG              0x07
#define BASEREG             0x60
#define MODE_REG_OFF        0x65    /*offset of select mode register*/
#define TIME_OUT_REG_OFF    0x66    /*offset of set time register*/

/*mode*/
#define MODE_MINUTES        0x00    /*watch dog count mode of minutes*/
#define MODE_SECOND         0x80    /*watch dog count mode of seconds*/

/* wdt_status */
#define WDTS_TIMER_RUN      0   /*is running*/
#define WDTS_DEV_OPEN       1   /*any other program has open*/
#define WDTS_KEEPALIVE      2   /*alive status*/
#define WDTS_LOCKED         3   /*whether lock or not*/
#define WDTS_EXPECTED       4   /*we just expected it's working ok*/

#endif /*!_watchdog_h__*/
