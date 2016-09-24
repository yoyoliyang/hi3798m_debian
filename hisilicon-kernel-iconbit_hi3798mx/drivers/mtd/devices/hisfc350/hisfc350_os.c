/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-02-06
 *
******************************************************************************/

#define pr_fmt(fmt) "spiflash: " fmt

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/semaphore.h> 
#include <linux/platform_device.h>
#include <linux/string_helpers.h>

#include "../../mtdcore.h"
#include "hisfc350_os.h"
#include "hisfc350.h"

/*****************************************************************************/
#define MAX_MTD_PARTITIONS        (32)

struct partition_entry {
	char name[16];
	unsigned long long start;
	unsigned long long length;
	unsigned int flags;
};

struct partition_info {
	int parts_num;
	struct partition_entry entry[MAX_MTD_PARTITIONS];
	struct mtd_partition parts[MAX_MTD_PARTITIONS];
};

static struct partition_info ptn_info;

/*****************************************************************************/

static int hisfc350_os_reg_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	int ret;
	int state;
	struct hisfc_host *host = MTD_TO_HOST(mtd);

	mutex_lock(&host->lock);

	ret = hisfc350_reg_erase(host, mtd->size, instr->addr,
		instr->len, &state);
	instr->state = (u_char)state;
	mtd_erase_callback(instr);

	mutex_unlock(&host->lock);

	return ret;
}
/*****************************************************************************/

static int hisfc350_os_dma_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	int ret;
	struct hisfc_host *host = MTD_TO_HOST(mtd);

	mutex_lock(&host->lock);

	ret = hisfc350_dma_write(host, mtd->size, (unsigned int)to,
		(unsigned int)len, (unsigned int *)retlen,
		(unsigned char *)buf);

	mutex_unlock(&host->lock);

	return ret;
}
/*****************************************************************************/

static int hisfc350_os_dma_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
	int ret;
	struct hisfc_host *host = MTD_TO_HOST(mtd);

	mutex_lock(&host->lock);

	ret = hisfc350_dma_read(host, mtd->size, (unsigned int)from,
		(unsigned int)len, (unsigned int *)retlen, buf);

	mutex_unlock(&host->lock);

	return ret;
}
/*****************************************************************************/
#ifdef CONFIG_PM
static int hisfc350_os_driver_suspend(struct platform_device *pltdev,
	pm_message_t state)
{
	int ix;
	struct hisfc_host *host = platform_get_drvdata(pltdev);
	struct hisfc_spi *spi = host->spi;

	for (ix = 0; ix < host->num_chip; ix++, spi++) {
		spi->driver->wait_ready(spi);
		if (spi->addrcycle == 4)
			spi->driver->entry_4addr(spi, FALSE);
	}

	host->set_system_clock(host, spi->read, FALSE);

	return 0;
}
/*****************************************************************************/

static int hisfc350_os_driver_resume(struct platform_device *pltdev)
{
	int ix;
	struct hisfc_host *host = platform_get_drvdata(pltdev);
	struct hisfc_spi *spi = host->spi;
	unsigned long regval;

	hisfc350_set_system_clock(host, NULL, TRUE);
	hisfc_write(host, HISFC350_TIMING,
		HISFC350_TIMING_TCSS(0x6)
		| HISFC350_TIMING_TCSH(0x6)
		| HISFC350_TIMING_TSHSL(0xf));

	for (ix = 0; ix < host->num_chip; spi++, ix++) {
		regval = hisfc_read(host, HISFC350_BUS_FLASH_SIZE);
		regval &= ~(HISFC350_BUS_FLASH_SIZE_CS0_MASK
			<< (spi->chipselect << 3));
		regval |= (hisfc350_map_chipsize(spi->chipsize)
			<< (spi->chipselect << 3));
		hisfc_write(host, HISFC350_BUS_FLASH_SIZE, regval);

		hisfc_write(host,
			(HISFC350_BUS_BASE_ADDR_CS0 + (spi->chipselect << 2)),
			(CONFIG_HISFC350_BUFFER_BASE_ADDRESS
				+ (unsigned long)((char *)spi->iobase
					- (char *)host->iobase)));
		if (spi->addrcycle == 4)
			spi->driver->entry_4addr(spi, TRUE);
	}
	return 0;
}
#endif /* CONFIG_PM */
/*****************************************************************************/

static int hisfc_os_add_paratitions(struct hisfc_host *host)
{
	int ix;
	int nr_parts = 0;
	int ret = 0;
	struct mtd_partition *parts = NULL;

#ifdef CONFIG_MTD_CMDLINE_PARTS
	static const char *part_probes[] = {"cmdlinepart", NULL, };
	nr_parts = parse_mtd_partitions(host->mtd, part_probes, &parts, 0);
#endif /* CONFIG_MTD_CMDLINE_PARTS */

	if (!nr_parts) {
		nr_parts = ptn_info.parts_num;
		parts    = ptn_info.parts;
	}

	if (nr_parts <= 0)
		return 0;

	for (ix = 0; ix < nr_parts; ix++)
		pr_debug("partitions[%d] = {.name = %s, .offset = 0x%.8x, "
			 ".size = 0x%08x (%uKiB) }\n",
			 ix, parts[ix].name,
			 (unsigned int)parts[ix].offset,
			 (unsigned int)parts[ix].size,
			 (unsigned int)parts[ix].size/1024);

	host->add_partition = 1;
	ret = add_mtd_partitions(host->mtd, parts, nr_parts);

	if (parts) {
		kfree(parts);
		parts = NULL;
	}

	return ret;
}
/*****************************************************************************/

static int hisfc350_os_driver_probe(struct platform_device * pltdev)
{
	int result = -EIO;
	struct hisfc_host *host;
	struct mtd_info   *mtd;

	host = (struct hisfc_host *)kmalloc(sizeof(struct hisfc_host),
		GFP_KERNEL);
	if (!host)
		return -ENOMEM;
	memset(host, 0, sizeof(struct hisfc_host));

	platform_set_drvdata(pltdev, host);

	host->sysreg = ioremap_nocache(CONFIG_HISFC350_SYSCTRL_ADDRESS,
		HISFC350_SYSCTRL_LENGTH);
	if (!host->sysreg) {
		pr_err("spi system reg ioremap failed.\n");
		goto fail;
	}

	host->regbase = ioremap_nocache(CONFIG_HISFC350_REG_BASE_ADDRESS,
		HISFC350_REG_BASE_LEN);
	if (!host->regbase) {
		pr_err("spi base reg ioremap failed.\n");
		goto fail;
	}

	host->iobase = ioremap_nocache(CONFIG_HISFC350_BUFFER_BASE_ADDRESS,
		HISFC350_BUFFER_BASE_LEN);
	if (!host->iobase) {
		pr_err("spi buffer ioremap failed.\n");
		goto fail;
	}

	host->buffer = dma_alloc_coherent(host->dev, HISFC350_DMA_MAX_SIZE,
		&host->dma_buffer, GFP_KERNEL);
	if (host->buffer == NULL) {
		pr_err("spi alloc dma buffer failed.\n");
		goto fail;
	}

	mutex_init(&host->lock);

	mtd = host->mtd;
	mtd->name = (char *)pltdev->name;
	mtd->type = MTD_NORFLASH;
	mtd->writesize = 1;
	mtd->flags = MTD_CAP_NORFLASH;
	mtd->owner = THIS_MODULE;

	if (hisfc350_probe(host)) {
		result = -ENODEV;
		goto fail;
	}
	mtd->_erase = hisfc350_os_reg_erase;
	mtd->_write = hisfc350_os_dma_write;
	mtd->_read  = hisfc350_os_dma_read;
	mtd->size      = host->chipsize;
	mtd->erasesize = host->erasesize;

	result = hisfc_os_add_paratitions(host);
	if (host->add_partition)
		return result;

	if (!add_mtd_device(host->mtd))
		return 0;
	result = -ENODEV;

fail:
	mutex_destroy(&host->lock);
	if (host->regbase)
		iounmap(host->regbase);
	if (host->iobase)
		iounmap(host->iobase);
	if (host->buffer)
		dma_free_coherent(host->dev, HISFC350_DMA_MAX_SIZE,
			host->buffer, host->dma_buffer);
	if (host->sysreg)
		iounmap(host->sysreg);
	kfree(host);
	platform_set_drvdata(pltdev, NULL);
	return result;
}
/*****************************************************************************/

static int hisfc350_os_driver_remove(struct platform_device * pltdev)
{
	struct hisfc_host *host = platform_get_drvdata(pltdev);

	if (host->add_partition == 1) {
#ifdef CONFIG_MTD_PARTITIONS
		del_mtd_partitions(host->mtd);
#endif /* CONFIG_MTD_PARTITIONS */
	} else
		del_mtd_device(host->mtd);

	mutex_destroy(&host->lock);
	if (host->regbase)
		iounmap(host->regbase);
	if (host->iobase)
		iounmap(host->iobase);
	if (host->buffer)
		dma_free_coherent(host->dev, HISFC350_DMA_MAX_SIZE,
			host->buffer, host->dma_buffer);
	if (host->sysreg)
		iounmap(host->sysreg);

	kfree(host);
	platform_set_drvdata(pltdev, NULL);

	return 0;
}
/*****************************************************************************/

static void hisfc350_os_driver_shutdown(struct platform_device * pltdev)
{
	hisfc350_driver_shutdown(platform_get_drvdata(pltdev));
}
/*****************************************************************************/

static void hisfc350_os_pltdev_release(struct device *dev)
{
}
/*****************************************************************************/

static int __init parse_spi_partitions(const struct tag *tag)
{
	int i;

	if (tag->hdr.size <= 2) {
		pr_warn("tag->hdr.size <= 2\n");
		return 0;
	}

	ptn_info.parts_num = (tag->hdr.size - 2) /
		(sizeof(struct partition_entry)/sizeof(int));
	memcpy(ptn_info.entry, &tag->u,
		ptn_info.parts_num * sizeof(struct partition_entry));

	for (i = 0; i < ptn_info.parts_num; i++) {
		ptn_info.parts[i].name   = ptn_info.entry[i].name;
		ptn_info.parts[i].size   = (ptn_info.entry[i].length);
		ptn_info.parts[i].offset = (ptn_info.entry[i].start);
		ptn_info.parts[i].mask_flags = 0;
	}

	return 0;
}
__tagtable(0x48695370, parse_spi_partitions);
/*****************************************************************************/

static struct platform_driver hisfc350_driver_pltdrv = {
	.probe  = hisfc350_os_driver_probe,
	.remove = hisfc350_os_driver_remove,
	.shutdown = hisfc350_os_driver_shutdown,
#ifdef CONFIG_PM
	.suspend  = hisfc350_os_driver_suspend,
	.resume   = hisfc350_os_driver_resume,
#endif /* CONFIG_PM */

	.driver.name  = "hi_sfc",
	.driver.owner = THIS_MODULE,
	.driver.bus   = &platform_bus_type,
};

static struct resource hisfc350_device_resources[] = {
	[0] = {
		.start = CONFIG_HISFC350_REG_BASE_ADDRESS,
		.end   = CONFIG_HISFC350_REG_BASE_ADDRESS
			+ HISFC350_REG_BASE_LEN - 1,
		.flags = IORESOURCE_MEM,
	},

	[1] = {
		.start = CONFIG_HISFC350_BUFFER_BASE_ADDRESS,
		.end   = CONFIG_HISFC350_BUFFER_BASE_ADDRESS
			+ HISFC350_BUFFER_BASE_LEN - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device hisfc350_device_pltdev = {
	.name           = "hi_sfc",
	.id             = -1,

	.dev.release    = hisfc350_os_pltdev_release,
	.dev.coherent_dma_mask = ~0,
	.num_resources  = ARRAY_SIZE(hisfc350_device_resources),
	.resource       = hisfc350_device_resources,
};
/*****************************************************************************/

static int hisfc350_os_version_check(void)
{
	void __iomem *regbase;
	unsigned long regval;

	regbase = ioremap_nocache(CONFIG_HISFC350_REG_BASE_ADDRESS,
		HISFC350_REG_BASE_LEN);
	if (!regbase) {
		pr_err("spi base reg ioremap failed.\n");
		return -EIO;
	}
	regval = readl(regbase + HISFC350_VERSION);
	iounmap(regbase);
	return (regval == 0x350);
}
/*****************************************************************************/

static int __init hisfc350_os_module_init(void)
{
	int result = 0;

	printk("spiflash: Check Spi Flash Controller V350. ");
	if (!hisfc350_os_version_check()) {
		printk("\n");
		return -ENODEV;
	}
	printk("Found\n");

	result = platform_driver_register(&hisfc350_driver_pltdrv);
	if (result < 0)
		return result;

	result = platform_device_register(&hisfc350_device_pltdev);
	if (result < 0) {
		platform_driver_unregister(&hisfc350_driver_pltdrv);
		return result;
	}

	return result;
}
/*****************************************************************************/

static void __exit hisfc350_os_module_exit(void)
{
	platform_device_unregister(&hisfc350_device_pltdev);
	platform_driver_unregister(&hisfc350_driver_pltdrv);
}
/*****************************************************************************/

module_init(hisfc350_os_module_init);
module_exit(hisfc350_os_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HiC");
MODULE_DESCRIPTION("Hisilicon Spi Flash Controller V350 Device Driver, "
	"Version 1.00");
