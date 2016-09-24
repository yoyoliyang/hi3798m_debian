/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-07-04
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

extern int hinfc504_dbgfs_debug_init(struct hinfc_host *host);

#ifdef CONFIG_HINFC504_DBG_NAND_DEBUG
extern struct hinfc504_dbg_inf_t *hinfc504_dbg_inf[];
#endif

void hinfc504_dbg_write(struct hinfc_host *host)
{
#ifdef CONFIG_HINFC504_DBG_NAND_DEBUG
	struct hinfc504_dbg_inf_t **inf;
	for (inf = hinfc504_dbg_inf; *inf; inf++)
		if ((*inf)->enable && (*inf)->write)
			(*inf)->write(host);
#endif
}

void hinfc504_dbg_erase(struct hinfc_host *host)
{
#ifdef CONFIG_HINFC504_DBG_NAND_DEBUG
	struct hinfc504_dbg_inf_t **inf;
	for (inf = hinfc504_dbg_inf; *inf; inf++)
		if ((*inf)->enable && (*inf)->erase)
			(*inf)->erase(host);
#endif
}

void hinfc504_dbg_read(struct hinfc_host *host)
{
#ifdef CONFIG_HINFC504_DBG_NAND_DEBUG
	struct hinfc504_dbg_inf_t **inf;
	for (inf = hinfc504_dbg_inf; *inf; inf++)
		if ((*inf)->enable && (*inf)->read)
			(*inf)->read(host);
#endif
}

void hinfc504_dbg_read_retry(struct hinfc_host *host, int index)
{
#ifdef CONFIG_HINFC504_DBG_NAND_DEBUG
	struct hinfc504_dbg_inf_t **inf;
	for (inf = hinfc504_dbg_inf; *inf; inf++)
		if ((*inf)->enable && (*inf)->read_retry)
			(*inf)->read_retry(host, index);
#endif
}

int hinfc504_dbg_init(struct hinfc_host *host)
{
#ifdef CONFIG_HINFC504_DBG_NAND_DEBUG
	return hinfc504_dbgfs_debug_init(host);
#endif
}
