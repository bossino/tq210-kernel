/*
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-i2c-drv.h>
#include <linux/time.h>
#include <mach/gpio-smdkc110.h>
#include <mach/gpio.h>

#define CONFIG_CAMERA_CONTROLLED_BY_ARM


#include <media/GC2015_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "GC2015.h"

#define GC2015_DRIVER_NAME	"GC2015"

//*********** proc *********************
#include <linux/proc_fs.h>
#include "../../../fs/proc/internal.h"
#define GC2015_PROC_NAME	"GC2015"
static struct proc_dir_entry * s_proc = NULL;
static struct i2c_client * s_i2c_client = NULL;
static int initted = 0;
static volatile unsigned int flash_mode_auto = 0;
static struct timer_list flash_power_timer;
static int GC2015_init(struct v4l2_subdev *sd, u32 val);
//**************************************

/* Default resolution & pixelformat. plz ref GC2015_platform.h */
#define DEFAULT_RES		VGA	/* Index of resoultion */
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */

unsigned char regABVal = 0x06;
unsigned char hueTable[21][3] = {};

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

/* Camera functional setting values configured by user concept */
struct GC2015_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
	unsigned int brightness;	/* V4L2_CID_CAMERA_BRIGHTNESS */
	unsigned int glamour;
};

struct GC2015_state {
	struct GC2015_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct GC2015_userset userset;
	int framesize_index;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
};

static inline struct GC2015_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct GC2015_state, sd);
}

static int GC2015_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[],
				unsigned char length);

#if 1
/*
 * OV7675 register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
static inline int GC2015_write(struct v4l2_subdev *sd, u8 addr, u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[1];
	unsigned char reg[2];
	int err = 0;
	int retry = 0;


	if (!client->adapter)
		return -ENODEV;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = reg;

	reg[0] = addr & 0xff;
	reg[1] = val & 0xff;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return err;	/* Returns here on success */

	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%02x%02x, " \
			"value: 0x%02x%02x\n", __func__, \
			reg[0], reg[1], reg[2], reg[3]);
		retry++;
		goto again;
	}

	return err;
}

static int GC2015_i2c_read(struct v4l2_subdev *sd, unsigned char reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int rc;

	unsigned char data[1]={0};
	data[0] = reg;
	i2c_master_recv(client, data ,1);
	return data[0];
}

static int GC2015_write_regs(struct v4l2_subdev *sd, unsigned char regs[],
				int size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, err;

	for (i = 0; i < size; i++) {
		err = GC2015_i2c_write(sd, &regs[i], sizeof(regs[i]));
		if (err < 0)
			v4l_info(client, "%s: register set failed\n", \
			__func__);
	}

	return 0;	/* FIXME */
}
#endif

extern s32 i2c_smbus_write_byte_data(struct i2c_client *client, u8 command, u8 value);


static int GC2015_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[],
				unsigned char length)
{
#if 0 //FIXME: urbetter skip I2C
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg = {client->addr, 0, length, i2c_data};
//	struct i2c_msg msg = {client->addr, I2C_M_IGNORE_NAK, length, buf};

	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;  
#else

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return i2c_smbus_write_byte_data(client, i2c_data[0], i2c_data[1]);
#endif
}

static struct v4l2_queryctrl GC2015_controls[] = {
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
//		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.id = V4L2_CID_CAMERA_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 1,
		.maximum = 5,
		.step = 1,
		.default_value = 1,	/* FIXME */
	},
	{
		.id = V4L2_CID_AUTO_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto white balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CAMERA_BRIGHTNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Brightness",
		.minimum = 0,
		.maximum = 8,
		.step = 1,
		.default_value = 4,
	},
	{
		.id = V4L2_CID_CAMERA_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0x10,
		.maximum = 0x30,
		.step = 1,
		.default_value = 0x20,
	},
	{
		.id = V4L2_CID_CAMERA_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0x00,
		.maximum = 0x80,
		.step = 1,
		.default_value = 0x40,
	},
	{
		.id = V4L2_CID_CAMERA_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0x00,
		.maximum = 0x09,
		.step = 1,
		.default_value = 0x04,
	},
	{
		.id = V4L2_CID_HUE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hue",
		.minimum = 0,
		.maximum = 21,
		.step = 1,
		.default_value = 10,
	},
	{
		.id = V4L2_CID_POWER_LINE_FREQUENCY,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Power line frequency",
		.minimum = 0,
		.maximum = 2,
		.step = 1,
		.default_value = 0,
	},
};

static inline struct v4l2_queryctrl const *GC2015_find_qctrl(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(GC2015_controls); i++)
		if (GC2015_controls[i].id == id)
			return &GC2015_controls[i];

	return NULL;
}

static int GC2015_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(GC2015_controls); i++) {
		if (GC2015_controls[i].id == qc->id) {
			memcpy(qc, &GC2015_controls[i], \
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

#ifdef CONFIG_VIDEO_GC2015_B
 void flash_power_onoff(unsigned int onoff)
{
		s3c_gpio_cfgpin(GPIO_FLASH_EN, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_FLASH_EN, S3C_GPIO_PULL_UP);
		gpio_set_value(GPIO_FLASH_EN,onoff);
}
EXPORT_SYMBOL(flash_power_onoff);
#endif

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int GC2015_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int GC2015_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}

static int GC2015_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}

static int GC2015_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;
	int i, num;

  struct GC2015_state *state = to_state(sd);
  
//printk("%s:type=%d fmt.pix.width=%d fmt.pix.height=%d mt.pix.pixelformat=%d\n",__func__,fmt->type,fmt->fmt.pix.width,fmt->fmt.pix.height ,fmt->fmt.pix.pixelformat);
  err = GC2015_try_fmt(sd, fmt);
  
  if((fmt->fmt.pix.width==640) && (fmt->fmt.pix.height==480))
  {
    if(initted == 0)
    {
  		num = ((sizeof(GC2015_preview)/sizeof(GC2015_preview[0])));			
  		for (i = 0; i < num ; i++)
  			GC2015_i2c_write(sd, GC2015_preview[i], 2);
  		initted = 1;
	  }
  	state->framesize_index = 0;
  }

  if((fmt->fmt.pix.width==1600) && (fmt->fmt.pix.height==1200))
  {
//		num = ((sizeof(GC2015_capture_uxga)/sizeof(GC2015_capture_uxga[0])));			
//		for (i = 0; i < num ; i++)
//			GC2015_i2c_write(sd, GC2015_capture_uxga[i], 2);
//
//		state->framesize_index = 8;
		num = ((sizeof(GC2015_capture_sxga)/sizeof(GC2015_capture_sxga[0])));			
		for (i = 0; i < num ; i++)
			GC2015_i2c_write(sd, GC2015_capture_sxga[i], 2);

		state->framesize_index = 7;
		initted = 0;
    msleep(100);
  }

  if((fmt->fmt.pix.width==1280) && (fmt->fmt.pix.height==960))
  {
		num = ((sizeof(GC2015_capture_sxga)/sizeof(GC2015_capture_sxga[0])));			
		for (i = 0; i < num ; i++)
			GC2015_i2c_write(sd, GC2015_capture_sxga[i], 2);

		state->framesize_index = 7;
		initted = 0;
    msleep(100);
  }
	return err;
}

static int GC2015_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	struct GC2015_state *state = to_state(sd);
	int num_entries = sizeof(gc2015_framesize_list) / sizeof(struct gc2015_enum_framesize);
	struct gc2015_enum_framesize *elem;
	int index = 0;
	int i = 0;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	index = state->framesize_index;

printk("%s %d\n", __FUNCTION__, index);
	for (i = 0; i < num_entries; i++) {
		elem = &gc2015_framesize_list[i];
		if (elem->index == index) {
			fsize->discrete.width = gc2015_framesize_list[i].width;
			fsize->discrete.height = gc2015_framesize_list[i].height;
			return 0;
		}
	}

	return -EINVAL;

}

static int GC2015_enum_frameintervals(struct v4l2_subdev *sd,
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int GC2015_enum_fmt(struct v4l2_subdev *sd, struct v4l2_fmtdesc *fmtdesc)
{
	int err = 0;

	return err;
}

static int GC2015_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s\n", __func__);

	return err;
}

static int GC2015_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s: numerator %d, denominator: %d\n", \
		__func__, param->parm.capture.timeperframe.numerator, \
		param->parm.capture.timeperframe.denominator);

	return err;
}

static int GC2015_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct GC2015_state *state = to_state(sd);
	struct GC2015_userset userset = state->userset;
	int err = 0;

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		ctrl->value = userset.auto_wb;
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		ctrl->value = userset.manual_wb;
		break;
	case V4L2_CID_CAMERA_EFFECT:
		ctrl->value = userset.effect;
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		ctrl->value = userset.contrast;
		break;
	case V4L2_CID_CAMERA_SATURATION:
		ctrl->value = userset.saturation;
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		ctrl->value = userset.sharpness;
		break;
	case V4L2_CID_CAMERA_BRIGHTNESS:
		ctrl->value = userset.brightness;
		break;
  
	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
	case V4L2_CID_CAM_JPEG_MEMSIZE:
	case V4L2_CID_CAM_JPEG_QUALITY:
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
	case V4L2_CID_CAM_DATE_INFO_YEAR:
	case V4L2_CID_CAM_DATE_INFO_MONTH:
	case V4L2_CID_CAM_DATE_INFO_DATE:
	case V4L2_CID_CAM_SENSOR_VER:
	case V4L2_CID_CAM_FW_MINOR_VER:
	case V4L2_CID_CAM_FW_MAJOR_VER:
	case V4L2_CID_CAM_PRM_MINOR_VER:
	case V4L2_CID_CAM_PRM_MAJOR_VER:
	case V4L2_CID_ESD_INT:
	case V4L2_CID_CAMERA_GET_ISO:
	case V4L2_CID_CAMERA_GET_SHT_TIME:
	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
	case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
		ctrl->value = 0;
		break;
	case V4L2_CID_EXPOSURE:
		ctrl->value = userset.exposure_bias;
		break;
	default:
		dev_err(&client->dev, "%s: no such ctrl\n", __func__);
		/* err = -EINVAL; */
		break;
	}
	return err;
}

void GC2015_color_effect(struct v4l2_subdev *sd, int which)
{
	int err, i;
	int num;
	
	printk("GC2015 color mode!!!\n");
	// normal,grayscale,sepia,sepia_green,sepia_blue,color_inv,gray_inv,embossment,sketch

	switch (which) {
	case 1:		
		num = ((sizeof(GC2015_effect_normal)/sizeof(GC2015_effect_normal[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_effect_normal[i], 2);
		}
	break;
	case 2:		
		num = ((sizeof(GC2015_effect_grayscale)/sizeof(GC2015_effect_grayscale[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_effect_grayscale[i], 2);
		}
	break;
	case 3:		
		num = ((sizeof(GC2015_effect_sepia)/sizeof(GC2015_effect_sepia[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_effect_sepia[i], 2);
		}
	break;
	case 4:		
		num = ((sizeof(GC2015_effect_sepia_green)/sizeof(GC2015_effect_sepia_green[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_effect_sepia_green[i], 2);
		}
	break;
	case 5:	
		num = ((sizeof(GC2015_effect_sepia_blue)/sizeof(GC2015_effect_sepia_blue[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_effect_sepia_blue[i], 2);
		}
	break;
	case 6:	
		num = ((sizeof(GC2015_effect_color_inv)/sizeof(GC2015_effect_color_inv[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_effect_color_inv[i], 2);
		}
	break;
	case 7:		
		num = ((sizeof(GC2015_effect_gray_inv)/sizeof(GC2015_effect_gray_inv[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_effect_gray_inv[i], 2);
		}
	break;
	case 8:		
		num = ((sizeof(GC2015_effect_embossment)/sizeof(GC2015_effect_embossment[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_effect_embossment[i], 2);
		}
	break;
	case 9:		   
		num = ((sizeof(GC2015_effect_sketch)/sizeof(GC2015_effect_sketch[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_effect_sketch[i], 2);
		}
	break;
	}
	
	mdelay(200);
}

void GC2015_white_balance_preset(struct v4l2_subdev *sd, int which)
{
	int err, i;
	int num;
	unsigned char ctrlreg[1][2];
	
//	printk("  GC2015 color mode!!!\n");
	// normal,grayscale,sepia,sepia_green,sepia_blue,color_inv,gray_inv,embossment,sketch

	switch (which) {
	case 1:		
		num = ((sizeof(GC2015_white_balance_auto)/sizeof(GC2015_white_balance_auto[0])));			
		for (i = 0; i < num ; i++) {
			err=GC2015_i2c_write(sd, GC2015_white_balance_auto[i], 2);
		}
		// enable AWB
		ctrlreg[0][0]=0x42; 
		ctrlreg[0][1]=GC2015_i2c_read(sd, 0x42)|0x02;
		err=GC2015_i2c_write(sd, ctrlreg[0], 2);
		
	break;
	case 2:		
		// disable AWB
		ctrlreg[0][0]=0x42; 
		ctrlreg[0][1]=GC2015_i2c_read(sd, 0x42) & (~0x02);
		err=GC2015_i2c_write(sd, ctrlreg[0], 2);
		
		num = ((sizeof(GC2015_white_balance_incandescence)/
						sizeof(GC2015_white_balance_incandescence[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_white_balance_incandescence[i], 2);
		}
	break;
	case 3:		
		// disable AWB
		ctrlreg[0][0]=0x22; 
		ctrlreg[0][1]=GC2015_i2c_read(sd, 0x42) & (~0x02);
		err=GC2015_i2c_write(sd, ctrlreg[0], 2);
		
		num = ((sizeof(GC2015_white_balance_daylight)/sizeof(GC2015_white_balance_daylight[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_white_balance_daylight[i], 2);
		}
	break;
	case 4:		
		// disable AWB
		ctrlreg[0][0]=0x22; 
		ctrlreg[0][1]=GC2015_i2c_read(sd, 0x42) & (~0x02);
		err=GC2015_i2c_write(sd, ctrlreg[0], 2);
		
		num = ((sizeof(GC2015_white_balance_fluorescent)/sizeof(GC2015_white_balance_fluorescent[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_white_balance_fluorescent[i], 2);
		}
	break;
	case 5:	
		// disable AWB
		ctrlreg[0][0]=0x22; 
		ctrlreg[0][1]=GC2015_i2c_read(sd, 0x42) & (~0x02);
		err=GC2015_i2c_write(sd, ctrlreg[0], 2);
		
		num = ((sizeof(GC2015_white_balance_cloud)/sizeof(GC2015_white_balance_cloud[0])));			
		for (i = 0; i < num ; i++) {
			err =GC2015_i2c_write(sd, GC2015_white_balance_cloud[i], 2);
		}
	break;
	}
	
	mdelay(200);
}

#ifdef CONFIG_VIDEO_GC2015_B
static void flash_power_timer_handler(unsigned long param)
{
	flash_power_onoff(0);
//  printk("tp_start = 1++++++++++++++++++\n");
}
static int gc2015_flashctrl(int value)
{
//	printk("sensor gc2015 flash control %d\n", value);
	switch(value)
	{
	  case FLASH_MODE_OFF:  		
  		gpio_request(GPIO_FLASH_MODE, "FLASH_MODE");
			gpio_direction_output(GPIO_FLASH_MODE, 1);
			gpio_free(GPIO_FLASH_MODE);
			flash_power_onoff(0);
			flash_mode_auto = 0;
	    break;
	  case FLASH_MODE_AUTO:
	  	gpio_request(GPIO_FLASH_MODE, "FLASH_MODE");
			gpio_direction_output(GPIO_FLASH_MODE, 0);
			gpio_free(GPIO_FLASH_MODE);
      flash_mode_auto = 1;
			flash_power_onoff(0);
	    break;
	  case FLASH_MODE_ON:
  		gpio_request(GPIO_FLASH_MODE, "FLASH_MODE");
			gpio_direction_output(GPIO_FLASH_MODE, 0);
			gpio_free(GPIO_FLASH_MODE);
			
			flash_power_onoff(1);
			flash_mode_auto = 0;
	    break;
	  case FLASH_MODE_TORCH:
			gpio_request(GPIO_FLASH_MODE, "FLASH_MODE");
			gpio_direction_output(GPIO_FLASH_MODE, 0);
			gpio_free(GPIO_FLASH_MODE);
			
			flash_power_onoff(1);
			flash_mode_auto = 0;
	    break;
	}
	return 0;
}
#endif /* CONFIG_VIDEO_GC2015_B */

static int GC2015_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, cnt = 0, err = -EINVAL;
	unsigned char ctrlreg[4][2];
	struct GC2015_state *state = to_state(sd);
	struct GC2015_userset userset = state->userset;
	
	switch (ctrl->id) {
	case V4L2_CID_CAMERA_BRIGHTNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_BRIGHTNESS %d\n", __func__, ctrl->value);
#define GC2015_SET_PAGE1 GC2015_write_cmos_sensor(0xfe , 0x01) 
 		ctrlreg[0][0] = 0xFE; ctrlreg[0][1] = 0x01;
 		ctrlreg[2][0] = 0xFE; ctrlreg[2][1] = 0x00;
		switch(ctrl->value)
		{
		  case EV_MINUS_4:
    		ctrlreg[1][0] = 0x13; ctrlreg[1][1] = 0x40;
    		ctrlreg[3][0] = 0xD5; ctrlreg[3][1] = 0xC0;
        break;
		  case EV_MINUS_3:
    		ctrlreg[1][0] = 0x13; ctrlreg[1][1] = 0x48;
    		ctrlreg[3][0] = 0xD5; ctrlreg[3][1] = 0xD0;
        break;
		  case EV_MINUS_2:
    		ctrlreg[1][0] = 0x13; ctrlreg[1][1] = 0x50;
    		ctrlreg[3][0] = 0xD5; ctrlreg[3][1] = 0xE0;
        break;
		  case EV_MINUS_1:
    		ctrlreg[1][0] = 0x13; ctrlreg[1][1] = 0x58;
    		ctrlreg[3][0] = 0xD5; ctrlreg[3][1] = 0xF0;
        break;
		  case EV_DEFAULT:
      default:
    		ctrlreg[1][0] = 0x13; ctrlreg[1][1] = 0x60;
    		ctrlreg[3][0] = 0xD5; ctrlreg[3][1] = 0x00;
        break;
		  case EV_PLUS_1:
    		ctrlreg[1][0] = 0x13; ctrlreg[1][1] = 0x68;
    		ctrlreg[3][0] = 0xD5; ctrlreg[3][1] = 0x10;
        break;
		  case EV_PLUS_2:
    		ctrlreg[1][0] = 0x13; ctrlreg[1][1] = 0x70;
    		ctrlreg[3][0] = 0xD5; ctrlreg[3][1] = 0x20;
        break;
		  case EV_PLUS_3:
    		ctrlreg[1][0] = 0x13; ctrlreg[1][1] = 0x78;
    		ctrlreg[3][0] = 0xD5; ctrlreg[3][1] = 0x30;
        break;
		  case EV_PLUS_4:
    		ctrlreg[1][0] = 0x13; ctrlreg[1][1] = 0x80;
    		ctrlreg[3][0] = 0xD5; ctrlreg[3][1] = 0x40;
        break;
		}
		cnt = 4;
		userset.brightness = ctrl->value;
		break;

	case V4L2_CID_CAMERA_CONTRAST:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_CONTRAST\n", __func__);
		ctrlreg[0][0] = 0x9C;
		ctrlreg[0][1] = ctrl->value;
		cnt = 1;
		break;

	case V4L2_CID_CAMERA_SATURATION:
		dev_dbg(&client->dev, "%s: V4L2_CID_SATURATION\n", __func__);
		ctrlreg[0][0] = 0xA7;
		ctrlreg[1][0] = 0xA8;
		ctrlreg[0][1] = ctrlreg[1][1] = ctrl->value;
		cnt = 2;
		break;

	case V4L2_CID_CAMERA_SHARPNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_SHARPNESS\n", __func__);
		ctrlreg[0][0] = 0x8F;
		ctrlreg[0][1] = ctrl->value;
		cnt = 1;
		break;

	case V4L2_CID_HUE:
		dev_dbg(&client->dev, "%s: V4L2_CID_HUE\n", __func__);
		if (hueTable[ctrl->value][2] == 0) {
			regABVal &= ~BIT(1);
			regABVal |= BIT(0);
		} else {
			regABVal |= BIT(1);
			regABVal &= ~BIT(0);
		}
		ctrlreg[0][0] = 0xA9;
		ctrlreg[0][1] = hueTable[ctrl->value][0];
		ctrlreg[1][0] = 0xAA;
		ctrlreg[1][1] = hueTable[ctrl->value][1];
		ctrlreg[2][0] = 0xAB;
		ctrlreg[2][1] = regABVal;
		cnt = 3;
		break;

	case V4L2_CID_POWER_LINE_FREQUENCY:
		dev_dbg(&client->dev, "%s: V4L2_CID_POWER_LINE_FREQUENCY\n", __func__);
		ctrlreg[0][0] = 0x22;
		ctrlreg[1][0] = 0x23;
		if ((ctrl->value == 0) || (ctrl->value == 2)) { // off or 60Hz
			ctrlreg[0][1] = 0x7F;
			ctrlreg[1][1] = 0x03;
			cnt = 2;
		} else if (ctrl->value == 1){ // 50Hz
			ctrlreg[0][1] = 0x99;
			ctrlreg[1][1] = 0x02;
			cnt = 2;
		}
		break;

	case V4L2_CID_CAMERA_WHITE_BALANCE:
		dev_dbg(&client->dev, "%s: in white balance preset %d\n",  __func__, ctrl->value);
		GC2015_white_balance_preset(sd, ctrl->value);
    userset.manual_wb = ctrl->value;
		return 0;
		break;
		
	case V4L2_CID_CAMERA_EFFECT:	  
		dev_dbg(&client->dev, "%s: in color effect %d\n", __func__,	ctrl->value);
		GC2015_color_effect(sd, ctrl->value);
		return 0;
		break;
#ifdef CONFIG_VIDEO_GC2015_B
	case V4L2_CID_CAMERA_FLASH_MODE:
		gc2015_flashctrl(ctrl->value);
		return 0;
		break;
  case V4L2_CID_CAMERA_START_FLASH:
  	  
		 if(flash_mode_auto)
		{
			gpio_request(GPIO_FLASH_MODE, "FLASH_MODE");
			s3c_gpio_cfgpin(GPIO_FLASH_MODE, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(GPIO_FLASH_MODE, S3C_GPIO_PULL_UP);
			gpio_set_value(GPIO_FLASH_MODE,0);
			gpio_free(GPIO_FLASH_MODE);
		
			mod_timer(&flash_power_timer,jiffies + 2*HZ);
			flash_power_onoff(1);
  	} 
  	break;
#else
  case V4L2_CID_CAMERA_FLASH_MODE:
#endif /* CONFIG_VIDEO_GC2015_B */
	case V4L2_CID_CAMERA_WDR:
	case V4L2_CID_CAMERA_FACE_DETECTION:
	case V4L2_CID_CAMERA_FOCUS_MODE:
	case V4L2_CID_CAM_JPEG_QUALITY:
	case V4L2_CID_CAMERA_SCENE_MODE:
	case V4L2_CID_CAMERA_GPS_LATITUDE:
	case V4L2_CID_CAMERA_GPS_LONGITUDE:
	case V4L2_CID_CAMERA_GPS_TIMESTAMP:
	case V4L2_CID_CAMERA_GPS_ALTITUDE:
	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
	case V4L2_CID_CAMERA_FRAME_RATE:
	case V4L2_CID_CAMERA_ISO:
	case V4L2_CID_CAMERA_METERING:
	case V4L2_CID_CAMERA_CHECK_DATALINE:
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
	case V4L2_CID_CAMERA_RETURN_FOCUS:

	default:
		dev_err(&client->dev, "%s: no such control %d\n", __func__, ctrl->id - V4L2_CID_PRIVATE_BASE);
  	return 0;
		break;
	}

	for (i = 0; i < cnt; ++i)
		err = GC2015_i2c_write(sd, ctrlreg[i], 2);

	if (err < 0) {
		dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed: %d\n", __func__, ctrl->id);
	}

	return err;
}

static int gc2015_read_reg(struct v4l2_subdev *sd, unsigned char reg)
{
//	struct i2c_client *client = v4l2_get_subdevdata(sd);
//	unsigned char data[1]={0};
//
//	data[0] = reg;
//	i2c_master_recv(client, data ,1);
//	return data[0];

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return i2c_smbus_read_byte_data(client, reg);

}

static int gc2015_detect(struct v4l2_subdev *sd)
{
//	struct i2c_client *client = v4l2_get_subdevdata(sd);
  unsigned char gc2015_check_id[2] = {0,0};

  gc2015_check_id[0] = gc2015_read_reg(sd, 0);
  gc2015_check_id[1] = gc2015_read_reg(sd, 1);
  
  printk("gc2015_id = 0x%02x 0x%02x\n", gc2015_check_id[0], gc2015_check_id[1]);
  return 0;
}


static int GC2015_init(struct v4l2_subdev *sd, u32 val)
{
	flash_mode_auto = 0;
#ifdef CONFIG_CAMERA_CONTROLLED_BY_ARM
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, err = -EINVAL;
	unsigned char initreg[2] = {0x12, 0x80};

//  gc2015_detect(sd);
	v4l_info(client, "%s: camera initialization start %d\n", __func__, GC2015_INIT_REGS);
	err = GC2015_i2c_write(sd, initreg, 2);
	if (err < 0)
		v4l_info(client, "%s: init register set failed\n", __func__);
	mdelay(150);

	for (i = 0; i < GC2015_INIT_REGS; i++) {
		err = GC2015_i2c_write(sd, GC2015_init_reg[i],
				sizeof(GC2015_init_reg[i]));
		if (err < 0)
			v4l_info(client, "%s: %d register set failed\n",
					__func__, i);
	}

	if (err < 0) {
		v4l_err(client, "%s: camera initialization failed\n",
				__func__);
		return -EIO;	/* FIXME */
	}
#endif

#ifdef CONFIG_VIDEO_GC2015_B
	init_timer(&flash_power_timer);
  flash_power_timer.function = flash_power_timer_handler;
  //flash_power_timer.expires = jiffies + 3*HZ;
  add_timer(&flash_power_timer);
#endif /* CONFIG_VIDEO_GC2015_B */
 
  initted = 1;
	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time therefor,
 * it is not necessary to be initialized on probe time. except for version checking
 * NOTE: version checking is optional
 */
static int GC2015_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct GC2015_state *state = to_state(sd);
	struct GC2015_platform_data *pdata;

	dev_info(&client->dev, "fetching platform data\n");

	pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (!(pdata->default_width && pdata->default_height)) {
		/* TODO: assign driver default resolution */
	} else {
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;
	}

	if (!pdata->pixelformat)
		state->pix.pixelformat = DEFAULT_FMT;
	else
		state->pix.pixelformat = pdata->pixelformat;

	if (!pdata->freq)
		state->freq = 12000000;	/* 12MHz default */
	else
		state->freq = pdata->freq;

	if (!pdata->is_mipi) {
		state->is_mipi = 0;
		dev_info(&client->dev, "parallel mode\n");
	} else
		state->is_mipi = pdata->is_mipi;

	return 0;
}

static const struct v4l2_subdev_core_ops GC2015_core_ops = {
	.init = GC2015_init,	/* initializing API */
	.s_config = GC2015_s_config,	/* Fetch platform data */
	.queryctrl = GC2015_queryctrl,
	.g_ctrl = GC2015_g_ctrl,
	.s_ctrl = GC2015_s_ctrl,
};

static const struct v4l2_subdev_video_ops GC2015_video_ops = {
	.s_crystal_freq = GC2015_s_crystal_freq,
	.g_fmt = GC2015_g_fmt,
	.s_fmt = GC2015_s_fmt,
	.enum_framesizes = GC2015_enum_framesizes,
	.enum_frameintervals = GC2015_enum_frameintervals,
	.enum_fmt = GC2015_enum_fmt,
	.try_fmt = GC2015_try_fmt,
	.g_parm = GC2015_g_parm,
	.s_parm = GC2015_s_parm,
};

static const struct v4l2_subdev_ops GC2015_ops = {
	.core = &GC2015_core_ops,
	.video = &GC2015_video_ops,
};


//*********** proc ***************
static void dump_i2c_regs(void)
{
	int i;
	if(!s_i2c_client) {
		printk("s_i2c_client not ready\n");
		return;
	}

	for(i = 0; i <= 0xFF; i++) 
		printk("dump_i2c_regs: 0x%02X=0x%02X\n", i, i2c_smbus_read_byte_data(s_i2c_client, i) & 0xFF);
}

static int GC2015_writeproc(struct file *file,const char *buffer,
                           unsigned long count, void *data)
{
	return count;
}

static int GC2015_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	dump_i2c_regs();
	return 0;
}
//********************************


/*
 * ov7675_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int GC2015_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct GC2015_state *state;
	struct v4l2_subdev *sd;

	state = kzalloc(sizeof(struct GC2015_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, GC2015_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &GC2015_ops);
	dev_info(&client->dev, "GC2015 has been probed\n");

	s_i2c_client = client;
	s_proc = create_proc_entry(GC2015_PROC_NAME, 0666, &proc_root);
	if (s_proc != NULL)
	{
		s_proc->write_proc = GC2015_writeproc;
		s_proc->read_proc = GC2015_readproc;
	}


//	// i2c_smbus_read_byte_data
//	//int GC2015_id = GC2015_i2c_read(sd, 0x00);
//	extern void GC2015_power(int onoff);
//	extern s32 i2c_smbus_read_byte_data(struct i2c_client *client, u8 command);	
//
//	int time;
//	char value;
//	GC2015_power(1);
//	for (time=0; time<8; time++){
//		value = i2c_smbus_read_byte_data(client, time);
//		dev_info(&client->dev, "GC2015 Read out the reg 0x%x is 0x%x\n", 
//					time,
//					value);
//	}

	return 0;
}


static int GC2015_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	if (s_proc != NULL)
		remove_proc_entry(GC2015_PROC_NAME, &proc_root);
#ifdef CONFIG_VIDEO_GC2015_B
	del_timer(&flash_power_timer);
#endif
	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id GC2015_id[] = {
	{ GC2015_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, GC2015_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = GC2015_DRIVER_NAME,
	.probe = GC2015_probe,
	.remove = GC2015_remove,
	.id_table = GC2015_id,
};

MODULE_DESCRIPTION("GC2015 camera driver");
MODULE_AUTHOR("Ingjye Huang <Ingjye_Huang@asus.com>");
MODULE_LICENSE("GPL");

