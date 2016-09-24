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

static int spi_general_wait_ready(struct hisfc_spi *spi)
{
	unsigned long regval;
	unsigned long deadline = jiffies + HISFC350_MAX_READY_WAIT_JIFFIES;
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	do {
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_RDSR);
		hisfc_write(host, HISFC350_CMD_CONFIG,
			HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
			| HISFC350_CMD_CONFIG_DATA_CNT(1)
			| HISFC350_CMD_CONFIG_DATA_EN
			| HISFC350_CMD_CONFIG_RW_READ
			| HISFC350_CMD_CONFIG_START);

		HISFC350_CMD_WAIT_CPU_FINISH(host);
		regval = hisfc_read(host, HISFC350_CMD_DATABUF0);
		if (!(regval & SPI_CMD_SR_WIP))
			return 0;

		cond_resched();

	} while (!time_after_eq(jiffies, deadline));

	pr_err("Wait spi flash ready timeout.\n");

	return 1;
}
/*****************************************************************************/

static int spi_general_write_enable(struct hisfc_spi *spi)
{
	unsigned int regval = 0;
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_WREN);

	regval = HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_START;
	hisfc_write(host, HISFC350_CMD_CONFIG, regval);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	return 0;
}
/*****************************************************************************/

static int spi_general_entry_4addr(struct hisfc_spi *spi, int enable)
{
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	if (spi->addrcycle != 4)
		return 0;

	if (enable)
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_EN4B);
	else
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_EX4B);

	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	host->set_host_addr_mode(host, enable);

	return 0;
}
/*****************************************************************************/

static int spi_general_bus_prepare(struct hisfc_spi *spi, int op)
{
	unsigned int regval = 0;
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	regval |= HISFC350_BUS_CONFIG1_WRITE_INS(spi->write->cmd);
	regval |= HISFC350_BUS_CONFIG1_WRITE_DUMMY_CNT(spi->write->dummy);
	regval |= HISFC350_BUS_CONFIG1_WRITE_IF_TYPE(spi->write->iftype);

	regval |= HISFC350_BUS_CONFIG1_READ_PREF_CNT(0);
	regval |= HISFC350_BUS_CONFIG1_READ_INS(spi->read->cmd);
	regval |= HISFC350_BUS_CONFIG1_READ_DUMMY_CNT(spi->read->dummy);
	regval |= HISFC350_BUS_CONFIG1_READ_IF_TYPE(spi->read->iftype);

	hisfc_write(host, HISFC350_BUS_CONFIG1, regval);

	if (op == READ)
		host->set_system_clock(host, spi->read, TRUE);
	else if (op == WRITE)
		host->set_system_clock(host, spi->write, TRUE);

	return 0;
}
