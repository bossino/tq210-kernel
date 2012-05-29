/* drivers/input/touchscreen/goodix_touch.c
 *
 * Copyright (C) 2010 Goodix, Inc.
 * 
 * Author: Eltonny
 * Date: 2010.11.08
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/irq.h>
#include "goodix_touch.h"
#include "goodix_queue.h"

//used by Guitar_Update
static struct workqueue_struct *goodix_wq;
static const char *s3c_ts_name = "S3C TouchScreen of Goodix";
static struct point_queue finger_list;
struct i2c_client * i2c_connect_client = NULL;
EXPORT_SYMBOL(i2c_connect_client);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
static void goodix_ts_early_suspend(struct early_suspend *h);
static void goodix_ts_late_resume(struct early_suspend *h);
#endif
//fighter++
static struct timer_list delay_tp_timer;
static volatile  int tp_start = 0;
//fighter--
static uint8_t last_touchkey;
#ifdef LED_PORT
  static struct timer_list led_timer;
#endif
#ifdef MOTO_PORT
  static struct timer_list moto_timer;
#endif
//static int pen_down;

static int goodix_ts_power(struct goodix_ts_data * ts, int on);
/*******************************************************	
功能：	
	读取从机数据
	每个读操作用两条i2c_msg组成，第1条消息用于发送从机地址，
	第2条用于发送读取地址和取回数据；每条消息前发送起始信号
参数：
	client:	i2c设备，包含设备地址
	buf[0]：	首字节为读取地址
	buf[1]~buf[len]：数据缓冲区
	len：	读取数据长度
return：
	执行消息数
*********************************************************/
/*Function as i2c_master_send */
static int i2c_read_bytes(struct i2c_client *client, uint8_t *buf, int len)
{
	struct i2c_msg msgs[2];
	int ret=-1;
	//发送写地址
	msgs[0].flags=!I2C_M_RD;//写消息
	msgs[0].addr=client->addr;
	msgs[0].len=1;
	msgs[0].buf=&buf[0];
	//接收数据
	msgs[1].flags=I2C_M_RD;//读消息
	msgs[1].addr=client->addr;
	msgs[1].len=len-1;
	msgs[1].buf=&buf[1];
	
	ret=i2c_transfer(client->adapter,msgs, 2);
	return ret;
}

/*******************************************************	
功能：
	向从机写数据
参数：
	client:	i2c设备，包含设备地址
	buf[0]：	首字节为写地址
	buf[1]~buf[len]：数据缓冲区
	len：	数据长度	
return：
	执行消息数
*******************************************************/
/*Function as i2c_master_send */
static int i2c_write_bytes(struct i2c_client *client,uint8_t *data,int len)
{
	struct i2c_msg msg;
	int ret=-1;
	//发送设备地址
	msg.flags=!I2C_M_RD;//写消息
	msg.addr=client->addr;
	msg.len=len;
	msg.buf=data;		
	
	ret=i2c_transfer(client->adapter,&msg, 1);
	return ret;
}

/*******************************************************
功能：
	Guitar初始化函数，用于发送配置信息，获取版本信息
参数：
	ts:	client私有数据结构体
return：
	执行结果码，0表示正常执行
*******************************************************/
static int goodix_init_panel(struct goodix_ts_data *ts)
{
	int ret=-1; 					
	uint8_t config_info[]    = {
            101,                                //操作地址
            0,                                  //原点
            (SCREEN_MAX_WIDTH&0x0000FF00)>>8,   //X最大输出
            SCREEN_MAX_WIDTH&0x000000FF,
            (SCREEN_MAX_HEIGHT&0x0000FF00)>>8,  //Y最大输出
            SCREEN_MAX_HEIGHT&0x000000FF,
            MAX_FINGER_NUM,                     //手指数
            (0|INT_TRIGGER),                    //module_switch
            40,                                 //POS_NORMAL_VALUE
            0xE7,                               //NEG_NORMAL_VALUE(-25)
            30,                                 //NORMAL_UPDATE_TIME
            2,                                  //SHAKE_COUNT
            8,                                  //COORD_FILTER
            25,                                 //LARTE_COUNT
            0x4c,                               //ADC_CFG
            0x4F,
            0x4F,
            0x20,
            0x00,
            0x07,
            0x00,
            0x20,
            0x50,                           //LEAVEL_VALUE_CFG
            150,                            //TOUCH_VALUE_CFG
            //DRIVE LINE
/////////////////////////////////////////////////////////////////////
            12,
            13,
            14,
            29,
            28,
            27,
            26,
            25,
            24,
            23,
            22,
            21,
            20,
            19,
            18,
            17,
            16,
            15,
            11,
            10,
            9,
            8,
            7,
            6,
            5,
            4,
            3,
            2,
            1,
            0,
/////////////////////////////////////////////////////////////////////
            0x60,                              //KEY_TOUCH_LEVEL     154
//            72,                                //KEY_COORD[KEY_NUM]
//            96,
//            158,
//            183,
            80,                                //KEY_COORD[KEY_NUM]
            171,
            0,
            0,

            0,
            0,
            0,
            0,
            0x11,                            //KEY_TYPE
//            0,                            //KEY_TYPE

            24,                              //KEY_WIDTH
            24,                              //KEY_HEIGHT
            27,                              //KEY_GAP
            
            0,                              //FREQ_INTERVAL       167
            0,
            0,
            0,
            0,                              //KEY2_GAP            171
            30,//29,                        //MASK_LEFT           172
            10,//9,
            1,//2,
            2,//3,
            0,                              //MASK2_LEFT          176
            0,
            0,
            0,
            0,                              //SPACE_TOP           180
            0,
            0,
//            53
            0
  };						 

	ret=i2c_write_bytes(ts->client,config_info, (sizeof(config_info)/sizeof(config_info[0])));
	if (ret < 0) 
	{
//	  printk("goodix_init_panel error\n");
		goto error_i2c_transfer;
	}
	msleep(10);
	return 0;

error_i2c_transfer:
	return ret;
}

//TODO:需要修改，后续使用
static int  goodix_read_version(struct goodix_ts_data *ts)
{
	int ret;
	uint8_t version_data[18]={0};	//store touchscreen version infomation
	memset(version_data, 0, 18);
	version_data[0]=240;
	version_data[17]='\0';
	msleep(2);
	ret=i2c_read_bytes(ts->client, version_data, sizeof(version_data)/sizeof(version_data[0])-1);
	if (ret < 0) 
		return ret;
	dev_info(&ts->client->dev," Guitar Version: %s\n", &version_data[1]);
	return 0;
}

//关闭KEY_LED
#ifdef LED_PORT
void led_off(unsigned long param)
{
//printk("led_off\n");
  gpio_direction_output(LED_PORT, 0);
}
#endif

//关闭马达
#ifdef MOTO_PORT
void moto_off(unsigned long param)
{
//printk("led_off\n");
  gpio_direction_output(MOTO_PORT, 0);
}
#endif

/*******************************************************	
功能：
	触摸屏工作函数
	由中断触发，接受1组坐标数据，校验后再分析输出
参数：
	ts:	client私有数据结构体
return：
	执行结果码，0表示正常执行
********************************************************/
static void goodix_ts_work_func(struct work_struct *work)
{	
	uint8_t  point_data[1+1+2+5*MAX_FINGER_NUM+1]={ 0 };
	uint8_t  check_sum = 0;
	static uint16_t   finger_last= 0;		//上次按键的手指索引
	uint16_t  finger_current = 0;			//当前按键的手指索引
	uint16_t  finger = 0, finger_bit = 0;
	unsigned int  count = 0, point_count = 0;
	unsigned int position = 0;	
	int ret=-1;
	int send_penup;

	struct goodix_ts_data *ts = container_of(work, struct goodix_ts_data, work);

	if( ts->retry > 9) {
		dev_info(&(ts->client->dev), "Because of transfer error,touchscreen stop working.\n");
		return ;
	}

   send_penup = 0;
//  printk(KERN_INFO "Enrey in INT\n");
#ifdef ENABLE_POLL
	while(gpio_get_value(INT_PORT) == (INT_TRIGGER&0x01))	//查询是否需要继续读取坐标
#endif
	{
	point_data[0] = 0;
	ret=i2c_read_bytes(ts->client, point_data,  sizeof(point_data)/sizeof(point_data[0]));
	if(ret <= 0)	
	{
		dev_err(&(ts->client->dev),"I2C transfer error. Number:%d\n ", ret);
		ts->retry++;
		goodix_ts_power(ts, 0);
		msleep(20);
		goodix_ts_power(ts, 1);
#ifdef ENABLE_POLL
		continue;
#else
  	enable_irq(ts->client->irq);
  	return;
#endif
	}
	else
	{
	  ts->retry = 0;
	}

	//触摸按键
	//fighter++
	if(tp_start)
	//fighter--
  {
    int j;
		//printk("TouchKey = 0x%02x ",point_data[1]);
    for(j=0; j<=MAX_TOUCH_KEY; j++)
    {
      uint8_t temp_cur, temp_last;
      temp_cur = (point_data[1]>>j) & 0x01;
      temp_last = (last_touchkey>>j) & 0x01;
      if(temp_cur != temp_last)
      {
        if(temp_cur != 0)
        { 
          input_report_key(ts->input_dev, Touch_Keycode[j], KEY_PRESS);
#ifdef LED_PORT
          gpio_direction_output(LED_PORT, 1);
          del_timer_sync(&led_timer);
          led_timer.expires = jiffies + LED_TIME*HZ;
          add_timer(&led_timer);
#endif

#ifdef MOTO_PORT
          gpio_direction_output(MOTO_PORT, 1);
          del_timer_sync(&moto_timer);
          moto_timer.expires = jiffies + HZ/10;
          add_timer(&moto_timer);
#endif

//printk("TouchKey %s press\n",Touch_KeycodeStr[j]);
        }
        else
        {
          input_report_key(ts->input_dev, Touch_Keycode[j], KEY_RELEASE);         
//printk("TouchKey %s release\n",Touch_KeycodeStr[j]);
        }
      }
    }
    last_touchkey = point_data[1];
  }

	ts->bad_data = 0; 
	finger_current =  (point_data[3]<<8) + point_data[2];
	
	finger = finger_current^finger_last;	
	if(finger == 0 && finger_current == 0)// && finger_list.length == 0)
	{
#ifdef ENABLE_POLL
		continue;
#else
  	enable_irq(ts->client->irq);
  	return;
#endif
  }

	else if(finger == 0)							
		goto BIT_NO_CHANGE;					//the same as last time
	//check which point(s) DOWN or UP
	finger_bit = finger_current;
	for(position = 0; (finger)&& (position < MAX_FINGER_NUM);  position++)
	{
		if((finger&0x01) == 1)	//current bit is 1,so NO.postion finger is changed
		{							
			if(finger_bit&0x01)		//NO.postion finger is DOWN
				add_point(&finger_list, position);
			else
				set_up_point(&finger_list, position);
		}
		finger>>=1;
		finger_bit>>=1;
	}

BIT_NO_CHANGE:	
	for(count=0; count < finger_list.length; count++)
	{	//if a finger is down, must read its position
		if(finger_list.pointer[count].state == FLAG_UP)
		{
			finger_list.pointer[count].x = finger_list.pointer[count].y = 0;
			finger_list.pointer[count].pressure = 0;
			continue;
		}
		finger_bit = finger_current, position = 0;
		//search the position to read data.
		for(point_count = 0; point_count < finger_list.pointer[count].num; point_count++)
		{
			if(finger_bit&0x01)
				position++;
			finger_bit>>=1;
		}
		position = 5*position + 4;
		finger_list.pointer[count].x = (unsigned int) (point_data[position]<<8) + (unsigned int)( point_data[position+1]);
		finger_list.pointer[count].y = (unsigned int)(point_data[position+2]<<8) + (unsigned int) (point_data[position+3]);
		finger_list.pointer[count].pressure =(unsigned int) (point_data[position+4]);		
//		finger_list.pointer[count].x = finger_list.pointer[count].x *SCREEN_MAX_WIDTH/(TOUCH_MAX_WIDTH);
//		finger_list.pointer[count].y = finger_list.pointer[count].y *SCREEN_MAX_HEIGHT/(TOUCH_MAX_HEIGHT);
#ifdef CONFIG_TOUCHSCREEN_ROTATION_90
		finger_list.pointer[count].x = finger_list.pointer[count].y;
		finger_list.pointer[count].y = TOUCH_MAX_WIDTH - finger_list.pointer[count].x;
#endif
#ifdef CONFIG_TOUCHSCREEN_ROTATION_180
		finger_list.pointer[count].x = TOUCH_MAX_WIDTH - finger_list.pointer[count].x;
		finger_list.pointer[count].y = TOUCH_MAX_HEIGHT - finger_list.pointer[count].y;
#endif
#ifdef CONFIG_TOUCHSCREEN_ROTATION_270
		finger_list.pointer[count].x = TOUCH_MAX_HEIGHT - finger_list.pointer[count].y;
		finger_list.pointer[count].y = finger_list.pointer[count].x;
#endif
	}	

	if(finger_current&0x01)
	{
		input_report_abs(ts->input_dev, ABS_X, finger_list.pointer[0].x);
		input_report_abs(ts->input_dev, ABS_Y, finger_list.pointer[0].y);		
	}	
  	input_report_abs(ts->input_dev, ABS_PRESSURE, finger_list.pointer[0].pressure);
	if(finger_list.pointer[0].pressure == 0)
	{
	  input_report_key(ts->input_dev, BTN_TOUCH, 0);
	  send_penup = 1;
	}
	else
	{
	  input_report_key(ts->input_dev, BTN_TOUCH, 1);
	}

//printk("%4d,%4d ", finger_list.pointer[0].x, finger_list.pointer[0].y);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, finger_list.pointer[0].x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, finger_list.pointer[0].y);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, finger_list.pointer[0].pressure);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, finger_list.pointer[0].pressure);	
	input_mt_sync(ts->input_dev);

	for(count = 1; count < MAX_FINGER_NUM; count++)
	{
		if (finger_list.length > count)
		{
			
			if(finger_list.pointer[count].state == FLAG_DOWN)
			{
//printk("%4d,%4d", finger_list.pointer[count].x, finger_list.pointer[count].y);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, finger_list.pointer[count].x);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, finger_list.pointer[count].y);
			} 
//printk(" p-%d", finger_list.pointer[count].pressure);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, finger_list.pointer[count].pressure);
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, finger_list.pointer[count].pressure);
			input_mt_sync(ts->input_dev);
		}
	}
	input_sync(ts->input_dev);
//printk("\n");

	del_point(&finger_list);
	finger_last =  finger_current;

#ifdef ENABLE_POLL
	msleep(POLL_TIME);
#endif
	}	

#ifdef ENABLE_POLL
//		printk(KERN_INFO "Exit in INT\n");
  if(send_penup == 0)
  {
    input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
    input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);	
    input_report_abs(ts->input_dev, ABS_PRESSURE, 0);
    input_report_key(ts->input_dev, BTN_TOUCH, 0);
    input_mt_sync(ts->input_dev);
    input_sync(ts->input_dev);
//printk("fix pen up\n");
  }
#endif
	enable_irq(ts->client->irq);
	return;
}
//fighter++
void delay_tp_start(unsigned long param)
{
	del_timer(&delay_tp_timer);
  tp_start = 1;
  //printk("tp_start = 1++++++++++++++++++\n");
}
//fighter--
/*******************************************************	
功能：
	中断响应函数
	由中断触发，调度触摸屏处理函数运行
参数：
	timer：函数关联的计时器	
return：
	计时器工作模式，HRTIMER_NORESTART表示不需要自动重启
********************************************************/
static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;
	//printk("touch screen irq+++++++++++++++++\n");
	disable_irq_nosync(ts->client->irq);
	queue_work(goodix_wq, &ts->work);
	
	return IRQ_HANDLED;
}

/*******************************************************	
功能：
	管理GT801的电源，允许GT801 PLUS进入睡眠或将其唤醒
参数：
	on:	0表示使能睡眠，1为唤醒
return：
	是否设置成功，0为成功
	错误码：-1为i2c错误，-2为GPIO错误；-EINVAL为参数on错误
********************************************************/
//static int goodix_ts_power(struct goodix_ts_data * ts, int on)
//{
//	int ret = -1;
//	unsigned char i2c_control_buf[2] = {80,  1};
//
//	if(on != 0 && on !=1)
//	{
//		printk(KERN_DEBUG "%s: Cant't support this command.", s3c_ts_name);
//		return -EINVAL;
//	}
//
//	if(on == 0)
//	{
//		gpio_direction_output(INT_PORT, 1);
//		if(!gpio_get_value(INT_PORT))
//			return -2;
//		ret = i2c_write_bytes(ts->client, i2c_control_buf, 2);
//		if(ret > 0)						//failed
//			ret = 0;
//	}
//	else if(on == 1)
//	{
//		gpio_set_value(INT_PORT, 0);
//		if(gpio_get_value(INT_PORT))	//failed	
//			return -2;
//		msleep(20);
//		s3c_gpio_cfgpin(INT_PORT, INT_CFG);	//Set IO port function	
//		msleep(260);
//		ret = 0;
//	}
//
//	//printk(KERN_INFO "Set Command:%d, ret:%d\n", on, ret);
//	return ret;
//}
static int goodix_ts_power(struct goodix_ts_data * ts, int on)
{
//printk("goodix_ts_power %d\n",on);

  if(on == 1)
  {
    int retry;
    gpio_direction_output(POWER_PORT, 1);
    msleep(50);
  	for(retry=0; retry<3; retry++)
  	{
  		if(goodix_init_panel(ts) != 0)	//Initiall failed
  		{
    		msleep(10);
  			continue;
  		}
  		else
  			break;
  	}
  }
  else if(on == 0)
  {
#ifdef LED_PORT
    led_off(0);
#endif
    gpio_direction_output(POWER_PORT, 0);
  }
  return 0;
}

/*******************************************************	
功能：
	触摸屏探测函数
	在注册驱动时调用（要求存在对应的client）；
	用于IO,中断等资源申请；设备注册；触摸屏初始化等工作
参数：
	client：待驱动的设备结构体
	id：设备ID
return：
	执行结果码，0表示正常执行
********************************************************/
static int goodix_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	//TODO:在测试失败后需要释放ts
	int ret = 0;
	int retry=0,code;
	uint8_t i2c_state_buf[2] = { 81, 0};
	struct goodix_ts_data *ts;

	struct goodix_i2c_rmi_platform_data *pdata;
	dev_dbg(&client->dev,"Initstall touch driver.\n");

	//Check I2C function
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
	{
		dev_err(&client->dev, "Must have I2C_FUNC_I2C.\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

  //tp power on
	ret = gpio_request(POWER_PORT, "TS_POWER");	//Request IO
	if (ret < 0) 
	{
		dev_err(&client->dev,"Failed to request GPIO:%d, ERRNO:%d\n",(int)POWER_PORT, ret);
		kfree(ts);
		goto err_check_functionality_failed;
	}	
	gpio_direction_output(POWER_PORT, 0);
  msleep(20);
	gpio_direction_output(POWER_PORT, 1);
  msleep(50);

  //test i2c connection
	i2c_connect_client = client;	//used by Guitar_Update
	for(retry=0; retry < 3; retry++)
	{
		ret =i2c_write_bytes(client, NULL, 0);
		if (ret > 0)
			break;
	}
	if(ret <= 0)
	{
		dev_err(&client->dev, "Warnning: I2C communication might be ERROR!\n");
//		goto err_i2c_failed;
	}
	
	i2c_read_bytes(client, i2c_state_buf, 2);
	if(i2c_state_buf[1] == GUITAR_UPDATE_STATE)
	{
		dev_info(&client->dev, "Guitar is goint to be updating.\n");
		ret =  -GUITAR_UPDATE_STATE;
//		goto err_i2c_failed;	
	}


	INIT_WORK(&ts->work, goodix_ts_work_func);
	
	ts->client = client;
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;
	
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		dev_dbg(&client->dev,"goodix_ts_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->input_dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y); 						// for android

	for(retry = 0; retry < MAX_TOUCH_KEY; retry++){
		code = Touch_Keycode[retry];
		if(code<=0)
			continue;
		set_bit(code & KEY_MAX, ts->input_dev->keybit);
	}

	input_set_abs_params(ts->input_dev, ABS_X, 0, SCREEN_MAX_WIDTH, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_Y, 0, SCREEN_MAX_HEIGHT, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);
	
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_WIDTH, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_HEIGHT, 0, 0);	

	sprintf(ts->phys, "input/ts)");
	ts->input_dev->name = s3c_ts_name;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;
	ts->input_dev->id.version = 10427;	//screen firmware version
	
	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_err(&client->dev,"Probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}

#ifdef LED_PORT
	gpio_request(LED_PORT, "TS_LED");
  init_timer(&led_timer);
  led_timer.data = 0;
  led_timer.function = led_off;
 gpio_direction_output(LED_PORT, 0);
#endif

#ifdef MOTO_PORT
	gpio_request(MOTO_PORT, "TS_MOTO");
  init_timer(&moto_timer);  
  moto_timer.data = 0;
  moto_timer.function = moto_off;
#endif

//fighter++
	init_timer(&delay_tp_timer);
  delay_tp_timer.function = delay_tp_start;
  delay_tp_timer.expires = jiffies + 30*HZ;
  add_timer(&delay_tp_timer);
//fighter--

	ts->bad_data = 0;
	finger_list.length = 0;
  
  //config irq pin
	client->irq=TS_INT;		//If not defined in client
	ret = gpio_request(INT_PORT, "TS_INT");	//Request IO
	if (ret < 0) 
	{
		dev_err(&client->dev, "Failed to request GPIO:%d, ERRNO:%d\n",(int)INT_PORT,ret);
		goto err_input_register_device_failed;
	}
	s3c_gpio_setpull(INT_PORT, S3C_GPIO_PULL_UP);	//ret > 0 ?
	s3c_gpio_cfgpin(INT_PORT, INT_CFG);	//Set IO port function	
	
	ret  = request_irq(client->irq, goodix_ts_irq_handler ,  GT801_PLUS_IRQ_TYPE,	client->name, ts);
	if (ret != 0) {
		dev_err(&client->dev,"Cannot allocate ts INT!ERRNO:%d\n", ret);
		gpio_direction_input(INT_PORT);
		gpio_free(INT_PORT);
		goto err_input_register_device_failed;
	}
	else 
	{	
		disable_irq(client->irq);
		dev_dbg(&client->dev,"Reques EIRQ %d succesd on GPIO:%d\n",TS_INT,INT_PORT);
	}	

	for(retry=0; retry<3; retry++)
	{
		ret=goodix_init_panel(ts);	
		if(ret != 0)	//Initiall failed
		{
  		msleep(2);
			continue;
		}
		else
			break;
	}
//	if(ret != 0) {
//		ts->bad_data=1;
//		goto err_init_godix_ts;
//	}
	
	enable_irq(client->irq);
	ts->power = goodix_ts_power;
	goodix_read_version(ts);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = goodix_ts_early_suspend;
	ts->early_suspend.resume = goodix_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	dev_info(&client->dev,"Start %s in interrupt mode\n", ts->input_dev->name);
//{
//  void __iomem		*ts_base;
//  ts_base = ioremap(0xE0200E00, 0x200);
//  printk("0x%08x\n", readl(ts_base+0x4));
//  printk("0x%08x\n", readl(ts_base+0x8C));
//  printk("0x%08x\n", readl(ts_base+0x104));
//  printk("0x%08x\n", readl(ts_base+0x144));
//  iounmap(ts_base);
//}
	return 0;

err_init_godix_ts:
	free_irq(client->irq,ts);
	gpio_direction_input(INT_PORT);
	gpio_free(INT_PORT);

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
	i2c_set_clientdata(client, NULL);
err_i2c_failed:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}


/*******************************************************	
功能：
	驱动资源释放
参数：
	client：设备结构体
return：
	执行结果码，0表示正常执行
********************************************************/
static int goodix_ts_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	if (ts) 
	{
		gpio_direction_input(INT_PORT);
		gpio_free(INT_PORT);
		free_irq(client->irq, ts);
	}	

	
	dev_notice(&client->dev,"The driver is removing...\n");
	i2c_set_clientdata(client, NULL);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

//停用设备
static int goodix_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	disable_irq(client->irq);
	ret = cancel_work_sync(&ts->work);
	if(ret)	
		enable_irq(client->irq);
	if (ts->power) {	/* 必须在取消work后再执行，避免因GPIO导致坐标处理代码死循环	*/
		ret = ts->power(ts, 0);
		if (ret < 0)
			printk(KERN_ERR "goodix_ts_resume power off failed\n");
	}
	return 0;
}

//重新唤醒
static int goodix_ts_resume(struct i2c_client *client)
{
	int ret;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->power) {
		ret = ts->power(ts, 1);
		if (ret < 0)
			printk(KERN_ERR "goodix_ts_resume power on failed\n");
	}

	enable_irq(client->irq);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void goodix_ts_early_suspend(struct early_suspend *h)
{
	struct goodix_ts_data *ts;
	ts = container_of(h, struct goodix_ts_data, early_suspend);
printk("%s\n", __FUNCTION__);
	goodix_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void goodix_ts_late_resume(struct early_suspend *h)
{
	struct goodix_ts_data *ts;
	ts = container_of(h, struct goodix_ts_data, early_suspend);
printk("%s\n", __FUNCTION__);
	goodix_ts_resume(ts->client);
}
#endif

//可用于该驱动的 设备名—设备ID 列表
//only one client
static const struct i2c_device_id goodix_ts_id[] = {
	{ GOODIX_I2C_NAME, 0 },
	{ }
};

//设备驱动结构体
static struct i2c_driver goodix_ts_driver = {
	.probe		= goodix_ts_probe,
	.remove		= goodix_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= goodix_ts_suspend,
	.resume		= goodix_ts_resume,
#endif
	.id_table	= goodix_ts_id,
	.driver = {
		.name	= GOODIX_I2C_NAME,
		.owner = THIS_MODULE,
	},
};

/*******************************************************	
功能：
	驱动加载函数
return：
	执行结果码，0表示正常执行
********************************************************/
static int __devinit goodix_ts_init(void)
{
	int ret;
	
	goodix_wq = create_workqueue("goodix_wq");
	if (!goodix_wq) {
		printk(KERN_ALERT "creat workqueue faiked\n");
		return -ENOMEM;
		
	}
	ret=i2c_add_driver(&goodix_ts_driver);
	return ret; 
}

/*******************************************************	
功能：
	驱动卸载函数
参数：
	client：设备结构体
********************************************************/
static void __exit goodix_ts_exit(void)
{
	printk(KERN_ALERT "Touchscreen driver of guitar exited.\n");
	i2c_del_driver(&goodix_ts_driver);
	if (goodix_wq)
		destroy_workqueue(goodix_wq);
}

late_initcall(goodix_ts_init);
module_exit(goodix_ts_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Driver");
MODULE_LICENSE("GPL");
