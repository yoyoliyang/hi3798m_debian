/*
 * xhci-plat.c - xHCI host controller driver platform Bus Glue.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 * Author: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * A lot of code borrowed from the Linux xHCI driver.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "xhci.h"
#include "hiusb.h"

static void xhci_plat_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_BROKEN_MSI;
}

/* called during probe() after chip reset completes */
static int xhci_plat_setup(struct usb_hcd *hcd)
{
	return xhci_gen_setup(hcd, xhci_plat_quirks);
}

static const struct hc_driver xhci_plat_xhci_driver = {
	.description =		"xhci-hcd",
	.product_desc =		"xHCI Host Controller",
	.hcd_priv_size =	sizeof(struct xhci_hcd *),

	/*
	 * generic hardware linkage
	 */
	.irq =			xhci_irq,
	.flags =		HCD_MEMORY | HCD_USB3 | HCD_SHARED,

	/*
	 * basic lifecycle operations
	 */
	.reset =		xhci_plat_setup,
	.start =		xhci_run,
	.stop =			xhci_stop,
	.shutdown =		xhci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		xhci_urb_enqueue,
	.urb_dequeue =		xhci_urb_dequeue,
	.alloc_dev =		xhci_alloc_dev,
	.free_dev =		xhci_free_dev,
	.alloc_streams =	xhci_alloc_streams,
	.free_streams =		xhci_free_streams,
	.add_endpoint =		xhci_add_endpoint,
	.drop_endpoint =	xhci_drop_endpoint,
	.endpoint_reset =	xhci_endpoint_reset,
	.check_bandwidth =	xhci_check_bandwidth,
	.reset_bandwidth =	xhci_reset_bandwidth,
	.address_device =	xhci_address_device,
	.update_hub_device =	xhci_update_hub_device,
	.reset_device =		xhci_discover_or_reset_device,

	/*
	 * scheduling support
	 */
	.get_frame_number =	xhci_get_frame,

	/* Root hub support */
	.hub_control =		xhci_hub_control,
	.hub_status_data =	xhci_hub_status_data,
	.bus_suspend =		xhci_bus_suspend,
	.bus_resume =		xhci_bus_resume,
};

static int xhci_plat_probe(struct platform_device *pdev)
{
	const struct hc_driver *driver;
	struct xhci_hcd *xhci;
	struct resource *res;
	struct usb_hcd *hcd;
	int ret;
	int irq;

	if (usb_disabled())
		return -ENODEV;

	driver = &xhci_plat_xhci_driver;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -ENODEV;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd)
		return -ENOMEM;

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
				driver->description)) {
		dev_dbg(&pdev->dev, "controller already in use\n");
		ret = -EBUSY;
		goto put_hcd;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_dbg(&pdev->dev, "error mapping memory\n");
		ret = -EFAULT;
		goto release_mem_region;
	}

	hiusb3_start_hcd(hcd->regs);

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto unmap_registers;

	/* USB 2.0 roothub is stored in the platform_device now. */
	hcd = dev_get_drvdata(&pdev->dev);
	xhci = hcd_to_xhci(hcd);
	xhci->shared_hcd = usb_create_shared_hcd(driver, &pdev->dev,
			dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto dealloc_usb2_hcd;
	}

	/*
	 * Set the xHCI pointer before xhci_plat_setup() (aka hcd_driver.reset)
	 * is called by usb_add_hcd().
	 */
	*((struct xhci_hcd **) xhci->shared_hcd->hcd_priv) = xhci;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret)
		goto put_usb3_hcd;

	return 0;

put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);

dealloc_usb2_hcd:
	usb_remove_hcd(hcd);

unmap_registers:
	iounmap(hcd->regs);
	hiusb3_stop_hcd();

release_mem_region:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);

put_hcd:
	usb_put_hcd(hcd);

	return ret;
}

static int xhci_plat_remove(struct platform_device *dev)
{
	struct usb_hcd	*hcd = platform_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);

	usb_remove_hcd(xhci->shared_hcd);
	usb_put_hcd(xhci->shared_hcd);

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
	kfree(xhci);
	hiusb3_stop_hcd();

	return 0;
}

#ifdef CONFIG_PM
static int hiusb_xhci_hcd_drv_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	int rc = 0;
	struct xhci_hcd *xhci;
	xhci = hcd_to_xhci(hcd);

	rc =  xhci_suspend(xhci);
	hiusb3_stop_hcd();
	return rc;
}

static int hiusb_xhci_hcd_drv_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct xhci_hcd *xhci;
	xhci = hcd_to_xhci(hcd);

	hiusb3_start_hcd(hcd->regs);
	
	return xhci_resume(xhci, false);
}

static const struct dev_pm_ops hiusb_xhci_pmops = {
	.suspend = hiusb_xhci_hcd_drv_suspend,
	.resume  = hiusb_xhci_hcd_drv_resume,
#ifdef CONFIG_PM_HIBERNATE
	.freeze = hiusb_xhci_hcd_drv_suspend,
	.thaw = hiusb_xhci_hcd_drv_resume,
	.poweroff = hiusb_xhci_hcd_drv_suspend,
	.restore = hiusb_xhci_hcd_drv_resume,
#endif

};

#define HIUSB_XHCI_PMOPS (&hiusb_xhci_pmops)

#else
#define HIUSB_XHCI_PMOPS NULL
#endif

static struct platform_driver usb_xhci_driver = {
	.probe	= xhci_plat_probe,
	.remove	= xhci_plat_remove,
	.driver	= {
		.name = "hiusb-xhci",
		.owner = THIS_MODULE,
		.pm    = HIUSB_XHCI_PMOPS,
	},
};
MODULE_ALIAS("platform:xhci-hcd");

static struct resource hiusb_xhci_res[] = {
	[0] = {
		.start  = CONFIG_HIUSB_XHCI_IOBASE,
		.end    = CONFIG_HIUSB_XHCI_IOBASE
			+ CONFIG_HIUSB_XHCI_IOSIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = CONFIG_HIUSB_XHCI_IRQNUM,
		.end    = CONFIG_HIUSB_XHCI_IRQNUM,
		.flags  = IORESOURCE_IRQ,
	},
};

static void usb_xhci_platdev_release(struct device *dev)
{
	/* These don't need to do anything because the
	   pdev structures are statically allocated. */
}

static u64 usb_dmamask = DMA_BIT_MASK(32);

static struct platform_device hiusb_xhci_platdev = {
	.name = "hiusb-xhci",
	.id = 0,
	.dev = {
		.dma_mask = &usb_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.release = usb_xhci_platdev_release,
	},
	.num_resources = ARRAY_SIZE(hiusb_xhci_res),
	.resource = hiusb_xhci_res,
};

int xhci_register_plat(void)
{
	return platform_driver_register(&usb_xhci_driver);
}

void xhci_unregister_plat(void)
{
	platform_driver_unregister(&usb_xhci_driver);
}
