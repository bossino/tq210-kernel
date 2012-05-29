/* linux/arch/arm/mach-s5pv210/mach-smdkc110.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/gpio_event.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/consumer.h>
#if defined(CONFIG_SMDKC110_REV03) || defined(CONFIG_SMDKV210_REV02)
#include <linux/mfd/max8998.h>
#else
#include <linux/mfd/max8698.h>
#endif
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/pwm_backlight.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/clk.h>
#include <linux/usb/ch9.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/skbuff.h>
#include <linux/console.h>
#include <linux/timed_gpio.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/system.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-mem.h>
#include <mach/gpio.h>
#include <mach/gpio-smdkc110.h>
#include <mach/regs-gpio.h>
#include <mach/ts-s3c.h>
#include <mach/adc.h>
#include <mach/param.h>
#include <mach/system.h>

#ifdef CONFIG_S3C64XX_DEV_SPI
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>
#endif

#include <linux/usb/gadget.h>

#include <plat/media.h>
#include <mach/media.h>

#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#include <plat/media.h>
#include <mach/media.h>
#endif

#ifdef CONFIG_WLAN
#include <linux/wlan_plat.h>
#endif

#ifdef CONFIG_S5PV210_POWER_DOMAIN
#include <mach/power-domain.h>
#endif

#ifdef CONFIG_VIDEO_S5K4BA
#include <media/s5k4ba_platform.h>
#undef	CAM_ITU_CH_A
#define	CAM_ITU_CH_B
#endif

#ifdef CONFIG_VIDEO_S5K4EA
#include <media/s5k4ea_platform.h>
#endif

#ifdef CONFIG_VIDEO_GC2015
#include <media/GC2015_platform.h>
#endif

#undef	CAM_ITU_CH_B
#define	CAM_ITU_CH_A


#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>
#include <plat/mfc.h>
#include <plat/iic.h>
#include <plat/pm.h>

#include <plat/sdhci.h>
#include <plat/fimc.h>
#include <plat/csis.h>
#include <plat/jpeg.h>
#include <plat/clock.h>
#include <plat/regs-otg.h>
#include <../../../drivers/video/samsung/s3cfb.h>

int active_gsensor = 1;
//fighter++
extern struct snd_soc_codec *cpy_codec;
extern void wm8960_poweroff(struct snd_soc_codec *codec);
extern unsigned int check_wifi_type();
//fighter--
struct class *sec_class;
EXPORT_SYMBOL(sec_class);

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

void s3c_setup_uart_cfg_gpio(unsigned char port);

#ifdef CONFIG_USB_S3C_OTG_HOST
extern struct platform_device s3c_device_usb_otghcd;
#endif

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define S5PV210_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define S5PV210_ULCON_DEFAULT	S3C2410_LCON_CS8

#define S5PV210_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg smdkc110_uartcfgs[] __initdata = {
	{
		.hwport		= 0,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	{
		.hwport		= 1,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
#ifndef CONFIG_FIQ_DEBUGGER
	{
		.hwport		= 2,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
#endif
	{
		.hwport		= 3,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
};

#ifdef CONFIG_FB_LCD_1024X600
	#define S5PV210_LCD_WIDTH 1024
	#define S5PV210_LCD_HEIGHT 600
#endif

#ifdef CONFIG_FB_LCD_800X480
	#define S5PV210_LCD_WIDTH 800
	#define S5PV210_LCD_HEIGHT 480
#endif

#ifdef CONFIG_FB_LCD_1366X768
	#define S5PV210_LCD_WIDTH 1366
	#define S5PV210_LCD_HEIGHT 768
#endif

#ifdef CONFIG_FB_LCD_1024X768
	#define S5PV210_LCD_WIDTH 1024
	#define S5PV210_LCD_HEIGHT 768
#endif

//#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0 (6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0 (24576 * SZ_1K)
//#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1 (9900 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1 (24576 * SZ_1K)
//#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2 (6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2 (24576 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0 (36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1 (36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD (S5PV210_LCD_WIDTH * \
					     S5PV210_LCD_HEIGHT * 4 * \
					     CONFIG_FB_S3C_NR_BUFFERS)
					     
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG (40960 * SZ_1K) 

/* 1920 * 1080 * 4 (RGBA)
 * - framesize == 1080p : 1920 * 1080 * 2(16bpp) * 2(double buffer) = 8MB
 * - framesize <  1080p : 1080 *  720 * 4(32bpp) * 2(double buffer) = under 8MB
 **/
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_G2D (8192 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXSTREAM (3000 * SZ_1K)
#define  S5PV210_ANDROID_PMEM_MEMSIZE_PMEM_GPU1 (3300 * SZ_1K)

static struct s5p_media_device smdkc110_media_devs[] = {
	[0] = {
		.id = S5P_MDEV_MFC,
		.name = "mfc",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0,
		.paddr = 0,
	},
	[1] = {
		.id = S5P_MDEV_MFC,
		.name = "mfc",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1,
		.paddr = 0,
	},
	[2] = {
		.id = S5P_MDEV_FIMC0,
		.name = "fimc0",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0,
		.paddr = 0,
	},
	[3] = {
		.id = S5P_MDEV_FIMC1,
		.name = "fimc1",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1,
		.paddr = 0,
	},
	[4] = {
		.id = S5P_MDEV_FIMC2,
		.name = "fimc2",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2,
		.paddr = 0,
	},
	[5] = {
		.id = S5P_MDEV_JPEG,
		.name = "jpeg",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG,
		.paddr = 0,
	},
	[6] = {
		.id = S5P_MDEV_FIMD,
		.name = "fimd",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD,
		.paddr = 0,
	},
	[7] = {
		.id = S5P_MDEV_TEXSTREAM,
		.name = "texstream",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXSTREAM,
		.paddr = 0,
	},
	[8] = {
		.id = S5P_MDEV_PMEM_GPU1,
		.name = "pmem_gpu1",
		.bank = 0, /* OneDRAM */
		.memsize = S5PV210_ANDROID_PMEM_MEMSIZE_PMEM_GPU1,
		.paddr = 0,
	},
	[9] = {
		.id = S5P_MDEV_G2D,
		.name = "g2d",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_G2D,
		.paddr = 0,
	},
#ifdef CONFIG_BCM4329_RESERVED_MEM
	[10] = {
		.id = S5P_MDEV_WIFI,
		.name = "wifi",
		.bank = 0,
		.memsize = CONFIG_BCM4329_MEMSIZE * SZ_1K,
		.paddr = 0,
	},
#endif
};

static struct regulator_consumer_supply ldo3_consumer[] = {
	REGULATOR_SUPPLY("pd_io", "s3c-usbgadget")
};

static struct regulator_consumer_supply ldo4_consumer[] = {
	{   .supply = "vddmipi", },
};

static struct regulator_consumer_supply ldo6_consumer[] = {
	{   .supply = "vddlcd", },
};

static struct regulator_consumer_supply ldo7_consumer[] = {
	{   .supply = "vddmodem", },
};

static struct regulator_consumer_supply ldo8_consumer[] = {
	REGULATOR_SUPPLY("pd_core", "s3c-usbgadget")
};

static struct regulator_consumer_supply buck1_consumer[] = {
	{   .supply = "vddarm", },
};

static struct regulator_consumer_supply buck2_consumer[] = {
	{   .supply = "vddint", },
};

static struct regulator_init_data smdkc110_ldo2_data = {
	.constraints	= {
		.name		= "VALIVE_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
};

static struct regulator_init_data smdkc110_ldo3_data = {
	.constraints	= {
		.name		= "VUOTG_D_1.1V/VUHOST_D_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo3_consumer),
	.consumer_supplies	= ldo3_consumer,
};

static struct regulator_init_data smdkc110_ldo4_data = {
	.constraints	= {
		.name		= "V_MIPI_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo4_consumer),
	.consumer_supplies	= ldo4_consumer,
};

static struct regulator_init_data smdkc110_ldo5_data = {
	.constraints	= {
		.name		= "VMMC_2.8V/VEXT_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
};

static struct regulator_init_data smdkc110_ldo6_data = {
	.constraints	= {
		.name		= "VCC_2.6V",
		.min_uV		= 2600000,
		.max_uV		= 2600000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	 = {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo6_consumer),
	.consumer_supplies	= ldo6_consumer,
};

static struct regulator_init_data smdkc110_ldo7_data = {
	.constraints	= {
		.name		= "VDAC_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies  = ARRAY_SIZE(ldo7_consumer),
	.consumer_supplies  = ldo7_consumer,
};

static struct regulator_init_data smdkc110_ldo8_data = {
	.constraints	= {
		.name		= "VUOTG_A_3.3V/VUHOST_A_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo8_consumer),
	.consumer_supplies	= ldo8_consumer,
};

static struct regulator_init_data smdkc110_ldo9_data = {
	.constraints	= {
		.name		= "{VADC/VSYS/VKEY}_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
};

static struct regulator_init_data smdkc110_buck1_data = {
	.constraints	= {
		.name		= "VCC_ARM",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1250000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck1_consumer),
	.consumer_supplies	= buck1_consumer,
};

static struct regulator_init_data smdkc110_buck2_data = {
	.constraints	= {
		.name		= "VCC_INTERNAL",
		.min_uV		= 950000,
		.max_uV		= 1200000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1100000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies  = ARRAY_SIZE(buck2_consumer),
	.consumer_supplies  = buck2_consumer,
};

static struct regulator_init_data smdkc110_buck3_data = {
	.constraints	= {
		.name		= "VCC_MEM",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.always_on	= 1,
		.apply_uV	= 1,
		.state_mem	= {
			.uV	= 1800000,
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 1,
		},
	},
};

#if defined(CONFIG_SMDKC110_REV03) || defined(CONFIG_SMDKV210_REV02)
static struct max8998_regulator_data smdkc110_regulators[] = {
	{ MAX8998_LDO2,  &smdkc110_ldo2_data },
	{ MAX8998_LDO3,  &smdkc110_ldo3_data },
	{ MAX8998_LDO4,  &smdkc110_ldo4_data },
	{ MAX8998_LDO5,  &smdkc110_ldo5_data },
	{ MAX8998_LDO6,  &smdkc110_ldo6_data },
	{ MAX8998_LDO7,  &smdkc110_ldo7_data },
	{ MAX8998_LDO8,  &smdkc110_ldo8_data },
	{ MAX8998_LDO9,  &smdkc110_ldo9_data },
	{ MAX8998_BUCK1, &smdkc110_buck1_data },
	{ MAX8998_BUCK2, &smdkc110_buck2_data },
	{ MAX8998_BUCK3, &smdkc110_buck3_data },
};

static struct max8998_platform_data max8998_pdata = {
	.num_regulators	= ARRAY_SIZE(smdkc110_regulators),
	.regulators	= smdkc110_regulators,
	.charger	= NULL,
};
#else
static struct max8698_regulator_data smdkc110_regulators[] = {
	{ MAX8698_LDO2,  &smdkc110_ldo2_data },
	{ MAX8698_LDO3,  &smdkc110_ldo3_data },
	{ MAX8698_LDO4,  &smdkc110_ldo4_data },
	{ MAX8698_LDO5,  &smdkc110_ldo5_data },
	{ MAX8698_LDO6,  &smdkc110_ldo6_data },
	{ MAX8698_LDO7,  &smdkc110_ldo7_data },
	{ MAX8698_LDO8,  &smdkc110_ldo8_data },
	{ MAX8698_LDO9,  &smdkc110_ldo9_data },
	{ MAX8698_BUCK1, &smdkc110_buck1_data },
	{ MAX8698_BUCK2, &smdkc110_buck2_data },
	{ MAX8698_BUCK3, &smdkc110_buck3_data },
};

static struct max8698_platform_data max8698_pdata = {
	.num_regulators = ARRAY_SIZE(smdkc110_regulators),
	.regulators     = smdkc110_regulators,

	/* 1GHz default voltage */
	.dvsarm1        = 0xa,  /* 1.25v */
	.dvsarm2        = 0x9,  /* 1.20V */
	.dvsarm3        = 0x6,  /* 1.05V */
	.dvsarm4        = 0x4,  /* 0.95V */
	.dvsint1        = 0x7,  /* 1.10v */
	.dvsint2        = 0x5,  /* 1.00V */

	.set1       = S5PV210_GPH1(6),
	.set2       = S5PV210_GPH1(7),
	.set3       = S5PV210_GPH0(4),
};
#endif

static void __init smdkc110_setup_clocks(void)
{
	struct clk *pclk;
	struct clk *clk;

#ifdef CONFIG_S3C_DEV_HSMMC
	/* set MMC0 clock */
	clk = clk_get(&s3c_device_hsmmc0.dev, "sclk_mmc");
	pclk = clk_get(NULL, "mout_mpll");
	clk_set_parent(clk, pclk);
	clk_set_rate(clk, 50*MHZ);

	pr_info("%s: %s: source is %s, rate is %ld\n",
				__func__, clk->name, clk->parent->name,
				clk_get_rate(clk));
#endif

#ifdef CONFIG_S3C_DEV_HSMMC1
	/* set MMC1 clock */
	clk = clk_get(&s3c_device_hsmmc1.dev, "sclk_mmc");
	pclk = clk_get(NULL, "mout_mpll");
	clk_set_parent(clk, pclk);
	clk_set_rate(clk, 50*MHZ);

	pr_info("%s: %s: source is %s, rate is %ld\n",
				__func__, clk->name, clk->parent->name,
				clk_get_rate(clk));
#endif

#ifdef CONFIG_S3C_DEV_HSMMC2
	/* set MMC2 clock */
	clk = clk_get(&s3c_device_hsmmc2.dev, "sclk_mmc");
	pclk = clk_get(NULL, "mout_mpll");
	clk_set_parent(clk, pclk);
	clk_set_rate(clk, 50*MHZ);

	pr_info("%s: %s: source is %s, rate is %ld\n",
				__func__, clk->name, clk->parent->name,
				clk_get_rate(clk));
#endif

#ifdef CONFIG_S3C_DEV_HSMMC3
	/* set MMC3 clock */
	clk = clk_get(&s3c_device_hsmmc3.dev, "sclk_mmc");
	pclk = clk_get(NULL, "mout_mpll");
	clk_set_parent(clk, pclk);
	clk_set_rate(clk, 50*MHZ);

	pr_info("%s: %s: source is %s, rate is %ld\n",
				__func__, clk->name, clk->parent->name,
			 clk_get_rate(clk));
#endif
}

#if defined(CONFIG_TOUCHSCREEN_S3C)
static struct s3c_ts_mach_info s3c_ts_platform __initdata = {
	.delay                  = 10000,
	.presc                  = 49,
	.oversampling_shift     = 2,
	.resol_bit              = 12,
	.s3c_adc_con            = ADC_TYPE_2,
};

/* Touch srcreen */
static struct resource s3c_ts_resource[] = {
	[0] = {
		.start = S3C_PA_ADC,
		.end   = S3C_PA_ADC + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_PENDN,
		.end   = IRQ_PENDN,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_ADC,
		.end   = IRQ_ADC,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_ts = {
	.name		  = "s3c-ts",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_ts_resource),
	.resource	  = s3c_ts_resource,
};

void __init s3c_ts_set_platdata(struct s3c_ts_mach_info *pd)
{
	struct s3c_ts_mach_info *npd;

	npd = kmalloc(sizeof(*npd), GFP_KERNEL);
	if (npd) {
		memcpy(npd, pd, sizeof(*npd));
		s3c_device_ts.dev.platform_data = npd;
	} else {
		pr_err("no memory for Touchscreen platform data\n");
	}
}
#endif

//#if defined(CONFIG_KEYPAD_S3C) || defined(CONFIG_KEYPAD_S3C_MODULE)
//#if defined(CONFIG_KEYPAD_S3C_MSM)
//void s3c_setup_keypad_cfg_gpio(void)
//{
//	unsigned int gpio;
//	unsigned int end;
//
//	/* gpio setting for KP_COL0 */
//	s3c_gpio_cfgpin(S5PV210_GPJ1(5), S3C_GPIO_SFN(3));
//	s3c_gpio_setpull(S5PV210_GPJ1(5), S3C_GPIO_PULL_NONE);
//
//	/* gpio setting for KP_COL1 ~ KP_COL7 and KP_ROW0 */
//	end = S5PV210_GPJ2(8);
//	for (gpio = S5PV210_GPJ2(0); gpio < end; gpio++) {
//		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
//		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
//	}
//
//	/* gpio setting for KP_ROW1 ~ KP_ROW8 */
//	end = S5PV210_GPJ3(8);
//	for (gpio = S5PV210_GPJ3(0); gpio < end; gpio++) {
//		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
//		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
//	}
//
//	/* gpio setting for KP_ROW9 ~ KP_ROW13 */
//	end = S5PV210_GPJ4(5);
//	for (gpio = S5PV210_GPJ4(0); gpio < end; gpio++) {
//		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
//		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
//	}
//}
//#else
//void s3c_setup_keypad_cfg_gpio(int rows, int columns)
//{
//	unsigned int gpio;
//	unsigned int end;
//
//	end = S5PV210_GPH3(rows);
//
//	/* Set all the necessary GPH2 pins to special-function 0 */
//	for (gpio = S5PV210_GPH3(0); gpio < end; gpio++) {
//		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
//		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
//	}
//
//	end = S5PV210_GPH2(columns);
//
//	/* Set all the necessary GPK pins to special-function 0 */
//	for (gpio = S5PV210_GPH2(0); gpio < end; gpio++) {
//		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
//		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
//	}
//}
//#endif /* if defined(CONFIG_KEYPAD_S3C_MSM)*/
//EXPORT_SYMBOL(s3c_setup_keypad_cfg_gpio);
//#endif

#ifdef CONFIG_DM9000
static void __init smdkc110_dm9000_set(void)
{
	unsigned int tmp;

//Apollo + power on GPJ4[3]
  tmp = __raw_readl(S5PV210_GPJ4_BASE+0x00);
  tmp = (tmp & (~(0xF<<12))) | (0x1<<12);
  __raw_writel(tmp, S5PV210_GPJ4_BASE+0x00);
  
  tmp = readl(S5PV210_GPJ4_BASE+0x04);
  tmp = tmp | (0x1<<3);
  __raw_writel(tmp, S5PV210_GPJ4_BASE+0x04);
//Apollo -

	tmp = ((0<<28)|(0<<24)|(5<<16)|(0<<12)|(0<<8)|(0<<4)|(0<<0));
	__raw_writel(tmp, (S5P_SROM_BW+0x18));

	tmp = __raw_readl(S5P_SROM_BW);
	tmp &= ~(0xf << 4);

#if defined(CONFIG_DM9000_16BIT)
	tmp |= (0x1 << 4); /* dm9000 16bit */
#endif

	__raw_writel(tmp, S5P_SROM_BW);

	tmp = __raw_readl(S5PV210_MP01CON);
	tmp &= ~(0xf << 4);
	tmp |= (2 << 4);

	__raw_writel(tmp, S5PV210_MP01CON);
}
#endif

#define S5PV210_GPD_0_0_TOUT_0  (0x2)
#define S5PV210_GPD_0_1_TOUT_1  (0x2 << 4)
#define S5PV210_GPD_0_2_TOUT_2  (0x2 << 8)
#define S5PV210_GPD_0_3_TOUT_3  (0x2 << 12)

#ifdef CONFIG_FB_S3C_LTE480WV
static struct s3cfb_lcd lte480wv = {
	.width = S5PV210_LCD_WIDTH,
	.height = S5PV210_LCD_HEIGHT,
	.bpp = 32,
	.freq = 60,

	.timing = {
		.h_fp = 8,
		.h_bp = 13,
		.h_sw = 3,
		.v_fp = 5,
		.v_fpe = 1,
		.v_bp = 7,
		.v_bpe = 1,
		.v_sw = 1,
	},
	.polarity = {
		.rise_vclk = 0,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
};

static void lte480wv_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}

static int lte480wv_backlight_on(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PV210_GPD0(3), "GPD0");

	if (err) {
		printk(KERN_ERR "failed to request GPD0 for "
			"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(3), 1);

	s3c_gpio_cfgpin(S5PV210_GPD0(3), S5PV210_GPD_0_3_TOUT_3);

	gpio_free(S5PV210_GPD0(3));
	return 0;
}

static int lte480wv_backlight_off(struct platform_device *pdev, int onoff)
{
	int err;

	err = gpio_request(S5PV210_GPD0(3), "GPD0");

	if (err) {
		printk(KERN_ERR "failed to request GPD0 for "
				"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(3), 0);
	gpio_free(S5PV210_GPD0(3));
	return 0;
}

static int lte480wv_reset_lcd(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PV210_GPH0(6), "GPH0");
	if (err) {
		printk(KERN_ERR "failed to request GPH0 for "
				"lcd reset control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPH0(6), 1);
	mdelay(100);

	gpio_set_value(S5PV210_GPH0(6), 0);
	mdelay(10);

	gpio_set_value(S5PV210_GPH0(6), 1);
	mdelay(10);

	gpio_free(S5PV210_GPH0(6));

	return 0;
}

static struct s3c_platform_fb lte480wv_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,

	.lcd = &lte480wv,
	.cfg_gpio	= lte480wv_cfg_gpio,
	.backlight_on	= lte480wv_backlight_on,
	.backlight_onoff    = lte480wv_backlight_off,
	.reset_lcd	= lte480wv_reset_lcd,
};
#endif /* CONFIG_FB_S3C_LTE480WV */

#ifdef CONFIG_FB_S3C_SEKEDE_BL
#ifdef CONFIG_FB_S3C_PIXELQI
struct s3cfb_lcd sekede_lcd = {
	.width = S5PV210_LCD_WIDTH,
	.height = S5PV210_LCD_HEIGHT,
	.bpp = 32,
	.freq = 60,

	.timing = {
		.h_fp = 48,
		.h_bp = 80,
		.h_sw = 32,
		.v_fp = 3,
		.v_fpe = 1,
		.v_bp = 6,
		.v_bpe = 1,
		.v_sw = 10,
	},

	.polarity = {
		.rise_vclk = 0,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},

};
#endif /* CONFIG_FB_S3C_PIXELQI */

#ifdef CONFIG_FB_S3C_LG97
struct s3cfb_lcd sekede_lcd = {
	.width = S5PV210_LCD_WIDTH,
	.height = S5PV210_LCD_HEIGHT,
	.bpp = 32,
	.freq = 50,

	.timing = {
		.h_fp = 256,
		.h_bp = 256,
		.h_sw = 256,
		.v_fp = 16,
		.v_fpe = 1,
		.v_bp = 6,
		.v_bpe = 1,
		.v_sw = 10,
	},

	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
};
#endif /* CONFIG_FB_S3C_LG97 */

#ifdef CONFIG_FB_S3C_LG97_NOCPLD
struct s3cfb_lcd sekede_lcd = {
	.width = S5PV210_LCD_WIDTH,
	.height = S5PV210_LCD_HEIGHT,
	.bpp = 32,
	.freq = 50,

	.timing = {
		.h_fp = 48,
		.h_bp = 32,
		.h_sw = 80,
		.v_fp = 3,
		.v_fpe = 1,
		.v_bp = 15,
		.v_bpe = 1,
		.v_sw = 4,
	},

	.polarity = {
		.rise_vclk = 0,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
};
#endif /* CONFIG_FB_S3C_LG97_NOCPLD */

static void skdv210_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}
/*
 * GPH2_5 for LCD power control, ACT_H
 * GPH2_3 for LED power control, ACT_H
 * GPJ2_3 for LVDS power on, ACT_H
 * GPH0_6 for LVDS LED enable, ACT_H
 * GPD0_0 for adjust brightness of LCD backlight, ACT_H
 */
static int last_mode = 1;
void skdv210_lcd_led_onoff(int onoff, int ifsys)
{
 	int data;
	int err;
 	
//printk("%s test+++++++++++++\n", __FUNCTION__);
#if defined(CONFIG_S5PV210_A8388_V1)|| defined(CONFIG_S5PV210_A8388_V2)
  msleep(100);
#endif
 	data =  onoff;
#ifdef GPIO_PIXELQI_MODE
 	if(gpio_get_value(GPIO_PIXELQI_MODE) != GPIO_PIXELQI_ACTIVE)
 	  data = 0;
#endif
  if((ifsys == 0) && (last_mode == 0) && (onoff == 1))
    return;

  if(ifsys == 1)
 	  last_mode = onoff;

 //lcd led power on	
	err = gpio_request(GPIO_LCD_LED_PWR, "GPH2");
	if (err) {
		printk(KERN_ERR "failed to request GPH2_3 for lcd backlight control\n");
		return;
	}
	gpio_direction_output(GPIO_LCD_LED_PWR, data);
	gpio_free(GPIO_LCD_LED_PWR);

	//lvds led power on	
#ifdef CONFIG_FB_S3C_PIXELQI
	gpio_request(GPIO_PIXELQI_LED_PWR, "GPH0");
	if (err) {
		printk(KERN_ERR "failed to request GPH0_6 for LVDS power control\n");
		return;
	}
	gpio_direction_output(GPIO_PIXELQI_LED_PWR, data);
	gpio_free(GPIO_PIXELQI_LED_PWR);
#endif

	//pwm on
	err = gpio_request(S5PV210_GPD0(0), "GPD0");
	if (err) {
		printk(KERN_ERR "failed to request GPD0 for lcd pwm control\n");
		return;
	}
	gpio_direction_output(S5PV210_GPD0(0), data);
	s3c_gpio_cfgpin(S5PV210_GPD0(0), S5PV210_GPD_0_0_TOUT_0);
	gpio_free(S5PV210_GPD0(0));
}
EXPORT_SYMBOL(skdv210_lcd_led_onoff);

void skdv210_lcd_ofoff(int onoff)
{
	int err;
	
	//lcd power on
	err = gpio_request(GPIO_LCD_PWR, "GPH2");
	if (err) {
		printk(KERN_ERR "failed to request GPH2_5 for lcd power control\n");
		return;
	}
	gpio_direction_output(GPIO_LCD_PWR, onoff);
	gpio_free(GPIO_LCD_PWR);

#if defined(CONFIG_FB_S3C_LG97) || defined(CONFIG_FB_S3C_LG97_NOCPLD) 
	//lvds chip power on
	gpio_request(GPIO_LVDS_PWR, "GPJ2");
	if (err) {
		printk(KERN_ERR "failed to request GPJ2_3 for LVDS power control\n");
		return;
	}
	gpio_direction_output(GPIO_LVDS_PWR, onoff);
	gpio_free(GPIO_LVDS_PWR);

//LVDS_RESET
#if defined(CONFIG_S5PV210_S9)
 	err = gpio_request(GPIO_CPLD_RESET, "cpld_reset");
 	if(onoff == 1)
 	{
 	  gpio_direction_output(GPIO_CPLD_RESET, 1);
 	  mdelay(5);
// 	  mdelay(600);
 	  gpio_direction_output(GPIO_CPLD_RESET, 0);
 	}
 	else
 	  gpio_direction_output(GPIO_CPLD_RESET, 1);
 	  
	gpio_free(GPIO_CPLD_RESET);
#endif /* CONFIG_S5PV210_S9 */
#endif /* CONFIG_FB_S3C_LG97 */

}

void tp_power(int onoff)
{
#ifdef GPIO_TP_PWR
	gpio_request(GPIO_TP_PWR, "TS_POWER");
	gpio_request(GPIO_TP_RST, "TS_RST");
  if(onoff)
  	{
    	gpio_direction_output(GPIO_TP_RST, 1);
    	gpio_direction_output(GPIO_TP_PWR, 1);
    	msleep(5);
//    	gpio_direction_output(GPIO_TP_RST, 0);
//    	msleep(5);
    }
  else{
//  	  gpio_direction_output(GPIO_TP_PWR, 0);
//    	gpio_direction_output(GPIO_TP_RST, 0);
  }
	gpio_free(GPIO_TP_PWR);
	gpio_free(GPIO_TP_RST);
#endif
}
EXPORT_SYMBOL(tp_power);

void usb_host_power(int onoff)
{
#ifdef GPIO_USB_5V
  gpio_request(GPIO_USB_5V, "USB_5V");
	gpio_direction_output(GPIO_USB_5V, onoff);
	gpio_free(GPIO_USB_5V);
#endif
}
EXPORT_SYMBOL(usb_host_power);

void usbhub_power(int onoff)
{
	//开启USB HUB

#ifdef GPIO_HUB_PWR
  gpio_request(GPIO_HUB_PWR, "HUB_PWR");
  gpio_direction_output(GPIO_HUB_PWR, onoff);
  gpio_free(GPIO_HUB_PWR);
#endif
printk("usbhub_power %d\n", onoff);
}
EXPORT_SYMBOL(usbhub_power);

static int skdv210_backlight_on(struct platform_device *pdev)
{
  skdv210_lcd_ofoff(1);
  skdv210_lcd_led_onoff(1, 1);
//printk("skdv210_backlight_on ok\n");
#if defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A9)
#if defined(CONFIG_S5PV210_S9)
  usbhub_power(1);
  msleep(100);
#endif
#if defined(CONFIG_S5PV210_A9)
  usbhub_power(0);
  msleep(100);
#endif    
	usb_host_power(1);
  msleep(100);
  tp_power(1);
#endif
	return 0;
}

static int skdv210_backlight_off(struct platform_device *pdev, int onoff)
{
#if defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A9)
//{
//  int i=100000;
//  while(i-->0)
//  {
//    tp_power(0);
//    msleep(500);
//    usbhub_power(0);
//    msleep(1000);
//    printk("power off done\n\n");
//    usbhub_power(1);
//    msleep(500);
//    tp_power(1);
//    msleep(1000);
//    printk("power on done\n\n");
//  }
//}
  tp_power(0);
//  msleep(500);
#endif
  skdv210_lcd_led_onoff(0, 1);
  skdv210_lcd_ofoff(0);
printk("skdv210_backlight_off ok\n");
	return 0;
}

static int skdv210_reset_lcd(struct platform_device *pdev)
{
	return 0;
}

static struct s3c_platform_fb skdv210_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,

	.lcd = &sekede_lcd,
	.cfg_gpio	= skdv210_cfg_gpio,
	.backlight_on	= skdv210_backlight_on,
	.backlight_onoff    = skdv210_backlight_off,
//	.reset_lcd	= skdv210_reset_lcd,
};
#endif /* CONFIG_FB_S3C_SEKEDE_BL */

#ifdef CONFIG_S3C64XX_DEV_SPI

#define SMDK_MMCSPI_CS 0
static struct s3c64xx_spi_csinfo smdk_spi0_csi[] = {
	[SMDK_MMCSPI_CS] = {
		.line = S5PV210_GPB(1),
		.set_level = gpio_set_value,
		.fb_delay = 0x0,
	},
};

static struct s3c64xx_spi_csinfo smdk_spi1_csi[] = {
	[SMDK_MMCSPI_CS] = {
		.line = S5PV210_GPB(5),
		.set_level = gpio_set_value,
		.fb_delay = 0x0,
	},
};

static struct spi_board_info s3c_spi_devs[] __initdata = {
	[0] = {
		.modalias        = "spidev",	/* device node name */
		.mode            = SPI_MODE_0,	/* CPOL=0, CPHA=0 */
		.max_speed_hz    = 10000000,
		/* Connected to SPI-0 as 1st Slave */
		.bus_num         = 0,
		.irq             = IRQ_SPI0,
		.chip_select     = 0,
		.controller_data = &smdk_spi0_csi[SMDK_MMCSPI_CS],
	},
	[1] = {
		.modalias        = "spidev",	/* device node name */
		.mode            = SPI_MODE_0,	/* CPOL=0, CPHA=0 */
		.max_speed_hz    = 10000000,
		/* Connected to SPI-1 as 1st Slave */
		.bus_num         = 1,
		.irq             = IRQ_SPI1,
		.chip_select     = 0,
		.controller_data = &smdk_spi1_csi[SMDK_MMCSPI_CS],
	},
};
#endif

#ifdef CONFIG_HAVE_PWM
struct s3c_pwm_data {
	/* PWM output port */
	unsigned int gpio_no;
	const char  *gpio_name;
	unsigned int gpio_set_value;
};

struct s3c_pwm_data pwm_data[] = {
	{
		.gpio_no    = S5PV210_GPD0(0),
		.gpio_name  = "GPD",
		.gpio_set_value = S5PV210_GPD_0_0_TOUT_0,
	}
//	, {
//		.gpio_no    = S5PV210_GPD0(1),
//		.gpio_name  = "GPD",
//		.gpio_set_value = S5PV210_GPD_0_1_TOUT_1,
//	}, {
//		.gpio_no    = S5PV210_GPD0(2),
//		.gpio_name      = "GPD",
//		.gpio_set_value = S5PV210_GPD_0_2_TOUT_2,
//	}, {
//		.gpio_no    = S5PV210_GPD0(3),
//		.gpio_name      = "GPD",
//		.gpio_set_value = S5PV210_GPD_0_3_TOUT_3,
//	}
};
#endif

#if defined(CONFIG_BACKLIGHT_PWM)
static struct platform_pwm_backlight_data smdk_backlight_data = {
	.pwm_id  = 0,
	.max_brightness = 255,
	.dft_brightness = 255,
	.pwm_period_ns  = 25000,
};

static struct platform_device smdk_backlight_device = {
	.name      = "pwm-backlight",
	.id        = -1,
	.dev        = {
		.parent = &s3c_device_timer[3].dev,
		.platform_data = &smdk_backlight_data,
	},
};

static void __init smdk_backlight_register(void)
{
	int ret;
#ifdef CONFIG_HAVE_PWM
	int i, ntimer;

	/* Timer GPIO Set */
	ntimer = ARRAY_SIZE(pwm_data);
	for (i = 0; i < ntimer; i++) {
		if (gpio_is_valid(pwm_data[i].gpio_no)) {
			ret = gpio_request(pwm_data[i].gpio_no,
				pwm_data[i].gpio_name);
			if (ret) {
				printk(KERN_ERR "failed to get GPIO for PWM\n");
				return;
			}
			s3c_gpio_cfgpin(pwm_data[i].gpio_no,
				pwm_data[i].gpio_set_value);

			gpio_free(pwm_data[i].gpio_no);
		}
	}
#endif
	ret = platform_device_register(&smdk_backlight_device);
	if (ret)
		printk(KERN_ERR "smdk: failed to register backlight device: %d\n", ret);
}
#endif

#ifdef CONFIG_S5P_ADC
static struct s3c_adc_mach_info s3c_adc_platform __initdata = {
	/* s5pc110 support 12-bit resolution */
	.delay  = 10000,
	.presc  = 49,
	.resolution = 12,
};
#endif

/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * This function also called at fimc_init_camera()
 * Do optimization for cameras on your platform.
*/
#ifdef CAM_ITU_CH_A
static int smdkv210_cam0_power(int onoff)
{
	int err;
	/* Camera A */
	err = gpio_request(GPIO_PS_VOUT, "GPH0");
	if (err)
		printk(KERN_ERR "failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(GPIO_PS_VOUT, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_PS_VOUT, 0);
	gpio_direction_output(GPIO_PS_VOUT, 1);
	gpio_free(GPIO_PS_VOUT);

	return 0;
}
#else
static int smdkv210_cam1_power(int onoff)
{
	int err;

	/* S/W workaround for the SMDK_CAM4_type board
	 * When SMDK_CAM4 type module is initialized at power reset,
	 * it needs the cam_mclk.
	 *
	 * Now cam_mclk is set as below, need only set the gpio mux.
	 * CAM_SRC1 = 0x0006000, CLK_DIV1 = 0x00070400.
	 * cam_mclk source is SCLKMPLL, and divider value is 8.
	*/

	/* gpio mux select the cam_mclk */
	err = gpio_request(GPIO_PS_ON, "GPJ1");
	if (err)
		printk(KERN_ERR "failed to request GPJ1 for CAM_2V8\n");

	s3c_gpio_setpull(GPIO_PS_ON, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_PS_ON, (0x3<<16));


	/* Camera B */
	err = gpio_request(GPIO_BUCK_1_EN_A, "GPH0");
	if (err)
		printk(KERN_ERR "failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(GPIO_BUCK_1_EN_A, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_BUCK_1_EN_A, 0);
	gpio_direction_output(GPIO_BUCK_1_EN_A, 1);

	udelay(1000);

	gpio_free(GPIO_PS_ON);
	gpio_free(GPIO_BUCK_1_EN_A);

	return 0;
}
#endif

#ifdef CONFIG_VIDEO_S5K4BA
static int s5k5ba_power_en(int onoff)
{
	if (onoff) {
#ifdef CAM_ITU_CH_A
		smdkv210_cam0_power(onoff);
#else
		smdkv210_cam1_power(onoff);
#endif
	} else {
#ifdef CAM_ITU_CH_A
		smdkv210_cam0_power(onoff);
#else
		smdkv210_cam1_power(onoff);
#endif
	}
	return 0;
}
#endif /* CONFIG_VIDEO_S5K4BA */

#ifdef CONFIG_VIDEO_S5K4EA
/* Set for MIPI-CSI Camera module Power Enable */
static int smdkv210_mipi_cam_pwr_en(int enabled)
{
	int err;

	err = gpio_request(S5PV210_GPH1(2), "GPH1");
	if (err)
		printk(KERN_ERR "#### failed to request(GPH1)for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPH1(2), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH1(2), enabled);
	gpio_free(S5PV210_GPH1(2));

	return 0;
}

/* Set for MIPI-CSI Camera module Reset */
static int smdkv210_mipi_cam_rstn(int enabled)
{
	int err;

	err = gpio_request(S5PV210_GPH0(3), "GPH0");
	if (err)
		printk(KERN_ERR "#### failed to reset(GPH0) for MIPI CAM\n");

	s3c_gpio_setpull(S5PV210_GPH0(3), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH0(3), enabled);
	gpio_free(S5PV210_GPH0(3));

	return 0;
}

/* MIPI-CSI Camera module Power up/down sequence */
static int smdkv210_mipi_cam_power(int on)
{
	if (on) {
		smdkv210_mipi_cam_pwr_en(1);
		mdelay(5);
		smdkv210_mipi_cam_rstn(1);
	} else {
		smdkv210_mipi_cam_rstn(0);
		mdelay(5);
		smdkv210_mipi_cam_pwr_en(0);
	}
	return 0;
}
#endif

#ifdef CONFIG_VIDEO_S5K4BA
static struct s5k4ba_platform_data s5k4ba_plat = {
	.default_width = 800,
	.default_height = 600,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 44000000,
	.is_mipi = 0,
};

static struct i2c_board_info  s5k4ba_i2c_info = {
	I2C_BOARD_INFO("S5K4BA", 0x2d),
	.platform_data = &s5k4ba_plat,
};

static struct s3c_platform_camera s5k4ba = {
	#ifdef CAM_ITU_CH_A
	.id		= CAMERA_PAR_A,
	#else
	.id		= CAMERA_PAR_B,
	#endif
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &s5k4ba_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "mout_mpll",
	/* .srclk_name	= "xusbxti", */
	.clk_name	= "sclk_cam1",
	.clk_rate	= 44000000,
	.line_length	= 1920,
	.width		= 800,
	.height		= 600,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 800,
		.height	= 600,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= s5k5ba_power_en,
};
#endif /* CONFIG_VIDEO_S5K4BA */

/* 2 MIPI Cameras */
#ifdef CONFIG_VIDEO_S5K4EA
static struct s5k4ea_platform_data s5k4ea_plat = {
	.default_width = 1920,
	.default_height = 1080,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
};

static struct i2c_board_info  s5k4ea_i2c_info = {
	I2C_BOARD_INFO("S5K4EA", 0x2d),
	.platform_data = &s5k4ea_plat,
};

static struct s3c_platform_camera s5k4ea = {
	.id		= CAMERA_CSI_C,
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &s5k4ea_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_cam0",
	/* .clk_name	= "sclk_cam1", */
	.clk_rate	= 48000000,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= smdkv210_mipi_cam_power,
};
#endif /* CONFIG_VIDEO_S5K4EA */

#ifdef CONFIG_VIDEO_GC2015
#define GC2015_DEFAULT_WIDTH   800
#define GC2015_DEFAULT_HEIGHT  600
void GC2015_power(int onoff)
{
//  gpio_request(GPIO_GC2015_CAMERA_PWR, "GC2015_POWER");
//  gpio_request(GPIO_GC2015_CAMERA_PWRDWN, "GC2015_PWRDWN");
//  gpio_request(GPIO_GC2015_CAMERA_RESET, "GC2015_RESET");
  s3c_gpio_cfgpin(GPIO_GC2015_CAMERA_PWR, S3C_GPIO_OUTPUT);
  s3c_gpio_setpull(GPIO_GC2015_CAMERA_PWR, S3C_GPIO_PULL_UP);
  
  s3c_gpio_cfgpin(GPIO_GC2015_CAMERA_PWRDWN, S3C_GPIO_OUTPUT);
  s3c_gpio_setpull(GPIO_GC2015_CAMERA_PWRDWN, S3C_GPIO_PULL_UP);
  
  s3c_gpio_cfgpin(GPIO_GC2015_CAMERA_RESET, S3C_GPIO_OUTPUT);
  s3c_gpio_setpull(GPIO_GC2015_CAMERA_RESET, S3C_GPIO_PULL_UP);
  if(onoff)
  {
    printk("GC2015_power on\n");
//	  gpio_direction_output(GPIO_GC2015_CAMERA_PWR, 1);
//    msleep(10);  
//	  gpio_direction_output(GPIO_GC2015_CAMERA_PWRDWN, 0);
//	  msleep(5);
//	  gpio_direction_output(GPIO_GC2015_CAMERA_RESET, 1);
//	  msleep(5);

			gpio_set_value(GPIO_GC2015_CAMERA_PWRDWN, 0);
		  msleep(5);
			gpio_set_value(GPIO_GC2015_CAMERA_PWR, 1);
	    msleep(10);  
			gpio_set_value(GPIO_GC2015_CAMERA_RESET, 0);
		  msleep(50);
			gpio_set_value(GPIO_GC2015_CAMERA_RESET, 1);
		  msleep(5);

 	}
	else
	{
    printk("GC2015_power off\n");
//	  gpio_direction_output(GPIO_GC2015_CAMERA_PWRDWN, 1);
//	  msleep(5);
//	  gpio_direction_output(GPIO_GC2015_CAMERA_RESET, 0);
//	  msleep(5);
//	  gpio_direction_output(GPIO_GC2015_CAMERA_PWR, 0);
			gpio_set_value(GPIO_GC2015_CAMERA_PWRDWN, 1);
		  msleep(5);
			gpio_set_value(GPIO_GC2015_CAMERA_PWR, 0);
	    msleep(10);  
			gpio_set_value(GPIO_GC2015_CAMERA_RESET, 1);
		  msleep(5);
	}

//	gpio_free(GPIO_GC2015_CAMERA_PWR);
//	gpio_free(GPIO_GC2015_CAMERA_PWRDWN);
//	gpio_free(GPIO_GC2015_CAMERA_RESET);
}
EXPORT_SYMBOL(GC2015_power);

static struct GC2015_platform_data GC2015_plat = {
	.default_width = GC2015_DEFAULT_WIDTH,
	.default_height = GC2015_DEFAULT_HEIGHT,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info  GC2015_i2c_info = {
	I2C_BOARD_INFO("GC2015", 0x30),
	.platform_data = &GC2015_plat,
};

static struct s3c_platform_camera GC2015 = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
#if defined(CONFIG_S5PV210_DEVBOARD)
	.i2c_busnum	= 2,
#endif
#if defined(CONFIG_S5PV210_A8388_V1) || defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A8388_V2) || defined(CONFIG_S5PV210_A9)
	.i2c_busnum	= 1,
#endif
	.info		= &GC2015_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_cam1",
	.clk_rate	= 48000000,
	.line_length	= 1200,
	.width		= GC2015_DEFAULT_WIDTH,
	.height		= GC2015_DEFAULT_HEIGHT,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= GC2015_DEFAULT_WIDTH,
		.height	= GC2015_DEFAULT_HEIGHT,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= GC2015_power,
};
#endif /* CONFIG_VIDEO_GC2015 */

/* Interface setting */
static struct s3c_platform_fimc fimc_plat_lsi = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_fimc",
	.lclk_name	= "sclk_fimc_lclk",
	.clk_rate	= 166750000,
#if defined(CONFIG_VIDEO_S5K4EA)
	.default_cam	= CAMERA_CSI_C,
#else
#ifdef CAM_ITU_CH_A
	.default_cam	= CAMERA_PAR_A,
#else
	.default_cam	= CAMERA_PAR_B,
#endif
#endif
	.camera		= {
#ifdef CONFIG_VIDEO_S5K4ECGX
			&s5k4ecgx,
#endif
#ifdef CONFIG_VIDEO_S5KA3DFX
			&s5ka3dfx,
#endif
#ifdef CONFIG_VIDEO_S5K4BA
			&s5k4ba,
#endif
#ifdef CONFIG_VIDEO_S5K4EA
			&s5k4ea,
#endif

#ifdef CONFIG_VIDEO_GC2015
			&GC2015,
#endif
	},
	.hw_ver		= 0x43,
};

#ifdef CONFIG_VIDEO_JPEG_V2
static struct s3c_platform_jpeg jpeg_plat __initdata = {
	.max_main_width	= 2592,
	.max_main_height	= 1936,
	.max_thumb_width	= 160,
	.max_thumb_height	= 120,
};
#endif

/* I2C0 */
static struct i2c_board_info i2c_devs0[] __initdata = {
#ifdef CONFIG_SND_SOC_WM8580
	{	I2C_BOARD_INFO("wm8580", 0x1b),	},
#endif
#ifdef CONFIG_TOUCHSCREEN_GT801
	{	I2C_BOARD_INFO("Goodix-TS", 0x55),},
#endif
#ifdef CONFIG_A9_TP_GT8015
	{	I2C_BOARD_INFO("Goodix-TS", 0x55),},
#endif
	{I2C_BOARD_INFO("pixcir_ts",0x5c),
	  .irq = S5P_EINT(15),
	},
};

/* I2C1 */
static struct i2c_board_info i2c_devs1[] __initdata = {
#ifdef CONFIG_VIDEO_TV20
	{	I2C_BOARD_INFO("s5p_ddc", (0x74>>1)),	},
#endif
#ifdef CONFIG_RTC_DRV_PCF8563
  { I2C_BOARD_INFO("pcf8563", 0x51), },
#endif
#ifdef CONFIG_SND_SOC_WM8960
	{	I2C_BOARD_INFO("wm8960", 0x1A), },
#endif
};

/* I2C2 */
static struct i2c_board_info i2c_devs2[] __initdata = {
	{
#if defined(CONFIG_SMDKC110_REV03) || defined(CONFIG_SMDKV210_REV02)
		/* The address is 0xCC used since SRAD = 0 */
		I2C_BOARD_INFO("max8998", (0xCC >> 1)),
		.platform_data = &max8998_pdata,
#else
		/* The address is 0xCC used since SRAD = 0 */
		I2C_BOARD_INFO("max8698", (0xCC >> 1)),
		.platform_data = &max8698_pdata,
#endif
	},
#ifdef CONFIG_SENSORS_MMA8452
	{ I2C_BOARD_INFO("mma8452", 0x1D), },
#endif
};

#define S5PV210_PS_HOLD_CONTROL_REG (S3C_VA_SYS+0xE81C)
static void smdkc110_power_off(void)
{
  if(cpy_codec != NULL)
  {
  	wm8960_poweroff(cpy_codec);
	  mdelay(10);
	}
	/* PS_HOLD output High --> Low */
	writel(readl(S5PV210_PS_HOLD_CONTROL_REG) & 0xFFFFFEFF,
			S5PV210_PS_HOLD_CONTROL_REG);

printk("system power off\n");
  s3c_gpio_cfgpin(GPIO_SYS_POWER, S3C_GPIO_OUTPUT);
  s3c_gpio_setpull(GPIO_SYS_POWER, S3C_GPIO_PULL_NONE);
  gpio_set_value(GPIO_SYS_POWER, GPIO_LEVEL_LOW);
  msleep(100);
  arch_reset('r', NULL);
	while (1);
}

#ifdef CONFIG_BATTERY_S3C
struct platform_device sec_device_battery = {
	.name   = "skdv210_battery",
	.id = -1,
};
#endif

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_gpu1_pdata = {
	.name = "pmem_gpu1",
	.no_allocator = 1,
	.cached = 1,
	.buffered = 1,
	.start = 0,
	.size = 0,
};

static struct platform_device pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &pmem_gpu1_pdata },
};

static void __init android_pmem_set_platdata(void)
{
	pmem_gpu1_pdata.start =
		(u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_GPU1, 0);
	pmem_gpu1_pdata.size =
		(u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_GPU1, 0);
}
#endif

//Apollo +
#ifdef CONFIG_SWITCH_HEADPHONE
#include <linux/switch.h>
static struct gpio_switch_platform_data hp_switch_data = {
	.name = "h2w",
	.gpio = GPIO_HEADPHONE,
	.name_on = "PULL IN",
	.name_off = "PULL OUT",
  .state_on = "0",
	.state_off = "1",
};
			
struct platform_device hp_switch_device = {
	.name	= "switch-hp",
	.dev = { .platform_data    = &hp_switch_data },
};
#endif

#ifdef CONFIG_WLAN
#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wifi_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static unsigned int wlan_sdio_on_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, GPIO_WLAN_SDIO_CLK_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, GPIO_WLAN_SDIO_CMD_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, GPIO_WLAN_SDIO_D0_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, GPIO_WLAN_SDIO_D1_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, GPIO_WLAN_SDIO_D2_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, GPIO_WLAN_SDIO_D3_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

static unsigned int wlan_sdio_off_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

/*
  WLAN-on  BT-on : WL_REG_ON(high) BT_REG_ON(high) WL_RST_N(high) BT_RST_N(high)
  WLAN-off BT-off: WL_REG_ON(low)  BT_REG_ON(low)  WL_RST_N(low)  BT_RST_N(low)
  WLAN-on  BT-off: WL_REG_ON(high) BT_REG_ON(high) WL_RST_N(high) BT_RST_N(low)
  WLAN-off BT-on : WL_REG_ON(high) BT_REG_ON(high) WL_RST_N(low)  BT_RST_N(high)
  flag  (0-wlan 1-bt)
  on    (0-off  1-on)
*/
static unsigned int wlan_power_state = 0;
void wlan_setup_power(int flag, int on)
{
  {//config gpio state when  system sleep
    unsigned int tmp_value;
    tmp_value = __raw_readl(GPIO_WIFI_POWER_CPDN);
    __raw_writel(tmp_value|GPIO_WIFI_POWER_CPDN_V, GPIO_WIFI_POWER_CPDN);

    tmp_value = __raw_readl(GPIO_WLAN_BT_EN_CPDN);
    __raw_writel(tmp_value|GPIO_WLAN_BT_EN_CPDN_V, GPIO_WLAN_BT_EN_CPDN);

    tmp_value = __raw_readl(GPIO_WLAN_nRST_CPDN);
    __raw_writel(tmp_value|GPIO_WLAN_nRST_CPDN_V, GPIO_WLAN_nRST_CPDN);

#ifndef GPIO_WIFI_SLEEP
    tmp_value = __raw_readl(GPIO_BT_nRST_CPDN);
    __raw_writel(tmp_value|GPIO_BT_nRST_CPDN_V, GPIO_BT_nRST_CPDN);
#else
    tmp_value = __raw_readl(GPIO_WIFI_SLEEP_CPDN);
    __raw_writel(tmp_value|GPIO_WIFI_SLEEP_CPDN_V, GPIO_WIFI_SLEEP_CPDN);
#endif
  }
	if(!check_wifi_type()){
#ifdef CONFIG_BCM4329_MODULE
  if(on)
  {
    //WIFI_PWR_Ctrl on
    s3c_gpio_cfgpin(GPIO_WIFI_POWER, S3C_GPIO_OUTPUT);
    s3c_gpio_setpull(GPIO_WIFI_POWER, S3C_GPIO_PULL_NONE);
    gpio_set_value(GPIO_WIFI_POWER, GPIO_LEVEL_HIGH);
    msleep(60);
    
    s3c_gpio_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_OUTPUT);
    s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
    if(wlan_power_state == 0)
    {
      gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
      msleep(60);
      gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);
      msleep(60);
    }
    gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
    s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
    msleep(60);

    //wlan power on
    if(flag == 0)
    {
#ifdef GPIO_WLAN_nRST
      s3c_gpio_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_OUTPUT);
      s3c_gpio_setpull(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
      gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_HIGH);
      msleep(60);
      gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);
      msleep(60);
      gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_HIGH);
      s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT1);
      s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
#endif /* GPIO_WLAN_nRST */
      wlan_power_state |= 0x01;
      printk("%s: wlan on\n", __FUNCTION__);
    }
 
    //bt power on
    if(flag == 1)
    {
#ifdef GPIO_BT_nRST
      s3c_gpio_cfgpin(GPIO_BT_nRST, S3C_GPIO_OUTPUT);
      s3c_gpio_setpull(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);
      gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_HIGH);
      msleep(60);
      gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_LOW);
      msleep(60);
      gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_HIGH);
      s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT1);
      s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);
#endif /* GPIO_BT_nRST */
      wlan_power_state |= 0x02;
      printk("%s: bt on\n", __FUNCTION__);
    }
  }
  else
  {
    //wlan power off
    if(flag == 0)
    {
#ifdef GPIO_WLAN_nRST
      s3c_gpio_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_OUTPUT);
      s3c_gpio_setpull(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
      gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);
      s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT0);
      s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
#endif
      wlan_power_state &= ~(0x01);
      printk("%s: wlan off\n", __FUNCTION__);
    }
 
    //bt power off
    if(flag == 1)
    {
#ifdef GPIO_BT_nRST
      s3c_gpio_cfgpin(GPIO_BT_nRST, S3C_GPIO_OUTPUT);
      s3c_gpio_setpull(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);
      gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_LOW);
      s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT0);
      s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);
#endif /* GPIO_BT_nRST */
      wlan_power_state &= ~(0x02);
      printk("%s: bt off\n", __FUNCTION__);
    }
    
    if(wlan_power_state == 0)
    {
#ifdef GPIO_WLAN_nRST
      gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);
#endif
#ifdef GPIO_BT_nRST
      gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_LOW);
#endif /* GPIO_BT_nRST */
			gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);
      gpio_set_value(GPIO_WIFI_POWER, GPIO_LEVEL_LOW);
			s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT0);
			s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
      printk("%s: wifi-bt chip power off\n", __FUNCTION__);
    }
  }
#endif /* CONFIG_BCM4329_MODULE */
}else{
#ifdef CONFIG_MRVL8787
  if(on)
  {
    //WIFI_PWR_Ctrl on
    s3c_gpio_cfgpin(GPIO_WIFI_POWER, S3C_GPIO_OUTPUT);
    s3c_gpio_setpull(GPIO_WIFI_POWER, S3C_GPIO_PULL_NONE);
    gpio_set_value(GPIO_WIFI_POWER, GPIO_LEVEL_HIGH);
    msleep(60);
    
    s3c_gpio_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_OUTPUT);
    s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
    if(wlan_power_state == 0)
    {
      gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
      msleep(60);
      gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);
      msleep(60);
    }
    gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
    s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
    msleep(60);

#ifdef GPIO_WLAN_nRST
    if(wlan_power_state == 0)
    {
      s3c_gpio_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_OUTPUT);
      s3c_gpio_setpull(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
      gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_HIGH);
      msleep(60);
      gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);
      msleep(60);
      gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_HIGH);
      s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT1);
      s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
    }
#endif /* GPIO_WLAN_nRST */
    //wlan power on
    if(flag == 0)
    {
      wlan_power_state |= 0x01;
      printk("%s: wlan on\n", __FUNCTION__);
    }
 
    //bt power on
    if(flag == 1)
    {
      wlan_power_state |= 0x02;
      printk("%s: bt on\n", __FUNCTION__);
    }
  }
  else
  {
    //wlan power off
    if(flag == 0)
    {
      wlan_power_state &= ~(0x01);
      printk("%s: wlan off\n", __FUNCTION__);
    }
 
    //bt power off
    if(flag == 1)
    {
      wlan_power_state &= ~(0x02);
      printk("%s: bt off\n", __FUNCTION__);
    }
    
    if(wlan_power_state == 0)
    {
#ifdef GPIO_WLAN_nRST
      s3c_gpio_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_OUTPUT);
      s3c_gpio_setpull(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
      gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);
      s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT0);
      s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
#endif
			gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);
      gpio_set_value(GPIO_WIFI_POWER, GPIO_LEVEL_LOW);
			s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT0);
			s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
      printk("%s: wifi-bt chip power off\n", __FUNCTION__);
    }
  }
#endif /* CONFIG_MRVL8787 */
}
}
EXPORT_SYMBOL(wlan_setup_power);

#ifdef CONFIG_MRVL8787
int get_wlan_power_state(void)
{
  return wlan_power_state;
}
EXPORT_SYMBOL(get_wlan_power_state);
#endif

static int wlan_power_en(int onoff)
{
  wlan_setup_power(0, onoff);
	if (onoff) {

		s3c_gpio_cfgpin(GPIO_WLAN_HOST_WAKE, S3C_GPIO_SFN(GPIO_WLAN_HOST_WAKE_AF));
		s3c_gpio_setpull(GPIO_WLAN_HOST_WAKE, S3C_GPIO_PULL_DOWN);

		s3c_gpio_cfgpin(GPIO_WLAN_WAKE, S3C_GPIO_SFN(GPIO_WLAN_WAKE_AF));
		s3c_gpio_setpull(GPIO_WLAN_WAKE, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_WAKE, GPIO_LEVEL_LOW);

		msleep(80);
	}
	return 0;
}

static int wlan_reset_en(int onoff)
{
#ifdef GPIO_WLAN_nRST
  printk("%s %d\n", __FUNCTION__,onoff);
	gpio_set_value(GPIO_WLAN_nRST, onoff ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
#endif
	return 0;
}

int wlan_carddetect_en(int onoff)
{
	u32 i;
	u32 sdio;

	if (onoff) {
		for (i = 0; i < ARRAY_SIZE(wlan_sdio_on_table); i++) {
			sdio = wlan_sdio_on_table[i][0];
			s3c_gpio_cfgpin(sdio, S3C_GPIO_SFN(wlan_sdio_on_table[i][1]));
			s3c_gpio_setpull(sdio, wlan_sdio_on_table[i][3]);
			if (wlan_sdio_on_table[i][2] != GPIO_LEVEL_NONE)
				gpio_set_value(sdio, wlan_sdio_on_table[i][2]);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(wlan_sdio_off_table); i++) {
			sdio = wlan_sdio_off_table[i][0];
			s3c_gpio_cfgpin(sdio, S3C_GPIO_SFN(wlan_sdio_off_table[i][1]));
			s3c_gpio_setpull(sdio, wlan_sdio_off_table[i][3]);
			if (wlan_sdio_off_table[i][2] != GPIO_LEVEL_NONE)
				gpio_set_value(sdio, wlan_sdio_off_table[i][2]);
		}
	}
	udelay(5);

#if defined(CONFIG_S5PV210_DEVBOARD)
	sdhci_s3c_force_presence_change(&s3c_device_hsmmc1);
#endif
#if defined(CONFIG_S5PV210_A8388_V1) || defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A8388_V2) || defined(CONFIG_S5PV210_A9)
	sdhci_s3c_force_presence_change(&s3c_device_hsmmc3);
#endif

	return 0;
}
EXPORT_SYMBOL(wlan_carddetect_en);

static struct resource wifi_resources[] = {
	[0] = {
		.name	= "bcm4329_wlan_irq",
		.start	= GPIO_WLAN_IRQ,
		.end	= GPIO_WLAN_IRQ,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct wifi_mem_prealloc wifi_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static void *skdv210_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wifi_mem_array[section].size < size)
		return NULL;

	return wifi_mem_array[section].mem_ptr;
}

int __init skdv210_init_wifi_mem(void)
{
	int i;
	int j;

	for (i = 0 ; i < WLAN_SKB_BUF_NUM ; i++) {
		wlan_static_skb[i] = dev_alloc_skb(
				((i < (WLAN_SKB_BUF_NUM / 2)) ? 4096 : 8192));

		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wifi_mem_array[i].mem_ptr =
				kmalloc(wifi_mem_array[i].size, GFP_KERNEL);

		if (!wifi_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wifi_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}

static struct wifi_platform_data wifi_pdata = {
	.set_power		= wlan_power_en,
	.set_reset		= wlan_reset_en,
	.set_carddetect		= wlan_carddetect_en,
	.mem_prealloc		= skdv210_mem_prealloc,
};

static struct platform_device sec_device_wifi = {
	.name			= "bcm4329_wlan",
#if defined(CONFIG_S5PV210_DEVBOARD)
	.id			= 1,
#endif
#if defined(CONFIG_S5PV210_A8388_V1) || defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A8388_V2)
	.id			= 3,
#endif
	.num_resources		= ARRAY_SIZE(wifi_resources),
	.resource		= wifi_resources,
	.dev			= {
		.platform_data = &wifi_pdata,
	},
};

static struct platform_device	sec_device_mrvl8787 = {
	.name = "mrvl8787",
	.id	  = -1,
};

static struct platform_device	sec_device_rfkill = {
	.name = "bt_rfkill",
	.id	  = -1,
};

static struct platform_device	sec_device_btsleep = {
	.name = "bt_sleep",
	.id	  = -1,
};
#endif /* CONFIG_WLAN */

//static struct platform_device sec_device_bat = {
//	.name			= "skdv210_battery",
//	.id			= -1,
//};

#ifdef GPIO_VIBRATOR_PWR
static struct timed_gpio timed_gpios[] = {
  {
    .name = "vibrator",
    .gpio = GPIO_VIBRATOR_PWR,
    .max_timeout = 15000,
    .active_low = GPIO_VIBRATOR_ACTIVE,
  },
};

static struct timed_gpio_platform_data timed_gpio_data = {
  .num_gpios	= ARRAY_SIZE(timed_gpios),
  .gpios		= timed_gpios,
};

static struct platform_device skdv210_timed_gpios = {
  .name   = "timed-gpio",
  .id     = -1,
  .dev    = {
    .platform_data = &timed_gpio_data,
  },
};
#endif /* GPIO_VIBRATOR_PWR */

#if defined(CONFIG_S5PV210_A8388_V1) || defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A8388_V2) || defined(CONFIG_S5PV210_A9)
static struct gpio_keys_button a8388_buttons[] = {
	{
		.gpio			= GPIO_KEY_POWER,
		.code			= KEY_POWER,
		.desc			= "Power",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
	{
		.gpio			= GPIO_KEY_HOME,
		.code			= KEY_HOME,
		.desc			= "Home",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
#if defined(CONFIG_S5PV210_A8388_V1) || defined(CONFIG_S5PV210_A8388_V2)
	{
		.gpio			= GPIO_KEY_VOLUME_UP,
		.code			= KEY_VOLUMEUP,
		.desc			= "VolumeUp",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
	{
		.gpio			= GPIO_KEY_VOLUME_DOWN,
		.code			= KEY_VOLUMEDOWN,
		.desc			= "VolumeDown",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
	{
		.gpio			= GPIO_PIXELQI_MODE,
		.code			= KEY_FN,
		.desc			= "Function",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
#endif
#if defined(CONFIG_S5PV210_A9)
	{
		.gpio			= GPIO_KEY_MENU,
		.code			= KEY_MENU,
		.desc			= "MENU",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
	{
		.gpio			= GPIO_KEY_ESC,
		.code			= KEY_ESC,
		.desc			= "ESC",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
	{
		.gpio			= GPIO_KEY_VOLUME_UP,
		.code			= KEY_VOLUMEUP,
		.desc			= "VOLUME_UP",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
	{
		.gpio			= GPIO_KEY_VOLUME_DOWN,
		.code			= KEY_VOLUMEDOWN,
		.desc			= "VOLUME_DOWN",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
#endif
#if defined(CONFIG_S5PV210_S9)
	{
		.gpio			= GPIO_KEY_MENU,
		.code			= KEY_MENU,
		.desc			= "MENU",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
	{
		.gpio			= GPIO_KEY_ESC,
		.code			= KEY_ESC,
		.desc			= "ESC",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
	{
		.gpio			= GPIO_ROTATION_MODE,
		.code			= KEY_FN,
		.desc			= "Function",
		.active_low		= 1,
		.debounce_interval	= 5,
		.type     = EV_KEY,
	},
//	{
//		.gpio			= GPIO_PIXELQI_MODE,
//		.code			= KEY_FN,
//		.desc			= "Function",
//		.active_low		= 1,
//		.debounce_interval	= 5,
//		.type     = EV_KEY,
//	},
#endif	
};

static struct gpio_keys_platform_data a8388_buttons_data  = {
	.buttons	= a8388_buttons,
	.nbuttons	= ARRAY_SIZE(a8388_buttons),
};

static struct platform_device a8388_buttons_device  = {
	.name		= "gpio-keys",
	.id		= 0,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &a8388_buttons_data,
	}
};

static void gps_gpio_init(void)
{
	struct device *gps_dev;

	gps_dev = device_create(sec_class, NULL, 0, NULL, "gps");
	if (IS_ERR(gps_dev)) {
		pr_err("Failed to create device(gps)!\n");
		goto err;
	}
#if defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A8388_V2) || defined(CONFIG_S5PV210_A8388_V1)
	gpio_request(GPIO_GPS_PWR_EN, "GPS_PWR_EN");
	s3c_gpio_setpull(GPIO_GPS_PWR_EN, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_PWR_EN, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_PWR_EN, 0);
	msleep(10);
	gpio_direction_output(GPIO_GPS_PWR_EN, 1);

	gpio_request(GPIO_GPS_nRST, "GPS_nRST");
	s3c_gpio_setpull(GPIO_GPS_nRST, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_nRST, S3C_GPIO_OUTPUT);
	msleep(10);
	gpio_direction_output(GPIO_GPS_nRST, 0);
	msleep(10);
	gpio_direction_output(GPIO_GPS_nRST, 1);

	gpio_request(GPIO_GPS_ENABLE, "GPS_ENABLE");
	s3c_gpio_setpull(GPIO_GPS_ENABLE, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_ENABLE, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_ENABLE, 0);

	s3c_gpio_setpull(GPIO_GPS_RXD, S3C_GPIO_PULL_UP);
//	s3c_gpio_setpull(GPIO_GPS_RTS, S3C_GPIO_PULL_UP);
	
	gpio_export(GPIO_GPS_nRST, 1);
	gpio_export(GPIO_GPS_PWR_EN, 1);
	gpio_export(GPIO_GPS_ENABLE, 1);
#endif
#if defined(CONFIG_S5PV210_A9)
	gpio_request(GPIO_GPS_PWR_EN, "GPS_PWR_EN");
	s3c_gpio_setpull(GPIO_GPS_PWR_EN, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_PWR_EN, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_PWR_EN, 0);
	msleep(10);
	gpio_direction_output(GPIO_GPS_PWR_EN, 1);

	gpio_request(GPIO_GPS_nRST, "GPS_nRST");
	s3c_gpio_setpull(GPIO_GPS_nRST, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_nRST, S3C_GPIO_OUTPUT);
	msleep(10);
	gpio_direction_output(GPIO_GPS_nRST, 0);
	msleep(10);
	gpio_direction_output(GPIO_GPS_nRST, 1);

	s3c_gpio_setpull(GPIO_GPS_RXD, S3C_GPIO_PULL_UP);
//	s3c_gpio_setpull(GPIO_GPS_RTS, S3C_GPIO_PULL_UP);
	
	gpio_export(GPIO_GPS_nRST, 1);
	gpio_export(GPIO_GPS_PWR_EN, 1);
#endif
#if  defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A9) 
  s3c_setup_uart_cfg_gpio(1);
#endif
	gpio_export_link(gps_dev, "GPS_nRST", GPIO_GPS_nRST);
	gpio_export_link(gps_dev, "GPS_PWR_EN", GPIO_GPS_PWR_EN);
	gpio_export_link(gps_dev, "GPS_ENABLE", GPIO_GPS_ENABLE);

 err:
	return;
}

#if defined(CONFIG_S5PV210_A8388_V1) || defined(CONFIG_S5PV210_A8388_V2)
static void w3g_gpio_init(void)
{
	struct device *w3g_dev;

	w3g_dev = device_create(sec_class, NULL, 0, NULL, "w3g");
	if (IS_ERR(w3g_dev)) {
		pr_err("Failed to create device(w3g)!\n");
		goto err;
	}

	gpio_request(GPIO_3G_PWR, "W3G_PWR_EN");
	s3c_gpio_setpull(GPIO_3G_PWR, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_3G_PWR, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_3G_PWR, 0);

	gpio_request(GPIO_3G_RST, "W3G_RST");
	s3c_gpio_setpull(GPIO_3G_RST, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_3G_RST, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_3G_RST, 0);

	gpio_request(GPIO_3G_DISABLE, "W3G_DISABLE");
	s3c_gpio_setpull(GPIO_3G_DISABLE, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_3G_DISABLE, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_3G_DISABLE, 0);

	gpio_export(GPIO_3G_PWR, 1);
	gpio_export(GPIO_3G_RST, 1);
	gpio_export(GPIO_3G_DISABLE, 1);

//  s3c_setup_uart_cfg_gpio(1);
	gpio_export_link(w3g_dev, "3GW_PWR_CTL", GPIO_3G_PWR);
	gpio_export_link(w3g_dev, "3GW_RST", GPIO_3G_RST);
	gpio_export_link(w3g_dev, "3GW_DISABLE", GPIO_3G_DISABLE);

 err:
	return;
}
#endif
#if defined(CONFIG_S5PV210_A9)
static void w3g_gpio_init(void)
{
	struct device *w3g_dev;

	w3g_dev = device_create(sec_class, NULL, 0, NULL, "w3g");
	if (IS_ERR(w3g_dev)) {
		pr_err("Failed to create device(w3g)!\n");
		goto err;
	}

	gpio_request(GPIO_3G_PWR, "W3G_PWR_EN");
	s3c_gpio_setpull(GPIO_3G_PWR, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_3G_PWR, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_3G_PWR, 0);

	gpio_request(GPIO_3G_RST, "W3G_RST");
	s3c_gpio_setpull(GPIO_3G_RST, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_3G_RST, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_3G_RST, 0);

	gpio_export(GPIO_3G_PWR, 1);
	gpio_export(GPIO_3G_RST, 1);

//  s3c_setup_uart_cfg_gpio(1);
	gpio_export_link(w3g_dev, "3GW_PWR_CTL", GPIO_3G_PWR);
	gpio_export_link(w3g_dev, "3GW_RST", GPIO_3G_RST);

 err:
	return;
}
#endif
#if defined(CONFIG_S5PV210_S9)
static void wifi_check_gpio(void)
{
	struct device *wifi_dev; 

	wifi_dev = device_create(sec_class, NULL, 0, NULL, "wifi_type");
	if (IS_ERR(wifi_dev)) {
		pr_err("Failed to create device(wifi_dev)!\n");
		goto err;
	}

	gpio_request(GPIO_WIFI_TYPE, "WIFI_TYPE");
	s3c_gpio_setpull(GPIO_WIFI_TYPE, S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(GPIO_WIFI_TYPE, S3C_GPIO_INPUT);

	gpio_export(GPIO_WIFI_TYPE, 0);
	gpio_export_link(wifi_dev, "WIFI_TYPE", GPIO_WIFI_TYPE);
 
 err:
	return;
}
#endif
//static void uart_switch_init(void)
//{
//	int ret;
//	struct device *uartswitch_dev;
//
//	uartswitch_dev = device_create(sec_class, NULL, 0, NULL, "uart_switch");
//	if (IS_ERR(uartswitch_dev)) {
//		pr_err("Failed to create device(uart_switch)!\n");
//		return;
//	}
//
//	ret = gpio_request(GPIO_UART_SEL, "UART_SEL");
//	if (ret < 0) {
//		pr_err("Failed to request GPIO_UART_SEL!\n");
//		return;
//	}
//	s3c_gpio_setpull(GPIO_UART_SEL, S3C_GPIO_PULL_NONE);
//	s3c_gpio_cfgpin(GPIO_UART_SEL, S3C_GPIO_OUTPUT);
//	gpio_direction_output(GPIO_UART_SEL, 1);
//
//	gpio_export(GPIO_UART_SEL, 1);
//
//	gpio_export_link(uartswitch_dev, "UART_SEL", GPIO_UART_SEL);
//}
//
static void skdv210_switch_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");

	if (IS_ERR(sec_class))
		pr_err("Failed to create class(sec)!\n");

	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	if (IS_ERR(switch_dev))
		pr_err("Failed to create device(switch)!\n");
};

#endif /* CONFIG_S5PV210_A8388_V1 CONFIG_S5PV210_A8388_V2*/

//#ifdef GPIO_TP_LED
//static int last_mode = 1;
//void skdv210_lcd_led_onoff(int onoff, int ifsys)
//{
// 	int data;
//	int err;
// 	
// 	data =  onoff;
//#ifdef GPIO_PIXELQI_MODE
// 	if(gpio_get_value(GPIO_PIXELQI_MODE) != GPIO_PIXELQI_ACTIVE)
// 	  data = 0;
//#endif
//  if((ifsys == 0) && (last_mode == 0) && (onoff == 1))
//    return;
//
//  if(ifsys == 1)
// 	  last_mode = onoff;
//
// //lcd led power on	
//	err = gpio_request(GPIO_LCD_LED_PWR, "GPH2");
//	if (err) {
//		printk(KERN_ERR "failed to request GPH2_3 for lcd backlight control\n");
//		return;
//	}
//	gpio_direction_output(GPIO_LCD_LED_PWR, data);
//	gpio_free(GPIO_LCD_LED_PWR);
//
//	//lvds led power on	
//#ifdef CONFIG_FB_S3C_PIXELQI
//	gpio_request(GPIO_PIXELQI_LED_PWR, "GPH0");
//	if (err) {
//		printk(KERN_ERR "failed to request GPH0_6 for LVDS power control\n");
//		return;
//	}
//	gpio_direction_output(GPIO_PIXELQI_LED_PWR, data);
//	gpio_free(GPIO_PIXELQI_LED_PWR);
//#endif
//
//	//pwm on
//	err = gpio_request(S5PV210_GPD0(0), "GPD0");
//	if (err) {
//		printk(KERN_ERR "failed to request GPD0 for lcd pwm control\n");
//		return;
//	}
//	gpio_direction_output(S5PV210_GPD0(0), data);
//	s3c_gpio_cfgpin(S5PV210_GPD0(0), S5PV210_GPD_0_0_TOUT_0);
//	gpio_free(S5PV210_GPD0(0));
//}
//EXPORT_SYMBOL(skdv210_lcd_led_onoff);
//#endif /* GPIO_TP_LED */
//Apollo -
#if defined(CONFIG_S5PV210_DEVBOARD) || defined(CONFIG_S5PV210_A8388_V2) || defined(CONFIG_S5PV210_S9)
void hdmi_gpio_i2c(int onoff)
{
//printk("%s %d\n", __func__, onoff);
	gpio_request(GPIO_HDMI_I2C_EN, "HDMI_I2C_EN");
  if(onoff)
	  gpio_direction_output(GPIO_HDMI_I2C_EN, 0);
	else
	  gpio_direction_output(GPIO_HDMI_I2C_EN, 1);
	  
	gpio_free(GPIO_HDMI_I2C_EN);
	return;  
}
EXPORT_SYMBOL(hdmi_gpio_i2c);
#endif

static struct platform_device *smdkc110_devices[] __initdata = {
#ifdef CONFIG_FIQ_DEBUGGER
	&s5pv210_device_fiqdbg_uart2,
#endif
#ifdef CONFIG_MTD_ONENAND
	&s5pc110_device_onenand,
#endif
#ifdef CONFIG_MTD_NAND
	&s3c_device_nand,
#endif
#ifdef CONFIG_S5P_RTC
	&s5p_device_rtc,
#endif
#ifdef CONFIG_SND_S3C64XX_SOC_I2S_V4
	&s5pv210_device_iis0,
#endif
#ifdef CONFIG_SND_S3C_SOC_AC97
	&s5pv210_device_ac97,
#endif
#ifdef CONFIG_SND_S3C_SOC_PCM
	&s5pv210_device_pcm0,
#endif
#ifdef CONFIG_SND_SOC_SPDIF
	&s5pv210_device_spdif,
#endif
	&s3c_device_wdt,

//Apollo +
#ifdef CONFIG_SWITCH_HEADPHONE
  &hp_switch_device,
#endif

#ifdef GPIO_VIBRATOR_PWR
  &skdv210_timed_gpios,
#endif

#if defined(CONFIG_S5PV210_A8388_V1) || defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A8388_V2) || defined(CONFIG_S5PV210_A9)
  &a8388_buttons_device,
#endif
//Apollo -

#ifdef CONFIG_FB_S3C
	&s3c_device_fb,
#endif
#ifdef CONFIG_DM9000
	&s5p_device_dm9000,
#endif

#ifdef CONFIG_VIDEO_MFC50
	&s3c_device_mfc,
#endif
#ifdef CONFIG_TOUCHSCREEN_S3C
	&s3c_device_ts,
#endif
	&s3c_device_keypad,
#ifdef CONFIG_S5P_ADC
	&s3c_device_adc,
#endif
#ifdef CONFIG_VIDEO_FIMC
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
#endif
#ifdef CONFIG_VIDEO_FIMC_MIPI
	&s3c_device_csis,
#endif
#ifdef CONFIG_VIDEO_JPEG_V2
	&s3c_device_jpeg,
#endif
#ifdef CONFIG_VIDEO_G2D
	&s3c_device_g2d,
#endif
#ifdef CONFIG_VIDEO_TV20
	&s5p_device_tvout,
	&s5p_device_cec,
	&s5p_device_hpd,
#endif

	&s3c_device_g3d,
	&s3c_device_lcd,

	&s3c_device_i2c0,
#ifdef CONFIG_S3C_DEV_I2C1
	&s3c_device_i2c1,
#endif
#ifdef CONFIG_S3C_DEV_I2C2
	&s3c_device_i2c2,
#endif

#ifdef CONFIG_USB_EHCI_HCD
	&s3c_device_usb_ehci,
#endif
#ifdef CONFIG_USB_OHCI_HCD
	&s3c_device_usb_ohci,
#endif

#ifdef CONFIG_USB_S3C_OTG_HOST
		&s3c_device_usb_otghcd,
#endif

#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#ifdef CONFIG_USB_ANDROID
	&s3c_device_android_usb,
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	&s3c_device_usb_mass_storage,
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	&s3c_device_rndis,
#endif
#endif
#ifdef CONFIG_BATTERY_S3C
	&sec_device_battery,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif

#ifdef CONFIG_S3C64XX_DEV_SPI
	&s5pv210_device_spi0,
	&s5pv210_device_spi1,
#endif
#ifdef CONFIG_S5PV210_POWER_DOMAIN
	&s5pv210_pd_audio,
	&s5pv210_pd_cam,
	&s5pv210_pd_tv,
	&s5pv210_pd_lcd,
	&s5pv210_pd_g3d,
	&s5pv210_pd_mfc,
#endif

#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[0],
	&s3c_device_timer[1],
	&s3c_device_timer[2],
	&s3c_device_timer[3],
#endif
#ifdef CONFIG_WLAN
#ifdef CONFIG_BCM4329_MODULE
	&sec_device_wifi,
#endif
#ifdef CONFIG_MRVL8787
  &sec_device_mrvl8787,
#endif
	&sec_device_rfkill,
	&sec_device_btsleep,
#endif
};

static void __init smdkc110_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s5pv210_gpiolib_init();
	s3c24xx_init_uarts(smdkc110_uartcfgs, ARRAY_SIZE(smdkc110_uartcfgs));
	s5p_reserve_bootmem(smdkc110_media_devs, ARRAY_SIZE(smdkc110_media_devs));
#ifdef CONFIG_MTD_ONENAND
	s5pc110_device_onenand.name = "s5pc110-onenand";
#endif
#ifdef CONFIG_MTD_NAND
	s3c_device_nand.name = "s5pv210-nand";
#endif
#ifdef CONFIG_S5P_RTC
	s5p_device_rtc.name = "smdkc110-rtc";
#endif
}

unsigned int pm_debug_scratchpad;

static void __init sound_init(void)
{
	u32 reg;

	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~(0x1f << 12);
	reg &= ~(0xf << 20);
	reg |= 0x12 << 12;
	reg |= 0x1  << 20;
	__raw_writel(reg, S5P_CLK_OUT);

	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x3 << 8);
	reg |= 0x0 << 8;
	__raw_writel(reg, S5P_OTHERS);
}

#ifdef CONFIG_VIDEO_TV20
static void s3c_set_qos(void)
{
	/* VP QoS */
	__raw_writel(0x00400001, S5P_VA_DMC0 + 0xC8);
	__raw_writel(0x387F0022, S5P_VA_DMC0 + 0xCC);
	/* MIXER QoS */
	__raw_writel(0x00400001, S5P_VA_DMC0 + 0xD0);
	__raw_writel(0x3FFF0062, S5P_VA_DMC0 + 0xD4);
	/* LCD1 QoS */
	__raw_writel(0x00800001, S5P_VA_DMC1 + 0x90);
	__raw_writel(0x3FFF005B, S5P_VA_DMC1 + 0x94);
	/* LCD2 QoS */
	__raw_writel(0x00800001, S5P_VA_DMC1 + 0x98);
	__raw_writel(0x3FFF015B, S5P_VA_DMC1 + 0x9C);
	/* VP QoS */
	__raw_writel(0x00400001, S5P_VA_DMC1 + 0xC8);
	__raw_writel(0x387F002B, S5P_VA_DMC1 + 0xCC);
	/* DRAM Controller QoS */
	__raw_writel(((__raw_readl(S5P_VA_DMC0)&~(0xFFF<<16))|(0x100<<16)),
			S5P_VA_DMC0 + 0x0);
	__raw_writel(((__raw_readl(S5P_VA_DMC1)&~(0xFFF<<16))|(0x100<<16)),
			S5P_VA_DMC1 + 0x0);
	/* BUS QoS AXI_DSYS Control */
	__raw_writel(0x00000007, S5P_VA_BUS_AXI_DSYS + 0x400);
	__raw_writel(0x00000007, S5P_VA_BUS_AXI_DSYS + 0x420);
	__raw_writel(0x00000030, S5P_VA_BUS_AXI_DSYS + 0x404);
	__raw_writel(0x00000030, S5P_VA_BUS_AXI_DSYS + 0x424);
}
#endif

static bool console_flushed;

static void flush_console(void)
{
	if (console_flushed)
		return;

	console_flushed = true;

	printk("\n");
	pr_emerg("Restarting %s\n", linux_banner);
	if (!try_acquire_console_sem()) {
		release_console_sem();
		return;
	}

	mdelay(50);

	local_irq_disable();
	if (try_acquire_console_sem())
		pr_emerg("flush_console: console was locked! busting!\n");
	else
		pr_emerg("flush_console: console was locked!\n");
	release_console_sem();
}
#if defined(CONFIG_S5PV210_A9)
static void camera_flash_init(void)
{
	
	gpio_request(GPIO_FLASH_EN, "GPIO_FLASH_EN");
	gpio_direction_output(GPIO_FLASH_EN, 0);
	gpio_free(GPIO_FLASH_EN);
	
	
	gpio_request(GPIO_FLASH_MODE, "FLASH_MODE");
	gpio_direction_output(GPIO_FLASH_MODE, 1);
	gpio_free(GPIO_FLASH_MODE);

	return;
}
#endif
static void smdkc110_pm_restart(char mode, const char *cmd)
{
	flush_console();
	arm_machine_restart(mode, cmd);
}

static void __init smdkc110_machine_init(void)
{
  unsigned int tmp;
  tmp = readl(S5PV210_GPA1PUD);
  tmp &= ~(0xf);
  tmp |= 0xa;
  writel(tmp,S5PV210_GPA1PUD);//内部上拉串口
	arm_pm_restart = smdkc110_pm_restart;

#ifdef CONFIG_VIDEO_GC2015
  GC2015_power(0);
#endif

#if defined(CONFIG_S5PV210_S9)
  hdmi_gpio_i2c(1);
#endif

#if defined(CONFIG_S5PV210_A9)
	camera_flash_init(); 
#endif	
#ifdef CONFIG_USB_ANDROID
	s3c_usb_set_serial();
#endif
	platform_add_devices(smdkc110_devices, ARRAY_SIZE(smdkc110_devices));
#ifdef CONFIG_ANDROID_PMEM
	platform_device_register(&pmem_gpu1_device);
#endif
	pm_power_off = smdkc110_power_off ;

#ifdef CONFIG_ANDROID_PMEM
	android_pmem_set_platdata();
#endif
	/* i2c */
	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);
	s3c_i2c2_set_platdata(NULL);

	sound_init();

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));

#ifdef CONFIG_DM9000
	smdkc110_dm9000_set();
#endif

#ifdef CONFIG_FB_S3C_LTE480WV
	s3cfb_set_platdata(&lte480wv_fb_data);
#endif

#ifdef CONFIG_FB_S3C_SEKEDE_BL
	s3cfb_set_platdata(&skdv210_fb_data);
#endif

	/* spi */
#ifdef CONFIG_S3C64XX_DEV_SPI
	if (!gpio_request(S5PV210_GPB(1), "SPI_CS0")) {
		gpio_direction_output(S5PV210_GPB(1), 1);
		s3c_gpio_cfgpin(S5PV210_GPB(1), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(S5PV210_GPB(1), S3C_GPIO_PULL_UP);
		s5pv210_spi_set_info(0, S5PV210_SPI_SRCCLK_PCLK,
			ARRAY_SIZE(smdk_spi0_csi));
	}
	if (!gpio_request(S5PV210_GPB(5), "SPI_CS1")) {
		gpio_direction_output(S5PV210_GPB(5), 1);
		s3c_gpio_cfgpin(S5PV210_GPB(5), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(S5PV210_GPB(5), S3C_GPIO_PULL_UP);
		s5pv210_spi_set_info(1, S5PV210_SPI_SRCCLK_PCLK,
			ARRAY_SIZE(smdk_spi1_csi));
	}
	spi_register_board_info(s3c_spi_devs, ARRAY_SIZE(s3c_spi_devs));
#endif

#if defined(CONFIG_TOUCHSCREEN_S3C)
	s3c_ts_set_platdata(&s3c_ts_platform);
#endif

#if defined(CONFIG_S5P_ADC)
	s3c_adc_set_platdata(&s3c_adc_platform);
#endif

#if defined(CONFIG_PM)
	s3c_pm_init();
#endif

#ifdef CONFIG_VIDEO_FIMC
	/* fimc */
	s3c_fimc0_set_platdata(&fimc_plat_lsi);
	s3c_fimc1_set_platdata(&fimc_plat_lsi);
	s3c_fimc2_set_platdata(&fimc_plat_lsi);
	/* external camera */
	/* smdkv210_cam0_power(1); */
	/* smdkv210_cam1_power(1); */
#endif

#ifdef CONFIG_VIDEO_FIMC_MIPI
	s3c_csis_set_platdata(NULL);
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
	s3c_jpeg_set_platdata(&jpeg_plat);
#endif

#ifdef CONFIG_VIDEO_MFC50
	/* mfc */
	s3c_mfc_set_platdata(NULL);
#endif

#ifdef CONFIG_VIDEO_TV20
	s3c_set_qos();
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
	s5pv210_default_sdhci0();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s5pv210_default_sdhci1();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s5pv210_default_sdhci2();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s5pv210_default_sdhci3();
#endif
#ifdef CONFIG_S5PV210_SETUP_SDHCI
	s3c_sdhci_set_platdata();
#endif

#ifdef CONFIG_BACKLIGHT_PWM
	smdk_backlight_register();
#endif

	regulator_has_full_constraints();

	smdkc110_setup_clocks();

#ifdef CONFIG_WLAN
	if(!check_wifi_type()){
#ifdef CONFIG_BCM4329_MODULE
  skdv210_init_wifi_mem();  
#endif	
}
  wlan_setup_power(0,0);
  wlan_setup_power(1,0);  
#endif

#if defined(CONFIG_S5PV210_A8388_V1) || defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A8388_V2) || defined(CONFIG_S5PV210_A9)
  skdv210_switch_init();
	gps_gpio_init();
#if defined(CONFIG_S5PV210_A8388_V1) || defined(CONFIG_S5PV210_A8388_V2) || defined(CONFIG_S5PV210_A9)
	w3g_gpio_init();
#endif
#endif
#if defined(CONFIG_S5PV210_S9)
	wifi_check_gpio();
#endif
#if defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A9)
#if defined(CONFIG_S5PV210_S9)
  usbhub_power(1);
#endif
#if defined(CONFIG_S5PV210_A9)
 usbhub_power(0);
#endif
  usb_host_power(1);
//  msleep(100);
  tp_power(1);
#endif
}

#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_phy_init(void)
{
	/* USB PHY0 Enable */
	writel(readl(S5P_USB_PHY_CONTROL) | (0x1<<0),
			S5P_USB_PHY_CONTROL);
	writel((readl(S3C_USBOTG_PHYPWR) & ~(0x3<<3) & ~(0x1<<0)) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	writel((readl(S3C_USBOTG_PHYCLK) & ~(0x5<<2)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	writel((readl(S3C_USBOTG_RSTCON) & ~(0x3<<1)) | (0x1<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);
	writel(readl(S3C_USBOTG_RSTCON) & ~(0x7<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);

	/* rising/falling time */
	writel(readl(S3C_USBOTG_PHYTUNE) | (0x1<<20),
			S3C_USBOTG_PHYTUNE);

	/* set DC level as 6 (6%) */
	writel((readl(S3C_USBOTG_PHYTUNE) & ~(0xf)) | (0x1<<2) | (0x1<<1),
			S3C_USBOTG_PHYTUNE);
}
EXPORT_SYMBOL(otg_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
struct usb_ctrlrequest usb_ctrl __attribute__((aligned(64)));

/* OTG PHY Power Off */
void otg_phy_off(void)
{
	writel(readl(S3C_USBOTG_PHYPWR) | (0x3<<3),
			S3C_USBOTG_PHYPWR);
	writel(readl(S5P_USB_PHY_CONTROL) & ~(1<<0),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_phy_init(void)
{
	struct clk *otg_clk;

	otg_clk = clk_get(NULL, "otg");
	clk_enable(otg_clk);

	if (readl(S5P_USB_PHY_CONTROL) & (0x1<<1))
		return;

	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) | (0x1<<1),
			S5P_USB_PHY_CONTROL);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
			& ~(0x1<<7) & ~(0x1<<6)) | (0x1<<8) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK) & ~(0x1<<7)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON)) | (0x1<<4) | (0x1<<3),
			S3C_USBOTG_RSTCON);
	__raw_writel(__raw_readl(S3C_USBOTG_RSTCON) & ~(0x1<<4) & ~(0x1<<3),
			S3C_USBOTG_RSTCON);
}
EXPORT_SYMBOL(usb_host_phy_init);

void usb_host_phy_off(void)
{
	__raw_writel(__raw_readl(S3C_USBOTG_PHYPWR) | (0x1<<7)|(0x1<<6),
			S3C_USBOTG_PHYPWR);
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) & ~(1<<1),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(usb_host_phy_off);

/* For OTG host mode */
#ifdef CONFIG_USB_S3C_OTG_HOST

/* Initializes OTG Phy */
void otg_host_phy_init(void) 
{
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL)
		|(0x1<<0), S5P_USB_PHY_CONTROL); /*USB PHY0 Enable */
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
		&~(0x3<<3)&~(0x1<<0))|(0x1<<5), S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK)
		&~(0x1<<4))|(0x7<<0), S3C_USBOTG_PHYCLK);

	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON)
		&~(0x3<<1))|(0x1<<0), S3C_USBOTG_RSTCON);
	udelay(10);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON)
		&~(0x7<<0)), S3C_USBOTG_RSTCON);
	udelay(10);

	__raw_writel((__raw_readl(S3C_UDC_OTG_GUSBCFG)
		|(0x3<<8)), S3C_UDC_OTG_GUSBCFG);

//	smb136_set_otg_mode(1);

	printk("otg_host_phy_int : USBPHYCTL=0x%x,PHYPWR=0x%x,PHYCLK=0x%x,USBCFG=0x%x\n", 
		readl(S5P_USB_PHY_CONTROL), 
		readl(S3C_USBOTG_PHYPWR),
		readl(S3C_USBOTG_PHYCLK), 
		readl(S3C_UDC_OTG_GUSBCFG)
		);
}
EXPORT_SYMBOL(otg_host_phy_init);
#endif

#endif

void s3c_setup_uart_cfg_gpio(unsigned char port)
{
	switch (port) {
	case 0:
		s3c_gpio_cfgpin(GPIO_BT_RXD, S3C_GPIO_SFN(GPIO_BT_RXD_AF));
		s3c_gpio_setpull(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_TXD, S3C_GPIO_SFN(GPIO_BT_TXD_AF));
		s3c_gpio_setpull(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_CTS, S3C_GPIO_SFN(GPIO_BT_CTS_AF));
		s3c_gpio_setpull(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_RTS, S3C_GPIO_SFN(GPIO_BT_RTS_AF));
		s3c_gpio_setpull(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_RXD, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_TXD, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_CTS, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_RTS, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
		break;
#if defined(GPIO_GPS_RXD)
	case 1:
		s3c_gpio_cfgpin(GPIO_GPS_RXD, S3C_GPIO_SFN(GPIO_GPS_RXD_AF));
		s3c_gpio_setpull(GPIO_GPS_RXD, S3C_GPIO_PULL_UP);
		s3c_gpio_cfgpin(GPIO_GPS_TXD, S3C_GPIO_SFN(GPIO_GPS_TXD_AF));
		s3c_gpio_setpull(GPIO_GPS_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_GPS_CTS, S3C_GPIO_SFN(GPIO_GPS_CTS_AF));
		s3c_gpio_setpull(GPIO_GPS_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_GPS_RTS, S3C_GPIO_SFN(GPIO_GPS_RTS_AF));
		s3c_gpio_setpull(GPIO_GPS_RTS, S3C_GPIO_PULL_NONE);
		break;
#endif
//	case 2:
//		s3c_gpio_cfgpin(GPIO_AP_RXD, S3C_GPIO_SFN(GPIO_AP_RXD_AF));
//		s3c_gpio_setpull(GPIO_AP_RXD, S3C_GPIO_PULL_NONE);
//		s3c_gpio_cfgpin(GPIO_AP_TXD, S3C_GPIO_SFN(GPIO_AP_TXD_AF));
//		s3c_gpio_setpull(GPIO_AP_TXD, S3C_GPIO_PULL_NONE);
//		break;
//	case 3:
//		s3c_gpio_cfgpin(GPIO_FLM_RXD, S3C_GPIO_SFN(GPIO_FLM_RXD_AF));
//		s3c_gpio_setpull(GPIO_FLM_RXD, S3C_GPIO_PULL_NONE);
//		s3c_gpio_cfgpin(GPIO_FLM_TXD, S3C_GPIO_SFN(GPIO_FLM_TXD_AF));
//		s3c_gpio_setpull(GPIO_FLM_TXD, S3C_GPIO_PULL_NONE);
//		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(s3c_setup_uart_cfg_gpio);

void s3c_config_sleep_gpio(void)
{
//设为高阻状态
//	writel(0x00000000, S5PV210_GPA0PUD);
//	writel(0x00000000, S5PV210_GPA1PUD);
//	writel(0x00000000, S5PV210_GPBPUD);
//	writel(0x00000000, S5PV210_GPC0PUD);
//	writel(0x00000000, S5PV210_GPC1PUD);
//	writel(0x00000000, S5PV210_GPD0PUD);
//	writel(0x00000000, S5PV210_GPD1PUD);
//	writel(0x00000000, S5PV210_GPE0PUD);
//	writel(0x00000000, S5PV210_GPE1PUD);
//	writel(0x00000000, S5PV210_GPF0PUD);
//	writel(0x00000000, S5PV210_GPF1PUD);
//	writel(0x00000000, S5PV210_GPF2PUD);
//	writel(0x00000000, S5PV210_GPF3PUD);
//	writel(0x00000000, S5PV210_GPG0PUD);
//	writel(0x00000000, S5PV210_GPG1PUD);
//	writel(0x00000000, S5PV210_GPG2PUD);
//	writel(0x00000000, S5PV210_GPG3PUD);
//	writel(0x00000000, S5PV210_GPH0PUD);
//	writel(0x00000000, S5PV210_GPH1PUD);//???
//	writel(0x00000000, S5PV210_GPH2PUD);
//	writel(0x00000000, S5PV210_GPH3PUD);
//	writel(0x00000000, S5PV210_GPIPUD);
//	writel(0x00000000, S5PV210_GPJ0PUD);
//	writel(0x00000000, S5PV210_GPJ1PUD);
//	writel(0x00000000, S5PV210_GPJ2PUD);
//	writel(0x00000000, S5PV210_GPJ3PUD);
//	writel(0x00000000, S5PV210_GPJ4PUD);
	
//设置为输入状态
	writel(0x00000000, S5PV210_GPA0CON);
//	writel(0x00000000, S5PV210_GPA1CON);
//	writel(0x00000000, S5PV210_GPBCON);
	writel(0x00000000, S5PV210_GPC0CON);
//	writel(0x00000000, S5PV210_GPC1CON);
//	writel(0x00000000, S5PV210_GPD0CON);
	writel(0x00000000, S5PV210_GPD1CON);
//	writel(0x00000000, S5PV210_GPE0CON);
//	writel(0x00000000, S5PV210_GPE1CON);
//	writel(0x00000000, S5PV210_GPF0CON);
//	writel(0x00000000, S5PV210_GPF1CON);
//	writel(0x00000000, S5PV210_GPF2CON);
//	writel(0x00000000, S5PV210_GPF3CON);
	writel(0x00000000, S5PV210_GPG0CON);
	writel(0x00000000, S5PV210_GPG1CON);
	writel(0x00000000, S5PV210_GPG2CON);
	writel(0x00000000, S5PV210_GPG3CON);
	writel(0x10001000, S5PV210_GPH0CON); //(earphone_detece GPS_RESET) output high
	writel(0x00000088, S5PV210_GPH0DAT);
	writel(0x11000000, S5PV210_GPH1CON); //(TP_RST TP_INT) output high
	writel(0x000000C0, S5PV210_GPH1DAT);
	writel(0x00010010, S5PV210_GPH2CON);//电源控制，usb hub power
	writel(0x00000000, S5PV210_GPH3CON);
//	writel(0x00000000, S5PV210_GPICON);
//	writel(0x00000000, S5PV210_GPJ0CON);
//	writel(0x00000000, S5PV210_GPJ1CON);
//	writel(0x00000000, S5PV210_GPJ2CON);
//	writel(0x00000000, S5PV210_GPJ3CON);
//	writel(0x00000000, S5PV210_GPJ4CON);
	writel(0x0000000A, S5PV210_ETC1PUD);
	//set motor power on to forbit motor to run after sleep -fighter
//	s3c_gpio_cfgpin(S5PV210_GPJ2(5),GPIO_OUTPUT);
//	s3c_gpio_setpull(S5PV210_GPJ2(5), S3C_GPIO_PULL_UP);
//	writel(0x800, S5PV210_GPJ2CONPDN);
	
}
EXPORT_SYMBOL(s3c_config_sleep_gpio);

#ifdef CONFIG_MACH_SMDKC110
MACHINE_START(SMDKC110, "SMDKC110")
#elif CONFIG_MACH_SMDKV210
MACHINE_START(SMDKV210, "SMDKV210")
#endif
	/* Maintainer: Kukjin Kim <kgene.kim@samsung.com> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= s5pv210_init_irq,
	.map_io		= smdkc110_map_io,
	.init_machine	= smdkc110_machine_init,
	.timer		= &s5p_systimer,
MACHINE_END
