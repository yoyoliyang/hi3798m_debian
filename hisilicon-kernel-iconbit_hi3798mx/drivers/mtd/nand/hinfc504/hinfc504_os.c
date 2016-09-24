/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-02-07
 *
******************************************************************************/

#include "hinfc504_os.h"
#include "hinfc504.h"

/*****************************************************************************/
#define MAX_MTD_PARTITIONS         (32)

struct partition_entry
{
	char name[16];
	unsigned long long start;
	unsigned long long length;
	unsigned int flags;
};

struct partition_info
{
	int parts_num;
	struct partition_entry entry[MAX_MTD_PARTITIONS];
	struct mtd_partition parts[MAX_MTD_PARTITIONS];
};

#ifdef CONFIG_MTD_PART_CHANGE
extern int register_mtd_partdev(struct mtd_info *mtd);
extern int unregister_mtd_partdev(struct mtd_info *mtd);
#else
static int register_mtd_partdev(struct mtd_info *mtd){ return 0; };
static int unregister_mtd_partdev(struct mtd_info *mtd){return 0;};
#endif

static const char *part_probes_type[] = { "cmdlinepart", NULL, };

static struct partition_info ptn_info = {0};

static int __init parse_nand_partitions(const struct tag *tag)
{
	int i;

	if (tag->hdr.size <= 2) {
		hinfc_pr_bug("tag->hdr.size <= 2\n");
		return 0;
	}
	ptn_info.parts_num = (tag->hdr.size - 2)
		/ (sizeof(struct partition_entry)/sizeof(int));
	memcpy(ptn_info.entry,
		&tag->u,
		ptn_info.parts_num * sizeof(struct partition_entry));

	for (i = 0; i < ptn_info.parts_num; i++) {
		ptn_info.parts[i].name   = ptn_info.entry[i].name;
		ptn_info.parts[i].size   = (ptn_info.entry[i].length);
		ptn_info.parts[i].offset = (ptn_info.entry[i].start);
		ptn_info.parts[i].mask_flags = 0;
		ptn_info.parts[i].ecclayout  = 0;
	}

	return 0;
}

/* turn to ascii is "HiNp" */
__tagtable(0x48694E70, parse_nand_partitions);
/*****************************************************************************/
static unsigned int  nand_otp_len = 0;
static unsigned char nand_otp[128] = {0};

/* Get NAND parameter table. */
static int __init parse_nand_param(const struct tag *tag)
{
	if (tag->hdr.size <= 2)
		return 0;

	nand_otp_len = ((tag->hdr.size << 2) - sizeof(struct tag_header));

	if (nand_otp_len > sizeof(nand_otp)) {
		hinfc_pr_bug("tag->hdr.size <= 2\n");
		return 0;
	}
	memcpy(nand_otp, &tag->u, nand_otp_len);
	return 0;
}
/* 0x48694E77 equal to fastoot ATAG_NAND_PARAM */
__tagtable(0x48694E77, parse_nand_param);
/*****************************************************************************/

static int hinfc504_os_enable(struct hinfc_host *host, int enable)
{
	if (enable == ENABLE)
		clk_enable(host->clk);
	else
		clk_disable(host->clk);
	return 0;
}
/*****************************************************************************/

static int hinfc504_os_probe(struct platform_device * pltdev)
{
	int size;
	int result = 0;
	struct hinfc_host *host;
	struct nand_chip *chip;
	struct mtd_info *mtd;
	struct resource *res;

	size = sizeof(struct hinfc_host) + sizeof(struct nand_chip)
		+ sizeof(struct mtd_info);
	host = kmalloc(size, GFP_KERNEL);
	if (!host) {
		hinfc_pr_bug("failed to allocate device structure.\n");
		return -ENOMEM;
	}
	memset((char *)host, 0, size);
	platform_set_drvdata(pltdev, host);

	host->dev  = &pltdev->dev;
	host->chip = chip = (struct nand_chip *)&host[1];
	host->mtd  = mtd  = (struct mtd_info *)&chip[1];

	res = platform_get_resource_byname(pltdev, IORESOURCE_MEM, "base");
	if (!res) {
		hinfc_pr_bug("Can't get resource.\n");
		return -EIO;
	}

	host->iobase = ioremap(res->start, res->end - res->start + 1);
	if (!host->iobase) {
		hinfc_pr_bug("ioremap failed\n");
		kfree(host);
		return -EIO;
	}

	mtd->priv  = chip;
	mtd->owner = THIS_MODULE;
	mtd->name  = (char*)(pltdev->name);

	res = platform_get_resource_byname(pltdev, IORESOURCE_MEM, "buffer");
	if (!res) {
		hinfc_pr_bug("Can't get resource.\n");
		return -EIO;
	}

	chip->IO_ADDR_R = chip->IO_ADDR_W = ioremap_nocache(res->start,
		res->end - res->start + 1);
	if (!chip->IO_ADDR_R) {
		hinfc_pr_bug("ioremap failed\n");
		return -EIO;
	}

	host->buffer = dma_alloc_coherent(host->dev,
		(NAND_MAX_PAGESIZE + NAND_MAX_OOBSIZE),
		&host->dma_buffer, GFP_KERNEL);
	if (!host->buffer) {
		PR_BUG("Can't malloc memory for NAND driver.");
		return -EIO;
	}

	chip->priv        = host;
	host->chip        = chip;
	chip->cmd_ctrl    = hinfc504_cmd_ctrl;
	chip->dev_ready   = hinfc504_dev_ready;
	chip->select_chip = hinfc504_select_chip;
	chip->read_byte   = hinfc504_read_byte;
	chip->read_word   = hinfc504_read_word;
	chip->write_buf   = hinfc504_write_buf;
	chip->read_buf    = hinfc504_read_buf;

	chip->chip_delay = HINFC504_CHIP_DELAY;
	chip->options    = NAND_NEED_READRDY | NAND_SKIP_BBTSCAN;
	chip->ecc.layout = NULL;
	chip->ecc.mode   = NAND_ECC_NONE;

	host->clk = clk_get_sys("hinfc504", NULL);
	if (IS_ERR(host->clk)) {
		hinfc_pr_bug("hinfc610 clock not found.\n");
		return -EIO;
	}
	host->enable = hinfc504_os_enable;

	if (hinfc504_nand_init(host, chip)) {
		hinfc_pr_bug("failed to allocate device buffer.\n");
		return -EIO;
	}

	if (nand_otp_len) {
		hinfc_pr_msg("Copy Nand read retry parameter from boot,"
		       " parameter length %d.\n", nand_otp_len);
		memcpy(host->rr_data, nand_otp, nand_otp_len);
	}

	if (nand_scan(mtd, CONFIG_HINFC504_MAX_CHIP)) {
		result = -ENXIO;
		goto fail;
	}

	register_mtd_partdev(host->mtd);

	if (!mtd_device_parse_register(mtd, part_probes_type,
		NULL, ptn_info.parts, ptn_info.parts_num))
		return 0;

	unregister_mtd_partdev(host->mtd);

	result = -ENODEV;
	nand_release(mtd);

fail:
	if (host->buffer) {
		dma_free_coherent(host->dev,
			(NAND_MAX_PAGESIZE + NAND_MAX_OOBSIZE),
			host->buffer,
			host->dma_buffer);
		host->buffer = NULL;
	}
	iounmap(chip->IO_ADDR_W);
	iounmap(host->iobase);
	kfree(host);
	platform_set_drvdata(pltdev, NULL);

	return result;
}
/*****************************************************************************/

static int hinfc504_os_remove(struct platform_device *pltdev)
{
	struct hinfc_host *host = platform_get_drvdata(pltdev);

	host->enable(host, DISABLE);

	unregister_mtd_partdev(host->mtd);

	nand_release(host->mtd);

	dma_free_coherent(host->dev,
		(NAND_MAX_PAGESIZE + NAND_MAX_OOBSIZE),
		host->buffer,
		host->dma_buffer);
	iounmap(host->chip->IO_ADDR_W);
	iounmap(host->iobase);
	kfree(host);
	platform_set_drvdata(pltdev, NULL);

	return 0;
}
/*****************************************************************************/

static void hinfc504_pltdev_release(struct device *dev)
{
}
/*****************************************************************************/
#ifdef CONFIG_PM
static int hinfc504_os_suspend(struct platform_device *pltdev,
			       pm_message_t state)
{
	struct hinfc_host *host = platform_get_drvdata(pltdev);

	while ((hinfc_read(host, HINFC504_STATUS ) & 0x1) == 0x0)
		;

	while ((hinfc_read(host, HINFC504_DMA_CTRL))
		& HINFC504_DMA_CTRL_DMA_START)
		_cond_resched();

	host->enable(host, DISABLE);

	return 0;
}
/*****************************************************************************/

static int hinfc504_os_resume(struct platform_device *pltdev)
{
	int cs;
	struct hinfc_host *host = platform_get_drvdata(pltdev);
	struct nand_chip *chip = host->chip;

	host->enable(host, ENABLE);
	for (cs = 0; cs < chip->numchips; cs++)
		host->send_cmd_reset(host, cs);
	hinfc_write(host,
		SET_HINFC504_PWIDTH(CONFIG_HINFC504_W_LATCH,
			CONFIG_HINFC504_R_LATCH, CONFIG_HINFC504_RW_LATCH),
		HINFC504_PWIDTH);

	return 0;
}
#endif /* CONFIG_PM */
/*****************************************************************************/
static struct platform_driver hinfc504_os_pltdrv =
{
	.driver.name   = "hinand",
	.probe  = hinfc504_os_probe,
	.remove = hinfc504_os_remove,
#ifdef CONFIG_PM
	.suspend  = hinfc504_os_suspend,
	.resume   = hinfc504_os_resume,
#endif /* CONFIG_PM */
};
/*****************************************************************************/

static struct platform_device hinfc504_os_pltdev =
{
	.name           = "hinand",
	.id             = -1,

	.dev.platform_data     = NULL,
	.dev.dma_mask          = (u64*)~0,
	.dev.coherent_dma_mask = (u64) ~0,
	.dev.release           = hinfc504_pltdev_release,
};
/*****************************************************************************/

static int __init hinfc504_os_module_init(void)
{
	int result = 0;

	result = find_cpu_resource("hinfc504", &hinfc504_os_pltdev.resource,
		&hinfc504_os_pltdev.num_resources);

	if (result)
		return result;

	pr_notice("Found Nand Flash Controller V504.\n");

	result = platform_driver_register(&hinfc504_os_pltdrv);
	if (result < 0)
		return result;

	result = platform_device_register(&hinfc504_os_pltdev);
	if (result < 0) {
		platform_driver_unregister(&hinfc504_os_pltdrv);
		return result;
	}

	return result;
}
/*****************************************************************************/

static void __exit hinfc504_os_module_exit (void)
{
	platform_driver_unregister(&hinfc504_os_pltdrv);
	platform_device_unregister(&hinfc504_os_pltdev);
}
/*****************************************************************************/

module_init(hinfc504_os_module_init);
module_exit(hinfc504_os_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Czyong");
MODULE_DESCRIPTION("Hisilicon Nand Flash Controller V504 Device Driver,"
	" Version 1.30");

/*****************************************************************************/
