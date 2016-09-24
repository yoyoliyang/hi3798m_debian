/*
 * EHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 2010 Hisilicon
 *
 * This file is licenced under the GPL.
 */

#include <linux/platform_device.h>
#include "hiusb.h"

static int hiusb_ehci_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int ret = ehci_init(hcd);

	/*
	 * usb2.0 host ip quirk(maybe lost IOC interrupt), so
	 * driver need_io_watchdog to avoid it.
	 */
	ehci->need_io_watchdog = 1;
	return ret;
}

static const struct hc_driver hiusb_ehci_hc_driver = {
	.description		= hcd_name,
	.product_desc		= "HIUSB EHCI",
	.hcd_priv_size		= sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq			= ehci_irq,
	.flags			= HCD_MEMORY | HCD_USB2 | HCD_BH,

	/*
	 * basic lifecycle operations
	 *
	 * FIXME -- ehci_init() doesn't do enough here.
	 * See ehci-ppc-soc for a complete implementation.
	 */
	.reset			= hiusb_ehci_setup,
	.start			= ehci_run,
	.stop			= ehci_stop,
	.shutdown		= ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ehci_urb_enqueue,
	.urb_dequeue		= ehci_urb_dequeue,
	.endpoint_disable	= ehci_endpoint_disable,
	.endpoint_reset		= ehci_endpoint_reset,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ehci_hub_status_data,
	.hub_control		= ehci_hub_control,
	.bus_suspend		= ehci_bus_suspend,
	.bus_resume		= ehci_bus_resume,
	.relinquish_port	= ehci_relinquish_port,
	.port_handed_over	= ehci_port_handed_over,

	.clear_tt_buffer_complete	= ehci_clear_tt_buffer_complete,
};

static int hiusb_ehci_hcd_drv_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	struct resource *res;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	if (pdev->resource[1].flags != IORESOURCE_IRQ) {
		pr_debug("resource[1] is not IORESOURCE_IRQ");
		return -ENOMEM;
	}
	hcd = usb_create_hcd(&hiusb_ehci_hc_driver, &pdev->dev, "hiusb-ehci");
	if (!hcd)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		pr_debug("request_mem_region failed");
		ret = -EBUSY;
		goto err1;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		pr_debug("ioremap failed");
		ret = -ENOMEM;
		goto err2;
	}

	hiusb_start_hcd(res->start);

	ehci = hcd_to_ehci(hcd);
	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs + HC_LENGTH(ehci, readl(&ehci->caps->hc_capbase));
	/* cache this readonly data; minimize chip reads */
	ehci->hcs_params = readl(&ehci->caps->hcs_params);

	ret = usb_add_hcd(hcd, pdev->resource[1].start,
			  IRQF_DISABLED | IRQF_SHARED);
	if (ret == 0) {
		platform_set_drvdata(pdev, hcd);
		return ret;
	}

	hiusb_stop_hcd(res->start);
	iounmap(hcd->regs);
err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
	return ret;
}

static int hiusb_ehci_hcd_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
	hiusb_stop_hcd(hcd->rsrc_start);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int hiusb_ehci_hcd_drv_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	bool do_wakeup = device_may_wakeup(dev);
	int rc = 0;

	rc =  ehci_suspend(hcd, do_wakeup);

	hiusb_stop_hcd(hcd->rsrc_start);
	return rc;
}

static int hiusb_ehci_hcd_drv_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);

	hiusb_start_hcd(hcd->rsrc_start);
	
	ehci_resume(hcd, false);

	return 0;
}

static const struct dev_pm_ops hiusb_ehci_pmops = {
	.suspend = hiusb_ehci_hcd_drv_suspend,
	.resume  = hiusb_ehci_hcd_drv_resume,
#ifdef CONFIG_PM_HIBERNATE
	.freeze = hiusb_ehci_hcd_drv_suspend,
	.thaw = hiusb_ehci_hcd_drv_resume,
	.poweroff = hiusb_ehci_hcd_drv_suspend,
	.restore = hiusb_ehci_hcd_drv_resume,
#endif
};

#define HIUSB_EHCI_PMOPS (&hiusb_ehci_pmops)

#else
#define HIUSB_EHCI_PMOPS NULL
#endif

static struct platform_driver hiusb_ehci_hcd_driver = {
	.probe         = hiusb_ehci_hcd_drv_probe,
	.remove        = hiusb_ehci_hcd_drv_remove,
	.shutdown      = usb_hcd_platform_shutdown,
	.driver = {
		.name  = "hiusb-ehci",
		.owner = THIS_MODULE,
		.pm    = HIUSB_EHCI_PMOPS,
	}
};

MODULE_ALIAS("platform:hiusb-ehci");

/*****************************************************************************/

static struct resource hiusb_ehci_res[] = {
	[0] = {
		.start = CONFIG_HIUSB_EHCI_IOBASE,
		.end   = CONFIG_HIUSB_EHCI_IOBASE
			+ CONFIG_HIUSB_EHCI_IOSIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = CONFIG_HIUSB_EHCI_IRQNUM,
		.end   = CONFIG_HIUSB_EHCI_IRQNUM,
		.flags = IORESOURCE_IRQ,
	},
};

static void usb_ehci_platdev_release(struct device *dev)
{
		/* These don't need to do anything because the
		 pdev structures are statically allocated. */
}

static u64 usb_dmamask = DMA_BIT_MASK(32);

static struct platform_device hiusb_ehci_platdev = {
	.name = "hiusb-ehci",
	.id = 0,
	.dev = {
		.platform_data     = NULL,
		.dma_mask          = &usb_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.release           = usb_ehci_platdev_release,
	},
	.num_resources = ARRAY_SIZE(hiusb_ehci_res),
	.resource      = hiusb_ehci_res,
};

#ifdef CONFIG_ARCH_HI3798MX
static struct resource hiusb_ehci1_res[] = {
	[0] = {
		.start = CONFIG_HIUSB_EHCI1_IOBASE,
		.end   = CONFIG_HIUSB_EHCI1_IOBASE
			+ CONFIG_HIUSB_EHCI1_IOSIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = CONFIG_HIUSB_EHCI1_IRQNUM,
		.end   = CONFIG_HIUSB_EHCI1_IRQNUM,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device hiusb_ehci1_platdev = {
	.name = "hiusb-ehci",
	.id = 1,
	.dev = {
		.platform_data     = NULL,
		.dma_mask          = &usb_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.release           = usb_ehci_platdev_release,
	},
	.num_resources = ARRAY_SIZE(hiusb_ehci1_res),
	.resource      = hiusb_ehci1_res,
};
#endif
