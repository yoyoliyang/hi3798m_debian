/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-07-06
 *
******************************************************************************/

#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>

#include "hinfc504_os.h"
#include "hinfc504.h"
#include "hinfc504_dbg.h"

/*****************************************************************************/

static void inline hinfc504_detect_ecc(unsigned char ecc[], int begin, int end,
				       unsigned int reg, unsigned int mask,
				       int shift)
{
	--begin;
	while (++begin < end) {
		ecc[begin] = (reg & mask);
		reg = (reg >> shift);
	}
}
/*****************************************************************************/

static void hinfc504_ecc_2k1b(struct hinfc_host *host, unsigned char ecc[])
{
	hinfc504_detect_ecc(ecc, 0, 8, hinfc_read(host, 0xA0), 3, 2);
}
/*****************************************************************************/

static void hinfc504_ecc_2k4b(struct hinfc_host *host, unsigned char ecc[])
{
	hinfc504_detect_ecc(ecc, 0, 2, hinfc_read(host, 0xA0), 0x3F, 6);
}
/*****************************************************************************/

static void hinfc504_ecc_2k24b(struct hinfc_host *host, unsigned char ecc[])
{
	hinfc504_detect_ecc(ecc, 0, 2, hinfc_read(host, 0xA0), 0x1F, 5);
}
/*****************************************************************************/

static void hinfc504_ecc_4k1b(struct hinfc_host *host, unsigned char ecc[])
{
	int ix, jx;
	for (ix = 0, jx = 4; ix < 16; ix += 8, jx -=4)
		hinfc504_detect_ecc(ecc, ix, ix + 8,
			hinfc_read(host, 0xA0 + jx), 0x03, 2);
}
/*****************************************************************************/

static void hinfc504_ecc_4k4b(struct hinfc_host *host, unsigned char ecc[])
{
	int ix, jx;
	for (ix = 0, jx = 4; ix < 4; ix += 2, jx -=4)
		hinfc504_detect_ecc(ecc, ix, ix + 2,
			hinfc_read(host, 0xA0 + jx), 0x3F, 6);
}
/*****************************************************************************/

static void hinfc504_ecc_4k24b(struct hinfc_host *host, unsigned char ecc[])
{
	int ix, jx;
	for (ix = 0, jx = 4; ix < 4; ix += 2, jx -=4)
		hinfc504_detect_ecc(ecc, ix, ix + 2,
		hinfc_read(host, 0xA0 + jx), 0x1F, 5);
}
/*****************************************************************************/

static void hinfc504_ecc_8k24b(struct hinfc_host *host, unsigned char ecc[])
{
	int ix, jx;
	for (ix = 0, jx = 12; ix < 8; ix += 2, jx -=4)
		hinfc504_detect_ecc(ecc, ix, ix + 2,
			hinfc_read(host, 0xA0 + jx), 0x1F, 5);
}
/*****************************************************************************/

static void hinfc504_ecc_8k40b(struct hinfc_host *host, unsigned char ecc[])
{
	int ix, jx;
	for (ix = 0, jx = 12; ix < 8; ix += 2, jx -=4)
		hinfc504_detect_ecc(ecc, ix, ix + 2,
			hinfc_read(host, 0xA0 + jx), 0x3F, 6);
}
/*****************************************************************************/

static void inline hinfc600_detect_ecc(unsigned char ecc[], int begin,
				       int end, unsigned int reg)
{
	while (begin < end) {
		ecc[begin] = (reg & 0xff);
		reg = (reg >> 8);
		begin++;
	}
}
/*****************************************************************************/

static void hinfc600_ecc_16k(struct hinfc_host *host, unsigned char ecc[])
{
	int ix, jx;
	for (ix = 0, jx = 0; ix < 4; ix ++, jx += 4)
		hinfc600_detect_ecc(ecc, jx, jx + 4,
				    hinfc_read(host, 0xA0 + jx));
}
/*****************************************************************************/

static void hinfc600_ecc_8k(struct hinfc_host *host, unsigned char ecc[])
{
	int ix, jx;
	for (ix = 0, jx = 0; ix < 2; ix ++, jx += 4)
		hinfc600_detect_ecc(ecc, jx, jx + 4,
				    hinfc_read(host, 0xA0 + jx));
}
/*****************************************************************************/

static void hinfc600_ecc_4k(struct hinfc_host *host, unsigned char ecc[])
{
	hinfc600_detect_ecc(ecc, 0, 4, hinfc_read(host, 0xA0));
}
/*****************************************************************************/

static void hinfc600_ecc_2k(struct hinfc_host *host, unsigned char ecc[])
{
	hinfc600_detect_ecc(ecc, 0, 2, hinfc_read(host, 0xA0));
}
/*****************************************************************************/

static struct hinfc504_ecc_inf_t hinfc504_ecc_inf[] = {

	{8192, NAND_ECC_40BIT, 8, hinfc504_ecc_8k40b},
	{8192, NAND_ECC_24BIT, 8, hinfc504_ecc_8k24b},

	{4096, NAND_ECC_24BIT, 4, hinfc504_ecc_4k24b},
	{4096, NAND_ECC_4BIT_512,  4, hinfc504_ecc_4k4b},
	{4096, NAND_ECC_1BIT_512, 16, hinfc504_ecc_4k1b},

	{2048, NAND_ECC_24BIT, 2, hinfc504_ecc_2k24b},
	{2048, NAND_ECC_4BIT_512,  2, hinfc504_ecc_2k4b},
	{2048, NAND_ECC_1BIT_512,  8, hinfc504_ecc_2k1b},
	{0, 0, 0},
};
/*****************************************************************************/

static struct hinfc504_ecc_inf_t hinfc600_ecc_inf[] = {

	{16384, NAND_ECC_40BIT, 16, hinfc600_ecc_16k},

	{8192, NAND_ECC_40BIT,  8, hinfc600_ecc_8k},
	{8192, NAND_ECC_24BIT,  8, hinfc600_ecc_8k},

	{4096, NAND_ECC_24BIT,  4, hinfc600_ecc_4k},
	{4096, NAND_ECC_4BIT_512,   4, hinfc600_ecc_4k},
	{4096, NAND_ECC_1BIT_512,   4, hinfc600_ecc_4k},

	{2048, NAND_ECC_24BIT,  2, hinfc600_ecc_2k},
	{2048, NAND_ECC_4BIT_512,   2, hinfc600_ecc_2k},
	{2048, NAND_ECC_1BIT_512,   2, hinfc600_ecc_2k},
	{0, 0, 0},
};
/*****************************************************************************/

struct hinfc504_ecc_inf_t *hinfc504_get_ecc_inf(struct hinfc_host *host,
						int pagesize, int ecctype)
{
	struct hinfc504_ecc_inf_t *inf, *ecc_inf;

	if (HINFC_VER_600 == host->version)
		ecc_inf = hinfc600_ecc_inf;
	else
		ecc_inf = hinfc504_ecc_inf;

	for (inf = ecc_inf; inf->pagesize; inf++)
		if (inf->pagesize == pagesize && inf->ecctype == ecctype)
			return inf;

	return NULL;
}
