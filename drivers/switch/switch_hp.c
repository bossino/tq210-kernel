/*
 *  drivers/switch/switch_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <mach/regs-adc.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <plat/adc.h>

//#define IRQ_HP	IRQ_EINT3
//static void __iomem     *adc_base;
volatile int earphone_mic =0;
static struct hp_switch_data *my_switch_data;
extern int s3c_adc_get_adc_data(int channel);
extern bool wm8960_setbias(int onoff);
struct hp_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct work_struct work;
};

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
//	//change channel
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

bool check_headset_mic(void)
{
	int adc_value;
#ifdef CONFIG_SND_SOC_WM8960
  adc_value = 8;
  while(adc_value-->0)
  {
    if(wm8960_setbias(1))
      break;
    msleep(500);
  }
#endif
//  adc_value = s3c_adc_read(client, 2);
  adc_value = s3c_adc_get_adc_data(2);
  if(adc_value == 0)
  {
    msleep(500);
//    adc_value = s3c_adc_read(client, 2);
    adc_value = s3c_adc_get_adc_data(2);
  }
//printk("hp_adc = %04d \n", adc_value);
  if(adc_value>0)
    return true;
  
  return false;
}

static void hp_switch_work(struct work_struct *work)
{
	int tmp;
	int i = 5;
	int dat = 0;
	int state;
	struct hp_switch_data	*data =
		container_of(work, struct hp_switch_data, work);

  msleep(500);
	state = gpio_get_value(data->gpio);
	switch_set_state(&data->sdev, state);
  if(!state)
  {
    printk("HP pull in\n");
    //checkout weather the earphone with mic  
//  	while(i--)
//  	{
//			tmp = s3c_getadcdata(2); 
//			printk("tmp = %d ++++++++++++++++\n",tmp);
//			dat += tmp;
//			msleep(100);
// 	 	}
//  	dat /= 5;
//printk("dat = %d ++++++++++++++++\n",dat);
//  	if(dat>1000)
    if(check_headset_mic())
  		earphone_mic = 1;
  	else
  		earphone_mic = 0;	
  }
  else
    printk("HP pull out\n");

}

static irqreturn_t hp_irq_handler(int irq, void *dev_id)
{
	struct hp_switch_data *switch_data =
	    (struct hp_switch_data *)dev_id;

	schedule_work(&switch_data->work);
	return IRQ_HANDLED;
}

static ssize_t switch_hp_print_state(struct switch_dev *sdev, char *buf)
{
	struct hp_switch_data	*switch_data =
		container_of(sdev, struct hp_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static int hp_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
	struct hp_switch_data *switch_data;
	int ret = 0;

	if (!pdata)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct hp_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	switch_data->sdev.name = pdata->name;
	switch_data->gpio = pdata->gpio;
	switch_data->name_on = pdata->name_on;
	switch_data->name_off = pdata->name_off;
	switch_data->state_on = pdata->state_on;
	switch_data->state_off = pdata->state_off;
	switch_data->sdev.print_state = switch_hp_print_state;

    ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	ret = gpio_request(switch_data->gpio, pdev->name);
	if (ret < 0)
		goto err_request_gpio;

	ret = gpio_direction_input(switch_data->gpio);
	if (ret < 0)
		goto err_set_gpio_input;

	INIT_WORK(&switch_data->work, hp_switch_work);

	switch_data->irq = gpio_to_irq(switch_data->gpio);
	if (switch_data->irq < 0) {
		ret = switch_data->irq;
		goto err_detect_irq_num_failed;
	}
//	switch_data->irq = IRQ_HP;

	ret = request_irq(switch_data->irq, hp_irq_handler, IRQ_TYPE_EDGE_BOTH, pdev->name, switch_data);
	if (ret < 0)
		goto err_request_irq;

	/* Perform initial detection */
//	hp_switch_work(&switch_data->work);
	schedule_work(&switch_data->work);

  my_switch_data = switch_data;
	return 0;

err_request_irq:
err_detect_irq_num_failed:
err_set_gpio_input:
	gpio_free(switch_data->gpio);
err_request_gpio:
    switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int __devexit hp_switch_remove(struct platform_device *pdev)
{
	struct hp_switch_data *switch_data = platform_get_drvdata(pdev);

	cancel_work_sync(&switch_data->work);
	gpio_free(switch_data->gpio);
    switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	return 0;
}

static int hp_switch_resume(struct platform_device *pdev)
{
  schedule_work(&my_switch_data->work);
  return 0;
}

static struct platform_driver gpio_switch_driver = {
	.probe		= hp_switch_probe,
	.remove		= __devexit_p(hp_switch_remove),
	.resume   = hp_switch_resume,
	.driver		= {
		.name	= "switch-hp",
		.owner	= THIS_MODULE,
	},
};

static int __init hp_switch_init(void)
{
//	adc_base = ioremap(0xE1700000, 0x20);
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit hp_switch_exit(void)
{
//	iounmap(adc_base);
	platform_driver_unregister(&gpio_switch_driver);
}

module_init(hp_switch_init);
module_exit(hp_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");
