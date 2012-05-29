/*
 * Driver for OV7675 from OV
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __GC2015_H__
#define __GC2015_H__

#define GC2015_48MHz_MCLK
#define CONFIG_CAMERA_CONTROLLED_BY_ARM

struct GC2015_reg {
	unsigned char addr;
	unsigned char val;
};

struct GC2015_regset_type {
	unsigned char *regset;
	int len;
};

/*
 * Macro
 */
//#define REGSET_LENGTH(x)	(sizeof(x)/sizeof(GC2015_reg))

/*
 * User defined commands
 */
/* S/W defined features for tune */
#define REG_DELAY	0xFF00	/* in ms */
#define REG_CMD		0xFFFF	/* Followed by command */

/* Following order should not be changed */
/* This SoC supports upto UXGA (1600*1200) */
enum {
    GC2015_PREVIEW_VGA,	/* 640*480 */
    GC2015_PREVIEW_SVGA,	/* 800*600 */
    GC2015_PREVIEW_QQVGA,	/* 160*120 */
    GC2015_PREVIEW_QCIF,	/* 176*144 */
    GC2015_PREVIEW_QVGA,	/* 320*240 */
    GC2015_PREVIEW_CIF,	/* 352*288 */
    GC2015_PREVIEW_HD720P,	/* 1280*720 */
    GC2015_PREVIEW_SXGA,	/* 1280*960 */
    GC2015_PREVIEW_UXGA,	/* 1600*1200 */
};

struct gc2015_enum_framesize {
	unsigned int index;
	unsigned int width;
	unsigned int height;
};

struct gc2015_enum_framesize gc2015_framesize_list[] = {
    { GC2015_PREVIEW_QQVGA,   160, 120 },
    { GC2015_PREVIEW_QCIF,    176, 144 },
    { GC2015_PREVIEW_QVGA,    320, 240 },
    { GC2015_PREVIEW_CIF,     352, 288 },
    { GC2015_PREVIEW_VGA,     640, 480 },
    { GC2015_PREVIEW_SVGA,    800, 600 },
    { GC2015_PREVIEW_HD720P,  1280, 720 },
    { GC2015_PREVIEW_SXGA,    1280, 960 },
    { GC2015_PREVIEW_UXGA,    1600, 1200 },
};

/*
 * Following values describe controls of camera
 * in user aspect and must be match with index of ov7675_regset[]
 * These values indicates each controls and should be used
 * to control each control
 */
enum GC2015_control {
	GC2015_INIT,
	GC2015_EV,
	GC2015_AWB,
	GC2015_MWB,
	GC2015_EFFECT,
	GC2015_CONTRAST,
	GC2015_SATURATION,
	GC2015_SHARPNESS,
};

#define GC2015_REGSET(x)	{	\
	.regset = x,			\
	.len = sizeof(x)/sizeof(GC2015_reg),}


#ifdef CONFIG_CAMERA_CONTROLLED_BY_ARM
/*
 * User tuned register setting values
 */
static unsigned char GC2015_init_reg[][2] = {

	{0xfe , 0x80}, //soft reset
	{0xfe , 0x80}, //soft reset
	{0xfe , 0x80}, //soft reset
	
	{0xfe , 0x00}, //page0
	{0x45 , 0x00}, //output_enable
	//////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////preview capture switch /////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////
	//preview
	{0x02 , 0x01}, //preview mode
	{0x2a , 0xca}, //[7]col_binning , 0x[6]even skip
	{0x48 , 0x40}, //manual_gain

	{0xfe , 0x01}, //page1
	////////////////////////////////////////////////////////////////////////
	////////////////////////// preview LSC /////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	
	{0xb0 , 0x13}, //[4]Y_LSC_en [3]lsc_compensate [2]signed_b4 [1:0]pixel array select
	{0xb1 , 0x20}, //P_LSC_red_b2
	{0xb2 , 0x20}, //P_LSC_green_b2
	{0xb3 , 0x20}, //P_LSC_blue_b2
	{0xb4 , 0x20}, //P_LSC_red_b4
	{0xb5 , 0x20}, //P_LSC_green_b4
	{0xb6 , 0x20}, //P_LSC_blue_b4
	{0xb7 , 0x00}, //P_LSC_compensate_b2
	{0xb8 , 0x80}, //P_LSC_row_center , 0x344 , 0x {0x600/2-100)/2=100
	{0xb9 , 0x80}, //P_LSC_col_center , 0x544 , 0x {0x800/2-200)/2=100

	////////////////////////////////////////////////////////////////////////
	////////////////////////// capture LSC ///////////////////////////
	////////////////////////////////////////////////////////////////////////
	{0xba , 0x13}, //[4]Y_LSC_en [3]lsc_compensate [2]signed_b4 [1:0]pixel array select
	{0xbb , 0x20}, //C_LSC_red_b2
	{0xbc , 0x20}, //C_LSC_green_b2
	{0xbd , 0x20}, //C_LSC_blue_b2
	{0xbe , 0x20}, //C_LSC_red_b4
	{0xbf , 0x20}, //C_LSC_green_b4
	{0xc0 , 0x20}, //C_LSC_blue_b4
	{0xc1 , 0x00}, //C_Lsc_compensate_b2
	{0xc2 , 0x80}, //C_LSC_row_center , 0x344 , 0x {0x1200/2-344)/2=128
	{0xc3 , 0x80}, //C_LSC_col_center , 0x544 , 0x {0x1600/2-544)/2=128

	{0xfe , 0x00}, //page0

	////////////////////////////////////////////////////////////////////////
	////////////////////////// analog configure ///////////////////////////
	////////////////////////////////////////////////////////////////////////
#ifdef CONFIG_S5PV210_S9 || defined(CONFIG_S5PV210_A9)
	{0x29 , 0x00}, //cisctl mode 1
#elif defined(CONFIG_S5PV210_A8388_V1)|| defined(CONFIG_S5PV210_A8388_V2)
	{0x29 , 0x01}, //cisctl mode 1
#else	
	{0x29 , 0x00}, //cisctl mode 1
#endif
	{0x2b , 0x06}, //cisctl mode 3	
	{0x32 , 0x0c}, //analog mode 1
	{0x33 , 0x0f}, //analog mode 2
	{0x34 , 0x00}, //[6:4]da_rsg
	
	{0x35 , 0x88}, //Vref_A25
//	{0x37 , 0x16}, //Drive Current  //四海科技使用此参数没有问题
	{0x37 , 0x03}, //Drive Current    //


	/////////////////////////////////////////////////////////////////////
	///////////////////////////ISP Related//////////////////////////////
	/////////////////////////////////////////////////////////////////////
	{0x40 , 0xff}, 
	{0x41 , 0x20}, //[5]skin_detectionenable[2]auto_gray , 0x[1]y_gamma //0x24
	{0x42 , 0x76}, //[7]auto_sa[6]auto_ee[5]auto_dndd[4]auto_lsc[3]na[2]abs , 0x[1]awb
	{0x4b , 0xea}, //[1]AWB_gain_mode , 0x1:atpregain0:atpostgain
	{0x4d , 0x03}, //[1]inbf_en
	{0x4f , 0x01}, //AEC enable

	////////////////////////////////////////////////////////////////////
	/////////////////////////// BLK  ///////////////////////////////////
	////////////////////////////////////////////////////////////////////
	{0x63 , 0x77}, //BLK mode 1
	{0x66 , 0x00}, //BLK global offset
	{0x6d , 0x04}, 
	{0x6e , 0x18}, //BLK offset submode,offset R
	{0x6f , 0x10},
	{0x70 , 0x18},
	{0x71 , 0x10},
	{0x73 , 0x03}, 


	////////////////////////////////////////////////////////////////////
	/////////////////////////// DNDD ////////////////////////////////
	////////////////////////////////////////////////////////////////////
	{0x80 , 0x07}, //[7]dn_inc_or_dec [4]zero_weight_mode[3]share [2]c_weight_adap [1]dn_lsc_mode [0]dn_b
	{0x82 , 0x08}, //DN lilat b base

	////////////////////////////////////////////////////////////////////
	/////////////////////////// EEINTP ////////////////////////////////
	////////////////////////////////////////////////////////////////////
	{0x8a , 0x7c},
	{0x8c , 0x02},
	{0x8e , 0x02},
	{0x8f , 0x48},

	/////////////////////////////////////////////////////////////////////
	/////////////////////////// CC_t ///////////////////////////////
	/////////////////////////////////////////////////////////////////////
	{0xb0 , 0x44},
	{0xb1 , 0xfe},
	{0xb2 , 0x00},
	{0xb3 , 0xf8},
	{0xb4 , 0x48},
	{0xb5 , 0xf8},
	{0xb6 , 0x00},
	{0xb7 , 0x04},
	{0xb8 , 0x00},

	/////////////////////////////////////////////////////////////////////
	/////////////////////////// GAMMA ///////////////////////////////////
	/////////////////////////////////////////////////////////////////////
	//RGB_GAMMA
	{0xbf , 0x0e},
	{0xc0 , 0x1c},
	{0xc1 , 0x34},
	{0xc2 , 0x48},
	{0xc3 , 0x5a},
	{0xc4 , 0x6b},
	{0xc5 , 0x7b},
	{0xc6 , 0x95},
	{0xc7 , 0xab},
	{0xc8 , 0xbf},
	{0xc9 , 0xce},
	{0xca , 0xd9},
	{0xcb , 0xe4},
	{0xcc , 0xec},
	{0xcd , 0xf7},
	{0xce , 0xfd},
	{0xcf , 0xff},

	/////////////////////////////////////////////////////////////////////
	/////////////////////////// YCP_t  ///////////////////////////////
	/////////////////////////////////////////////////////////////////////
	{0xd1 , 0x38}, //saturation
	{0xd2 , 0x38}, //saturation
	{0xde , 0x21}, //auto_gray

	////////////////////////////////////////////////////////////////////
	/////////////////////////// ASDE ////////////////////////////////
	////////////////////////////////////////////////////////////////////
	{0x98 , 0x30},
	{0x99 , 0xf0},
	{0x9b , 0x00},

	{0xfe , 0x01}, //page1
	////////////////////////////////////////////////////////////////////
	/////////////////////////// AEC  ////////////////////////////////
	////////////////////////////////////////////////////////////////////
	{0x10 , 0x45}, //AEC mode 1
	{0x11 , 0x32}, //[7]fix target
	{0x13 , 0x60},
	{0x17 , 0x00},
	{0x1c , 0x96},
	{0x1e , 0x11},
	{0x21 , 0xc0}, //max_post_gain
	{0x22 , 0x40}, //max_pre_gain
	{0x2d , 0x06}, //P_N_AEC_exp_level_1[12:8]
	{0x2e , 0x00}, //P_N_AEC_exp_level_1[7:0]
	{0x1e , 0x32},
	{0x33 , 0x00}, //[6:5]max_exp_level [4:0]min_exp_level

	////////////////////////////////////////////////////////////////////
	///////////////////////////  AWB  ////////////////////////////////
	////////////////////////////////////////////////////////////////////
	{0x57 , 0x40}, //number limit
	{0x5d , 0x44}, //
	{0x5c , 0x35}, //show mode,close dark_mode
	{0x5e , 0x29}, //close color temp
	{0x5f , 0x50},
	{0x60 , 0x50}, 
	{0x65 , 0xc0},

	////////////////////////////////////////////////////////////////////
	///////////////////////////  ABS  ////////////////////////////////
	////////////////////////////////////////////////////////////////////
	{0x80 , 0x82},
	{0x81 , 0x00},
	{0x83 , 0x00}, //ABS Y stretch limit

	{0xfe , 0x00},

	//////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////// Crop //////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////
	///  640X480
	{0x50 , 0x01},//out window
	{0x51 , 0x00},
	{0x52 , 0x3c},
	{0x53 , 0x00},
	{0x54 , 0x50},
	{0x55 , 0x01},
	{0x56 , 0xe0},
	{0x57 , 0x02},
	{0x58 , 0x80},

	////////////////////////////////////////////////////////////////////
	/////////////////////////// banding ////////////////////////////////
	////////////////////////////////////////////////////////////////////
#ifdef GC2015_48MHz_MCLK
	{0x05 , 0x01},//HB
	{0x06 , 0xc1},
	{0x07 , 0x00},//VB
	{0x08 , 0x40},
	
	{0xfe , 0x01},
	{0x29 , 0x01},//Anti Step
	{0x2a , 0x00},
	
	{0x2b , 0x05},//Level_0  20fps
	{0x2c , 0x00},
	{0x2d , 0x08},//Level_1  12.5fps
	{0x2e , 0x00},
	{0x2f , 0x0c},//Level_2  8.33fps
	{0x30 , 0x00},
	{0x31 , 0x10},//Level_3  6.25fps
	{0x32 , 0x00},
	{0xfe , 0x00},
#else	
	{0x05 , 0x01},//HB
	{0x06 , 0xc1},
	{0x07 , 0x00},//VB
	{0x08 , 0x40},
	
	{0xfe , 0x01},
	{0x29 , 0x00},//Anti Step
	{0x2a , 0x80},
	
	{0x2b , 0x05},//Level_0  10.00fps
	{0x2c , 0x00},
	{0x2d , 0x06},//Level_1   8.33fps
	{0x2e , 0x00},
	{0x2f , 0x08},//Level_2   6.25fps
	{0x30 , 0x00},
	{0x31 , 0x09},//Level_3   5.55fps
	{0x32 , 0x00},
	{0xfe , 0x00}
#endif
	
#ifdef GC2015_48MHz_MCLK
	{0x2c , 0x01},
	{0x32 , 0x2c}, //analog mode 1
	{0x33 , 0x08}, //analog mode 2
	{0x34 , 0x10}, //[6:4]da_rsg
#endif

	////////////////////////////////////////////////////////////////////
	///////////////////////////  OUT  ////////////////////////////////
	////////////////////////////////////////////////////////////////////
	{0x44 , 0xa0}, //YUV sequence
	{0x45 , 0x0f}, //output enable
	{0x46 , 0x03}, //sync mode
	
};
#define GC2015_INIT_REGS	(sizeof(GC2015_init_reg) / sizeof(GC2015_init_reg[0]))    


// effect
unsigned char GC2015_effect_normal [][2] = {

	{0x43 , 0x00},

};

unsigned char GC2015_effect_grayscale [][2] = {
	
	{0x43 , 0x02},
	{0xda , 0x00},
	{0xdb , 0x00},

};

unsigned char GC2015_effect_sepia [][2] = {
	
	{0x43 , 0x02},
	{0xda , 0xd0},
	{0xdb , 0x28},

};

unsigned char GC2015_effect_sepia_green  [][2] = {
	
	{0x43 , 0x02},
	{0xda , 0xc0},
	{0xdb , 0xc0},
		
};

unsigned char GC2015_effect_sepia_blue[][2] = {
	
	{0x43 , 0x02},
	{0xda , 0x50},
	{0xdb , 0xe0},

};

unsigned char GC2015_effect_color_inv [][2] = {
	
	{0x43 , 0x01},

};

unsigned char GC2015_effect_gray_inv [][2] = {

};


unsigned char GC2015_effect_embossment [][2] = {

};

unsigned char GC2015_effect_sketch [][2] = {

};

unsigned char GC2015_white_balance_auto  [][2] = {
	
	// eable AWB
};

unsigned char GC2015_white_balance_cloud [][2] = {
	// disable AWB
	{0x7a , 0x8c},
	{0x7b , 0x50},
	{0x7c , 0x40},

};

unsigned char GC2015_white_balance_daylight [][2] = {
	// disable AWB
	{0x7a , 0x74},
	{0x7b , 0x52},
	{0x7c , 0x40},	
};

unsigned char GC2015_white_balance_incandescence [][2] = {
	// disable AWB
	{0x7a , 0x48},
	{0x7b , 0x40},
	{0x7c , 0x5c},	
};

unsigned char GC2015_white_balance_fluorescent [][2] = {
	// disable AWB
	{0x7a , 0x40},
	{0x7b , 0x42},
	{0x7c , 0x50},	
};

unsigned char GC2015_capture_uxga [][2] = {
  {0xfe , 0x00},
	{0x48 , 0x68},
	
	{0x02 , 0x00}, 
	{0x2a , 0x0a}, 

	{0x55 , 0x04},  //height
	{0x56 , 0xb0},	
	{0x57 , 0x06},  //width
	{0x58 , 0x40},	
};

unsigned char GC2015_capture_sxga [][2] = {
  {0xfe , 0x00},
	{0x48 , 0x68},
	
	{0x02 , 0x00}, 
	{0x2a , 0x0a}, 

	{0x55 , 0x03},  //height
	{0x56 , 0xc0},	
	{0x57 , 0x05},  //width
	{0x58 , 0x00},	
};

unsigned char GC2015_preview [][2] = {
  {0xfe , 0x00},
	{0x48 , 0x40},
	
	{0x02 , 0x01}, 
	{0x2a , 0xca}, 

	{0x55 , 0x01},
	{0x56 , 0xe0},
	{0x57 , 0x02},
	{0x58 , 0x80},
};


#endif     
#endif
