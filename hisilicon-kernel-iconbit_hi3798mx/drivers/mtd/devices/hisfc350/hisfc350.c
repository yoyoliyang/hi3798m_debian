/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-02-06
 *
******************************************************************************/

#include "hisfc350_os.h"
#include "hisfc350.h"

extern struct spi_info hisfc350_spi_info_table[];

/*****************************************************************************/
/* Don't change the follow config */
#define HISFC350_SUPPORT_READ (SPI_IF_READ_STD \
	| SPI_IF_READ_FAST \
	| SPI_IF_READ_DUAL \
	| SPI_IF_READ_DUAL_ADDR \
	| SPI_IF_READ_QUAD \
	| SPI_IF_READ_QUAD_ADDR)

#define HISFC350_SUPPORT_WRITE (SPI_IF_WRITE_STD \
	| SPI_IF_WRITE_DUAL \
	| SPI_IF_WRITE_DUAL_ADDR \
	| SPI_IF_WRITE_QUAD \
	| SPI_IF_WRITE_QUAD_ADDR)

#define HISFC350_SUPPORT_MAX_DUMMY        (7)

/* this function only for debug, reg read is slower then dma read */
#undef HISFCV350_SUPPORT_REG_READ

/*****************************************************************************/

#ifdef CONFIG_HISFC350_SHOW_CYCLE_TIMING
static char *hisfc350_get_ifcycle_str(int ifcycle)
{
	static char *ifcycle_str[] = {
		"single",
		"dual",
		"dual-addr",
		"dual-cmd",
		"reserve",
		"quad",
		"quad-addr",
		"quad-cmd",
	};

	return ifcycle_str[(ifcycle & 0x07)];
}
#endif /* CONFIG_HISFC350_SHOW_CYCLE_TIMING */
/*****************************************************************************/

static void hisfc350_set_host_addr_mode(struct hisfc_host *host, int enable)
{
	unsigned int regval;

	regval = hisfc_read(host, HISFC350_GLOBAL_CONFIG);
	if (enable)
		regval |= HISFC350_GLOBAL_CONFIG_ADDR_MODE_4B;
	else
		regval &= ~HISFC350_GLOBAL_CONFIG_ADDR_MODE_4B;

	hisfc_write(host, HISFC350_GLOBAL_CONFIG, regval);
}
/*****************************************************************************/

static void hisfc350_map_iftype_and_clock(struct hisfc_spi *spi)
{
	int ix;
	const int iftype_read[] = {
		SPI_IF_READ_STD,       HISFC350_IFCYCLE_STD,
		SPI_IF_READ_FAST,      HISFC350_IFCYCLE_STD,
		SPI_IF_READ_DUAL,      HISFC350_IFCYCLE_DUAL,
		SPI_IF_READ_DUAL_ADDR, HISFC350_IFCYCLE_DUAL_ADDR,
		SPI_IF_READ_QUAD,      HISFC350_IFCYCLE_QUAD,
		SPI_IF_READ_QUAD_ADDR, HISFC350_IFCYCLE_QUAD_ADDR,
		0,0,
	};
	const int iftype_write[] = {
		SPI_IF_WRITE_STD,       HISFC350_IFCYCLE_STD,
		SPI_IF_WRITE_DUAL,      HISFC350_IFCYCLE_DUAL,
		SPI_IF_WRITE_DUAL_ADDR, HISFC350_IFCYCLE_DUAL_ADDR,
		SPI_IF_WRITE_QUAD,      HISFC350_IFCYCLE_QUAD,
		SPI_IF_WRITE_QUAD_ADDR, HISFC350_IFCYCLE_QUAD_ADDR,
		0,0,
	};

	for (ix = 0; iftype_write[ix]; ix += 2) {
		if (spi->write->iftype == iftype_write[ix]) {
			spi->write->iftype = iftype_write[ix + 1];
			break;
		}
	}
	hisfc350_get_best_clock(&spi->write->clock);

	for (ix = 0; iftype_read[ix]; ix += 2) {
		if (spi->read->iftype == iftype_read[ix]) {
			spi->read->iftype = iftype_read[ix + 1];
			break;
		}
	}
	hisfc350_get_best_clock(&spi->read->clock);

	hisfc350_get_best_clock(&spi->erase->clock);
	spi->erase->iftype = HISFC350_IFCYCLE_STD;
}
/*****************************************************************************/

static void hisfc350_dma_transfer(struct hisfc_host *host,
				  unsigned int spi_start_addr,
				  unsigned char *dma_buffer,
				  unsigned char is_read,
				  unsigned int size,
				  unsigned char chipselect)
{
	hisfc_write(host, HISFC350_BUS_DMA_MEM_SADDR, dma_buffer);

	hisfc_write(host, HISFC350_BUS_DMA_FLASH_SADDR,
		spi_start_addr);

	hisfc_write(host, HISFC350_BUS_DMA_LEN,
		HISFC350_BUS_DMA_LEN_DATA_CNT(size));

	hisfc_write(host, HISFC350_BUS_DMA_AHB_CTRL,
		HISFC350_BUS_DMA_AHB_CTRL_INCR4_EN
		| HISFC350_BUS_DMA_AHB_CTRL_INCR8_EN
		| HISFC350_BUS_DMA_AHB_CTRL_INCR16_EN);

	hisfc_write(host, HISFC350_BUS_DMA_CTRL,
		HISFC350_BUS_DMA_CTRL_RW(is_read)
		| HISFC350_BUS_DMA_CTRL_CS(chipselect)
		| HISFC350_BUS_DMA_CTRL_START);

	HISFC350_DMA_WAIT_CPU_FINISH(host);

}
/*****************************************************************************/

int hisfc350_dma_read(struct hisfc_host *host,
		      unsigned long long prt_size,
		      unsigned int from,
		      unsigned int len,
		      unsigned int *retlen,
		      unsigned char *buf)
{
	int num;
	int result = -EIO;
	unsigned char *ptr = buf;
	struct hisfc_spi *spi = host->spi;

	if ((unsigned long long)(from + len) > prt_size) {
		pr_warn("read area out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		pr_warn("read length is 0.\n");
		return 0;
	}

	if (spi->driver->wait_ready(spi))
		goto fail;
	spi->driver->bus_prepare(spi, READ);

	if (from & HISFC350_DMA_ALIGN_MASK) {
		num = HISFC350_DMA_ALIGN_SIZE - (from & HISFC350_DMA_ALIGN_MASK);
		if (num > len)
			num = len;
		while (from >= spi->chipsize) {
			from -= spi->chipsize;
			spi++;
			if (!spi->name)
				hisfc_pr_bug("write memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->bus_prepare(spi, READ);
		}
		hisfc350_dma_transfer(host, from,
			(unsigned char *)host->dma_buffer, READ,
			num, spi->chipselect);
		memcpy(ptr, host->buffer, num);
		from  += num;
		ptr += num;
		len -= num;
	}

	while (len > 0) {
		while (from >= spi->chipsize) {
			from -= spi->chipsize;
			spi++;
			if (!spi->name)
				hisfc_pr_bug("read memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->bus_prepare(spi, READ);
		}

		num = ((from + len) >= spi->chipsize)
			? (spi->chipsize - from) : len;
		while (num >= HISFC350_DMA_MAX_SIZE) {
			hisfc350_dma_transfer(host, from,
				(unsigned char *)host->dma_buffer, READ,
				HISFC350_DMA_MAX_SIZE, spi->chipselect);
			memcpy(ptr, host->buffer, HISFC350_DMA_MAX_SIZE);
			ptr  += HISFC350_DMA_MAX_SIZE;
			from += HISFC350_DMA_MAX_SIZE;
			len  -= HISFC350_DMA_MAX_SIZE;
			num  -= HISFC350_DMA_MAX_SIZE;
		}

		if (num) {
			hisfc350_dma_transfer(host, from,
				(unsigned char *)host->dma_buffer, READ,
				num, spi->chipselect);
			memcpy(ptr, host->buffer, num);
			from += num;
			ptr  += num;
			len  -= num;
		}
	}
	result = 0;
	*retlen = (unsigned int)(ptr - buf);

fail:
	return result;
}
/*****************************************************************************/

static unsigned char *hisfc350_read_ids(struct hisfc_host *host,
					int chipselect,
					unsigned char *buffer)
{
	int regindex = 0;
	int numread = 8;
	unsigned int *ptr = (unsigned int *)buffer;

	if (numread > HISFC350_REG_BUF_SIZE)
		numread = HISFC350_REG_BUF_SIZE;

	hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_RDID);
	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_SEL_CS(chipselect)
		| HISFC350_CMD_CONFIG_RW_READ
		| HISFC350_CMD_CONFIG_DATA_EN
		| HISFC350_CMD_CONFIG_DATA_CNT(numread)
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	while (numread) {
		*ptr = hisfc_read(host,
			HISFC350_CMD_DATABUF0 + regindex);
		ptr      += 1;
		regindex += 4;
		numread  -= 4;
	}

	return buffer;
}
/*****************************************************************************/

static int hisfc350_reg_erase_one_block(struct hisfc_host *host,
					struct hisfc_spi *spi,
					unsigned int offset)
{
	if (spi->driver->wait_ready(spi))
		return 1;
	spi->driver->write_enable(spi);
	host->set_system_clock(host, spi->erase, TRUE);

	hisfc_write(host, HISFC350_CMD_INS, spi->erase->cmd);

	hisfc_write(host, HISFC350_CMD_ADDR,
		(offset & HISFC350_CMD_ADDR_MASK));

	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_MEM_IF_TYPE(spi->erase->iftype)
		| HISFC350_CMD_CONFIG_DUMMY_CNT(spi->erase->dummy)
		| HISFC350_CMD_CONFIG_ADDR_EN
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	return 0;
}
/*****************************************************************************/

int hisfc350_dma_write(struct hisfc_host *host,
		       unsigned long long prt_size,
		       unsigned int to,
		       unsigned int len,
		       unsigned int *retlen,
		       unsigned char *buf)
{
	int num;
	int result = -EIO;

	unsigned char *ptr = (unsigned char *)buf;
	struct hisfc_spi *spi = host->spi;

	if ((unsigned long long)(to + len) > prt_size) {
		pr_warn("write data out of range.\n");
		return -EINVAL;
	}

	*retlen = 0;
	if (!len) {
		pr_warn("write length is 0.\n");
		return 0;
	}

	if (spi->driver->wait_ready(spi))
		goto fail;

	spi->driver->write_enable(spi);
	spi->driver->bus_prepare(spi, WRITE);

	if (to & HISFC350_DMA_ALIGN_MASK) {
		num = HISFC350_DMA_ALIGN_SIZE - (to & HISFC350_DMA_ALIGN_MASK);
		if (num > len)
			num = len;
		while (to >= spi->chipsize) {
			to -= spi->chipsize;
			spi++;
			if (!spi->name)
				hisfc_pr_bug("write memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->write_enable(spi);
			spi->driver->bus_prepare(spi, WRITE);
		}
		memcpy(host->buffer, ptr, num);
		hisfc350_dma_transfer(host, to,
			(unsigned char *)host->dma_buffer, WRITE,
			num, spi->chipselect);

		to  += num;
		ptr += num;
		len -= num;
	}

	while (len > 0) {
		num = ((len >= HISFC350_DMA_MAX_SIZE)
			? HISFC350_DMA_MAX_SIZE : len);
		while (to >= spi->chipsize) {
			to -= spi->chipsize;
			spi++;
			if (!spi->name)
				hisfc_pr_bug("write memory out of range.\n");
			if (spi->driver->wait_ready(spi))
				goto fail;
			spi->driver->write_enable(spi);
			spi->driver->bus_prepare(spi, WRITE);
		}

		memcpy(host->buffer, ptr, num);
		hisfc350_dma_transfer(host, to,
			(unsigned char *)host->dma_buffer, WRITE,
			num, spi->chipselect);

		to  += num;
		ptr += num;
		len -= num;
	}
	*retlen = (unsigned int)(ptr - buf);
	result = 0;
fail:

	return result;
}
/*****************************************************************************/

int hisfc350_reg_erase(struct hisfc_host *host,
		       unsigned long long prt_size,
		       unsigned long long offset,
		       unsigned long long length,
		       int *state)
{
	struct hisfc_spi *spi = host->spi;

	if (offset + length > prt_size) {
		pr_warn("erase area out of range of mtd.\n");
		return -EINVAL;
	}

	if ((unsigned int)offset & (host->erasesize-1)) {
		pr_warn("erase start address is not alignment.\n");
		return -EINVAL;
	}

	if ((unsigned int)length & (host->erasesize-1)) {
		pr_warn("erase length is not alignment.\n");
		return -EINVAL;
	}

	while (length) {
		if (spi->chipsize <= offset) {
			offset -= spi->chipsize;
			spi++;
			if (!spi->name)
				hisfc_pr_bug("erase memory out of range.\n");
		}
		if (hisfc350_reg_erase_one_block(host, spi, offset)) {
			mutex_unlock(&host->lock);
			return -1;
		}

		offset += spi->erase->size;
		length -= spi->erase->size;
	}

	*state = MTD_ERASE_DONE;

	return 0;
}
/*****************************************************************************/

int hisfc350_map_chipsize(unsigned long long chipsize)
{
	int shift = 0;
	chipsize >>= (19 - 3); /* 19: 512K; 3: Bytes -> bit */

	while (chipsize) {
		chipsize >>= 1;
		shift++;
	}
	return shift;
}
/*****************************************************************************/

static int hisfc350_controller_spi_init(struct hisfc_spi *spi, int offset)
{
	int regval;
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	regval = hisfc_read(host, HISFC350_BUS_FLASH_SIZE);
	regval &= ~(HISFC350_BUS_FLASH_SIZE_CS0_MASK
		<< (spi->chipselect << 3));
	regval |= (hisfc350_map_chipsize(spi->chipsize)
		<< (spi->chipselect << 3));
	hisfc_write(host, HISFC350_BUS_FLASH_SIZE, regval);

	hisfc_write(host,
		(HISFC350_BUS_BASE_ADDR_CS0 + (spi->chipselect << 2)),
		(CONFIG_HISFC350_BUFFER_BASE_ADDRESS + offset));

	return 0;
}
/*****************************************************************************/

static void hisfc350_show_spi(struct hisfc_spi *spi)
{
#define MAX_PRINT_BUFFER          1024
	char tmp[20];
	char *ptr, *str;
	int size, num;

	str = (char *)kmalloc(MAX_PRINT_BUFFER, GFP_KERNEL);
	if (!str) {
		pr_err("Can't malloc memory.\n");
		return;
	}

	ptr = str;
	size = MAX_PRINT_BUFFER;

	num = snprintf(ptr, size, "Spi(cs%d): ", spi->chipselect);
	ptr += num;
	size -= num;

	ultohstr((u64)spi->erasesize, tmp, sizeof(tmp));
	num = snprintf(ptr, size, "Block:%s ", tmp);
	ptr += num;
	size -= num;

	ultohstr(spi->chipsize, tmp, sizeof(tmp));
	num = snprintf(ptr, size, "Chip:%s ", tmp);
	ptr += num;
	size -= num;

	num = snprintf(ptr, size, "Name:\"%s\"\n", spi->name);

	pr_info("%s", str);


#ifdef CONFIG_HISFC350_SHOW_CYCLE_TIMING

	ptr = str;
	size = MAX_PRINT_BUFFER;

	num = snprintf(ptr, size, "Spi(cs%d): ", spi->chipselect);
	ptr += num;
	size -= num;

	num = snprintf(ptr, size, "Spi(cs%d): ", spi->chipselect);
	ptr += num;
	size -= num;

	num = snprintf(ptr, size, "read:%s,%02X,%s ",
		       hisfc350_get_ifcycle_str(spi->read->iftype),
		       spi->read->cmd,
		       hisfc350_get_clock_str(spi->read->clock));
	ptr += num;
	size -= num;

	num = snprintf(ptr, size, "write:%s,%02X,%s ",
		       hisfc350_get_ifcycle_str(spi->write->iftype),
		       spi->write->cmd,
		       hisfc350_get_clock_str(spi->write->clock));
	ptr += num;
	size -= num;

	num = snprintf(ptr, size, "erase:%s,%02X,%s\n",
		       hisfc350_get_ifcycle_str(spi->erase->iftype),
		       spi->erase->cmd,
		       hisfc350_get_clock_str(spi->erase->clock));

	pr_info("%s", str);

#endif /* CONFIG_HISFC350_SHOW_CYCLE_TIMING */

	kfree(str);
#undef MAX_PRINT_BUFFER
}
/*****************************************************************************/

static int hisfc350_spi_probe(struct hisfc_host *host)
{
	unsigned int total = 0;
	unsigned char ids[8];
	struct spi_info *spiinfo;
	struct hisfc_spi *spi = host->spi;
	int chipselect = (CONFIG_HISFC350_CHIP_NUM - 1);

	host->num_chip = 0;
	for (; chipselect >= 0; chipselect--) {

		hisfc350_read_ids(host, chipselect, ids);

		/* can't find spi flash device. */
		if (!(ids[0] | ids[1] | ids[2])
			|| ((ids[0] & ids[1] & ids[2]) == 0xFF))
			continue;

		pr_info("Spi(cs%d) ID: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
			chipselect,
			ids[0], ids[1], ids[2], ids[3], ids[4], ids[5]);

		spiinfo = spi_serach_ids(hisfc350_spi_info_table, ids);

		if (!spiinfo) {
			pr_warn("find unrecognized spi flash.\n");
			continue;
		}

		spi->name = spiinfo->name;
		spi->chipselect = chipselect;
		spi->chipsize   = spiinfo->chipsize;
		spi->erasesize  = spiinfo->erasesize;
		spi->addrcycle  = spiinfo->addrcycle;
		spi->driver     = spiinfo->driver;
		spi->host       = host;

		spi_search_rw(spiinfo, spi->read,
			HISFC350_SUPPORT_READ,
			HISFC350_SUPPORT_MAX_DUMMY, READ);

		spi_search_rw(spiinfo, spi->write,
			HISFC350_SUPPORT_WRITE,
			HISFC350_SUPPORT_MAX_DUMMY, WRITE);

		spi_get_erase(spiinfo, spi->erase);

		hisfc350_map_iftype_and_clock(spi);

		hisfc350_controller_spi_init(spi, total);

		spi->iobase = (char *)host->iobase + total;

		if (spi->addrcycle == 4)
			spi->driver->entry_4addr(spi, TRUE);

		hisfc350_show_spi(spi);

		host->num_chip++;
		total += spi->chipsize;
		spi++;
	}
	return host->num_chip;
}
/*****************************************************************************/

static int mcm(int m, int n)
{
	unsigned int total = m * n;
	unsigned int tt;
	if (m < n) {
		tt = m;
		m = n;
		n = tt;
	}

	while (n) {
		m = (m % n);
		if (m < n) {
			tt = m;
			m = n;
			n = tt;
		}
	}

	return (int)(total / m);
}
/*****************************************************************************/

void hisfc350_probe_spi_size(struct hisfc_host *host)
{
	int ix = 1;
	struct hisfc_spi *spi = host->spi;

	int total     = spi->chipsize;
	int erasesize = spi->erasesize;

	for (++spi; ix < host->num_chip; ix++, spi++) {
		erasesize = mcm(erasesize, spi->erasesize);
		total += spi->chipsize;
	}

	host->chipsize = total;
	host->erasesize = erasesize;
}
/*****************************************************************************/

void hisfc350_driver_shutdown(struct hisfc_host *host)
{
	int ix;
	struct hisfc_spi *spi = host->spi;

	for (ix = 0; ix < host->num_chip; ix++, spi++) {
		spi->driver->wait_ready(spi);
		if (spi->addrcycle == 4)
			spi->driver->entry_4addr(spi, FALSE);
	}
}
/*****************************************************************************/

int hisfc350_probe(struct hisfc_host *host)
{
	host->set_system_clock = hisfc350_set_system_clock;
	host->set_host_addr_mode = hisfc350_set_host_addr_mode;

	hisfc350_set_system_clock(host, NULL, TRUE);
	hisfc_write(host, HISFC350_TIMING,
		HISFC350_TIMING_TCSS(0x6)
		| HISFC350_TIMING_TCSH(0x6)
		| HISFC350_TIMING_TSHSL(0xf));

	if (!hisfc350_spi_probe(host))
		return -1;

	hisfc350_probe_spi_size(host);

	return 0;
}

