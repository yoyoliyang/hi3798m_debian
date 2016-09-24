/******************************************************************************
*    Copyright (c) 2009-2011 by Hisi.
*    All rights reserved.
* ***
*    Create by Czyong. 2011-12-03
*
******************************************************************************/

#include <linux/match.h>
#include "hinfc_gen.h"

/*****************************************************************************/

static struct match_t match_ecc[] = {
	MATCH_SET_TYPE_DATA(NAND_ECC_NONE, "none"),
	MATCH_SET_TYPE_DATA(NAND_ECC_1BIT_512, "1bit/512" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_4BIT_512, "4bit/512" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_8BIT, "4bit/512" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_8BIT_512, "8bit/512" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_24BIT, "24bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_40BIT, "40bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_4BYTE, "4byte/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_8BYTE, "8byte/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_13BIT, "13bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_16BIT, "16bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_18BIT, "18bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_27BIT, "27bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_32BIT, "32bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_41BIT, "41bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_48BIT, "48bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_60BIT, "60bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_72BIT, "72bit/1k" ),
	MATCH_SET_TYPE_DATA(NAND_ECC_80BIT, "80bit/1k" ),
};

const char *nand_ecc_name(int type)
{
	return (char *)match_type_to_data(match_ecc, ARRAY_SIZE(match_ecc),
		type, "unknown");
}
/*****************************************************************************/

int get_bits(unsigned int n)
{
	int loop;
	int ret = 0;

	if (!n)
		return 0;

	if (n > 0xFFFF)
		loop = n > 0xFFFFFF ? 32 : 24;
	else
		loop = n > 0xFF ? 16 : 8;

	while (loop-- > 0 && n) {
		if (n & 1)
			ret++;
		n >>= 1;
	}
	return ret;
}
/*****************************************************************************/

char *nand_dbgfs_options = NULL;

static int __init dbgfs_options_setup(char *s)
{
	nand_dbgfs_options = s;
	return 1;
}
__setup("nanddbgfs=", dbgfs_options_setup);
