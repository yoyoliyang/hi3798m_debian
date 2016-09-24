
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/ahci_platform.h>
#include "ahci.h"

extern int hi_sata_init(struct device *dev, void __iomem *mmio);
extern void hi_sata_exit(struct device *dev);

static int hi_sata_suspend(struct device *dev)
{
	hi_sata_exit(dev);

	return 0;
}

static int hi_sata_resume(struct device *dev)
{
	struct ata_host *host = dev_get_drvdata(dev);
	struct ahci_host_priv *hpriv = host->private_data;
	hi_sata_init(dev, hpriv->mmio);

	return 0;
}

struct ahci_platform_data hi_ahci_platdata = {
	.init    = hi_sata_init,
	.exit    = hi_sata_exit,
	.suspend = hi_sata_suspend,
	.resume  = hi_sata_resume,
};

static struct resource hisata_ahci_resources[] = {
	[0] = {
		.start          = CONFIG_HI_SATA_IOBASE,
		.end            = CONFIG_HI_SATA_IOBASE +
					CONFIG_HI_SATA_IOSIZE - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = CONFIG_HI_SATA_IRQNUM,
		.end            = CONFIG_HI_SATA_IRQNUM,
		.flags		= IORESOURCE_IRQ,
	},
};

static u64 ahci_dmamask = ~(u32)0;

static void hisatav100_ahci_platdev_release(struct device *dev)
{
	return;
}

static struct platform_device hisata_ahci_device = {
	.name           = "ahci",
	.dev = {
		.platform_data          = &hi_ahci_platdata,
		.dma_mask               = &ahci_dmamask,
		.coherent_dma_mask      = 0xffffffff,
		.release                = hisatav100_ahci_platdev_release,
	},
	.num_resources  = ARRAY_SIZE(hisata_ahci_resources),
	.resource       = hisata_ahci_resources,
};

static int __init hi_ahci_init(void)
{
	int ret = 0;

	printk(KERN_INFO "hiahci: initializing\n");

	ret = platform_device_register(&hisata_ahci_device);
	if (ret) {
		printk(KERN_ERR "[%s %d] Hisilicon sata platform device register "
				"is failed!!!\n", __func__, __LINE__);
		return ret;
	}

	return ret;
}

static void __exit hi_ahci_exit(void)
{
	printk(KERN_INFO "hiahci: exit\n");

	platform_device_unregister(&hisata_ahci_device);
	return;
}
module_init(hi_ahci_init);
module_exit(hi_ahci_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Hisilicon SATA controller low level driver");
MODULE_VERSION("1.00");

