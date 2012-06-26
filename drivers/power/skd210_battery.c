/*
 * linux/drivers/power/s3c_fake_battery.c
 *
 * Battery measurement code for S3C platform.
 *
 * based on palmtx_battery.c
 *
 * Copyright (C) 2009 Samsung Electronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/regs-adc.h>
//#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-smdkc110.h>
/*  S9 8388_A1 分压电阻 47K(上拉) 20K（下拉）
5.8 2140
5.9 2170
6.0 2217
6.1 2250
6.2 2300
6.3 2350
6.4 2380
6.5 2420
6.6 2460
6.7 2500
6.8 2540
6.9 2580
7.0 2620
7.1 2655
7.2 2695
7.3 2735
7.4 2770
7.5 2810
7.6 2845
7.7 2888
7.8 2925
7.9 2961
8.0 3000
8.1 3035
8.2 3075
8.3 3090
8.4 3130
8.5 3163

full  8.35 3103
*/
//#undef   CONFIG_S5PV210_A8388_V1
//#define CONFIG_S5PV210_A8388_V2

#define DRIVER_NAME  "skdv210_battery"

static struct wake_lock vbus_wake_lock;
#define POLL_MSTIME       1000
//#define BATT_MAX_ADC      3103
#define BATT_MAX_ADC      3000
#define BATT_MIN_ADC      2217  // 6
#define BATT_15_ADC       2620  // 7
#define BATT_MAX_VOLTAGE  8400
#define BATT_15_VOLTAGE   7091
#define BATT_MIN_VOLTAGE  6000
#define VOLTAGE_OFF       (BATT_MAX_VOLTAGE - BATT_MIN_VOLTAGE)
#define BATT_ADC_OFF      (BATT_MAX_ADC - BATT_MIN_ADC)
#define BATT_CORRECTION_VAL  (220)  //电池电压修正值

static  int adc_data_count = 10;
static  int last_battery_vol = 0;
static int one_time = 1; 
static int last_charge_stat= 0;
static struct timer_list flash_power_timer;
static int skip_data = 0;
static int out_charge_flag = 0;
/* Prototypes */
extern int s3c_adc_get_adc_data(int channel);

static ssize_t s3c_bat_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf);
static ssize_t s3c_bat_store(struct device *dev, 
           struct device_attribute *attr,
           const char *buf, size_t count);

static void polling_timer_func(unsigned long unused);
//extern void mrvl8787_power_set(unsigned mrvl_module_onoff,unsigned wifi_onoff,unsigned bt_onoff);
#define FAKE_BAT_LEVEL  100

static struct device *dev;
static int s3c_battery_initial;
static int force_update;
//static int suspend = 2;
static char *status_text[] = {
  [POWER_SUPPLY_STATUS_UNKNOWN] =    "Unknown",
  [POWER_SUPPLY_STATUS_CHARGING] =  "Charging",
  [POWER_SUPPLY_STATUS_DISCHARGING] =  "Discharging",
  [POWER_SUPPLY_STATUS_NOT_CHARGING] =  "Not Charging",
  [POWER_SUPPLY_STATUS_FULL] =    "Full",
};

typedef enum {
  CHARGER_BATTERY = 0,
  CHARGER_USB,
  CHARGER_AC,
  CHARGER_DISCHARGE
} charger_type_t;

struct battery_info {
  u32 batt_id;    /* Battery ID from ADC */
  u32 batt_vol;    /* Battery voltage from ADC */
  u32 batt_vol_adc;  /* Battery ADC value */
  u32 batt_vol_adc_cal;  /* Battery ADC value (calibrated)*/
  u32 batt_temp;    /* Battery Temperature (C) from ADC */
  u32 batt_temp_adc;  /* Battery Temperature ADC value */
  u32 batt_temp_adc_cal;  /* Battery Temperature ADC value (calibrated) */
  u32 batt_current;  /* Battery current from ADC */
  u32 level;    /* formula */
  u32 charging_source;  /* 0: no cable, 1:usb, 2:AC */
  u32 charging_enabled;  /* 0: Disable, 1: Enable */
  u32 batt_health;  /* Battery Health (Authority) */
  u32 batt_is_full;       /* 0 : Not full 1: Full */
};

/* lock to protect the battery info */
static DEFINE_MUTEX(work_lock);

struct s3c_battery_info {
  int present;
  int polling;
  unsigned long polling_interval;

  struct battery_info bat_info;
};
static struct s3c_battery_info s3c_bat_info;

//static struct timer_list charger_timer;
//static struct timer_list supply_timer;
static struct timer_list polling_timer;
static void __iomem     *adc_base;
static int adc_data[10];
static int adc_data_index;
struct battery_info pre_info;
//charger_type_t pre_charge_state;

static void flash_power_timer_handler(unsigned long param)
{
	skip_data = 0;
}

static int s3c_get_bat_level(struct power_supply *bat_ps)
{
//  printk("%s\n",__func__);
//  return FAKE_BAT_LEVEL;
  return s3c_bat_info.bat_info.level;
}

static int s3c_get_bat_vol(struct power_supply *bat_ps)
{
//  int bat_vol = 0;
//  printk("%s\n",__func__);

//  return bat_vol;
  return s3c_bat_info.bat_info.batt_vol;
}

static u32 s3c_get_bat_health(void)
{
  return s3c_bat_info.bat_info.batt_health;
}

static int s3c_get_bat_temp(struct power_supply *bat_ps)
{
  int temp = 0;

  return s3c_bat_info.bat_info.batt_health;
  return temp;
}

static int s3c_bat_get_charging_status(void)
{
  charger_type_t charger = CHARGER_BATTERY; 
  int ret = 0;
        
  charger = s3c_bat_info.bat_info.charging_source;
        
  switch (charger) {
  case CHARGER_BATTERY:
    ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
    break;
  case CHARGER_USB:
  case CHARGER_AC:
    if (s3c_bat_info.bat_info.level == 100 
      && s3c_bat_info.bat_info.batt_is_full) {
      ret = POWER_SUPPLY_STATUS_FULL;
    } else {
      ret = POWER_SUPPLY_STATUS_CHARGING;
    }
    break;
  case CHARGER_DISCHARGE:
    ret = POWER_SUPPLY_STATUS_DISCHARGING;
    break;
  default:
    ret = POWER_SUPPLY_STATUS_UNKNOWN;
  }
  dev_dbg(dev, "%s : %s\n", __func__, status_text[ret]);

  return ret;
}

static int s3c_bat_get_property(struct power_supply *bat_ps, 
    enum power_supply_property psp,
    union power_supply_propval *val)
{
  dev_dbg(bat_ps->dev, "%s : psp = %d\n", __func__, psp);

  switch (psp) {
  case POWER_SUPPLY_PROP_STATUS:
    val->intval = s3c_bat_get_charging_status();
    break;
  case POWER_SUPPLY_PROP_HEALTH:
    val->intval = s3c_get_bat_health();
    break;
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = s3c_bat_info.present;
    break;
  case POWER_SUPPLY_PROP_TECHNOLOGY:
    val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
    break;
  case POWER_SUPPLY_PROP_CAPACITY:
    val->intval = s3c_bat_info.bat_info.level;
    dev_dbg(dev, "%s : level = %d\n", __func__, 
        val->intval);
    break;
  case POWER_SUPPLY_PROP_TEMP:
    val->intval = s3c_bat_info.bat_info.batt_temp;
    dev_dbg(bat_ps->dev, "%s : temp = %d\n", __func__, 
        val->intval);
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

static int s3c_power_get_property(struct power_supply *bat_ps, 
    enum power_supply_property psp, 
    union power_supply_propval *val)
{
  charger_type_t charger;
  
  dev_dbg(bat_ps->dev, "%s : psp = %d\n", __func__, psp);

  charger = s3c_bat_info.bat_info.charging_source;

  switch (psp) {
  case POWER_SUPPLY_PROP_ONLINE:
    if (bat_ps->type == POWER_SUPPLY_TYPE_MAINS)
      val->intval = (charger == CHARGER_AC ? 1 : 0);
    else if (bat_ps->type == POWER_SUPPLY_TYPE_USB)
      val->intval = (charger == CHARGER_USB ? 1 : 0);
    else
      val->intval = 0;
    break;
  default:
    return -EINVAL;
  }
  
  return 0;
}

#define SEC_BATTERY_ATTR(_name)                \
{                      \
  .attr = { .name = #_name, .mode = S_IRUGO | S_IWUGO, .owner = THIS_MODULE },  \
  .show = s3c_bat_show_property,              \
  .store = s3c_bat_store,                \
}

static struct device_attribute s3c_battery_attrs[] = {
  SEC_BATTERY_ATTR(batt_vol),
  SEC_BATTERY_ATTR(batt_vol_adc),
  SEC_BATTERY_ATTR(batt_vol_adc_cal),
  SEC_BATTERY_ATTR(batt_temp),
  SEC_BATTERY_ATTR(batt_temp_adc),
  SEC_BATTERY_ATTR(batt_temp_adc_cal),
//  SEC_BATTERY_ATTR(wifi_state),
};

enum {
  BATT_VOL = 0,
  BATT_VOL_ADC,
  BATT_VOL_ADC_CAL,
  BATT_TEMP,
  BATT_TEMP_ADC,
  BATT_TEMP_ADC_CAL,
//  WIFI_STATE,
};

static int s3c_bat_create_attrs(struct device * dev)
{
  int i, rc;

  for (i = 0; i < ARRAY_SIZE(s3c_battery_attrs); i++) {
    rc = device_create_file(dev, &s3c_battery_attrs[i]);
    if (rc)
    goto s3c_attrs_failed;
  }
  goto succeed;

s3c_attrs_failed:
  while (i--)
  device_remove_file(dev, &s3c_battery_attrs[i]);
succeed:
  return rc;
}

static ssize_t s3c_bat_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
  int i = 0;
  const ptrdiff_t off = attr - s3c_battery_attrs;

  switch (off) {
  case BATT_VOL:
    i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
               s3c_bat_info.bat_info.batt_vol);
  break;
  case BATT_VOL_ADC:
    i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
               s3c_bat_info.bat_info.batt_vol_adc);
    break;
  case BATT_VOL_ADC_CAL:
    i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
               s3c_bat_info.bat_info.batt_vol_adc_cal);
    break;
  case BATT_TEMP:
    i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
               s3c_bat_info.bat_info.batt_temp);
    break;
  case BATT_TEMP_ADC:
    i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
               s3c_bat_info.bat_info.batt_temp_adc);
    break;  
  case BATT_TEMP_ADC_CAL:
    i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
               s3c_bat_info.bat_info.batt_temp_adc_cal);
    break;
//   case WIFI_STATE:
//		    i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
//               suspend);
//    break;
  default:
    i = -EINVAL;
  }       

  return i;
}

static ssize_t s3c_bat_store(struct device *dev, 
           struct device_attribute *attr,
           const char *buf, size_t count)
{
  int x = 0;
  int ret = 0;
  const ptrdiff_t off = attr - s3c_battery_attrs;

  switch (off) {
  case BATT_VOL_ADC_CAL:
    if (sscanf(buf, "%d\n", &x) == 1) {
      s3c_bat_info.bat_info.batt_vol_adc_cal = x;
      ret = count;
    }
    dev_info(dev, "%s : batt_vol_adc_cal = %d\n", __func__, x);
    break;
  case BATT_TEMP_ADC_CAL:
    if (sscanf(buf, "%d\n", &x) == 1) {
      s3c_bat_info.bat_info.batt_temp_adc_cal = x;
      ret = count;
    }
    dev_info(dev, "%s : batt_temp_adc_cal = %d\n", __func__, x);
    break;
//  case WIFI_STATE:
//    if (sscanf(buf, "%d\n", &x) == 1) {
//      suspend = x;
//      ret = count;
//    }
//    dev_info(dev, "%s : batt_temp_adc_cal = %d\n", __func__, x);
//    break;
  default:
    ret = -EINVAL;
  }       

  return ret;
}

static enum power_supply_property s3c_battery_properties[] = {
  POWER_SUPPLY_PROP_STATUS,
  POWER_SUPPLY_PROP_HEALTH,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_TECHNOLOGY,
  POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property s3c_power_properties[] = {
  POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
  "battery",
};

static struct power_supply s3c_power_supplies[] = {
  {
    .name = "battery",
    .type = POWER_SUPPLY_TYPE_BATTERY,
    .properties = s3c_battery_properties,
    .num_properties = ARRAY_SIZE(s3c_battery_properties),
    .get_property = s3c_bat_get_property,
  },
  {
    .name = "usb",
    .type = POWER_SUPPLY_TYPE_USB,
    .supplied_to = supply_list,
    .num_supplicants = ARRAY_SIZE(supply_list),
    .properties = s3c_power_properties,
    .num_properties = ARRAY_SIZE(s3c_power_properties),
    .get_property = s3c_power_get_property,
  },
  {
    .name = "ac",
    .type = POWER_SUPPLY_TYPE_MAINS,
    .supplied_to = supply_list,
    .num_supplicants = ARRAY_SIZE(supply_list),
    .properties = s3c_power_properties,
    .num_properties = ARRAY_SIZE(s3c_power_properties),
    .get_property = s3c_power_get_property,
  },
};

static int s3c_cable_status_update(int status)
{
  int ret = 0;
  charger_type_t source = CHARGER_BATTERY;

  dev_dbg(dev, "%s\n", __func__);

  if(!s3c_battery_initial)
    return -EPERM;

  switch(status) {
  case CHARGER_BATTERY:
    dev_dbg(dev, "%s : cable NOT PRESENT\n", __func__);
    s3c_bat_info.bat_info.charging_source = CHARGER_BATTERY;
    break;
  case CHARGER_USB:
    dev_dbg(dev, "%s : cable USB\n", __func__);
    s3c_bat_info.bat_info.charging_source = CHARGER_USB;
    break;
  case CHARGER_AC:
    dev_dbg(dev, "%s : cable AC\n", __func__);
    s3c_bat_info.bat_info.charging_source = CHARGER_AC;
    break;
  case CHARGER_DISCHARGE:
    dev_dbg(dev, "%s : Discharge\n", __func__);
    s3c_bat_info.bat_info.charging_source = CHARGER_DISCHARGE;
    break;
  default:
    dev_err(dev, "%s : Nat supported status\n", __func__);
    ret = -EINVAL;
  }
  source = s3c_bat_info.bat_info.charging_source;

  if (source == CHARGER_USB || source == CHARGER_AC) {
//充电时也可休眠
//    wake_lock(&vbus_wake_lock);
  } else {
    /* give userspace some time to see the uevent and update
     * LED state or whatnot...
     */
//    wake_lock_timeout(&vbus_wake_lock, HZ / 2);
  }

  /* if the power source changes, all power supplies may change state */
  power_supply_changed(&s3c_power_supplies[CHARGER_BATTERY]);
  /*
  power_supply_changed(&s3c_power_supplies[CHARGER_USB]);
  power_supply_changed(&s3c_power_supplies[CHARGER_AC]);
  */
  dev_dbg(dev, "%s : call power_supply_changed\n", __func__);
  return ret;
}

static void s3c_bat_status_update(struct power_supply *bat_ps)
{
  int old_level, old_temp, old_is_full;
  dev_dbg(dev, "%s ++\n", __func__);

  if(!s3c_battery_initial)
    return;

  mutex_lock(&work_lock);
  old_temp = s3c_bat_info.bat_info.batt_temp;
  old_level = s3c_bat_info.bat_info.level; 
  old_is_full = s3c_bat_info.bat_info.batt_is_full;
  s3c_bat_info.bat_info.batt_temp = s3c_get_bat_temp(bat_ps);
  s3c_bat_info.bat_info.level = s3c_get_bat_level(bat_ps);
  s3c_bat_info.bat_info.batt_vol = s3c_get_bat_vol(bat_ps);

  if (old_level != s3c_bat_info.bat_info.level 
      || old_temp != s3c_bat_info.bat_info.batt_temp
      || old_is_full != s3c_bat_info.bat_info.batt_is_full
      || force_update) {
    force_update = 0;
    power_supply_changed(bat_ps);
    dev_dbg(dev, "%s : call power_supply_changed\n", __func__);
  }

  mutex_unlock(&work_lock);
  dev_dbg(dev, "%s --\n", __func__);
}

void s3c_cable_check_status(int flag)
{
  charger_type_t status = 0;

  if (flag == 0)  // Battery
    status = CHARGER_BATTERY;
  else    // USB
    status = CHARGER_USB;
  s3c_cable_status_update(status);
}
EXPORT_SYMBOL(s3c_cable_check_status);

#ifdef CONFIG_PM
static int s3c_bat_suspend(struct platform_device *pdev, pm_message_t state)
{
//  dev_info(dev, "%s\n", __func__);
//	suspend = 1;
  del_timer_sync(&polling_timer);
  return 0;
}

static int s3c_bat_resume(struct platform_device *pdev)
{
//  dev_info(dev, "%s\n", __func__);

  setup_timer(&polling_timer, polling_timer_func, 0);
  mod_timer(&polling_timer, jiffies + HZ+HZ);
//  polling_timer_func(0);
  return 0;
}
#else
#define s3c_bat_suspend NULL
#define s3c_bat_resume NULL
#endif /* CONFIG_PM */

//static int s3c_getadcdata(unsigned int ch)
//{
//  unsigned int preScaler = 0;
//  unsigned int rADCCON_save = 0;
//	unsigned int rADCMUX_save = 0;  
//	unsigned int temp = 0;;
//  unsigned int itemp = 0;
//  int iRet =0;
//
//	local_irq_disable();
//  rADCCON_save = readl(adc_base+S3C_ADCCON);
//	rADCMUX_save = readl(adc_base+S3C_ADCMUX);  
//	
//  // setup ADCCON
//  preScaler = 65; //A/D converter freq = PCLK/(prescaler+1) == 66/(65+1) = 1
//  temp |= (0x1<<16) | (0x1<<14) | (preScaler<<6);
//  writel(temp,adc_base+S3C_ADCCON);
//	
//  //change channel
//  writel(ch&0xf,adc_base + S3C_ADCMUX);
//  udelay(10);
//	
//  // start ADC
//  itemp = readl(adc_base+S3C_ADCCON) | 0x01;
//  writel(itemp, adc_base+S3C_ADCCON);
//
//  // wait for ADC end conversion
//  while (!(readl(adc_base+S3C_ADCCON) & (0x1<<15)))
//    udelay(1);
//
//  iRet = readl(adc_base+S3C_ADCDAT0) & 0xFFF;
//
//  writel(rADCCON_save, adc_base+S3C_ADCCON);
//  writel(rADCMUX_save,adc_base+S3C_ADCMUX);  
//
//	local_irq_enable();
//  return iRet;
//}
/*
 比较data1,data2的大小，若其差的绝对值大于等于stand_value,则返回1，否则返回0
*/
static unsigned int check_value(unsigned int battery_chager_state,unsigned int new_data,unsigned int last_data,unsigned int stand_value)
{
	if(battery_chager_state == 2)
	{
		printk("s3c_bat_info.bat_info.batt_vol-last_battery_vol = %d +++++++\n",new_data-last_data);
		if((new_data-last_data) >= stand_value)
			return 1;
		else
			return 0;
	}
	return 0;
}
static void polling_timer_func(unsigned long unused)
{
  int charge_stat;
  int acpg = 0;
  int usbpg = 0;
  unsigned int adc_read;
  int i;
  int lock_screen;
  int ischarge = 0;
  int level_fix = 0;

#if defined(CONFIG_S5PV210_A8388_V1)
  charge_stat = gpio_get_value(GPIO_CHARGE_STAT1);
//  adc_read = s3c_getadcdata(0);
  adc_read = s3c_adc_get_adc_data(0);
#elif defined(CONFIG_S5PV210_A8388_V2)	
	charge_stat = gpio_get_value(GPIO_CHARGE_STAT1);
//  adc_read = s3c_getadcdata(0);
  adc_read = s3c_adc_get_adc_data(0);
  charge_stat = (gpio_get_value(GPIO_CHARGE_STAT1)<<1) | gpio_get_value(S5PV210_GPH3(0));  //charge_stata = state1<<1|state2
#elif defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A9)
//  adc_read = s3c_getadcdata(0);
  adc_read = s3c_adc_get_adc_data(0);
  charge_stat = (gpio_get_value(GPIO_CHARGE_STAT1)<<1) | gpio_get_value(GPIO_CHARGE_STAT2);  //charge_stata = state1<<1|state2
#endif
//printk("adc = %d\n", adc_read);	
  //取前10次平均值
  if (adc_data_index==0xff)
  {
    for (i=0;i<adc_data_count;i++)
      adc_data[i]=adc_read;
    adc_data_index = 0;
  }
  else
  {
    if (adc_data_index>=adc_data_count)
      adc_data_index=0;
      
    adc_data[adc_data_index++] = adc_read;
    adc_read = 0;
    for (i=0; i<adc_data_count; i++)
      adc_read += adc_data[i] ;
    adc_read = adc_read/adc_data_count;
  }
  
  s3c_bat_info.bat_info.batt_vol_adc = adc_read;
  s3c_bat_info.bat_info.batt_vol_adc_cal = adc_read;
  s3c_bat_info.bat_info.charging_source = CHARGER_BATTERY;
  //计算电池电压
  s3c_bat_info.bat_info.batt_vol = (adc_read - BATT_MIN_ADC)*VOLTAGE_OFF/BATT_ADC_OFF + BATT_MIN_VOLTAGE;
  
   		
#if defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A9)
  if(charge_stat == 2)
    s3c_bat_info.bat_info.batt_vol -= BATT_CORRECTION_VAL;
#endif

	if(charge_stat != last_charge_stat )
		{
			adc_data_index==0xff;
			last_charge_stat = charge_stat;
			
			skip_data =1;
			mod_timer(&flash_power_timer,jiffies + 10*HZ);
		}

if(skip_data != 1)
{	   	

  //剩余电量级别
  if (adc_read >= BATT_MAX_ADC)
  {
    s3c_bat_info.bat_info.batt_is_full = 1;
    s3c_bat_info.bat_info.level = 100;
  }
  else
  {
    s3c_bat_info.bat_info.batt_is_full = 0;
    if(s3c_bat_info.bat_info.batt_vol >= BATT_15_VOLTAGE)
    {
      s3c_bat_info.bat_info.level = (s3c_bat_info.bat_info.batt_vol - BATT_15_VOLTAGE)*85/(BATT_MAX_VOLTAGE - BATT_15_VOLTAGE) + 15;
    }
    else
    {
      s3c_bat_info.bat_info.level = (s3c_bat_info.bat_info.batt_vol - BATT_MIN_VOLTAGE)*15/(BATT_15_VOLTAGE - BATT_MIN_VOLTAGE);
    }
  }
  
  //当电量过低时系统关机
  if(adc_read <= BATT_MIN_ADC)
  {
    s3c_bat_info.bat_info.batt_is_full = 0;
    s3c_bat_info.bat_info.level = 0;//电量低关机
  }

}

#if defined(CONFIG_S5PV210_A8388_V1)
//  ischarge = s3c_getadcdata(1);
  ischarge = s3c_adc_get_adc_data(1);
  if(ischarge>2000)
  {
    if (charge_stat == 1) //已充满
    {
      s3c_bat_info.bat_info.charging_source = CHARGER_DISCHARGE;
      s3c_bat_info.bat_info.batt_is_full = 1;
      s3c_bat_info.bat_info.level = 100;
    }
    else
    {
      s3c_bat_info.bat_info.charging_source = CHARGER_AC;
      s3c_bat_info.bat_info.batt_is_full = 0;
      if(s3c_bat_info.bat_info.level >= 100)
        s3c_bat_info.bat_info.level = 99;
      if(adc_read <= BATT_MIN_ADC)
        s3c_bat_info.bat_info.level = 1;//充电时电量低不关机
    }
  }
  else
  {
    s3c_bat_info.bat_info.charging_source = CHARGER_DISCHARGE;
    //printk("no charge = %d ---------\n",ischarge);
  }
#elif defined(CONFIG_S5PV210_A8388_V2)  
 switch(charge_stat){
    case 3:
    {
      s3c_bat_info.bat_info.charging_source = CHARGER_DISCHARGE;
//      s3c_bat_info.bat_info.batt_is_full = 1;
//      s3c_bat_info.bat_info.level = 100;
      break;
    }
    case 2: //正在充电
    {
      s3c_bat_info.bat_info.charging_source = CHARGER_AC;
      s3c_bat_info.bat_info.batt_is_full = 0;
      if(s3c_bat_info.bat_info.level >= 100)
        s3c_bat_info.bat_info.level = 99;
      if(adc_read <= BATT_MIN_ADC)
        s3c_bat_info.bat_info.level = 1;//充电时电量低不关机
      break;
    }
    case 1:
    {
      s3c_bat_info.bat_info.charging_source = CHARGER_DISCHARGE;
      s3c_bat_info.bat_info.batt_is_full = 1;
      s3c_bat_info.bat_info.level = 100;
      break;
    }
    default:
      break;
  }
#elif defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A9)
  switch(charge_stat){
    case 3:
    {
      s3c_bat_info.bat_info.charging_source = CHARGER_DISCHARGE;
//      s3c_bat_info.bat_info.batt_is_full = 1;
//      s3c_bat_info.bat_info.level = 100;
      break;
    }
    case 2: //正在充电
    {
      s3c_bat_info.bat_info.charging_source = CHARGER_AC;
      s3c_bat_info.bat_info.batt_is_full = 0;
      if(s3c_bat_info.bat_info.level >= 100)
        s3c_bat_info.bat_info.level = 99;
      if(adc_read <= BATT_MIN_ADC)
        s3c_bat_info.bat_info.level = 1;//充电时电量低不关机
      break;
    }
    case 1:
    {
      s3c_bat_info.bat_info.charging_source = CHARGER_DISCHARGE;
      s3c_bat_info.bat_info.batt_is_full = 1;
      s3c_bat_info.bat_info.level = 100;
      break;
    }
    default:
      break;
  }
#endif

//  s3c_bat_info.bat_info.batt_vol = adc_read;

#ifdef CONFIG_BATTERY_S3C_FAKE
  s3c_bat_info.bat_info.level = FAKE_BAT_LEVEL;
#endif

  memcpy(&pre_info, &(s3c_bat_info.bat_info), sizeof(struct battery_info));
  pre_info.batt_id = 1;

  s3c_cable_status_update(s3c_bat_info.bat_info.charging_source);
  mod_timer(&polling_timer, jiffies + msecs_to_jiffies(POLL_MSTIME));
}

static int __devinit s3c_bat_probe(struct platform_device *pdev)
{
  int i;
  int ret = 0;

  adc_base = ioremap(0xE1700000, 0x20);
  dev = &pdev->dev;
  dev_info(dev, "%s\n", __func__);

  s3c_bat_info.present = 1;

  s3c_bat_info.bat_info.batt_id = 0;
  s3c_bat_info.bat_info.batt_vol = 0;
  s3c_bat_info.bat_info.batt_vol_adc = 0;
  s3c_bat_info.bat_info.batt_vol_adc_cal = 0;
  s3c_bat_info.bat_info.batt_temp = 0;
  s3c_bat_info.bat_info.batt_temp_adc = 0;
  s3c_bat_info.bat_info.batt_temp_adc_cal = 0;
  s3c_bat_info.bat_info.batt_current = 0;
  s3c_bat_info.bat_info.level = 0;
  s3c_bat_info.bat_info.charging_source = CHARGER_BATTERY;
  s3c_bat_info.bat_info.charging_enabled = 0;
  s3c_bat_info.bat_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;
  
  memset(&pre_info, 0x00 , sizeof(struct battery_info));
  adc_data_index = 0xff;

  /* init power supplier framework */
  for (i = 0; i < ARRAY_SIZE(s3c_power_supplies); i++) {
    ret = power_supply_register(&pdev->dev, 
        &s3c_power_supplies[i]);
    if (ret) {
      dev_err(dev, "Failed to register"
          "power supply %d,%d\n", i, ret);
      goto __end__;
    }
  }

  /* create sec detail attributes */
  s3c_bat_create_attrs(s3c_power_supplies[CHARGER_BATTERY].dev);

  s3c_battery_initial = 1;
  force_update = 0;

	init_timer(&flash_power_timer);
  flash_power_timer.function = flash_power_timer_handler;
  //flash_power_timer.expires = jiffies + 3*HZ;
  add_timer(&flash_power_timer);

//  setup_timer(&charger_timer, charger_timer_func, 0);
//  setup_timer(&supply_timer, supply_timer_func, 0);
    setup_timer(&polling_timer, polling_timer_func, 0);

    polling_timer_func(0);
  s3c_bat_status_update(&s3c_power_supplies[CHARGER_BATTERY]);

//  mod_timer(&polling_timer, jiffies + msecs_to_jiffies(POLL_MSTIME));


  
__end__:
  return ret;
}

static int __devexit s3c_bat_remove(struct platform_device *pdev)
{
  int i;
  dev_info(dev, "%s\n", __func__);

//  del_timer_sync(&charger_timer);
//  del_timer_sync(&supply_timer);
    del_timer_sync(&polling_timer);

  for (i = 0; i < ARRAY_SIZE(s3c_power_supplies); i++) {
    power_supply_unregister(&s3c_power_supplies[i]);
  }
 del_timer(&flash_power_timer);
  iounmap(adc_base);
  return 0;
}

static struct platform_driver s3c_bat_driver = {
  .driver.name  = DRIVER_NAME,
  .driver.owner  = THIS_MODULE,
  .probe    = s3c_bat_probe,
  .remove    = __devexit_p(s3c_bat_remove),
  .suspend  = s3c_bat_suspend,
  .resume    = s3c_bat_resume,
};

/* Initailize GPIO */
static void s3c_bat_init_hw(void)
{
#if defined(CONFIG_S5PV210_A8388_V1)
  s3c_gpio_cfgpin(GPIO_CHARGE_STAT1, S3C_GPIO_INPUT);
  s3c_gpio_setpull(GPIO_CHARGE_STAT1, S3C_GPIO_PULL_NONE);
#elif defined(CONFIG_S5PV210_A8388_V2)	
  s3c_gpio_cfgpin(GPIO_CHARGE_STAT1, S3C_GPIO_INPUT);
  s3c_gpio_setpull(GPIO_CHARGE_STAT1, S3C_GPIO_PULL_NONE);

  s3c_gpio_cfgpin(S5PV210_GPH3(0), S3C_GPIO_INPUT);
  s3c_gpio_setpull(S5PV210_GPH3(0), S3C_GPIO_PULL_NONE);
#elif defined(CONFIG_S5PV210_S9) || defined(CONFIG_S5PV210_A9)
  s3c_gpio_cfgpin(GPIO_CHARGE_STAT1, S3C_GPIO_INPUT);
  s3c_gpio_setpull(GPIO_CHARGE_STAT1, S3C_GPIO_PULL_NONE);

  s3c_gpio_cfgpin(GPIO_CHARGE_STAT2, S3C_GPIO_INPUT);
  s3c_gpio_setpull(GPIO_CHARGE_STAT2, S3C_GPIO_PULL_NONE);
#endif
}

static int __init s3c_bat_init(void)
{
  pr_info("%s\n", __func__);

  s3c_bat_init_hw();

  wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");

  return platform_driver_register(&s3c_bat_driver);
}

static void __exit s3c_bat_exit(void)
{
  pr_info("%s\n", __func__);
  platform_driver_unregister(&s3c_bat_driver);
}

module_init(s3c_bat_init);
module_exit(s3c_bat_exit);

MODULE_AUTHOR("HuiSung Kang <hs1218.kang@samsung.com>");
MODULE_DESCRIPTION("S3C battery driver for SMDK Board");
MODULE_LICENSE("GPL");
