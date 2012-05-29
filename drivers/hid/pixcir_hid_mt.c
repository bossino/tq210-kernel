/******************************************************************************
* pixcir_hid_mt.c v1.0 --  Driver for Pixcir Multi-Touch USB Touchscreens
* 							--2010.06.31
* pixcir_hid_mt.c v2.0 --  Driver modified to add ts calibration
* 							--2010.08.24
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of the
* License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*******************************************************************************/

#include <linux/version.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/time.h>

#include "pixcir_hid_mt.h"

#define DRIVER_VERSION "v2.0"
#define DRIVER_AUTHOR "bee<http://www.pixcir.com.cn>"
#define DRIVER_DESC "Pixcir HID Touchscreen Driver"
#define DRIVER_LICENSE "GPL"
      
#define TOUCH_PACKAGE_LEN	14

#define MAX_TRANSFER (PAGE_SIZE - 512)

#define USB_PIXCIR_MINOR_BASE 192
/* temp store */
static unsigned char finger1,finger2;

#define USB_REQ_SET_REPORT 0x09

#define USB_VENDOR_ID_PIXCIR CONFIG_USB_HOLYMAP_MULTITOUCH_VENDOR_ID
#define USB_DEVICE_ID_PIXCIR CONFIG_USB_HOLYMAP_MULTITOUCH_PRODUCT_ID

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

struct pixcir_mt_usb {
  unsigned char *data;
  struct input_dev *input_dev;
  struct psmt_device_info *type;
  struct usb_device *udev;
  dma_addr_t data_dma;
     	struct urb *urb;
  char name[128];
  char phys0[64];
  char phys1[64];
  int x1, y1, x2, y2;     
  int touch1, touch2, press;
	struct usb_interface *interface;
  struct kref kref;
  int open_count;
	struct mutex		io_mutex;		/* synchronize I/O with disconnect */
};
#define to_pixcir_dev(d) container_of(d,struct pixcir_mt_usb,kref)

static struct usb_driver pixcir_driver;

struct psmt_device_info {
      int min_xc, max_xc;
      int min_yc, max_yc;
      int min_press, max_press;
      int rept_size;
      void (*process_pkt) (struct pixcir_mt_usb *psmt, unsigned char *pkt, int len);
      /*
       * used to get the packet len. possible return values:
       * > 0: packet len
       * = 0: skip one byte
       * < 0: -return value more bytes needed
       */
      int  (*get_pkt_len) (unsigned char *pkt, int len);
      int  (*read_data)   (struct pixcir_mt_usb *psmt, unsigned char *pkt);
      int  (*init)        (struct pixcir_mt_usb *psmt);
};


#define NO_TOUCH      0xf8
#define TOUCH_PRESS   0xff
#define TOUCH_RELEASE 0xfe


static int pixcir_read_data(struct pixcir_mt_usb *dev, unsigned char *pkt){
      
      dev->x1 = ((pkt[4] & 0xFF) << 8) | (pkt[3] & 0xFF);
      dev->y1 = ((pkt[6] & 0xFF) << 8) | (pkt[5] & 0xFF);

      dev->x2 = ((pkt[10] & 0xFF) << 8) | (pkt[9] & 0xFF);
      dev->y2 = ((pkt[12] & 0xFF) << 8) | (pkt[11] & 0xFF);

      //printk("x:%d\n",dev->x1);
      //printk("y:%d\n",dev->y1);
      //printk("x:%d\n",dev->x2);
      //printk("y:%d\n",dev->y2);

      finger1 = pkt[1];
      finger2 = pkt[7];

      if(finger1 == NO_TOUCH || finger1 == TOUCH_RELEASE) dev->touch1 = 0;
      if(finger1 == TOUCH_PRESS)                           dev->touch1 = 1;

      if(finger2 == NO_TOUCH || finger2 == TOUCH_RELEASE) dev->touch2 = 0;
      if(finger2 == TOUCH_PRESS)                           dev->touch2 = 1;

      return 1;
}

static struct psmt_device_info type = {
             .min_xc  = TOUCHSCREEN_MINX,
             .max_xc  = TOUCHSCREEN_MAXX,
             .min_yc  = TOUCHSCREEN_MINY,
             .max_yc  = TOUCHSCREEN_MAXY,
             .rept_size = 8,
             .read_data = pixcir_read_data,
};


static void usbtouch_process_pkt(struct pixcir_mt_usb *psmt,
                                 unsigned char *pkt, int len)
{
  int z=50;
  int w=15;
  int x,y;

  struct psmt_device_info *type = psmt->type;
	  
  if (!type->read_data(psmt, pkt))
    return;
     
  if((psmt->x1 > TOUCHSCREEN_MAXX) || (psmt->y1 > TOUCHSCREEN_MAXY))
    return;
  if((psmt->x2 > TOUCHSCREEN_MAXX) || (psmt->y2 > TOUCHSCREEN_MAXY))
    return;

  input_report_key(psmt->input_dev, BTN_TOUCH, psmt->touch1);    

  if(psmt->touch1){
    input_report_abs(psmt->input_dev, ABS_X, psmt->x1);
    input_report_abs(psmt->input_dev, ABS_Y, psmt->y1);
  }

  if(!(psmt->touch2+psmt->touch1)){
    z=0;
    w=0;
  }

  x = psmt->x1;
  y = psmt->y1;
#ifdef CONFIG_TOUCHSCREEN_ROTATION_90
  x = psmt->y1;
  y = psmt->x1;
#endif
#ifdef CONFIG_TOUCHSCREEN_ROTATION_180
  x = TOUCHSCREEN_MAXX - psmt->x1;
  y = TOUCHSCREEN_MAXY - psmt->y1;
#endif
#ifdef CONFIG_TOUCHSCREEN_ROTATION_270
  x = TOUCHSCREEN_MAXX - psmt->y1;
  y = TOUCHSCREEN_MAXY - psmt->x1;
#endif

  input_report_abs(psmt->input_dev, ABS_MT_TOUCH_MAJOR, z);
  input_report_abs(psmt->input_dev, ABS_MT_WIDTH_MAJOR, w);
  input_report_abs(psmt->input_dev, ABS_MT_POSITION_X, x);
  input_report_abs(psmt->input_dev, ABS_MT_POSITION_Y, y);
  input_mt_sync(psmt->input_dev);
//printk("x1= %04d y1=%04d\n", psmt->x1, psmt->y1);
  if (psmt->touch2) {
    x = psmt->x2;
    y = psmt->y2;
  #ifdef CONFIG_TOUCHSCREEN_ROTATION_90
    x = psmt->y2;
    y = psmt->x2;
  #endif
  #ifdef CONFIG_TOUCHSCREEN_ROTATION_180
    x = TOUCHSCREEN_MAXX - psmt->x2;
    y = TOUCHSCREEN_MAXY - psmt->y2;
  #endif
  #ifdef CONFIG_TOUCHSCREEN_ROTATION_270
    x = TOUCHSCREEN_MAXX - psmt->y2;
    y = TOUCHSCREEN_MAXY - psmt->x2;
  #endif
    input_report_abs(psmt->input_dev, ABS_MT_TOUCH_MAJOR, z);
    input_report_abs(psmt->input_dev, ABS_MT_WIDTH_MAJOR, w);
    input_report_abs(psmt->input_dev, ABS_MT_POSITION_X, x);
    input_report_abs(psmt->input_dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(psmt->input_dev);
//printk("x2= %04d y2=%04d\n", psmt->x2, psmt->y2);
  }
  input_sync(psmt->input_dev);
}


static void pixcir_mt_irq(struct urb *urb)
{
  struct pixcir_mt_usb *psmt = urb->context;
  int retval;
#ifdef DEBUG 
  unsigned char* data = psmt->data;
#endif
  switch (urb->status) {
    case 0:
      // success
      break;
  case -ETIME:
      // this urb is timing out
      dbg("%s - urb timed out - was the device unplugged?", __FUNCTION__);
      return;
  case -ECONNRESET:
  case -ENOENT:
  case -ESHUTDOWN:
    return;
  default:
    goto resubmit;
  }

  psmt->type->process_pkt(psmt, psmt->data, urb->actual_length);
    resubmit:
  retval = usb_submit_urb(urb, GFP_ATOMIC);
  if (retval)
    err("%s - usb_submit_urb failed with result: %d", __FUNCTION__, retval);
}

static void pixcir_delete(struct kref *kref)
{
	struct pixcir_mt_usb *dev = to_pixcir_dev(kref);
	usb_put_dev(dev->udev);
	kfree(dev);
}

/**********************************************************/
/*                   pixcir open				   */
/**********************************************************/
static int pixcir_open (struct inode *inode, struct file *file)
{
	struct pixcir_mt_usb *dev;
	struct usb_interface *interface;
	int subminor;
	char *buf,*getbuf;
	int retval;
	
	buf = NULL;
	getbuf = NULL;
	retval=0;	

	subminor = iminor(inode);
	
	//printk("enter pixcir_open function\n");	

	interface = usb_find_interface(&pixcir_driver, subminor);
	if (!interface) {
		err("%s - error, can't find device for minor %d",
		     __func__, subminor);
		retval = -ENODEV;
		return retval;	
	}
	
	dev = usb_get_intfdata(interface);
	if (!dev) {
		retval = -ENODEV;
		return retval;
	}
	
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	kref_get(&dev->kref);
	mutex_lock(&dev->io_mutex);

	if (!dev->open_count++) {
		retval = usb_autopm_get_interface(interface);
			if (retval) {
				dev->open_count--;
				mutex_unlock(&dev->io_mutex);
				kref_put(&dev->kref, pixcir_delete);
				return retval;
			}
	}
	/* save our object in the file's private structure */
	file->private_data = dev;
	mutex_unlock(&dev->io_mutex);
	
	return 0;
}


/**********************************************************/
/*                   pixcir read			   */
/**********************************************************/
static ssize_t pixcir_read(struct file *file, char __user *buf, size_t count,loff_t *ppos)
{
	struct pixcir_mt_usb *dev;
	char *readbuf;
	int retval;
	
	readbuf = NULL;

	dev = (struct pixcir_mt_usb *) file->private_data;
	readbuf = kmalloc(count,GFP_KERNEL);
	retval=0;

	retval = usb_control_msg(dev->udev,
                       usb_rcvctrlpipe(dev->udev, 0),
                       USB_REQ_GET_DESCRIPTOR,
                       USB_DIR_IN,
                       (0x01 << 8),
                       dev->interface->cur_altsetting->desc.bInterfaceNumber,
                       readbuf,count, HZ);
	//printk("retval = %d\n",retval);
	if (copy_to_user(buf,readbuf,count)){
		printk("read descripter failed\n");
		kfree(readbuf);
		readbuf = NULL;
		return -EFAULT;
	}
	kfree(readbuf);
	readbuf = NULL;

	return retval;
}


/**********************************************************/
/*			pixcir write				   */
/**********************************************************/
static ssize_t pixcir_write(struct file *file, const char *buf,size_t count, loff_t *ppos)
{
	struct pixcir_mt_usb *dev;
	char *writebuf;
	int retval;

	writebuf = NULL;

	dev = (struct pixcir_mt_usb *) file->private_data;

	writebuf = kmalloc(count,GFP_KERNEL);
	if (writebuf == NULL)
		return -ENOMEM;
	if (copy_from_user(writebuf,buf,count)){
		kfree(writebuf);
		return -EFAULT;
	}
	
	retval = usb_control_msg(dev->udev,
                       usb_sndctrlpipe(dev->udev,0),
                       USB_REQ_SET_REPORT,
                       USB_TYPE_CLASS|USB_RECIP_INTERFACE,
                       (2 << 8)+7,
                       dev->interface->cur_altsetting->desc.bInterfaceNumber, 
		       writebuf,count, HZ);
	//printk("retval = %d\n",retval);
	if(retval<0){
		printk("send calibration command failed!\n");
		kfree(writebuf);
		writebuf = NULL;
		return retval;
	}
	kfree(writebuf);
	writebuf = NULL;

	return retval;
}

/**********************************************************/
/*                   pixcir release			   */
/**********************************************************/
static int pixcir_release(struct inode *inode, struct file *file)
{
	struct pixcir_mt_usb *dev;
	//printk("enter pixcir_release function\n");

	dev = (struct pixcir_mt_usb *)file->private_data;
	if (dev == NULL)
		return -ENODEV;

	mutex_lock(&dev->io_mutex);
	if (!--dev->open_count)// && dev->interface)
		usb_autopm_put_interface(dev->interface);
	mutex_unlock(&dev->io_mutex);

	kref_put(&dev->kref, pixcir_delete);
	
	return 0;
}


static const struct file_operations pixcir_fops = {
	.owner = THIS_MODULE,
	.open = pixcir_open,
	.read = pixcir_read,
	.write = pixcir_write,
	.release = pixcir_release,
	//.flush = pixcir_flush,
};

//define a structure which conmunicate with user space app by usb master ID
static struct usb_class_driver pixcir_class_driver = {
	.name = "pixhid%d",
	.fops = &pixcir_fops,
	.minor_base = USB_PIXCIR_MINOR_BASE,
};


static int pixcir_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_host_interface *interface = intf->cur_altsetting;
	struct usb_device *dev = interface_to_usbdev(intf);
	struct input_dev *input_dev1;

	int n = 0, insize = TOUCH_PACKAGE_LEN;
	int err;
 	const char *path;
 	int  len;
 	char input_buf[10];
	
 	struct pixcir_mt_usb *psmt = kzalloc(sizeof(struct pixcir_mt_usb), GFP_KERNEL);

	kref_init(&psmt->kref);
	mutex_init(&psmt->io_mutex);
	
  	printk("%s\n", __FUNCTION__);
	psmt->type = &type;
	psmt->udev = dev;

	if (dev->manufacturer)
    strlcpy(psmt->name, dev->manufacturer, sizeof(psmt->name));

	if (dev->product) {
    if (dev->manufacturer)
      strlcat(psmt->name, " ", sizeof(psmt->name));
    strlcat(psmt->name, dev->product, sizeof(psmt->name));
	}

	if (!strlen(psmt->name))
    snprintf(psmt->name, sizeof(psmt->name), "USB Touchscreen %04x:%04x", le16_to_cpu(dev->descriptor.idVendor), le16_to_cpu(dev->descriptor.idProduct));

	if (!psmt->type->process_pkt) {
    printk("process_pkt is null\n");
    psmt->type->process_pkt = usbtouch_process_pkt;
 	}

	usb_set_intfdata(intf, psmt);
	
	err = usb_register_dev(intf,&pixcir_class_driver);
	if(err){
		printk("Not able to get minor for this device.");
	}

	dev_info(&intf->dev,"now attach to USB-%d",intf->minor);	

	input_dev1 = input_allocate_device();
	input_dev1->name = "pixcir_hid_mt_v2.0";
	usb_to_input_id(dev, &input_dev1->id);
	psmt->input_dev = input_dev1;

	if(!psmt|| !input_dev1) {
    printk("Memory is not enough\n");
    goto fail1;
	}

	input_dev1->dev.parent = &intf->dev;
	input_set_drvdata(input_dev1, psmt);

	set_bit(EV_SYN, input_dev1->evbit);
	set_bit(EV_KEY, input_dev1->evbit);
	set_bit(EV_ABS, input_dev1->evbit);
	set_bit(BTN_TOUCH, input_dev1->keybit);
	input_set_abs_params(input_dev1, ABS_X, psmt->type->min_xc, psmt->type->max_xc, 0, 0);
	input_set_abs_params(input_dev1, ABS_Y, psmt->type->min_yc, psmt->type->max_yc, 0, 0);
	input_set_abs_params(input_dev1, ABS_MT_POSITION_X, psmt->type->min_xc, psmt->type->max_xc, 0, 0);
	input_set_abs_params(input_dev1, ABS_MT_POSITION_Y, psmt->type->min_yc, psmt->type->max_yc, 0, 0);
  input_set_abs_params(input_dev1, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
  input_set_abs_params(input_dev1, ABS_MT_WIDTH_MAJOR, 0, 25, 0, 0);

  psmt->data = usb_alloc_coherent(dev, insize, GFP_KERNEL, &psmt->data_dma);

  if(!psmt->data) {
    printk("psmt->data allocating fail");
    goto fail;
  }

  for (n = 0; n < interface->desc.bNumEndpoints; n++) {
    struct usb_endpoint_descriptor *endpoint;
    int pipe;
    int interval;

    endpoint = &interface->endpoint[n].desc;


		/*find a int endpoint*/
    if (!usb_endpoint_xfer_int(endpoint))
                   continue;

    interval = endpoint->bInterval;
    if (usb_endpoint_dir_in(endpoint)) {
      if (psmt->urb)
        continue;
      if (!(psmt->urb = usb_alloc_urb(0, GFP_KERNEL)))
        goto fail;

      pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
      usb_fill_int_urb(psmt->urb, dev, pipe, psmt->data, insize, pixcir_mt_irq, psmt, interval);
      psmt->urb->transfer_dma = psmt->data_dma;
      psmt->urb->transfer_flags = URB_NO_TRANSFER_DMA_MAP;
    }
  }


  if (usb_submit_urb(psmt->urb, GFP_ATOMIC)) {
    printk("usb submit urb error\n");
    return -EIO;
  }

  err = input_register_device(psmt->input_dev);
  if(err) {
    printk("pixcir_probe: Could not register input device(touchscreen)!\n");
      return -EIO;
  }

  path = kobject_get_path(&psmt->input_dev->dev.kobj, GFP_KERNEL);
  len=strlen(path);
  if(len>10){
		if(path[len-2]=='t'){
			memset(input_buf,'\0',9);
			strncpy(input_buf,&path[len-6],6);
		}else if(path[len-3]=='t'){
			memset(input_buf,'\0',9);
			strncpy(input_buf,&path[len-7],7);
		}else{
			 goto fail;
		}
  }else{
     	    goto fail;
  }

  usb_make_path(dev, psmt->phys0, sizeof(psmt->phys0));
  strlcat(psmt->phys0,input_buf, sizeof(psmt->phys0));

  return 0;

fail:
  usb_free_urb(psmt->urb);
  psmt->urb = NULL;
  usb_free_coherent(dev, insize, psmt->data, psmt->data_dma);
fail1:
  input_free_device(input_dev1);
  kfree(psmt);
  return 1;
}


static void pixcir_disconnect(struct usb_interface *intf)
{
  struct pixcir_mt_usb *psmt = usb_get_intfdata(intf);
  usb_set_intfdata(intf, NULL);

	usb_deregister_dev(intf,&pixcir_class_driver);
  if (psmt) {
    input_unregister_device(psmt->input_dev);
    usb_kill_urb(psmt->urb);
    usb_free_urb(psmt->urb);
    usb_free_coherent(interface_to_usbdev(intf), TOUCH_PACKAGE_LEN, psmt->data, psmt->data_dma);
    kfree(psmt);
  }
}


static const struct usb_device_id pixcir_devices[] = {
  { USB_DEVICE(USB_VENDOR_ID_PIXCIR, USB_DEVICE_ID_PIXCIR) },
  { }
};

MODULE_DEVICE_TABLE(usb, pixcir_devices);

static struct usb_driver pixcir_driver = {
  .name = "pixcir-hid-touchscreen-v2.0",
  .probe = pixcir_probe,
  .disconnect = pixcir_disconnect,
  .id_table = pixcir_devices,
};

static int pixcir_init(void)
{
  printk("pixcir_init\n");
  return usb_register(&pixcir_driver);
}

static void pixcir_exit(void)
{
  printk("pixcir_exit\n");
  usb_deregister(&pixcir_driver);
}

module_init(pixcir_init);
module_exit(pixcir_exit);
