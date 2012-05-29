/*
 * wm8960.c  --  WM8960 ALSA SoC Audio driver
 *
 * Author: Liam Girdwood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/wm8960.h>

#include "wm8960.h"

#define AUDIO_NAME "wm8960"
//fighter++
extern volatile int earphone_mic;
struct snd_soc_codec *cpy_codec;
//fighter--
struct snd_soc_codec_device soc_codec_dev_wm8960;

/* R25 - Power 1 */
#define WM8960_VMID_MASK 0x180
#define WM8960_VREF      0x40

/* R26 - Power 2 */
#define WM8960_PWR2_LOUT1	0x40
#define WM8960_PWR2_ROUT1	0x20
#define WM8960_PWR2_OUT3	0x02

/* R28 - Anti-pop 1 */
#define WM8960_POBCTRL   0x80
#define WM8960_BUFDCOPEN 0x10
#define WM8960_BUFIOEN   0x08
#define WM8960_SOFT_ST   0x04
#define WM8960_HPSTBY    0x01

/* R29 - Anti-pop 2 */
#define WM8960_DISOP     0x40
#define WM8960_DRES_MASK 0x30

/*
 * wm8960 register cache
 * We can't read the WM8960 register space when we are
 * using 2 wire for device control, so we cache them instead.
 */
static const u16 wm8960_reg[WM8960_CACHEREGNUM] = {
	0x0097, 0x0097, 0x0000, 0x0000,
	0x0000, 0x0008, 0x0000, 0x000a,
	0x01c0, 0x0000, 0x00ff, 0x00ff,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x007b, 0x0100, 0x0032,
	0x0000, 0x00c3, 0x00c3, 0x01c0,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0100, 0x0100, 0x0050, 0x0050,
	0x0050, 0x0050, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0040, 0x0000,
	0x0000, 0x0050, 0x0050, 0x0000,
	0x0002, 0x0037, 0x004d, 0x0080,
	0x0008, 0x0031, 0x0026, 0x00e9,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef void (*select_route)(struct snd_soc_codec *);
typedef void (*select_mic_route)(struct snd_soc_codec *);

enum playback_path{ PLAYBACK_OFF, SPK, HP, BT };

void wm8960_set_off(struct snd_soc_codec *codec);
void wm8960_set_playback_headset(struct snd_soc_codec *codec);
void wm8960_set_playback_speaker(struct snd_soc_codec *codec);
void wm8960_set_playback_bluetooth(struct snd_soc_codec *codec);

void wm8960_record_main_mic( struct snd_soc_codec *codec);
void wm8960_record_headset_mic( struct snd_soc_codec *codec);
void wm8960_record_bluetooth(struct snd_soc_codec *codec);
void wm8960_record_off(struct snd_soc_codec *codec);

static const char *playback_path[] = { "OFF", "SPK", "HP", "BT" };
static const char *mic_path[] = { "Main Mic", "Hands Free Mic", "BT Sco Mic", "MIC OFF"};
//static const char *fmradio_path[]   = { "FMR_OFF", "FMR_SPK", "FMR_HP", "FMR_SPK_MIX", "FMR_HP_MIX", "FMR_DUAL_MIX"};
static const char *fmradio_path[] = { "FMR_OFF", "FMR_SPK", "FMR_HP"};

enum fmradio_audio_path { FMR_OFF, FMR_SPK, FMR_HP};


select_route universal_wm8960_playback_paths[] = 
{
	wm8960_set_off, 
	wm8960_set_playback_speaker,
	wm8960_set_playback_headset,
	wm8960_set_playback_bluetooth
};

select_mic_route universal_wm8960_mic_paths[] = 
{
	wm8960_record_main_mic,
	wm8960_record_headset_mic,
	wm8960_record_bluetooth,
	wm8960_record_off
};

void dump_reg(struct snd_soc_codec *codec)
{
  int i=0;
  for(i=0; i<0x38; i++)
    printk("0x%02x = 0x%04x\n", i,  snd_soc_read(codec, i));
}

void wm8960_set_off(struct snd_soc_codec *codec)
{
//  printk("%s\n", __FUNCTION__);
}
int temp = 1;
void wm8960_set_playback_speaker(struct snd_soc_codec *codec)
{
	unsigned short val;
  printk("%s\n", __FUNCTION__);

	//Disable HP
	val = snd_soc_read(codec, WM8960_POWER2);
	val &= ~(0x3<<5);
	snd_soc_write(codec, WM8960_POWER2, val);

	//Disable speaker for preventing pop up noise.
	val = snd_soc_read(codec, WM8960_POWER2);
	val &= ~(0x3<<3);
	snd_soc_write(codec, WM8960_POWER2, val);

	// Speaker Volume Control
	val = snd_soc_read(codec, WM8960_LOUT2 );
	val &= ~(0x7F);
	val |= 0x7F;
	snd_soc_write(codec, WM8960_LOUT2 ,val);
	val = snd_soc_read(codec,WM8960_ROUT2 );
	val &= ~(0x7F);
	val |= 0x7F;
	snd_soc_write(codec, WM8960_ROUT2 ,val);

  //set DAC Volume
//	val = snd_soc_read(codec, WM8960_LDAC );
//	val |= 0x01FF;
	val = 0x01F0;
	snd_soc_write(codec, WM8960_LDAC ,val);
	
//	val = snd_soc_read(codec, WM8960_RDAC );
//	val |= 0x01FF;
	val = 0x01F0;
	snd_soc_write(codec, WM8960_RDAC ,val);

  //enable DAC power
	val = snd_soc_read(codec, WM8960_POWER2 );
	val |= ((0x1<<7) | (0x1<<8));
	snd_soc_write(codec, WM8960_POWER2 ,val);

  //enable mix
	val = snd_soc_read(codec, WM8960_LOUTMIX );
	val |= (0x1<<8);
	snd_soc_write(codec, WM8960_LOUTMIX ,val);
	val = snd_soc_read(codec, WM8960_ROUTMIX );
	val |= (0x1<<8);
	snd_soc_write(codec, WM8960_ROUTMIX ,val);

  //enable mix power
	val = snd_soc_read(codec, WM8960_POWER3 );
	val |= (0x3<<2);
	snd_soc_write(codec, WM8960_POWER3 ,val);

	//Enbale vmid
	val = snd_soc_read(codec, WM8960_POWER1);
	val &= ~(0x3<<7);
	val |= (0x1<<7);
	snd_soc_write(codec, WM8960_POWER1, val);

	//Enbale speaker
	val = snd_soc_read(codec, WM8960_POWER2);
	val |= (0x3<<3);
	snd_soc_write(codec, WM8960_POWER2, val);

	//Enbale D Class
	val = snd_soc_read(codec, WM8960_CLASSD1);
	val |= (0x3<<6);
	snd_soc_write(codec, WM8960_CLASSD1, val);

	val = snd_soc_read(codec, WM8960_CLASSD3);
	val &= ~(0x3F<<0);
	val |= (0x3F<<0);
	snd_soc_write(codec, WM8960_CLASSD3, val);
	
	if(temp)
	{
			cpy_codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
			if (cpy_codec == NULL)
				return -ENOMEM;
			memcpy(cpy_codec,codec,sizeof(struct snd_soc_codec));
		 	temp = 0;
	}
//  printk("%s done\n", __FUNCTION__);
}

void wm8960_set_playback_headset(struct snd_soc_codec *codec)
{
	unsigned short val;

printk("%s\n", __FUNCTION__);

	//Disbale D Class
	val = snd_soc_read(codec, WM8960_CLASSD1);
	val &= ~(0x3<<6);
	snd_soc_write(codec, WM8960_CLASSD1, val);

	//Disable speaker
	val = snd_soc_read(codec, WM8960_POWER2);
	val &= ~(0x3<<3);
	snd_soc_write(codec, WM8960_POWER2, val);

	//Disable HP
	val = snd_soc_read(codec, WM8960_POWER2);
	val &= ~(0x3<<5);
	snd_soc_write(codec, WM8960_POWER2, val);

	//HP Volume Control
	val = snd_soc_read(codec, WM8960_LOUT1 );
	val &= ~(0x7F);
	val |= 0x7F;
	snd_soc_write(codec, WM8960_LOUT1 ,val);
	val = snd_soc_read(codec,WM8960_ROUT1 );
	val &= ~(0x7F);
	val |= 0x7F;
	snd_soc_write(codec, WM8960_ROUT1 ,val);

  //set DAC Volume
	val = snd_soc_read(codec, WM8960_LDAC );
	val = 0x01dF;
	snd_soc_write(codec, WM8960_LDAC ,val);
	val = snd_soc_read(codec, WM8960_RDAC );
	val = 0x01dF;
	snd_soc_write(codec, WM8960_RDAC ,val);

  //enable DAC power
	val = snd_soc_read(codec, WM8960_POWER2 );
	val |= ((0x1<<7) | (0x1<<8));
	snd_soc_write(codec, WM8960_POWER2 ,val);

  //enable mix
	val = snd_soc_read(codec, WM8960_LOUTMIX );
	val |= (0x1<<8);
	snd_soc_write(codec, WM8960_LOUTMIX ,val);
	val = snd_soc_read(codec, WM8960_ROUTMIX );
	val |= (0x1<<8);
	snd_soc_write(codec, WM8960_ROUTMIX ,val);

  //enable mix power
	val = snd_soc_read(codec, WM8960_POWER3 );
	val |= (0x3<<2);
	snd_soc_write(codec, WM8960_POWER3 ,val);

	//Enbale vmid
	val = snd_soc_read(codec, WM8960_POWER1);
	val &= ~(0x3<<7);
	val |= (0x1<<7);
	snd_soc_write(codec, WM8960_POWER1, val);

	//Enable HP
	val = snd_soc_read(codec, WM8960_POWER2);
	val |= (0x3<<5);
	snd_soc_write(codec, WM8960_POWER2, val);
}

void wm8960_set_playback_bluetooth(struct snd_soc_codec *codec)
{
}

static int wm8960_get_playback_path(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
//  printk("%s\n", __FUNCTION__);
	return 0;
}

static int wm8960_set_playback_path(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int path_num = ucontrol->value.integer.value[0];
  universal_wm8960_playback_paths[path_num](codec);
	return 0;
}

void wm8960_record_main_mic(struct snd_soc_codec *codec)
{
  unsigned short val;
	printk("%s+++++++++++++++++++++++\n", __FUNCTION__);
	//Disbale FM(LIN3 RIN3)
	val = snd_soc_read(codec, WM8960_LINPATH);
	val &= ~(0x1<<7);
	snd_soc_write(codec, WM8960_LINPATH, val);
  
  //Enable MIC(LIN2)
	val = snd_soc_read(codec, WM8960_LINPATH);
	val |= ((0x1<<6) | (0x1<<3));
	snd_soc_write(codec, WM8960_LINPATH, val);
	
  //unmute LINPUT and set vomule
	val = snd_soc_read(codec, WM8960_LINVOL);
	val &= ~(0x1<<7);
	val |= ((0x1<<8) | (0x3F<<0));
	snd_soc_write(codec, WM8960_LINVOL, val);

  //enable ADC
 	val = snd_soc_read(codec, WM8960_POWER1);
	val |= (0x1<<3);
	val |= ((0x1<<3) | (0x1<<5));
	snd_soc_write(codec, WM8960_POWER1, val);
	 
  //LEFT ADC vomule
 	val = snd_soc_read(codec, WM8960_LADC);
	val = 0x01F8;
	snd_soc_write(codec, WM8960_LADC, val);

  //RIGHT ADC vomule
 	val = snd_soc_read(codec, WM8960_RADC);
	val |= 0x100;
	snd_soc_write(codec, WM8960_RADC, val);

  //enable ALC 
	val = snd_soc_read(codec, WM8960_ALC1);
	val |= (0x3<<7);
	snd_soc_write(codec, WM8960_ALC1, val);

  //ADC DATA SOURCE
 	val = snd_soc_read(codec, WM8960_ADDCTL1);
	val &= ~(0x3<<2);
	val |= 0x1<<2;
	snd_soc_write(codec, WM8960_ADDCTL1, val);	

  //enable micbas
 	val = snd_soc_read(codec, WM8960_POWER1);
	val |= (0x1<<1);
	snd_soc_write(codec, WM8960_POWER1, val);

	//Enbale vmid
	val = snd_soc_read(codec, WM8960_POWER1);
	val &= ~(0x3<<7);
	val |= (0x1<<7);
	snd_soc_write(codec, WM8960_POWER1, val);
	
	//disable RINPUT adc
	val = snd_soc_read(codec, WM8960_POWER1);
	val &= ~(0x1<<2);
	snd_soc_write(codec, WM8960_POWER1, val);

	//Disbale RINPUT2
	val = snd_soc_read(codec, WM8960_RINPATH);
	val &= ~(0x1<<6);
	snd_soc_write(codec, WM8960_RINPATH, val);

	//Disbale RINPUT2
	val = snd_soc_read(codec, WM8960_POWER3);
	val &= ~(0x1<<4);
	snd_soc_write(codec, WM8960_POWER3, val);

//  val = snd_soc_read(codec, WM8960_ADDCTL1);
//printk("0x17 = 0x%03x\n", val); //1c7
//	val = 0x01c4;
//	snd_soc_write(codec, WM8960_ADDCTL1, val);	
}

void wm8960_record_headset_mic( struct snd_soc_codec *codec)
{
	unsigned short val;
	printk("%s+++++++++++++++++++\n", __FUNCTION__);
  if(earphone_mic){	
  	printk("head mic++++++++++\n");
  	//Disbale FM(LIN3 RIN3)
  	val = snd_soc_read(codec, WM8960_LINPATH);
  	val &= ~(0x1<<7);
  	snd_soc_write(codec, WM8960_LINPATH, val);
    
    //Enable HP_MIC(RIN2)
  	val = snd_soc_read(codec, WM8960_RINPATH);
  	val |= ((0x1<<6) | (0x1<<3));
  	snd_soc_write(codec, WM8960_RINPATH, val);
  	
    //unmute LINPUT and set vomule
  	val = snd_soc_read(codec, WM8960_RINVOL);
  	val &= ~(0x1<<7);
  	val |= ((0x1<<8) | (0x3F<<0));
  	snd_soc_write(codec, WM8960_RINVOL, val);
  
    //enable ADC
   	val = snd_soc_read(codec, WM8960_POWER1);
  	val |= ((0x1<<2) | (0x1<<4));
  	snd_soc_write(codec, WM8960_POWER1, val);
  	 
    //LEFT ADC vomule
   	val = snd_soc_read(codec, WM8960_LADC);
  	val = 0x0100;
  	snd_soc_write(codec, WM8960_LADC, val);
  
    //RIGHT ADC vomule
   	val = snd_soc_read(codec, WM8960_RADC);
  	val |= 0x1FF;
  	snd_soc_write(codec, WM8960_RADC, val);
  
    //enable ALC 
  	val = snd_soc_read(codec, WM8960_ALC1);
  	val |= (0x3<<7);
  	snd_soc_write(codec, WM8960_ALC1, val);
  
    //ADC DATA SOURCE
   	val = snd_soc_read(codec, WM8960_ADDCTL1);
  	val &= ~(0x3<<2);
  	val |= 0x2<<2;
  	snd_soc_write(codec, WM8960_ADDCTL1, val);	
  
    //enable micbas
   	val = snd_soc_read(codec, WM8960_POWER1);
  	val |= (0x1<<1);
  	snd_soc_write(codec, WM8960_POWER1, val);
  
  	//Enbale vmid
  	val = snd_soc_read(codec, WM8960_POWER1);
  	val &= ~(0x3<<7);
  	val |= (0x1<<7);
  	snd_soc_write(codec, WM8960_POWER1, val);
  	
  	//disable LINPUT adc
  	val = snd_soc_read(codec, WM8960_POWER1);
  	val &= ~(0x1<<3);
  	snd_soc_write(codec, WM8960_POWER1, val);
  
  	//Disbale LINPUT2
  	val = snd_soc_read(codec, WM8960_LINPATH);
  	val &= ~(0x1<<6);
  	snd_soc_write(codec, WM8960_LINPATH, val);
  
  	//Disbale LINPUT2
  	val = snd_soc_read(codec, WM8960_POWER3);
  	val &= ~(0x1<<5);
  	snd_soc_write(codec, WM8960_POWER3, val);
	}
	else{
	  wm8960_record_main_mic(codec);
	}
}

void wm8960_record_bluetooth(struct snd_soc_codec *codec)
{
}

void wm8960_record_off(struct snd_soc_codec *codec)
{
//printk("%s++++++++++++++++++++\n", __FUNCTION__);

//  unsigned short val;
//  //disable adc and micbas
// 	val = snd_soc_read(codec, WM8960_POWER1);
//	val &= ~((0x1<<1) | (0x1<<3) | (0x1<<2));
//	snd_soc_write(codec, WM8960_POWER1, val);    
}

static int wm8960_set_mic_path(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int path_num = ucontrol->value.integer.value[0];

  universal_wm8960_mic_paths[path_num](codec);
  return 0;
}

static int wm8960_get_mic_path(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
  return 0;
}

void wm8960_disable_fmradio_path(struct snd_soc_codec *codec, enum fmradio_audio_path path)
{
  unsigned short val;
  //disable bypass
	val = snd_soc_read(codec, WM8960_LOUTMIX );
	val &= ~(0x1<<7);
	snd_soc_write(codec, WM8960_LOUTMIX ,val);
	val = snd_soc_read(codec, WM8960_ROUTMIX );
	val &= ~(0x1<<7);
	snd_soc_write(codec, WM8960_ROUTMIX ,val);
}

void wm8960_set_fmradio_speaker(struct snd_soc_codec *codec)
{
  unsigned short val;

//printk("wm8960_set_fmradio_speaker\n");
	//munte  LINPUT and set vomule
	val = snd_soc_read(codec, WM8960_LINVOL);
	val &= ~(0x1<<7);
	snd_soc_write(codec, WM8960_LINVOL, val);
	
  //unmute RINPUT and set vomule
	val = snd_soc_read(codec, WM8960_RINVOL);
	val &= ~(0x1<<7);
	snd_soc_write(codec, WM8960_RINVOL, val);

  //disable mix power
	val = snd_soc_read(codec, WM8960_POWER3 );
	val &= ~(0x3<<4);
	snd_soc_write(codec, WM8960_POWER3 ,val);

  //disable PGAmix
	val = snd_soc_read(codec, WM8960_LINPATH );
	val &= ~(0x1<<7);
	snd_soc_write(codec, WM8960_LINPATH ,val);
	val = snd_soc_read(codec, WM8960_RINPATH );
	val &= ~(0x1<<7);
	snd_soc_write(codec, WM8960_RINPATH ,val);

  //bypass boost
//	val = snd_soc_read(codec, WM8960_INBMIX1 );
//	val |= (0x7<<4);
//	snd_soc_write(codec, WM8960_INBMIX1 ,val);
//	val = snd_soc_read(codec, WM8960_INBMIX2 );
//	val |= (0x7<<4);
//	snd_soc_write(codec, WM8960_INBMIX2 ,val);

  //enable bypass
	val = snd_soc_read(codec, WM8960_LOUTMIX );
	val &= ~(0x1<<8);
	val |= (0xF<<4);
	snd_soc_write(codec, WM8960_LOUTMIX ,val);
	val = snd_soc_read(codec, WM8960_ROUTMIX );
	val &= ~(0x1<<8);
	val |= (0xF<<4);
	snd_soc_write(codec, WM8960_ROUTMIX ,val);
  
  //enable power
	val = snd_soc_read(codec, WM8960_POWER1 );
	val |= (0x3<<4);
	snd_soc_write(codec, WM8960_POWER1 ,val);

}

void wm8960_set_fmradio_headset(struct snd_soc_codec *codec)
{
//  printk("wm8960_set_fmradio_headset\n");
  wm8960_set_fmradio_speaker(codec);
}

static int wm8960_get_fmradio_path(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int wm8960_set_fmradio_path(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_enum *mc =	(struct soc_enum *)kcontrol->private_value;
//	struct wm8960_priv *wm8960 = codec->drvdata;
	
	int path_num = ucontrol->value.integer.value[0];	
	
	if(strcmp( mc->texts[path_num], fmradio_path[path_num]) )
	{		
		printk("Unknown path %s\n", mc->texts[path_num] );		
	}
	
//	if(path_num == (int)(wm8960->fmradio_path))
//	{
//		printk("%s is already set. skip to set path.. \n", mc->texts[path_num]);
//		return 0;
//	}
		
	switch(path_num)
	{
		case FMR_OFF:
			printk("Switching off output path\n");
			wm8960_disable_fmradio_path(codec, FMR_OFF);
			break;
			
		case FMR_SPK:
			printk("routing  fmradio path to  %s \n", mc->texts[path_num] );
			wm8960_set_fmradio_speaker(codec);
			break;

		case FMR_HP:
			printk("routing  fmradio path to  %s \n", mc->texts[path_num] );
			wm8960_set_fmradio_headset(codec);
			break;

		default:
			printk("The audio path[%d] does not exists!! \n", path_num);
			return -ENODEV;
			break;
	}
	
	return 0;
}

static const struct soc_enum path_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(playback_path),playback_path),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mic_path),mic_path),
//	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(voicecall_path),voicecall_path),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fmradio_path),fmradio_path),
//	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(codec_tuning_control), codec_tuning_control),
//	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(codec_status_control), codec_status_control),
//	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(voice_record_path), voice_record_path),
//	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(call_recording_channel), call_recording_channel),
};

static const struct snd_kcontrol_new wm8960_snd_controls_ext[] = {
  /* Path Control */
  SOC_ENUM_EXT("Playback Path", path_control_enum[0], wm8960_get_playback_path, wm8960_set_playback_path),
	SOC_ENUM_EXT("Capture MIC Path", path_control_enum[1], wm8960_get_mic_path, wm8960_set_mic_path),
//	SOC_ENUM_EXT("Voice Call Path", path_control_enum[1], wm8960_get_call_path, wm8960_set_call_path),
	SOC_ENUM_EXT("FM Radio Path", path_control_enum[2], wm8960_get_fmradio_path, wm8960_set_fmradio_path),
//	SOC_ENUM_EXT("Codec Tuning", path_control_enum[4], wm8960_get_codec_tuning, wm8960_set_codec_tuning),
//	SOC_ENUM_EXT("Codec Status", path_control_enum[5], wm8960_get_codec_status, wm8960_set_codec_status),
//	SOC_ENUM_EXT("Voice Call Recording", path_control_enum[6], wm8960_get_voice_call_recording, wm8960_set_voice_call_recording),
//	SOC_ENUM_EXT("Recording Channel", path_control_enum[7], wm8960_get_voice_recording_ch, wm8960_set_voice_recording_ch),
} ;//snd_ctrls


////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct wm8960_priv {
	u16 reg_cache[WM8960_CACHEREGNUM];
	struct snd_soc_codec codec;
	struct snd_soc_dapm_widget *lout1;
	struct snd_soc_dapm_widget *rout1;
	struct snd_soc_dapm_widget *out3;
	enum fmradio_audio_path fmradio_path;
};

#define wm8960_reset(c)	snd_soc_write(c, WM8960_RESET, 0)

/* enumerated controls */
static const char *wm8960_deemph[] = {"None", "32Khz", "44.1Khz", "48Khz"};
static const char *wm8960_polarity[] = {"No Inversion", "Left Inverted",
	"Right Inverted", "Stereo Inversion"};
static const char *wm8960_3d_upper_cutoff[] = {"High", "Low"};
static const char *wm8960_3d_lower_cutoff[] = {"Low", "High"};
static const char *wm8960_alcfunc[] = {"Off", "Right", "Left", "Stereo"};
static const char *wm8960_alcmode[] = {"ALC", "Limiter"};

static const struct soc_enum wm8960_enum[] = {
	SOC_ENUM_SINGLE(WM8960_DACCTL1, 1, 4, wm8960_deemph),
	SOC_ENUM_SINGLE(WM8960_DACCTL1, 5, 4, wm8960_polarity),
	SOC_ENUM_SINGLE(WM8960_DACCTL2, 5, 4, wm8960_polarity),
	SOC_ENUM_SINGLE(WM8960_3D, 6, 2, wm8960_3d_upper_cutoff),
	SOC_ENUM_SINGLE(WM8960_3D, 5, 2, wm8960_3d_lower_cutoff),
	SOC_ENUM_SINGLE(WM8960_ALC1, 7, 4, wm8960_alcfunc),
	SOC_ENUM_SINGLE(WM8960_ALC3, 8, 2, wm8960_alcmode),
};

static const DECLARE_TLV_DB_SCALE(adc_tlv, -9700, 50, 0);
static const DECLARE_TLV_DB_SCALE(dac_tlv, -12700, 50, 1);
static const DECLARE_TLV_DB_SCALE(bypass_tlv, -2100, 300, 0);
static const DECLARE_TLV_DB_SCALE(out_tlv, -12100, 100, 1);

static const struct snd_kcontrol_new wm8960_snd_controls[] = {
SOC_DOUBLE_R_TLV("Capture Volume", WM8960_LINVOL, WM8960_RINVOL,
		 0, 63, 0, adc_tlv),
SOC_DOUBLE_R("Capture Volume ZC Switch", WM8960_LINVOL, WM8960_RINVOL,
	6, 1, 0),
SOC_DOUBLE_R("Capture Switch", WM8960_LINVOL, WM8960_RINVOL,
	7, 1, 0),

SOC_DOUBLE_R_TLV("Playback Volume", WM8960_LDAC, WM8960_RDAC,
		 0, 255, 0, dac_tlv),

SOC_DOUBLE_R_TLV("Headphone Playback Volume", WM8960_LOUT1, WM8960_ROUT1,
		 0, 127, 0, out_tlv),
SOC_DOUBLE_R("Headphone Playback ZC Switch", WM8960_LOUT1, WM8960_ROUT1,
	7, 1, 0),

SOC_DOUBLE_R_TLV("Speaker Playback Volume", WM8960_LOUT2, WM8960_ROUT2,
		 0, 127, 0, out_tlv),
SOC_DOUBLE_R("Speaker Playback ZC Switch", WM8960_LOUT2, WM8960_ROUT2,
	7, 1, 0),
SOC_SINGLE("Speaker DC Volume", WM8960_CLASSD3, 3, 5, 0),
SOC_SINGLE("Speaker AC Volume", WM8960_CLASSD3, 0, 5, 0),

SOC_SINGLE("PCM Playback -6dB Switch", WM8960_DACCTL1, 7, 1, 0),
SOC_ENUM("ADC Polarity", wm8960_enum[1]),
SOC_ENUM("Playback De-emphasis", wm8960_enum[0]),
SOC_SINGLE("ADC High Pass Filter Switch", WM8960_DACCTL1, 0, 1, 0),

SOC_ENUM("DAC Polarity", wm8960_enum[2]),

SOC_ENUM("3D Filter Upper Cut-Off", wm8960_enum[3]),
SOC_ENUM("3D Filter Lower Cut-Off", wm8960_enum[4]),
SOC_SINGLE("3D Volume", WM8960_3D, 1, 15, 0),
SOC_SINGLE("3D Switch", WM8960_3D, 0, 1, 0),

SOC_ENUM("ALC Function", wm8960_enum[5]),
SOC_SINGLE("ALC Max Gain", WM8960_ALC1, 4, 7, 0),
SOC_SINGLE("ALC Target", WM8960_ALC1, 0, 15, 1),
SOC_SINGLE("ALC Min Gain", WM8960_ALC2, 4, 7, 0),
SOC_SINGLE("ALC Hold Time", WM8960_ALC2, 0, 15, 0),
SOC_ENUM("ALC Mode", wm8960_enum[6]),
SOC_SINGLE("ALC Decay", WM8960_ALC3, 4, 15, 0),
SOC_SINGLE("ALC Attack", WM8960_ALC3, 0, 15, 0),

SOC_SINGLE("Noise Gate Threshold", WM8960_NOISEG, 3, 31, 0),
SOC_SINGLE("Noise Gate Switch", WM8960_NOISEG, 0, 1, 0),

SOC_DOUBLE_R("ADC PCM Capture Volume", WM8960_LINPATH, WM8960_RINPATH,
	0, 127, 0),

SOC_SINGLE_TLV("Left Output Mixer Boost Bypass Volume",
	       WM8960_BYPASS1, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Left Output Mixer LINPUT3 Volume",
	       WM8960_LOUTMIX, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Right Output Mixer Boost Bypass Volume",
	       WM8960_BYPASS2, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Right Output Mixer RINPUT3 Volume",
	       WM8960_ROUTMIX, 4, 7, 1, bypass_tlv),
};

static const struct snd_kcontrol_new wm8960_lin_boost[] = {
SOC_DAPM_SINGLE("LINPUT2 Switch", WM8960_LINPATH, 6, 1, 0),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM8960_LINPATH, 7, 1, 0),
SOC_DAPM_SINGLE("LINPUT1 Switch", WM8960_LINPATH, 8, 1, 0),
};

static const struct snd_kcontrol_new wm8960_lin[] = {
SOC_DAPM_SINGLE("Boost Switch", WM8960_LINPATH, 3, 1, 0),
};

static const struct snd_kcontrol_new wm8960_rin_boost[] = {
SOC_DAPM_SINGLE("RINPUT2 Switch", WM8960_RINPATH, 6, 1, 0),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM8960_RINPATH, 7, 1, 0),
SOC_DAPM_SINGLE("RINPUT1 Switch", WM8960_RINPATH, 8, 1, 0),
};

static const struct snd_kcontrol_new wm8960_rin[] = {
SOC_DAPM_SINGLE("Boost Switch", WM8960_RINPATH, 3, 1, 0),
};

static const struct snd_kcontrol_new wm8960_loutput_mixer[] = {
SOC_DAPM_SINGLE("PCM Playback Switch", WM8960_LOUTMIX, 8, 1, 0),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM8960_LOUTMIX, 7, 1, 0),
SOC_DAPM_SINGLE("Boost Bypass Switch", WM8960_BYPASS1, 7, 1, 0),
};

static const struct snd_kcontrol_new wm8960_routput_mixer[] = {
SOC_DAPM_SINGLE("PCM Playback Switch", WM8960_ROUTMIX, 8, 1, 0),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM8960_ROUTMIX, 7, 1, 0),
SOC_DAPM_SINGLE("Boost Bypass Switch", WM8960_BYPASS2, 7, 1, 0),
};

static const struct snd_kcontrol_new wm8960_mono_out[] = {
SOC_DAPM_SINGLE("Left Switch", WM8960_MONOMIX1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Switch", WM8960_MONOMIX2, 7, 1, 0),
};

static const struct snd_soc_dapm_widget wm8960_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("LINPUT1"),
SND_SOC_DAPM_INPUT("RINPUT1"),
SND_SOC_DAPM_INPUT("LINPUT2"),
SND_SOC_DAPM_INPUT("RINPUT2"),
SND_SOC_DAPM_INPUT("LINPUT3"),
SND_SOC_DAPM_INPUT("RINPUT3"),

SND_SOC_DAPM_MICBIAS("MICB", WM8960_POWER1, 1, 0),

SND_SOC_DAPM_MIXER("Left Boost Mixer", WM8960_POWER1, 5, 0,
		   wm8960_lin_boost, ARRAY_SIZE(wm8960_lin_boost)),
SND_SOC_DAPM_MIXER("Right Boost Mixer", WM8960_POWER1, 4, 0,
		   wm8960_rin_boost, ARRAY_SIZE(wm8960_rin_boost)),

SND_SOC_DAPM_MIXER("Left Input Mixer", WM8960_POWER3, 5, 0,
		   wm8960_lin, ARRAY_SIZE(wm8960_lin)),
SND_SOC_DAPM_MIXER("Right Input Mixer", WM8960_POWER3, 4, 0,
		   wm8960_rin, ARRAY_SIZE(wm8960_rin)),

SND_SOC_DAPM_ADC("Left ADC", "Capture", WM8960_POWER1, 3, 0),
SND_SOC_DAPM_ADC("Right ADC", "Capture", WM8960_POWER1, 2, 0),

SND_SOC_DAPM_DAC("Left DAC", "Playback", WM8960_POWER2, 8, 0),
SND_SOC_DAPM_DAC("Right DAC", "Playback", WM8960_POWER2, 7, 0),

SND_SOC_DAPM_MIXER("Left Output Mixer", WM8960_POWER3, 3, 0,
	&wm8960_loutput_mixer[0],
	ARRAY_SIZE(wm8960_loutput_mixer)),
SND_SOC_DAPM_MIXER("Right Output Mixer", WM8960_POWER3, 2, 0,
	&wm8960_routput_mixer[0],
	ARRAY_SIZE(wm8960_routput_mixer)),

SND_SOC_DAPM_PGA("LOUT1 PGA", WM8960_POWER2, 6, 0, NULL, 0),
SND_SOC_DAPM_PGA("ROUT1 PGA", WM8960_POWER2, 5, 0, NULL, 0),

SND_SOC_DAPM_PGA("Left Speaker PGA", WM8960_POWER2, 4, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Speaker PGA", WM8960_POWER2, 3, 0, NULL, 0),

SND_SOC_DAPM_PGA("Right Speaker Output", WM8960_CLASSD1, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left Speaker Output", WM8960_CLASSD1, 6, 0, NULL, 0),

SND_SOC_DAPM_OUTPUT("SPK_LP"),
SND_SOC_DAPM_OUTPUT("SPK_LN"),
SND_SOC_DAPM_OUTPUT("HP_L"),
SND_SOC_DAPM_OUTPUT("HP_R"),
SND_SOC_DAPM_OUTPUT("SPK_RP"),
SND_SOC_DAPM_OUTPUT("SPK_RN"),
SND_SOC_DAPM_OUTPUT("OUT3"),
};

static const struct snd_soc_dapm_widget wm8960_dapm_widgets_out3[] = {
SND_SOC_DAPM_MIXER("Mono Output Mixer", WM8960_POWER2, 1, 0,
	&wm8960_mono_out[0],
	ARRAY_SIZE(wm8960_mono_out)),
};

/* Represent OUT3 as a PGA so that it gets turned on with LOUT1/ROUT1 */
static const struct snd_soc_dapm_widget wm8960_dapm_widgets_capless[] = {
SND_SOC_DAPM_PGA("OUT3 VMID", WM8960_POWER2, 1, 0, NULL, 0),
};

static const struct snd_soc_dapm_route audio_paths[] = {
	{ "Left Boost Mixer", "LINPUT1 Switch", "LINPUT1" },
	{ "Left Boost Mixer", "LINPUT2 Switch", "LINPUT2" },
	{ "Left Boost Mixer", "LINPUT3 Switch", "LINPUT3" },

	{ "Left Input Mixer", "Boost Switch", "Left Boost Mixer", },
	{ "Left Input Mixer", NULL, "LINPUT1", },  /* Really Boost Switch */
	{ "Left Input Mixer", NULL, "LINPUT2" },
	{ "Left Input Mixer", NULL, "LINPUT3" },

	{ "Right Boost Mixer", "RINPUT1 Switch", "RINPUT1" },
	{ "Right Boost Mixer", "RINPUT2 Switch", "RINPUT2" },
	{ "Right Boost Mixer", "RINPUT3 Switch", "RINPUT3" },

	{ "Right Input Mixer", "Boost Switch", "Right Boost Mixer", },
	{ "Right Input Mixer", NULL, "RINPUT1", },  /* Really Boost Switch */
	{ "Right Input Mixer", NULL, "RINPUT2" },
	{ "Right Input Mixer", NULL, "LINPUT3" },

	{ "Left ADC", NULL, "Left Input Mixer" },
	{ "Right ADC", NULL, "Right Input Mixer" },

	{ "Left Output Mixer", "LINPUT3 Switch", "LINPUT3" },
	{ "Left Output Mixer", "Boost Bypass Switch", "Left Boost Mixer"} ,
	{ "Left Output Mixer", "PCM Playback Switch", "Left DAC" },

	{ "Right Output Mixer", "RINPUT3 Switch", "RINPUT3" },
	{ "Right Output Mixer", "Boost Bypass Switch", "Right Boost Mixer" } ,
	{ "Right Output Mixer", "PCM Playback Switch", "Right DAC" },

	{ "LOUT1 PGA", NULL, "Left Output Mixer" },
	{ "ROUT1 PGA", NULL, "Right Output Mixer" },

	{ "HP_L", NULL, "LOUT1 PGA" },
	{ "HP_R", NULL, "ROUT1 PGA" },

	{ "Left Speaker PGA", NULL, "Left Output Mixer" },
	{ "Right Speaker PGA", NULL, "Right Output Mixer" },

	{ "Left Speaker Output", NULL, "Left Speaker PGA" },
	{ "Right Speaker Output", NULL, "Right Speaker PGA" },

	{ "SPK_LN", NULL, "Left Speaker Output" },
	{ "SPK_LP", NULL, "Left Speaker Output" },
	{ "SPK_RN", NULL, "Right Speaker Output" },
	{ "SPK_RP", NULL, "Right Speaker Output" },
};

static const struct snd_soc_dapm_route audio_paths_out3[] = {
	{ "Mono Output Mixer", "Left Switch", "Left Output Mixer" },
	{ "Mono Output Mixer", "Right Switch", "Right Output Mixer" },

	{ "OUT3", NULL, "Mono Output Mixer", }
};

static const struct snd_soc_dapm_route audio_paths_capless[] = {
	{ "HP_L", NULL, "OUT3 VMID" },
	{ "HP_R", NULL, "OUT3 VMID" },

	{ "OUT3 VMID", NULL, "Left Output Mixer" },
	{ "OUT3 VMID", NULL, "Right Output Mixer" },
};

static int wm8960_add_widgets(struct snd_soc_codec *codec)
{
	struct wm8960_data *pdata = codec->dev->platform_data;
	struct wm8960_priv *wm8960 = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_widget *w;

	snd_soc_dapm_new_controls(codec, wm8960_dapm_widgets,
				  ARRAY_SIZE(wm8960_dapm_widgets));

	snd_soc_dapm_add_routes(codec, audio_paths, ARRAY_SIZE(audio_paths));

	/* In capless mode OUT3 is used to provide VMID for the
	 * headphone outputs, otherwise it is used as a mono mixer.
	 */
	if (pdata && pdata->capless) {
		snd_soc_dapm_new_controls(codec, wm8960_dapm_widgets_capless,
					  ARRAY_SIZE(wm8960_dapm_widgets_capless));

		snd_soc_dapm_add_routes(codec, audio_paths_capless,
					ARRAY_SIZE(audio_paths_capless));
	} else {
		snd_soc_dapm_new_controls(codec, wm8960_dapm_widgets_out3,
					  ARRAY_SIZE(wm8960_dapm_widgets_out3));

		snd_soc_dapm_add_routes(codec, audio_paths_out3,
					ARRAY_SIZE(audio_paths_out3));
	}

	/* We need to power up the headphone output stage out of
	 * sequence for capless mode.  To save scanning the widget
	 * list each time to find the desired power state do so now
	 * and save the result.
	 */
	list_for_each_entry(w, &codec->dapm_widgets, list) {
		if (strcmp(w->name, "LOUT1 PGA") == 0)
			wm8960->lout1 = w;
		if (strcmp(w->name, "ROUT1 PGA") == 0)
			wm8960->rout1 = w;
		if (strcmp(w->name, "OUT3 VMID") == 0)
			wm8960->out3 = w;
	}
	
	return 0;
}

static int wm8960_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface |= 0x0040;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
		return -EINVAL;
	}

	/* set iface */
	snd_soc_write(codec, WM8960_IFACE1, iface);
	return 0;
}

static int wm8960_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	u16 iface = snd_soc_read(codec, WM8960_IFACE1) & 0xfff3;

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	}

	/* set iface */
	snd_soc_write(codec, WM8960_IFACE1, iface);
	return 0;
}

static int wm8960_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = snd_soc_read(codec, WM8960_DACCTL1) & 0xfff7;

	if (mute)
		snd_soc_write(codec, WM8960_DACCTL1, mute_reg | 0x8);
	else
		snd_soc_write(codec, WM8960_DACCTL1, mute_reg);
	return 0;
}

//Apollo +
static struct snd_soc_codec *t_codec = NULL;
bool wm8960_setbias(int onoff)
{
  u16 reg;

  if(t_codec == NULL)
    return false;

  reg = snd_soc_read(t_codec, WM8960_POWER1);
  if(onoff)
    reg |= 0x02;
  else
    reg &= (~(0x02));
  
  snd_soc_write(t_codec, WM8960_POWER1, reg);
  
  return true;
}
EXPORT_SYMBOL(wm8960_setbias);
//Apollo -

static int wm8960_set_bias_level_out3(struct snd_soc_codec *codec,
				      enum snd_soc_bias_level level)
{
	u16 reg;

	switch (level) {
	case SND_SOC_BIAS_ON:
//printk("SND_SOC_BIAS_ON\n");
		break;

	case SND_SOC_BIAS_PREPARE:
		/* Set VMID to 2x50k */
		reg = snd_soc_read(codec, WM8960_POWER1);
		reg &= ~0x180;
		reg |= 0x80;
		snd_soc_write(codec, WM8960_POWER1, reg);

//Apollo +
//		/* Enable ADCL */
		reg = snd_soc_read(codec, WM8960_POWER1);
		snd_soc_write(codec, WM8960_POWER1, reg | 0x02A);
//printk("SND_SOC_BIAS_PREPARE\n");
//Apollo -
		break;

	case SND_SOC_BIAS_STANDBY:
		if (codec->bias_level == SND_SOC_BIAS_OFF) {
			/* Enable anti-pop features */
			snd_soc_write(codec, WM8960_APOP1,
				      WM8960_POBCTRL | WM8960_SOFT_ST |
				      WM8960_BUFDCOPEN | WM8960_BUFIOEN);

			/* Enable & ramp VMID at 2x50k */
			reg = snd_soc_read(codec, WM8960_POWER1);
			reg |= 0x80;
			snd_soc_write(codec, WM8960_POWER1, reg);
			msleep(100);

			/* Enable VREF */
			snd_soc_write(codec, WM8960_POWER1, reg | WM8960_VREF);

			/* Disable anti-pop features */
			snd_soc_write(codec, WM8960_APOP1, WM8960_BUFIOEN);
		}

		/* Set VMID to 2x250k */
		reg = snd_soc_read(codec, WM8960_POWER1);
		reg &= ~0x180;
		reg |= 0x100;
		snd_soc_write(codec, WM8960_POWER1, reg);

//Apollo +
//		/* Enable ADCL */
//		reg = snd_soc_read(codec, WM8960_POWER1);
//		snd_soc_write(codec, WM8960_POWER1, reg | 0x00A);
//printk("SND_SOC_BIAS_STANDBY\n");
//Apollo -
		break;

	case SND_SOC_BIAS_OFF:
		/* Enable anti-pop features */
		snd_soc_write(codec, WM8960_APOP1,
			     WM8960_POBCTRL | WM8960_SOFT_ST |
			     WM8960_BUFDCOPEN | WM8960_BUFIOEN);

		/* Disable VMID and VREF, let them discharge */
		snd_soc_write(codec, WM8960_POWER1, 0);
		msleep(600);
//printk("SND_SOC_BIAS_OFF\n");
		break;
	}

	codec->bias_level = level;

	return 0;
}

static int wm8960_set_bias_level_capless(struct snd_soc_codec *codec,
					 enum snd_soc_bias_level level)
{
	struct wm8960_priv *wm8960 = snd_soc_codec_get_drvdata(codec);
	int reg;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		switch (codec->bias_level) {
		case SND_SOC_BIAS_STANDBY:
			/* Enable anti pop mode */
			snd_soc_update_bits(codec, WM8960_APOP1,
					    WM8960_POBCTRL | WM8960_SOFT_ST |
					    WM8960_BUFDCOPEN,
					    WM8960_POBCTRL | WM8960_SOFT_ST |
					    WM8960_BUFDCOPEN);

			/* Enable LOUT1, ROUT1 and OUT3 if they're enabled */
			reg = 0;
			if (wm8960->lout1 && wm8960->lout1->power)
				reg |= WM8960_PWR2_LOUT1;
			if (wm8960->rout1 && wm8960->rout1->power)
				reg |= WM8960_PWR2_ROUT1;
			if (wm8960->out3 && wm8960->out3->power)
				reg |= WM8960_PWR2_OUT3;
			snd_soc_update_bits(codec, WM8960_POWER2,
					    WM8960_PWR2_LOUT1 |
					    WM8960_PWR2_ROUT1 |
					    WM8960_PWR2_OUT3, reg);

			/* Enable VMID at 2*50k */
			snd_soc_update_bits(codec, WM8960_POWER1,
					    WM8960_VMID_MASK, 0x80);

			/* Ramp */
			msleep(100);

			/* Enable VREF */
			snd_soc_update_bits(codec, WM8960_POWER1,
					    WM8960_VREF, WM8960_VREF);

			msleep(100);
			break;

		case SND_SOC_BIAS_ON:
			/* Enable anti-pop mode */
			snd_soc_update_bits(codec, WM8960_APOP1,
					    WM8960_POBCTRL | WM8960_SOFT_ST |
					    WM8960_BUFDCOPEN,
					    WM8960_POBCTRL | WM8960_SOFT_ST |
					    WM8960_BUFDCOPEN);

			/* Disable VMID and VREF */
			snd_soc_update_bits(codec, WM8960_POWER1,
					    WM8960_VREF | WM8960_VMID_MASK, 0);
			break;

		default:
			break;
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		switch (codec->bias_level) {
		case SND_SOC_BIAS_PREPARE:
			/* Disable HP discharge */
			snd_soc_update_bits(codec, WM8960_APOP2,
					    WM8960_DISOP | WM8960_DRES_MASK,
					    0);

			/* Disable anti-pop features */
			snd_soc_update_bits(codec, WM8960_APOP1,
					    WM8960_POBCTRL | WM8960_SOFT_ST |
					    WM8960_BUFDCOPEN,
					    WM8960_POBCTRL | WM8960_SOFT_ST |
					    WM8960_BUFDCOPEN);
			break;

		default:
			break;
		}
		break;

	case SND_SOC_BIAS_OFF:
		break;
	}

	codec->bias_level = level;

	return 0;
}

/* PLL divisors */
struct _pll_div {
	u32 pre_div:1;
	u32 n:4;
	u32 k:24;
};

/* The size in bits of the pll divide multiplied by 10
 * to allow rounding later */
#define FIXED_PLL_SIZE ((1 << 24) * 10)

static int pll_factors(unsigned int source, unsigned int target,
		       struct _pll_div *pll_div)
{
	unsigned long long Kpart;
	unsigned int K, Ndiv, Nmod;

	pr_debug("WM8960 PLL: setting %dHz->%dHz\n", source, target);

	/* Scale up target to PLL operating frequency */
	target *= 4;

	Ndiv = target / source;
	if (Ndiv < 6) {
		source >>= 1;
		pll_div->pre_div = 1;
		Ndiv = target / source;
	} else
		pll_div->pre_div = 0;

	if ((Ndiv < 6) || (Ndiv > 12)) {
		pr_err("WM8960 PLL: Unsupported N=%d\n", Ndiv);
		return -EINVAL;
	}

	pll_div->n = Ndiv;
	Nmod = target % source;
	Kpart = FIXED_PLL_SIZE * (long long)Nmod;

	do_div(Kpart, source);

	K = Kpart & 0xFFFFFFFF;

	/* Check if we need to round */
	if ((K % 10) >= 5)
		K += 5;

	/* Move down to proper range now rounding is done */
	K /= 10;

	pll_div->k = K;

	pr_debug("WM8960 PLL: N=%x K=%x pre_div=%d\n",
		 pll_div->n, pll_div->k, pll_div->pre_div);

	return 0;
}

static int wm8960_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;
	static struct _pll_div pll_div;
	int ret;

	if (freq_in && freq_out) {
		ret = pll_factors(freq_in, freq_out, &pll_div);
		if (ret != 0)
			return ret;
	}

	/* Disable the PLL: even if we are changing the frequency the
	 * PLL needs to be disabled while we do so. */
	snd_soc_write(codec, WM8960_CLOCK1,
		     snd_soc_read(codec, WM8960_CLOCK1) & ~1);
	snd_soc_write(codec, WM8960_POWER2,
		     snd_soc_read(codec, WM8960_POWER2) & ~1);

	if (!freq_in || !freq_out)
		return 0;

	reg = snd_soc_read(codec, WM8960_PLL1) & ~0x3f;
	reg |= pll_div.pre_div << 4;
	reg |= pll_div.n;

	if (pll_div.k) {
		reg |= 0x20;

		snd_soc_write(codec, WM8960_PLL2, (pll_div.k >> 18) & 0x3f);
		snd_soc_write(codec, WM8960_PLL3, (pll_div.k >> 9) & 0x1ff);
		snd_soc_write(codec, WM8960_PLL4, pll_div.k & 0x1ff);
	}
	snd_soc_write(codec, WM8960_PLL1, reg);

	/* Turn it on */
	snd_soc_write(codec, WM8960_POWER2,
		     snd_soc_read(codec, WM8960_POWER2) | 1);
	msleep(250);
	snd_soc_write(codec, WM8960_CLOCK1,
		     snd_soc_read(codec, WM8960_CLOCK1) | 1);

	return 0;
}

static int wm8960_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
		int div_id, int div)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;

	switch (div_id) {
	case WM8960_SYSCLKDIV:
		reg = snd_soc_read(codec, WM8960_CLOCK1) & 0x1f9;
		snd_soc_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_DACDIV:
		reg = snd_soc_read(codec, WM8960_CLOCK1) & 0x1c7;
		snd_soc_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_OPCLKDIV:
		reg = snd_soc_read(codec, WM8960_PLL1) & 0x03f;
		snd_soc_write(codec, WM8960_PLL1, reg | div);
		break;
	case WM8960_DCLKDIV:
		reg = snd_soc_read(codec, WM8960_CLOCK2) & 0x03f;
		snd_soc_write(codec, WM8960_CLOCK2, reg | div);
		break;
	case WM8960_TOCLKSEL:
		reg = snd_soc_read(codec, WM8960_ADDCTL1) & 0x1fd;
		snd_soc_write(codec, WM8960_ADDCTL1, reg | div);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define WM8960_RATES SNDRV_PCM_RATE_8000_48000

#define WM8960_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops wm8960_dai_ops = {
	.hw_params = wm8960_hw_params,
	.digital_mute = wm8960_mute,
	.set_fmt = wm8960_set_dai_fmt,
	.set_clkdiv = wm8960_set_dai_clkdiv,
	.set_pll = wm8960_set_dai_pll,
};

struct snd_soc_dai wm8960_dai = {
	.name = "WM8960",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8960_RATES,
		.formats = WM8960_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8960_RATES,
		.formats = WM8960_FORMATS,},
	.ops = &wm8960_dai_ops,
	.symmetric_rates = 1,
};
EXPORT_SYMBOL_GPL(wm8960_dai);

static unsigned short pwr_ctrl[3];
static int wm8960_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

printk("wm8960_suspend start\n");
	//Disbale D Class
	unsigned short val;
	val = snd_soc_read(codec, WM8960_CLASSD1);
	val &= ~(0x3<<6);
	snd_soc_write(codec, WM8960_CLASSD1, val);

//	codec->set_bias_level(codec, SND_SOC_BIAS_OFF);
//disable_allpower
//  pwr_ctrl[0] = snd_soc_read(codec, WM8960_POWER1);
//  pwr_ctrl[1] = snd_soc_read(codec, WM8960_POWER2);
//  pwr_ctrl[2] = snd_soc_read(codec, WM8960_POWER3);
//  snd_soc_write(codec, WM8960_POWER1, 0x0000);
//  snd_soc_write(codec, WM8960_POWER2, 0x0000);
//  snd_soc_write(codec, WM8960_POWER3, 0x0000);
  
printk("wm8960_suspend done\n");
	return 0;
}
void wm8960_poweroff(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, WM8960_POWER1, 0x0000);
  snd_soc_write(codec, WM8960_POWER2, 0x0000);
  snd_soc_write(codec, WM8960_POWER3, 0x0000);
  printk("wm8960 poweroff ++++++++++++++++++\n");
}
EXPORT_SYMBOL(wm8960_poweroff);

static int wm8960_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

printk("wm8960_resume start\n");
//  //enable all power
//  snd_soc_write(codec, WM8960_POWER1, pwr_ctrl[0]);
//  snd_soc_write(codec, WM8960_POWER2, pwr_ctrl[1]);
//  snd_soc_write(codec, WM8960_POWER3, pwr_ctrl[2]);
  
	//Disbale D Class
	unsigned short val;
	val = snd_soc_read(codec, WM8960_CLASSD1);
	val &= ~(0x3<<6);
	snd_soc_write(codec, WM8960_CLASSD1, val);

//fighter++   //ÐÝÃß»½ÐÑºóÂ¼ÒôÊ§Ð§ £¬ÔÝÊ±ÆÁ±Î
//	/* Sync reg_cache with the hardware */
//	for (i = 0; i < ARRAY_SIZE(wm8960_reg); i++) {
//		data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
//		data[1] = cache[i] & 0x00ff;
//		codec->hw_write(codec->control_data, data, 2);
//	}
//fighter--

//	codec->set_bias_level(codec, SND_SOC_BIAS_STANDBY);

printk("wm8960_resume done\n");
	return 0;
}

static struct snd_soc_codec *wm8960_codec;

static int wm8960_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	if (wm8960_codec == NULL) {
		dev_err(&pdev->dev, "Codec device not registered\n");
		return -ENODEV;
	}

	socdev->card->codec = wm8960_codec;
	codec = wm8960_codec;

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(codec->dev, "failed to create pcms: %d\n", ret);
		goto pcm_err;
	}

	snd_soc_add_controls(codec, wm8960_snd_controls,
			     ARRAY_SIZE(wm8960_snd_controls));
//Apollo +
 	snd_soc_add_controls(codec, wm8960_snd_controls_ext, ARRAY_SIZE(wm8960_snd_controls_ext));
//Apollo -
	wm8960_add_widgets(codec);

//Apollo +
  t_codec = codec;
//Apollo -
	return ret;

pcm_err:
	return ret;
}

/* power down chip */
static int wm8960_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm8960 = {
	.probe = 	wm8960_probe,
	.remove = 	wm8960_remove,
	.suspend = 	wm8960_suspend,
	.resume =	wm8960_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_wm8960);

static int wm8960_register(struct wm8960_priv *wm8960,
			   enum snd_soc_control_type control)
{
	struct wm8960_data *pdata = wm8960->codec.dev->platform_data;
	struct snd_soc_codec *codec = &wm8960->codec;
	int ret;
	u16 reg;

	if (wm8960_codec) {
		dev_err(codec->dev, "Another WM8960 is registered\n");
		ret = -EINVAL;
		goto err;
	}

	codec->set_bias_level = wm8960_set_bias_level_out3;

	if (!pdata) {
		dev_warn(codec->dev, "No platform data supplied\n");
	} else {
		if (pdata->dres > WM8960_DRES_MAX) {
			dev_err(codec->dev, "Invalid DRES: %d\n", pdata->dres);
			pdata->dres = 0;
		}

		if (pdata->capless)
			codec->set_bias_level = wm8960_set_bias_level_capless;
	}

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	snd_soc_codec_set_drvdata(codec, wm8960);
	codec->name = "WM8960";
	codec->owner = THIS_MODULE;
	codec->bias_level = SND_SOC_BIAS_OFF;
	codec->dai = &wm8960_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = WM8960_CACHEREGNUM;
	codec->reg_cache = &wm8960->reg_cache;

	memcpy(codec->reg_cache, wm8960_reg, sizeof(wm8960_reg));

	ret = snd_soc_codec_set_cache_io(codec, 7, 9, control);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		goto err;
	}

	ret = wm8960_reset(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to issue reset\n");
		goto err;
	}

	wm8960_dai.dev = codec->dev;

	codec->set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Latch the update bits */
	reg = snd_soc_read(codec, WM8960_LINVOL);
	snd_soc_write(codec, WM8960_LINVOL, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_RINVOL);
	snd_soc_write(codec, WM8960_RINVOL, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_LADC);
	snd_soc_write(codec, WM8960_LADC, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_RADC);
	snd_soc_write(codec, WM8960_RADC, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_LDAC);
	snd_soc_write(codec, WM8960_LDAC, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_RDAC);
	snd_soc_write(codec, WM8960_RDAC, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_LOUT1);
	snd_soc_write(codec, WM8960_LOUT1, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_ROUT1);
	snd_soc_write(codec, WM8960_ROUT1, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_LOUT2);
	snd_soc_write(codec, WM8960_LOUT2, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_ROUT2);
	snd_soc_write(codec, WM8960_ROUT2, reg | 0x100);

//Apollo +
////////////////////////////////////////////////////////////////////////////////////////////////
//	reg = snd_soc_read(codec, WM8960_ADDCTL2);  //0x18 HPSWEN=1 HPSWPOL=1
//	snd_soc_write(codec, WM8960_ADDCTL2, reg | 0x060);

	reg = snd_soc_read(codec, WM8960_IFACE2);   //0x09 ALRCGPIO=1(GPIO)
	snd_soc_write(codec, WM8960_IFACE2, reg | 0x040);
	
//	reg = snd_soc_read(codec, WM8960_ADDCTL3);  //0x1B OUT3CAP=1
//	snd_soc_write(codec, WM8960_ADDCTL3, reg | 0x008);
	
	reg = snd_soc_read(codec, WM8960_ADDCTL1);  //0x17 TOEN=1 TOCLKSEL=1
	snd_soc_write(codec, WM8960_ADDCTL1, reg | 0x003);
//Apollo -

	wm8960_codec = codec;

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register codec: %d\n", ret);
		goto err;
	}

	ret = snd_soc_register_dai(&wm8960_dai);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register DAI: %d\n", ret);
		goto err_codec;
	}

	return 0;

err_codec:
	snd_soc_unregister_codec(codec);
err:
	kfree(wm8960);
	return ret;
}

static void wm8960_unregister(struct wm8960_priv *wm8960)
{
	wm8960->codec.set_bias_level(&wm8960->codec, SND_SOC_BIAS_OFF);
	snd_soc_unregister_dai(&wm8960_dai);
	snd_soc_unregister_codec(&wm8960->codec);
	kfree(wm8960);
	wm8960_codec = NULL;
}

static __devinit int wm8960_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct wm8960_priv *wm8960;
	struct snd_soc_codec *codec;

	wm8960 = kzalloc(sizeof(struct wm8960_priv), GFP_KERNEL);
	if (wm8960 == NULL)
		return -ENOMEM;

	codec = &wm8960->codec;

	i2c_set_clientdata(i2c, wm8960);
	codec->control_data = i2c;

	codec->dev = &i2c->dev;

	return wm8960_register(wm8960, SND_SOC_I2C);
}

static __devexit int wm8960_i2c_remove(struct i2c_client *client)
{
	struct wm8960_priv *wm8960 = i2c_get_clientdata(client);
	wm8960_unregister(wm8960);
	return 0;
}

static const struct i2c_device_id wm8960_i2c_id[] = {
	{ "wm8960", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8960_i2c_id);

static struct i2c_driver wm8960_i2c_driver = {
	.driver = {
		.name = "wm8960",
		.owner = THIS_MODULE,
	},
	.probe =    wm8960_i2c_probe,
	.remove =   __devexit_p(wm8960_i2c_remove),
	.id_table = wm8960_i2c_id,
};

static int __init wm8960_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&wm8960_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register WM8960 I2C driver: %d\n",
		       ret);
	}

	return ret;
}
module_init(wm8960_modinit);

static void __exit wm8960_exit(void)
{
	i2c_del_driver(&wm8960_i2c_driver);
}
module_exit(wm8960_exit);


MODULE_DESCRIPTION("ASoC WM8960 driver");
MODULE_AUTHOR("Liam Girdwood");
MODULE_LICENSE("GPL");
