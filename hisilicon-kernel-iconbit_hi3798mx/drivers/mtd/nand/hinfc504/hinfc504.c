/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-02-07
 *
******************************************************************************/

#include "hinfc504_os.h"
#include "hinfc504.h"
#include "hinfc504_dbg_inf.h"
#include "hinfc504_gen.h"
/*****************************************************************************/

extern struct read_retry_t hinfc504_hynix_bg_cdie_read_retry;
extern struct read_retry_t hinfc504_hynix_bg_bdie_read_retry;
extern struct read_retry_t hinfc504_hynix_cg_adie_read_retry;
extern struct read_retry_t hinfc504_micron_read_retry;
extern struct read_retry_t hinfc504_toshiba_24nm_read_retry;
extern struct read_retry_t hinfc504_samsung_read_retry;

static struct read_retry_t *read_retry_list[] = {
	&hinfc504_hynix_bg_bdie_read_retry,
	&hinfc504_hynix_bg_cdie_read_retry,
	&hinfc504_hynix_cg_adie_read_retry,
	&hinfc504_micron_read_retry,
	&hinfc504_toshiba_24nm_read_retry,
	&hinfc504_samsung_read_retry,
	NULL,
};
/*****************************************************************************/

static void hinfc504_dma_transfer(struct hinfc_host *host, int todev)
{
	unsigned long reg_val;
	unsigned int dma_addr = (unsigned int)host->dma_buffer;

	hinfc_write(host, dma_addr, HINFC504_DMA_ADDR_DATA);

	dma_addr += HINFC504_DMA_ADDR_OFFSET;
	hinfc_write(host, dma_addr, HINFC504_DMA_ADDR_DATA1);

	dma_addr += HINFC504_DMA_ADDR_OFFSET;
	hinfc_write(host, dma_addr, HINFC504_DMA_ADDR_DATA2);

	dma_addr += HINFC504_DMA_ADDR_OFFSET;
	hinfc_write(host, dma_addr, HINFC504_DMA_ADDR_DATA3);

	hinfc_write(host, host->dma_oob, HINFC504_DMA_ADDR_OOB);

	if (host->ecctype == NAND_ECC_NONE) {
		hinfc_write(host,
			((host->oobsize & HINFC504_DMA_LEN_OOB_MASK)
				<< HINFC504_DMA_LEN_OOB_SHIFT),
			HINFC504_DMA_LEN);

		hinfc_write(host,
			HINFC504_DMA_PARA_DATA_RW_EN
			| HINFC504_DMA_PARA_OOB_RW_EN
			| ((host->n24bit_ext_len
				& HINFC504_DMA_PARA_EXT_LEN_MASK)
				<< HINFC504_DMA_PARA_EXT_LEN_SHIFT),
			HINFC504_DMA_PARA);
	} else
		hinfc_write(host,
			HINFC504_DMA_PARA_DATA_RW_EN
			| HINFC504_DMA_PARA_OOB_RW_EN
			| HINFC504_DMA_PARA_DATA_EDC_EN
			| HINFC504_DMA_PARA_OOB_EDC_EN
			| HINFC504_DMA_PARA_DATA_ECC_EN
			| HINFC504_DMA_PARA_OOB_ECC_EN
			| ((host->n24bit_ext_len
				& HINFC504_DMA_PARA_EXT_LEN_MASK)
				<< HINFC504_DMA_PARA_EXT_LEN_SHIFT),
			HINFC504_DMA_PARA);

	reg_val = (HINFC504_DMA_CTRL_DMA_START
		| HINFC504_DMA_CTRL_BURST4_EN
		| HINFC504_DMA_CTRL_BURST8_EN
		| HINFC504_DMA_CTRL_BURST16_EN
		| HINFC504_DMA_CTRL_DATA_AREA_EN
		| HINFC504_DMA_CTRL_OOB_AREA_EN
		| ((host->addr_cycle == 4 ? 1 : 0)
			<< HINFC504_DMA_CTRL_ADDR_NUM_SHIFT)
		| ((host->chipselect & HINFC504_DMA_CTRL_CS_MASK)
			<< HINFC504_DMA_CTRL_CS_SHIFT));

	if (todev)
		reg_val |= HINFC504_DMA_CTRL_WE;

	hinfc_write(host, reg_val, HINFC504_DMA_CTRL);

	do {
		unsigned int timeout = 0xF0000000;
		while ((hinfc_read(host, HINFC504_DMA_CTRL))
			& HINFC504_DMA_CTRL_DMA_START && timeout) {
			_cond_resched();
			timeout--;
		}
		if (!timeout)
			hinfc_pr_bug("Wait DMA finish timeout.\n");
	} while (0);
}
/*****************************************************************************/

void hinfc504_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	int is_cache_invalid = 1;
	struct nand_chip *chip = mtd->priv;
	struct hinfc_host *host = chip->priv;

	if (ctrl & NAND_ALE) {
		unsigned int addr_value = 0;
		unsigned int addr_offset = 0;

		if (ctrl & NAND_CTRL_CHANGE) {
			host->addr_cycle = 0x0;
			host->addr_value[0] = 0x0;
			host->addr_value[1] = 0x0;
		}
		addr_offset =  host->addr_cycle << 3;

		if (host->addr_cycle >= HINFC504_ADDR_CYCLE_MASK) {
			addr_offset =
				(host->addr_cycle - HINFC504_ADDR_CYCLE_MASK)
					<< 3;
			addr_value = 1;
		}

		host->addr_value[addr_value] |=
			((dat & 0xff) << addr_offset);

		host->addr_cycle ++;
	}

	if ((ctrl & NAND_CLE) && (ctrl & NAND_CTRL_CHANGE)) {
		host->command = dat & 0xff;
		switch (host->command) {
		case NAND_CMD_PAGEPROG:
			host->send_cmd_pageprog(host);
			hinfc504_dbg_write(host);
			break;

		case NAND_CMD_READSTART:
			is_cache_invalid = 0;
			host->send_cmd_readstart(host);
			hinfc504_dbg_read(host);
			host->ecc_status = 0;

			break;

		case NAND_CMD_ERASE2:
			host->send_cmd_erase(host);
			hinfc504_dbg_erase(host);

			break;

		case NAND_CMD_READID:
			memset((unsigned char *)(chip->IO_ADDR_R), 0, 0x10);
			host->send_cmd_readid(host);
			break;

		case NAND_CMD_STATUS:
			host->send_cmd_status(host);
			break;

		case NAND_CMD_SEQIN:
		case NAND_CMD_ERASE1:
		case NAND_CMD_READ0:
			break;
		case NAND_CMD_RESET:
			host->send_cmd_reset(host, host->chipselect);
			break;

		default :
			break;
		}
	}

	if ((dat == NAND_CMD_NONE) && host->addr_cycle) {
		if (host->command == NAND_CMD_SEQIN
			|| host->command == NAND_CMD_READ0
			|| host->command == NAND_CMD_READID) {
			host->offset = 0x0;
			host->column = (host->addr_value[0] & 0xffff);
		}
	}

	if (is_cache_invalid) {
		host->cache_addr_value[0] = ~0;
		host->cache_addr_value[1] = ~0;
	}
}
/*****************************************************************************/

static int hinfc504_send_cmd_pageprog(struct hinfc_host *host)
{
	if (*host->bbm != 0xFF && *host->bbm != 0x00)
		pr_warn("Attempt to write an invalid bbm. page: 0x%08x, mark: 0x%02x, current process(pid): %s(%d).\n",
			GET_PAGE_INDEX(host), *host->bbm,
			current->comm, current->pid);

	host->enable_ecc_randomizer(host, ENABLE, ENABLE);

	hinfc_write(host,
		host->addr_value[0] & 0xffff0000,
		HINFC504_ADDRL);
	hinfc_write(host,
		host->addr_value[1], HINFC504_ADDRH);
	hinfc_write(host,
		NAND_CMD_PAGEPROG << 8 | NAND_CMD_SEQIN,
		HINFC504_CMD);

	if (IS_NAND_RANDOM(host))
		*host->epm = host->epmvalue;

	hinfc504_dma_transfer(host, 1);

	return 0;
}
/*****************************************************************************/
#define NAND_BAD_BLOCK              1
#define NAND_EMPTY_PAGE             2
#define NAND_VALID_DATA             3

static int hinfc504_get_data_status(struct hinfc_host *host)
{
	/* this is block start address */
	if (!((host->addr_value[0] >> 16) & host->block_page_mask)) {

		/* it is a bad block */
		if (*host->bbm == 0)
			return NAND_BAD_BLOCK;
		/*
		 * if there are more than 2 bits flipping, it is
		 * maybe a bad block
		 */
		if (GET_UC_ECC(host) && *host->bbm != 0xFF &&
		    get_bits(*host->bbm) <= 6)
			return NAND_BAD_BLOCK;
	}

	/* it is an empty page */
	if (*host->epm != host->epmvalue && IS_NAND_RANDOM(host))
		return NAND_EMPTY_PAGE;

	return NAND_VALID_DATA;
}
/*****************************************************************************/

static int hinfc504_do_read_retry(struct hinfc_host *host)
{
	int ix;

	for (ix = 1; GET_UC_ECC(host) && ix < host->read_retry->count; ix++) {

		hinfc_write(host, HINFC504_INTCLR_UE | HINFC504_INTCLR_CE,
			HINFC504_INTCLR);

		host->enable_ecc_randomizer(host, DISABLE, DISABLE);
		host->read_retry->set_rr_param(host, ix);

		/* enable ecc and randomizer */
		host->enable_ecc_randomizer(host, ENABLE, ENABLE);

		hinfc_write(host, HINFC504_INTCLR_UE | HINFC504_INTCLR_CE,
			HINFC504_INTCLR);
		// TODO: need update
		hinfc_write(host, host->NFC_CON, HINFC504_CON);
		hinfc_write(host, host->addr_value[0] & 0xffff0000,
			HINFC504_ADDRL);
		hinfc_write(host, host->addr_value[1], HINFC504_ADDRH);
		hinfc_write(host,
			HINFC_CMD_SEQ(NAND_CMD_READ0, NAND_CMD_READSTART),
			HINFC504_CMD);

		hinfc_write(host, 0, HINFC504_LOG_READ_ADDR);
		hinfc_write(host, (host->pagesize + host->oobsize),
			HINFC504_LOG_READ_LEN);

		hinfc504_dma_transfer(host, 0);

		SET_UC_ECC(host,
			(hinfc_read(host, HINFC504_INTS) & HINFC504_INTS_UE));
	}

	hinfc504_dbg_read_retry(host, ix);

	host->enable_ecc_randomizer(host, DISABLE, DISABLE);

	host->read_retry->set_rr_param(host, 0);

	return 0;
}
/*****************************************************************************/

static int hinfc504_send_cmd_readstart(struct hinfc_host *host)
{
	if ((host->addr_value[0] == host->cache_addr_value[0])
	    && (host->addr_value[1] == host->cache_addr_value[1]))
		return 0;

	host->enable_ecc_randomizer(host, ENABLE, ENABLE);

	hinfc_write(host, HINFC504_INTCLR_UE | HINFC504_INTCLR_CE,
		HINFC504_INTCLR);
	hinfc_write(host, host->NFC_CON, HINFC504_CON);
	hinfc_write(host, host->addr_value[0] & 0xffff0000, HINFC504_ADDRL);
	hinfc_write(host, host->addr_value[1], HINFC504_ADDRH);
	hinfc_write(host, NAND_CMD_READSTART << 8 | NAND_CMD_READ0,
		HINFC504_CMD);

	hinfc_write(host, 0, HINFC504_LOG_READ_ADDR);
	hinfc_write(host, (host->pagesize + host->oobsize),
		HINFC504_LOG_READ_LEN);

	hinfc504_dma_transfer(host, 0);

	SET_UC_ECC(host,
		(hinfc_read(host, HINFC504_INTS) & HINFC504_INTS_UE));

	if (host->read_retry || IS_NAND_RANDOM(host)) {
		int status = hinfc504_get_data_status(host);

		if (status == NAND_EMPTY_PAGE) {
			if (IS_NAND_RANDOM(host))
				memset(host->buffer, 0xFF,
				       host->pagesize + host->oobsize);
			SET_EMPTY_PAGE(host);

		} else if (status == NAND_VALID_DATA) {

			/* if NAND chip support read retry */
			if (GET_UC_ECC(host) && host->read_retry)
				hinfc504_do_read_retry(host);

		} else
			SET_BAD_BLOCK(host);
	}

	host->cache_addr_value[0] = host->addr_value[0];
	host->cache_addr_value[1] = host->addr_value[1];

	return 0;
}
/*****************************************************************************/

static int hinfc504_send_cmd_erase(struct hinfc_host *host)
{
	/* Don't case the read retry config */
	host->enable_ecc_randomizer(host, DISABLE, DISABLE);

	hinfc_write(host,
		host->addr_value[0],
		HINFC504_ADDRL);
	hinfc_write(host,
		(NAND_CMD_ERASE2 << 8) | NAND_CMD_ERASE1,
		HINFC504_CMD);

	hinfc_write(host,
		HINFC504_OP_WAIT_READY_EN
		| HINFC504_OP_CMD2_EN
		| HINFC504_OP_CMD1_EN
		| HINFC504_OP_ADDR_EN
		| ((host->chipselect
			& HINFC504_OP_NF_CS_MASK)
			<< HINFC504_OP_NF_CS_SHIFT)
		| ((host->addr_cycle
			& HINFC504_OP_ADDR_CYCLE_MASK)
			<< HINFC504_OP_ADDR_CYCLE_SHIFT),
		HINFC504_OP);

	WAIT_CONTROLLER_FINISH();

	return 0;
}
/*****************************************************************************/

static int hinfc504_send_cmd_readid(struct hinfc_host *host)
{
	host->enable_ecc_randomizer(host, DISABLE, DISABLE);

	hinfc_write(host, HINFC504_NANDINFO_LEN, HINFC504_DATA_NUM);
	hinfc_write(host, NAND_CMD_READID, HINFC504_CMD);
	hinfc_write(host, 0, HINFC504_ADDRL);

	hinfc_write(host,
		HINFC504_OP_CMD1_EN
		| HINFC504_OP_ADDR_EN
		| HINFC504_OP_READ_DATA_EN
		| ((host->chipselect & HINFC504_OP_NF_CS_MASK)
			<< HINFC504_OP_NF_CS_SHIFT)
		| (1 << HINFC504_OP_ADDR_CYCLE_SHIFT),
		HINFC504_OP);

	host->addr_cycle = 0x0;

	WAIT_CONTROLLER_FINISH();

	return 0;
}
/*****************************************************************************/

static int hinfc504_enable_ecc_randomizer(struct hinfc_host *host,
					  int ecc_en, int randomizer_en)
{
	unsigned int regval;

	if (IS_NAND_RANDOM(host)) {
		regval = hinfc_read(host, HINFC504_RANDOMIZER);
		if (randomizer_en)
			regval |= HINFC504_RANDOMIZER_ENABLE;
		else
			regval &= ~HINFC504_RANDOMIZER_ENABLE;
		hinfc_write(host, regval, HINFC504_RANDOMIZER);
	}

	if (ecc_en)
		hinfc_write(host, host->NFC_CON, HINFC504_CON);
	else
		hinfc_write(host, host->NFC_CON_ECC_NONE, HINFC504_CON);

	return 0;
}
/*****************************************************************************/

static int hinfc600_enable_ecc_randomizer(struct hinfc_host *host,
					  int ecc_en, int randomizer_en)
{
	unsigned int nfc_con;

	if (IS_NAND_RANDOM(host)) {
		if (randomizer_en) {
			host->NFC_CON |= HINFC600_CON_RANDOMIZER_EN;
			host->NFC_CON_ECC_NONE |= HINFC600_CON_RANDOMIZER_EN;
		} else {
			host->NFC_CON &= ~HINFC600_CON_RANDOMIZER_EN;
			host->NFC_CON_ECC_NONE &= ~HINFC600_CON_RANDOMIZER_EN;
		}
	}

	nfc_con = (ecc_en ? host->NFC_CON : host->NFC_CON_ECC_NONE);

	hinfc_write(host, nfc_con, HINFC600_CON);

	return 0;
}
/*****************************************************************************/

static int hinfc504_send_cmd_status(struct hinfc_host *host)
{
	host->enable_ecc_randomizer(host, DISABLE, DISABLE);

	hinfc_write(host, HINFC504_NANDINFO_LEN, HINFC504_DATA_NUM);
	hinfc_write(host, NAND_CMD_STATUS, HINFC504_CMD);
	hinfc_write(host,
		HINFC504_OP_CMD1_EN
		| HINFC504_OP_READ_DATA_EN
		| ((host->chipselect & HINFC504_OP_NF_CS_MASK)
			<< HINFC504_OP_NF_CS_SHIFT),
		HINFC504_OP);

	WAIT_CONTROLLER_FINISH();

	return 0;
}
/*****************************************************************************/

static int hinfc504_send_cmd_reset(struct hinfc_host *host, int chipselect)
{
	hinfc_write(host,
		NAND_CMD_RESET, HINFC504_CMD);

	hinfc_write(host,
		(HINFC504_OP_CMD1_EN
		| ((chipselect & HINFC504_OP_NF_CS_MASK)
			<< HINFC504_OP_NF_CS_SHIFT)
		| HINFC504_OP_WAIT_READY_EN),
		HINFC504_OP);

	WAIT_CONTROLLER_FINISH();

	return 0;
}
/*****************************************************************************/

int hinfc504_dev_ready(struct mtd_info *mtd)
{
	return 0x1;
}
/*****************************************************************************/

void hinfc504_select_chip(struct mtd_info *mtd, int chipselect)
{
	struct nand_chip *chip = mtd->priv;
	struct hinfc_host *host = chip->priv;

	if (chipselect < 0)
		return;

	if (chipselect > CONFIG_HINFC504_MAX_CHIP)
		hinfc_pr_bug("invalid chipselect: %d\n", chipselect);

	host->chipselect = chipselect;
}
/*****************************************************************************/

uint8_t hinfc504_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct hinfc_host *host = chip->priv;

	if (host->command == NAND_CMD_STATUS)
		return readb(chip->IO_ADDR_R);

	host->offset++;

	if (host->command == NAND_CMD_READID)
		return readb(chip->IO_ADDR_R + host->offset - 1);

	return readb(host->buffer + host->column + host->offset - 1);
}
/*****************************************************************************/

u16 hinfc504_read_word(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct hinfc_host *host = chip->priv;

	host->offset += 2;
	return readw(host->buffer + host->column + host->offset - 2);
}
/*****************************************************************************/

void hinfc504_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct hinfc_host *host = chip->priv;

	memcpy(host->buffer + host->column + host->offset, buf, len);
	host->offset += len;
}
/*****************************************************************************/

void hinfc504_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct hinfc_host *host = chip->priv;

	memcpy(buf, host->buffer + host->column + host->offset, len);
	host->offset += len;
}
/*****************************************************************************/
/*
 * 'host->epm' only use the first oobfree[0] field, it looks very simple, But...
 */
static struct nand_ecclayout nand_ecc_default =
{
	.oobfree = {{2, 30}}
};

static struct nand_ecclayout nand_ecc_2K_1bit =
{
	.oobfree = {{22, 30}}
};
/*****************************************************************************/

static struct nand_ctrl_info_t hinfc504_soft_auto_config_table[] =
{
	{SZ_16K, NAND_ECC_40BIT,   1200 /*1152*/, &nand_ecc_default},
	{SZ_16K, NAND_ECC_NONE,    32 , &nand_ecc_default},

	{SZ_8K, NAND_ECC_40BIT,    600 /*592*/, &nand_ecc_default},
	{SZ_8K, NAND_ECC_24BIT,    368, &nand_ecc_default},
	{SZ_8K, NAND_ECC_NONE,     32,  &nand_ecc_default},

	{SZ_4K, NAND_ECC_24BIT,    200, &nand_ecc_default},
	{SZ_4K, NAND_ECC_4BIT_512, 128 /*104*/, &nand_ecc_default},
	{SZ_4K, NAND_ECC_1BIT_512, 128, &nand_ecc_default},
	{SZ_4K, NAND_ECC_NONE,     32,  &nand_ecc_default},

	{SZ_2K, NAND_ECC_24BIT,    128 /*116*/, &nand_ecc_default},
	{SZ_2K, NAND_ECC_4BIT_512, 64,  &nand_ecc_default},
	{SZ_2K, NAND_ECC_1BIT_512, 64,  &nand_ecc_2K_1bit},
	{SZ_2K, NAND_ECC_NONE,     32,  &nand_ecc_default},

	{0,0,0,NULL},
};
/*****************************************************************************/
/* used the best correct arithmetic. */
struct nand_ctrl_info_t *hinfc504_get_best_ecc(struct mtd_info *mtd)
{
	struct nand_ctrl_info_t *best = NULL;
	struct nand_ctrl_info_t *config = hinfc504_soft_auto_config_table;

	for (; config->layout; config++) {
		if (config->pagesize != mtd->writesize)
			continue;

		if (mtd->oobsize < config->oobsize)
			continue;

		if (!best || (best->ecctype < config->ecctype))
			best = config;
	}

	if (!best)
		hinfc_pr_bug(ERSTR_DRIVER "Driver does not support the pagesize(%d) and oobsize(%d).\n",
			     mtd->writesize, mtd->oobsize);

	return best;
}
/*****************************************************************************/
/* force the pagesize and ecctype */
struct nand_ctrl_info_t *hinfc504_force_ecc(struct mtd_info *mtd, int pagesize,
					    int ecctype, char *cfgmsg,
					    int allow_pagediv)
{
	struct nand_ctrl_info_t *fit = NULL;
	struct nand_ctrl_info_t *config = hinfc504_soft_auto_config_table;

	for (; config->layout; config++) {
		if (config->pagesize == pagesize
			&& config->ecctype == ecctype) {
			fit = config;
			break;
		}
	}

	if (!fit) {
		hinfc_pr_bug(ERSTR_DRIVER "Driver(%s mode) does not support this Nand Flash pagesize:%d, ecctype:%s\n",
			     cfgmsg, pagesize, nand_ecc_name(ecctype));
		return NULL;
	}

	if ((pagesize != mtd->writesize)
		&& (pagesize > mtd->writesize || !allow_pagediv)) {
		hinfc_pr_bug(ERSTR_HARDWARE "Hardware (%s mode) configure pagesize %d, but the Nand Flash pageszie is %d\n",
			     cfgmsg, pagesize, mtd->writesize);
		return NULL;
	}

	if (fit->oobsize > mtd->oobsize) {
		hinfc_pr_bug(ERSTR_HARDWARE "(%s mode) The Nand Flash offer space area is %d bytes, but the controller request %d bytes in ecc %s. "
			     "Please make sure the hardware ECC configuration is correct.",
			     cfgmsg, mtd->oobsize, fit->oobsize,
			     nand_ecc_name(ecctype));
		return NULL;
	}

	return fit;
}
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
		printk("%s(%d): Get Nand OTP from tag fail.\n",
			__FUNCTION__, __LINE__);
		return 0;
	}
	memcpy(nand_otp, &tag->u, nand_otp_len);
	return 0;
}
/* 0x48694E77 equal to fastoot ATAG_NAND_PARAM */
__tagtable(0x48694E77, parse_nand_param);
/*****************************************************************************/

static int hinfc504_ecc_probe(struct mtd_info *mtd, struct nand_chip *chip,
			      struct nand_dev_t *nand_dev)
{
	int pagesize;
	int ecctype;
	char *start_type = "unknown";
	struct nand_ctrl_info_t *best = NULL;
	struct hinfc_host *host = chip->priv;

#ifdef CONFIG_HINFC504_AUTO_PAGESIZE_ECC
	best = hinfc504_get_best_ecc(mtd);
	start_type = "Auto";
#endif /* CONFIG_HINFC504_AUTO_PAGESIZE_ECC */

#ifdef CONFIG_HINFC504_HARDWARE_PAGESIZE_ECC
#  ifdef CONFIG_HINFC504_AUTO_PAGESIZE_ECC
#  error you SHOULD NOT define CONFIG_HINFC504_AUTO_PAGESIZE_ECC and CONFIG_HINFC504_HARDWARE_PAGESIZE_ECC at the same time
#  endif

	pagesize = hinfc504_get_pagesize(host);
	ecctype = hinfc504_get_ecctype(host);

	best = hinfc504_force_ecc(mtd, pagesize, ecctype, "hardware config", 0);
	start_type = "Hardware";

#endif /* CONFIG_HINFC504_HARDWARE_PAGESIZE_ECC */

#ifdef CONFIG_HINFC504_PAGESIZE_AUTO_ECC_NONE
#  ifdef CONFIG_HINFC504_AUTO_PAGESIZE_ECC
#  error you SHOULD NOT define CONFIG_HINFC504_PAGESIZE_AUTO_ECC_NONE and CONFIG_HINFC504_AUTO_PAGESIZE_ECC at the same time
#  endif
#  ifdef CONFIG_HINFC504_HARDWARE_PAGESIZE_ECC
#  error you SHOULD NOT define CONFIG_HINFC504_PAGESIZE_AUTO_ECC_NONE and CONFIG_HINFC504_HARDWARE_PAGESIZE_ECC at the same time
#  endif

	pagesize = mtd->writesize;
	ecctype = NAND_ECC_NONE;
	best = hinfc504_force_ecc(mtd, pagesize, ecctype, "force config", 0);
	start_type = "AutoForce";

#endif /* CONFIG_HINFC504_PAGESIZE_AUTO_ECC_NONE */

	if (!best)
		hinfc_pr_bug(ERSTR_HARDWARE "Please configure Nand Flash pagesize and ecctype!\n");

	/* only in case fastboot check randomizer failed. 
	 * Update fastboot or configure hardware randomizer pin fix this problem.
	 */
	if (IS_NAND_RANDOM(nand_dev) && !(IS_NAND_RANDOM(host)))
		hinfc_pr_bug(ERSTR_HARDWARE "Hardware is not configure randomizer, but it is more suitable for this Nand Flash. "
			     "1. Please configure hardware randomizer PIN. "
			     "2. Please updata fastboot.\n");

	if (best->ecctype != NAND_ECC_NONE)
		mtd->oobsize = best->oobsize;
	chip->ecc.layout = best->layout;

	host->ecctype  = best->ecctype;
	host->pagesize = best->pagesize;
	host->oobsize  = mtd->oobsize;
	host->block_page_mask = ((mtd->erasesize / mtd->writesize) - 1);

	if (host->ecctype == NAND_ECC_24BIT) {
		if (host->pagesize == SZ_4K)
			host->n24bit_ext_len = 0x03; /* 8bytes; */
		else if (host->pagesize == SZ_8K)
			host->n24bit_ext_len = 0x01; /* 4bytes; */
	}
	host->dma_oob = host->dma_buffer + host->pagesize;
	host->bbm = (unsigned char *)(host->buffer
		+ host->pagesize + HINFC_BAD_BLOCK_POS);

	host->epm = (unsigned short *)(host->buffer
		+ host->pagesize + chip->ecc.layout->oobfree[0].offset + 28);

	hinfc504_set_pagesize(host, best->pagesize);

	hinfc504_set_ecctype(host, best->ecctype);

	if (mtd->writesize > NAND_MAX_PAGESIZE
		|| mtd->oobsize > NAND_MAX_OOBSIZE) {
		hinfc_pr_bug(ERSTR_DRIVER
		       "Driver does not support this Nand Flash. "
		       "Please increase NAND_MAX_PAGESIZE and NAND_MAX_OOBSIZE.\n");
	}

	if (mtd->writesize != host->pagesize) {
		unsigned int shift = 0;
		unsigned int writesize = mtd->writesize;
		while (writesize > host->pagesize) {
			writesize >>= 1;
			shift++;
		}
		chip->chipsize = chip->chipsize >> shift;
		mtd->erasesize = mtd->erasesize >> shift;
		mtd->writesize = host->pagesize;
		hinfc_pr_msg("Nand divide into 1/%u\n", (1 << shift));
	}

	nand_dev->start_type = start_type;
	nand_dev->ecctype = host->ecctype;

	host->flags |= IS_NAND_RANDOM(nand_dev);

	host->read_retry = NULL;
	if (nand_dev->read_retry_type != NAND_RR_NONE) {
		struct read_retry_t **rr;
		for (rr = read_retry_list; rr; rr++) {
			if ((*rr)->type == nand_dev->read_retry_type) {
				host->read_retry = *rr;
				break;
			}
		}

		if (!host->read_retry) {
			hinfc_pr_bug(ERSTR_DRIVER "This Nand Flash need to enable the 'read retry' feature. "
				     "but the driver dose not offer the feature");
		}

		if (nand_otp_len) {
			memcpy(host->rr_data, nand_otp, nand_otp_len);
		}
	}

	/*
	 * If it want to support the 'read retry' feature, the 'randomizer'
	 * feature must be support first.
	 */
	if (host->read_retry && !IS_NAND_RANDOM(host)) {
		hinfc_pr_bug(ERSTR_HARDWARE "This Nand flash need to enable 'randomizer' feature. "
			     "Please configure hardware randomizer PIN.");
	}

	hinfc504_dbg_init(host);

	return 0;
}
/*****************************************************************************/

static int hinfc504_get_randomizer(struct hinfc_host *host)
{
	unsigned int regval;

	regval = hinfc_read(host, HINFC504_RANDOMIZER);
	if (regval & HINFC504_RANDOMIZER_PAD)
		host->flags |= NAND_RANDOMIZER;

	regval |= (IS_NAND_RANDOM(host) ? HINFC504_RANDOMIZER_ENABLE : 0);
	hinfc_write(host, regval, HINFC504_RANDOMIZER);

	return IS_NAND_RANDOM(host);
}
/*****************************************************************************/

static int hinfc600_get_randomizer(struct hinfc_host *host)
{
	unsigned int regval;

	regval = hinfc_read(host, HINFC600_BOOT_CFG);
	if (regval & HINFC600_BOOT_CFG_RANDOMIZER_PAD)
		host->flags |= NAND_RANDOMIZER;

	return IS_NAND_RANDOM(host);
}
/*****************************************************************************/

int hinfc504_nand_init(struct hinfc_host *host, struct nand_chip *chip)
{
	host->version = hinfc_read(host, HINFC504_VERSION);
	host->enable(host, ENABLE);

	host->addr_cycle    = 0;
	host->addr_value[0] = 0;
	host->addr_value[1] = 0;
	host->cache_addr_value[0] = ~0;
	host->cache_addr_value[1] = ~0;
	host->chipselect    = 0;

	host->send_cmd_pageprog  = hinfc504_send_cmd_pageprog;
	host->send_cmd_readstart = hinfc504_send_cmd_readstart;
	host->send_cmd_erase     = hinfc504_send_cmd_erase;
	host->send_cmd_readid    = hinfc504_send_cmd_readid;
	host->send_cmd_status    = hinfc504_send_cmd_status;
	host->send_cmd_reset     = hinfc504_send_cmd_reset;

	host->NFC_CON = (hinfc_read(host, HINFC504_CON)
		| HINFC504_CON_OP_MODE_NORMAL
		| HINFC504_CON_READY_BUSY_SEL);

	host->NFC_CON_ECC_NONE = (host->NFC_CON
		& (~(HINFC504_CON_ECCTYPE_MASK << HINFC504_CON_ECCTYPE_SHIFT))
		& (~HINFC600_CON_RANDOMIZER_EN));

	memset((char *)chip->IO_ADDR_R,
		0xff, HINFC504_BUFFER_BASE_ADDRESS_LEN);
	memset(host->buffer,
		0xff, (NAND_MAX_PAGESIZE + NAND_MAX_OOBSIZE));

	hinfc_write(host,
		SET_HINFC504_PWIDTH(CONFIG_HINFC504_W_LATCH,
			CONFIG_HINFC504_R_LATCH,
			CONFIG_HINFC504_RW_LATCH),
		HINFC504_PWIDTH);

	if ((HINFC_VER_600 == host->version)
	    || (HINFC_VER_610 == host->version)) {
		hinfc600_get_randomizer(host);
		host->enable_ecc_randomizer = hinfc600_enable_ecc_randomizer;
		host->epmvalue = 0x0000;
	} else {
		hinfc504_get_randomizer(host);
		host->enable_ecc_randomizer = hinfc504_enable_ecc_randomizer;
		host->epmvalue = 0xffff;
	}

	hinfc_param_adjust = hinfc504_ecc_probe;

	return 0;
}
