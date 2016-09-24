/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-02-06
 *
******************************************************************************/

#include "hisfc350_os.h"
#include "hisfc350.h"

static int spi_n25q256a_entry_4addr(struct hisfc_spi *spi, int enable)
{
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	if (spi->addrcycle != 4)
		return 0;

	if (enable) {
		spi->driver->write_enable(spi);
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_EN4B);
	} else {
		spi->driver->write_enable(spi);
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_EX4B);
	}

	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	host->set_host_addr_mode(host, enable);

	return 0;
}
