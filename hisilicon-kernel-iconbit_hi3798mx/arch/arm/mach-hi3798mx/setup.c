/******************************************************************************
 *    COPYRIGHT (C) 2013 Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-12-20
 *
******************************************************************************/

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/compiler.h>
#include <asm/setup.h>

static char boot_sdkversion[64] = "0.0.0.0";

/*****************************************************************************/

static int __init parse_tag_sdkversion(const struct tag *tag)
{
	memcpy(boot_sdkversion, &tag->u, sizeof(boot_sdkversion));
	return 0;
}
__tagtable(0x726d6d75, parse_tag_sdkversion);
/*****************************************************************************/

const char * get_sdkversion(void)
{
	return boot_sdkversion;
}
EXPORT_SYMBOL(get_sdkversion);
/*****************************************************************************/

unsigned long get_dram_size(void)
{
	int i;
	unsigned long dram_size = 0;

	for_each_bank(i, &meminfo) {
		struct membank *bank = &meminfo.bank[i];
		dram_size += bank->size;
	}
	return dram_size;
}
EXPORT_SYMBOL(get_dram_size);
