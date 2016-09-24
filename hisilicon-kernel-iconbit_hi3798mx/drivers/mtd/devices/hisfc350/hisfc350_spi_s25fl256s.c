/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-02-06
 *
******************************************************************************/

#include "hisfc350_os.h"
#include "hisfc350.h"
/*****************************************************************************/

#define SPI_BRWR        0x17
#define SPI_EN4B        0x80
#define SPI_EX4B        0x00

static int spi_s25fl256s_entry_4addr(struct hisfc_spi *spi, int enable)
{
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	if (spi->addrcycle != 4)
		return 0;

	if (enable) {
		hisfc_write(host, HISFC350_CMD_INS, SPI_BRWR);
		hisfc_write(host, HISFC350_CMD_DATABUF0, SPI_EN4B);
	} else {
		hisfc_write(host, HISFC350_CMD_INS, SPI_BRWR);
		hisfc_write(host, HISFC350_CMD_DATABUF0, SPI_EX4B);
	}

	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_DATA_CNT(1)
		| HISFC350_CMD_CONFIG_DATA_EN
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	host->set_host_addr_mode(host, enable);

	return 0;
}

