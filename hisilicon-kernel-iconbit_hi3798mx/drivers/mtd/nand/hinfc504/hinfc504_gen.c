/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-08-15
 *
******************************************************************************/

#include <linux/match.h>
#include "hinfc504_gen.h"

/*****************************************************************************/

static struct match_t hinfc504_pagesize[] = {
	MATCH_SET_TYPE_REG(SZ_512,  hinfc504_pagesize_512),
	MATCH_SET_TYPE_REG(SZ_2K,  hinfc504_pagesize_2K),
	MATCH_SET_TYPE_REG(SZ_4K,  hinfc504_pagesize_4K),
	MATCH_SET_TYPE_REG(SZ_8K,  hinfc504_pagesize_8K),
	MATCH_SET_TYPE_REG(SZ_16K, hinfc504_pagesize_16K),
};

int hinfc504_get_pagesize(struct hinfc_host *host)
{
	int regval = host->NFC_CON >> HINFC504_CON_PAGEISZE_SHIFT;
	regval &= HINFC504_CON_PAGESIZE_MASK;

	return match_reg_to_type(hinfc504_pagesize,
		ARRAY_SIZE(hinfc504_pagesize), regval, SZ_2K);
}

void hinfc504_set_pagesize(struct hinfc_host *host, int pagesize)
{
	int mask = ~(HINFC504_CON_PAGESIZE_MASK << HINFC504_CON_PAGEISZE_SHIFT);
	int regval = match_type_to_reg(hinfc504_pagesize,
		ARRAY_SIZE(hinfc504_pagesize), pagesize, hinfc504_pagesize_2K);

	regval = (regval & HINFC504_CON_PAGESIZE_MASK) << HINFC504_CON_PAGEISZE_SHIFT;

	host->NFC_CON &=  mask;
	host->NFC_CON |=  regval;

	host->NFC_CON_ECC_NONE &=  mask;
	host->NFC_CON_ECC_NONE |=  regval;
}
/*****************************************************************************/

static struct match_t hinfc504_ecc[] = {
	MATCH_SET_TYPE_REG(NAND_ECC_NONE,  hinfc504_ecc_none),
	MATCH_SET_TYPE_REG(NAND_ECC_1BIT_512,  hinfc504_ecc_1bit),
	MATCH_SET_TYPE_REG(NAND_ECC_4BIT_512, hinfc504_ecc_4bit),
	MATCH_SET_TYPE_REG(NAND_ECC_24BIT, hinfc504_ecc_24bit1k),
	MATCH_SET_TYPE_REG(NAND_ECC_40BIT, hinfc504_ecc_40bit1k),
};

int hinfc504_get_ecctype(struct hinfc_host *host)
{
	int regval = host->NFC_CON >> HINFC504_CON_ECCTYPE_SHIFT;

	regval &= HINFC504_CON_ECCTYPE_MASK;

	return match_reg_to_type(hinfc504_ecc, ARRAY_SIZE(hinfc504_ecc),
		regval, NAND_ECC_1BIT_512);
}

void hinfc504_set_ecctype(struct hinfc_host *host, int ecctype)
{
	int regval = match_type_to_reg(hinfc504_ecc, ARRAY_SIZE(hinfc504_ecc),
		ecctype, hinfc504_ecc_1bit);

	host->NFC_CON &= ~(HINFC504_CON_ECCTYPE_MASK << HINFC504_CON_ECCTYPE_SHIFT);

	host->NFC_CON |= (regval & HINFC504_CON_ECCTYPE_MASK) << HINFC504_CON_ECCTYPE_SHIFT;
}
