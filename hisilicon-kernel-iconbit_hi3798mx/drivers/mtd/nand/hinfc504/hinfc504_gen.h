/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-08-15
 *
******************************************************************************/
#ifndef HINFC504_GENH
#define HINFC504_GENH
/******************************************************************************/

#include <hinfc_gen.h>
#include "hinfc504.h"

enum hinfc504_ecc_reg {
	hinfc504_ecc_none    = 0x00,
	hinfc504_ecc_1bit    = 0x01,
	hinfc504_ecc_4bit    = 0x02,
	hinfc504_ecc_4bytes  = 0x02,
	hinfc504_ecc_8bytes  = 0x03,
	hinfc504_ecc_24bit1k = 0x04,
	hinfc504_ecc_40bit1k = 0x05,
};

enum hinfc504_page_reg {
	hinfc504_pagesize_512   = 0x00,
	hinfc504_pagesize_2K    = 0x01,
	hinfc504_pagesize_4K    = 0x02,
	hinfc504_pagesize_8K    = 0x03,
	hinfc504_pagesize_16K   = 0x04,
};

int hinfc504_get_pagesize(struct hinfc_host *host);

void hinfc504_set_pagesize(struct hinfc_host *host, int pagesize);

int hinfc504_get_ecctype(struct hinfc_host *host);

void hinfc504_set_ecctype(struct hinfc_host *host, int ecctype);

/******************************************************************************/
#endif /* HINFC504_GENH */
