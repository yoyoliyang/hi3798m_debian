/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-02-06
 *
******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sizes.h>

#include "spi_ids.h"

#define SPI_DRV_VERSION       "1.30"

/*****************************************************************************/

struct spi_info *spi_serach_ids(struct spi_info * spi_info_table,
				unsigned char ids[8])
{
	struct spi_info *info;
	struct spi_info *fit_info = NULL;

	for (info = spi_info_table; info->name; info++) {
		if (memcmp(info->id, ids, info->id_len))
			continue;

		if (fit_info == NULL
			|| fit_info->id_len < info->id_len)
			fit_info = info;
	}
	return fit_info;
}
/*****************************************************************************/

void spi_search_rw(struct spi_info *spiinfo, struct spi_operation *spiop_rw,
		   unsigned int iftype, unsigned int max_dummy, int is_read)
{
	int ix = 0;
	struct spi_operation **spiop, **fitspiop;

	for (fitspiop = spiop = (is_read ? spiinfo->read : spiinfo->write);
	     (*spiop) && ix < MAX_SPI_OP; spiop++, ix++) {
		if (((*spiop)->iftype & iftype)
			&& ((*spiop)->dummy <= max_dummy)
			&& (*fitspiop)->iftype < (*spiop)->iftype)
			fitspiop = spiop;
	}
	memcpy(spiop_rw, (*fitspiop), sizeof(struct spi_operation));
}
/*****************************************************************************/

void spi_get_erase(struct spi_info *spiinfo, struct spi_operation *spiop_erase)
{
	int ix;

	spiop_erase->size = 0;
	for (ix = 0; ix < MAX_SPI_OP; ix++) {
		if (spiinfo->erase[ix] == NULL)
			break;
		if (spiinfo->erasesize == spiinfo->erase[ix]->size) {
			memcpy(spiop_erase, spiinfo->erase[ix],
				sizeof(struct spi_operation));
			break;
		}
	}
	if (!spiop_erase->size) {
		pr_err("SPIFlash erasesize %d error!\n",
		       spiop_erase->size);
		BUG();
	}
}

/*****************************************************************************/

void spi_get_erase_sfcv300(struct spi_info *spiinfo,
			   struct spi_operation *spiop_erase,
			   unsigned int *erasesize)
{
	int ix;

	(*erasesize) = spiinfo->erasesize;

	for (ix = 0; ix < MAX_SPI_OP; ix++) {
		if (spiinfo->erase[ix] == NULL)
			break;

		memcpy(&spiop_erase[ix], spiinfo->erase[ix],
			sizeof(struct spi_operation));

		switch (spiop_erase[ix].size) {
		case SPI_IF_ERASE_SECTOR:
			spiop_erase[ix].size = spiinfo->erasesize;
			break;
		case SPI_IF_ERASE_CHIP:
			spiop_erase[ix].size = spiinfo->chipsize;
			break;
		}

		if (spiop_erase[ix].size < (*erasesize))
			(*erasesize) = spiop_erase[ix].size;
	}
}
