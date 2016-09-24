/******************************************************************************
 *    COPYRIGHT (C) 2013 llui. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by llui 2013-09-03
 *
******************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <asm/byteorder.h>
#include <linux/io.h>
#include <asm/system.h>
#include <asm/unaligned.h>
#include <linux/kthread.h>

#define HIUSBOTG_AUTHOR                "hisilicon LiuHui"
#define HIUSBOTG_DESC                  "USB 2.0 OTG Driver"

static char *usbotg_name = "usbotg";

extern int do_usbotg(void);

static struct task_struct *kusbotg_task = NULL;

static int usbotg_thread(void *__unused)
{
	pr_info("%s: usb otg driver registered", usbotg_name);

	while (!kthread_should_stop()) {
		do_usbotg();
		msleep(CONFIG_HIUSB_OTG_SWITCH_TIME);
	}

	return 0;
}

static int __init usb_otg_init(void)
{
	kusbotg_task = kthread_run(usbotg_thread, NULL, "usb-otg");
	if (IS_ERR(kusbotg_task)) {
		pr_err("%s: creating kthread failed\n", usbotg_name);
		kusbotg_task = NULL;
		return -1;
	}
	return 0;
}

static void __exit usb_otg_exit(void)
{
	if (kusbotg_task) {
		kthread_stop(kusbotg_task);
		kusbotg_task = NULL;
	}
	pr_info("%s: usb otg driver exit", usbotg_name);
}

module_init(usb_otg_init);
module_exit(usb_otg_exit);

MODULE_DESCRIPTION(HIUSBOTG_DESC);
MODULE_AUTHOR(HIUSBOTG_AUTHOR);
MODULE_LICENSE("GPL");
