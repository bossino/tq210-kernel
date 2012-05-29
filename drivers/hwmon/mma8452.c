/*
 *  mma8452.c - Linux kernel modules for 3-Axis Orientation/Motion
 *  Detection Sensor 
 *
 *  Copyright (C) 2009-2010 Freescale Semiconductor Hong Kong Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/input-polldev.h>
#include <linux/earlysuspend.h>

extern int active_gsensor;
#ifdef CONFIG_HAS_EARLYSUSPEND
struct early_suspend early_suspend;
static void mma8452_early_suspend(struct early_suspend *h);
static void mma8452_late_resume(struct early_suspend *h);
#endif
extern unsigned int m_sensor_switch;
/*
 * Defines
 */
#define assert(expr)\
	if (!(expr)) {\
		printk(KERN_ERR "Assertion failed! %s,%d,%s,%s\n",\
			__FILE__, __LINE__, __func__, #expr);\
	}

#define MMA8452_DRV_NAME	"mma8452"
#define MMA8452_I2C_ADDR	0x1C
#define MMA8452_ID		0x2A

/* register enum for mma8452 registers */
enum {
	MMA8452_STATUS = 0x00,
	MMA8452_OUT_X_MSB,
	MMA8452_OUT_X_LSB,
	MMA8452_OUT_Y_MSB,
	MMA8452_OUT_Y_LSB,
	MMA8452_OUT_Z_MSB,
	MMA8452_OUT_Z_LSB,
	
	MMA8452_SYSMOD = 0x0B,
	MMA8452_INT_SOURCE,
	MMA8452_WHO_AM_I,
	MMA8452_XYZ_DATA_CFG,
	MMA8452_HP_FILTER_CUTOFF,
	
	MMA8452_PL_STATUS,
	MMA8452_PL_CFG,
	MMA8452_PL_COUNT,
	MMA8452_PL_BF_ZCOMP,
	MMA8452_PL_P_L_THS_REG,
	
	MMA8452_FF_MT_CFG,
	MMA8452_FF_MT_SRC,
	MMA8452_FF_MT_THS,
	MMA8452_FF_MT_COUNT,

	MMA8452_TRANSIENT_CFG = 0x1D,
	MMA8452_TRANSIENT_SRC,
	MMA8452_TRANSIENT_THS,
	MMA8452_TRANSIENT_COUNT,
	
	MMA8452_PULSE_CFG,
	MMA8452_PULSE_SRC,
	MMA8452_PULSE_THSX,
	MMA8452_PULSE_THSY,
	MMA8452_PULSE_THSZ,
	MMA8452_PULSE_TMLT,
	MMA8452_PULSE_LTCY,
	MMA8452_PULSE_WIND,
	
	MMA8452_ASLP_COUNT,
	MMA8452_CTRL_REG1,
	MMA8452_CTRL_REG2,
	MMA8452_CTRL_REG3,
	MMA8452_CTRL_REG4,
	MMA8452_CTRL_REG5,
	
	MMA8452_OFF_X,
	MMA8452_OFF_Y,
	MMA8452_OFF_Z,
	
	MMA8452_REG_END,
};

enum {
	MODE_2G = 0,
	MODE_4G,
	MODE_8G,
};

#define MODE_CHANGE_DELAY_MS  100      

/* mma8452 status */
struct mma8452_status {
	u8 mode;
	u8 ctl_reg1;
};

static struct mma8452_status mma_status = {
	.mode 		= 0,
	.ctl_reg1	= 0
};

static struct device *hwmon_dev;
static int test_test_mode = 1;
static struct i2c_client *mma8452_i2c_client;
static int ifsuspend = 0;

/*
 * Initialization function
 */
static int mma8452_init_client(struct i2c_client *client)
{
	int result;

  //reset mm8452
	result = i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG2, 0x40);
	msleep(50);
	result = i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG2, 0x00);	

	if(test_test_mode)
	{
		/*
		 * Probe the device. We try to set the device to Test Mode and then to
		 * write & verify XYZ registers
		 */
		mma_status.ctl_reg1 = 0x18;//0x20;
		result = i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1, mma_status.ctl_reg1);
		assert(result==0);
		
		mma_status.mode	= MODE_2G;
		result = i2c_smbus_write_byte_data(client, MMA8452_XYZ_DATA_CFG, mma_status.mode);
		assert(result==0);
		
		mma_status.ctl_reg1|=0x01;
		result = i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1, mma_status.ctl_reg1);
		assert(result==0);
	}

	mdelay(MODE_CHANGE_DELAY_MS);
	return result;
}

/***************************************************************
*
* read sensor data from mma8452
*
***************************************************************/ 				
static int mma8452_read_data(short *x, short *y, short *z) {
	u8	tmp_data[7];

	if (i2c_smbus_read_i2c_block_data(mma8452_i2c_client,MMA8452_OUT_X_MSB,7,tmp_data) < 7) {
		dev_err(&mma8452_i2c_client->dev, "i2c block read failed\n");
			return -3;
	}

	*x = ((tmp_data[0] << 8) & 0xff00) | tmp_data[1];
	*y = ((tmp_data[2] << 8) & 0xff00) | tmp_data[3];
	*z = ((tmp_data[4] << 8) & 0xff00) | tmp_data[5];

	*x = (short)(*x) >> 4;
	*y = (short)(*y) >> 4;
	*z = (short)(*z) >> 4;

	if (mma_status.mode==MODE_4G){
		(*x)=(*x)<<1;
		(*y)=(*y)<<1;
		(*z)=(*z)<<1;
	}
	else if (mma_status.mode==MODE_8G){
		(*x)=(*x)<<2;
		(*y)=(*y)<<2;
		(*z)=(*z)<<2;
	}
	return 0;
}

static struct input_polled_dev *mma8452_idev;
//#define POLL_INTERVAL		100
//#define INPUT_FUZZ	32
//#define INPUT_FLAT	32
#define POLL_INTERVAL		40
#define INPUT_FUZZ	16
#define INPUT_FLAT	16
#include <linux/input-polldev.h>
static void report_abs(void)
{
	short 	x,y,z;
	int result;
	
	do{
		result=i2c_smbus_read_byte_data(mma8452_i2c_client, MMA8452_STATUS);
		msleep(1);
	} while (!(result & 0x01));		/* wait for new data */

	if (mma8452_read_data(&x,&y,&z) != 0) {
		printk(KERN_ERR, "mma8452 data read failed\n");
		return;
	}
	if(m_sensor_switch){
			input_report_abs(mma8452_idev->input, ABS_X, -y);
	input_report_abs(mma8452_idev->input, ABS_Y, x);
	input_report_abs(mma8452_idev->input, ABS_Z, z);
	}else{
	input_report_abs(mma8452_idev->input, ABS_X, x);
	input_report_abs(mma8452_idev->input, ABS_Y, y);
	input_report_abs(mma8452_idev->input, ABS_Z, z);
}
//printk(" x=%4d, y=%4d, z=%4d\n" , x,y,z );
	input_sync(mma8452_idev->input);
}

static void mma8452_dev_poll(struct input_polled_dev *dev)
{
  if((active_gsensor == 1) && (ifsuspend == 0))
	  report_abs();
} 
/////////////////////////end//////

/*
 * I2C init/probing/exit functions
 */
static int __devinit mma8452_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int result;
	struct i2c_adapter *adapter;	
	struct input_dev *idev;
	
	ifsuspend = 0;

	mma8452_i2c_client = client;
  adapter = to_i2c_adapter(client->dev.parent);
	result = i2c_check_functionality(adapter, 
		I2C_FUNC_SMBUS_BYTE|I2C_FUNC_SMBUS_BYTE_DATA);
	assert(result);
	
	printk(KERN_INFO "check mma8452 chip ID\n");
	result = i2c_smbus_read_byte_data(client, MMA8452_WHO_AM_I);

	if (MMA8452_ID != (result)) {	//compare the address value 
		dev_err(&client->dev,"read chip ID 0x%x is not equal to 0x%x!\n", result,MMA8452_ID);
		printk(KERN_INFO "read chip ID failed\n");
		result = -EINVAL;
		goto err_detach_client;
	}

	/* Initialize the MMA8452 chip */
	result = mma8452_init_client(client);
	assert(result==0);

	hwmon_dev = hwmon_device_register(&client->dev);
	assert(!(IS_ERR(hwmon_dev)));

	dev_info(&client->dev, "build time %s %s\n", __DATE__, __TIME__);

	/*input poll device register */
	mma8452_idev = input_allocate_polled_device();
	if (!mma8452_idev) {
		dev_err(&client->dev, "alloc poll device failed!\n");
		result = -ENOMEM;
		return result;
	}
	mma8452_idev->poll = mma8452_dev_poll;
	mma8452_idev->poll_interval = POLL_INTERVAL;
	idev = mma8452_idev->input;
	idev->name = MMA8452_DRV_NAME;
	idev->id.bustype = BUS_I2C;
	idev->dev.parent = &client->dev;
	idev->evbit[0] = BIT_MASK(EV_ABS);

	input_set_abs_params(idev, ABS_X, -8192, 8191, INPUT_FUZZ, INPUT_FLAT);
	input_set_abs_params(idev, ABS_Y, -8192, 8191, INPUT_FUZZ, INPUT_FLAT);
	input_set_abs_params(idev, ABS_Z, -8192, 8191, INPUT_FUZZ, INPUT_FLAT);
	result = input_register_polled_device(mma8452_idev);
	if (result) {
		dev_err(&client->dev, "register poll device failed!\n");
		return result;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	early_suspend.suspend = mma8452_early_suspend;
	early_suspend.resume = mma8452_late_resume;
	register_early_suspend(&early_suspend);
#endif
	return result;
	
err_detach_client:
	return result;
}

static int __devexit mma8452_remove(struct i2c_client *client)
{
	int result;
	mma_status.ctl_reg1 = i2c_smbus_read_byte_data(client, MMA8452_CTRL_REG1);
	result = i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1,mma_status.ctl_reg1 & 0xFE);
	assert(result==0);

	hwmon_device_unregister(hwmon_dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
#endif

	return result;
}

static int mma8452_suspend(struct i2c_client *client, pm_message_t mesg)
{
//	int result;
//	mma_status.ctl_reg1 = i2c_smbus_read_byte_data(client, MMA8452_CTRL_REG1);
//	result = i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1,mma_status.ctl_reg1 & 0xFE);
//	assert(result==0);
//	return result;
    return 0;
}

static int mma8452_resume(struct i2c_client *client)
{
//	int result;
//	result = i2c_smbus_write_byte_data(client, MMA8452_CTRL_REG1, mma_status.ctl_reg1);
//	assert(result==0);
//	return result;
    mma8452_init_client(client);
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mma8452_early_suspend(struct early_suspend *h)
{
//printk("%s\n", __FUNCTION__);
  ifsuspend = 1;
	mma8452_suspend(mma8452_i2c_client, PMSG_SUSPEND);
}

static void mma8452_late_resume(struct early_suspend *h)
{
//printk("%s\n", __FUNCTION__);
	mma8452_resume(mma8452_i2c_client);
  ifsuspend = 0;
}
#endif

static const struct i2c_device_id mma8452_id[] = {
	{ MMA8452_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mma8452_id);

static struct i2c_driver mma8452_driver = {
	.driver = {
		.name	= MMA8452_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.suspend = mma8452_suspend,
	.resume	= mma8452_resume,
	.probe	= mma8452_probe,
	.remove	= __devexit_p(mma8452_remove),
	.id_table = mma8452_id,
};

static int __init mma8452_init(void)
{
	/* register driver */
	int res;

	res = i2c_add_driver(&mma8452_driver);
	if (res < 0){
		printk(KERN_INFO "add mma8452 i2c driver failed\n");
		return -ENODEV;
	}
	printk(KERN_INFO "add mma8452 i2c driver\n");

	return (res);
}

static void __exit mma8452_exit(void)
{
	printk(KERN_INFO "remove mma8452 i2c driver.\n");
	i2c_del_driver(&mma8452_driver);
}

MODULE_AUTHOR("Yang Yonghui <yonghui.yang@freescale.com>");
MODULE_DESCRIPTION("MMA8452 3-Axis Orientation/Motion Detection Sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1");

module_init(mma8452_init);
module_exit(mma8452_exit);
