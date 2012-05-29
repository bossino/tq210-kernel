/*
 * include/linux/goodix_touch.h
 *
 * Copyright (C) 2010 Goodix, Inc.
 * 
 * Author: 	Eltonny
 * Date:	2010.9.27
 */

#ifndef 	_LINUX_GOODIX_TOUCH_H
#define		_LINUX_GOODIX_TOUCH_H

#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <mach/gpio-smdkc110.h>

#define GOODIX_I2C_NAME "Goodix-TS"
#define GUITAR_UPDATE_STATE 0x02
//触摸屏的分辨率
#define TOUCH_MAX_WIDTH	1024
#define TOUCH_MAX_HEIGHT 	600

//显示屏的分辨率
#define SCREEN_MAX_WIDTH	1024
#define SCREEN_MAX_HEIGHT	600				

//复位、中断、电源控制脚
#define POWER_PORT    GPIO_TP_PWR
#ifdef GPIO_TP_RESET
  #define RESET_PORT    GPIO_TP_RESET   //确保在连接该管脚时输出低
//  #define RESET_ENABLE  0
#endif

#define INT_PORT      GPIO_TP_INT   //Int IO port
#define TS_INT        GPIO_TP_INT_NO      //Interrupt Number,EINT18(119)
#define INT_CFG       GPIO_TP_INT_CFG   //IO configer as EINT type, set according to datasheet
#define FLAG_UP       0
#define FLAG_DOWN     1
#define INT_TRIGGER   2	  //中断方式为，touch时保持低电平
#define GT801_PLUS_IRQ_TYPE IRQ_TYPE_LEVEL_LOW
//#define ENABLE_POLL   1 //中断+查询方式
#define POLL_TIME     1	//电平触发+查询方式的时间间隔(ms)，轮询方式为:POLL_TIME+6
#define MAX_FINGER_NUM	10	//最大多点数

//触摸按键相关定义
#ifdef GPIO_TP_LED
  #define LED_PORT			GPIO_TP_LED
  #define LED_TIME      3
#endif
//#define MOTO_PORT			GPIO_MOTO_PWR
#define	KEY_PRESS     1
#define	KEY_RELEASE   0
//#define MAX_TOUCH_KEY 4
//int Touch_Keycode[MAX_TOUCH_KEY] = {	KEY_BACK, KEY_SEARCH, KEY_MENU, KEY_HOME	};
//char Touch_KeycodeStr[MAX_TOUCH_KEY][20] = {"KEY_BACK",  "KEY_SEARCH", "KEY_MENU", "KEY_HOME" };
#define MAX_TOUCH_KEY 2
int Touch_Keycode[MAX_TOUCH_KEY] = {	KEY_BACK, KEY_MENU};
char Touch_KeycodeStr[MAX_TOUCH_KEY][20] = {"KEY_BACK", "KEY_MENU"};


struct goodix_ts_data {
	uint16_t addr;
	uint8_t bad_data;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct  work;
	char phys[32];
	int retry;
	struct early_suspend early_suspend;
	int (*power)(struct goodix_ts_data * ts, int on);
};

struct goodix_i2c_rmi_platform_data {
	uint32_t version;	/* Use this entry for panels with */
	//该结构体用于管理设备平台资源
	//预留，用于之后的功能扩展
};

#endif /* _LINUX_GOODIX_TOUCH_H */
