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
#include <linux/ctype.h>

#include "hinfc504_os.h"
#include "hinfc504.h"
#include "hinfc504_dbg.h"

extern struct hinfc504_dbg_inf_t hinfc504_dbg_inf_dump;
extern struct hinfc504_dbg_inf_t hinfc504_dbg_inf_erase_count;
extern struct hinfc504_dbg_inf_t hinfc504_dbg_inf_ecc_count;
extern struct hinfc504_dbg_inf_t hinfc504_dbg_inf_read_retry;
extern struct hinfc504_dbg_inf_t hinfc504_dbg_inf_read_retry_notice;
extern struct hinfc504_dbg_inf_t hinfc504_dbg_inf_ecc_notice;

struct hinfc504_dbg_inf_t *hinfc504_dbg_inf[] = {
	&hinfc504_dbg_inf_ecc_notice,
	&hinfc504_dbg_inf_read_retry_notice,
#ifdef CONFIG_HINFC504_DBG_NAND_DUMP
	&hinfc504_dbg_inf_dump,
#endif
#ifdef CONFIG_HINFC504_DBG_NAND_ERASE_COUNT
	&hinfc504_dbg_inf_erase_count,
#endif
#ifdef CONFIG_HINFC504_DBG_NAND_ECC_COUNT
	&hinfc504_dbg_inf_ecc_count,
#endif
#ifdef CONFIG_HINFC504_DBG_NAND_READ_RETRY
	&hinfc504_dbg_inf_read_retry,
#endif
	NULL,
};

static struct dentry *dbgfs_root = NULL;
static struct hinfc_host *dbgfs_host = NULL;

/*****************************************************************************/
static ssize_t dbgfs_debug_read(struct file *filp, char __user *buffer,
				size_t count, loff_t *ppos)
{
	char *msg, *p;
	struct hinfc504_dbg_inf_t **inf;

	if (*ppos != 0)
		return 0;

	msg = (char *)__get_free_page(GFP_TEMPORARY);
	if (!msg)
		return -ENOMEM;

	p = msg;
	if (count > PAGE_SIZE)
		count = PAGE_SIZE;

	for (inf = hinfc504_dbg_inf; *inf; inf++) {
		if ((p - msg) + MAX_OPTION_SIZE + 2 > count) {
			PR_ERR("Not enough memory.\n");
			break;
		}
		p += snprintf(p, (MAX_OPTION_SIZE + 2), "%c%s,",
			      ((*inf)->enable ? '+' : '-'),
			      (*inf)->name);
	}

	p += sprintf(p, "\n");
	count = (p - msg);
	if (copy_to_user(buffer, msg, count)) {
		free_page((unsigned long) msg);
		return -EFAULT;
	}

	free_page((unsigned long) msg);

	*ppos += count;
	return count;
}
/*****************************************************************************/

static void dbgfs_debug_do_cmd(struct hinfc504_dbg_inf_t **dbg_inf,
			       const char *cmd, unsigned int length, int enable)
{
	int ret = 0;
	struct hinfc504_dbg_inf_t **inf;

	if (length >= sizeof((*inf)->name))
		return;

	for (inf = dbg_inf; *inf; inf++) {
		if (!(*inf)->name[length] &&
		    !memcmp((*inf)->name, cmd, length))
			break;
	}

	if (!(*inf) || (*inf)->enable == enable)
		return;

	if (enable) {
		if ((*inf)->init)
			ret = (*inf)->init(dbgfs_root, dbgfs_host);
	} else {
		if ((*inf)->uninit)
			ret = (*inf)->uninit();
	}

	if (!ret)
		(*inf)->enable = enable;
}
/*****************************************************************************/

static void dbgfs_debug_ops(const char *options,
			    struct hinfc504_dbg_inf_t **dbg_inf)
{
	int enable;
	const char *pos, *cmd;

	pos = options;

	while (*pos) {

		while (*pos && *pos != '+' && *pos != '-')
			pos++;

		switch (*pos++) {
		case '+': enable = 1; break;
		case '-': enable = 0; break;
		default:  return;
		}

		cmd = pos;
		while (*pos == '_' || isalpha(*pos))
			pos++;

		if (*cmd && pos > cmd)
			dbgfs_debug_do_cmd(dbg_inf, cmd, (pos - cmd), enable);

		while (isspace(*pos) || *pos == ',' || *pos == ';')
			pos++;
	}
}
/*****************************************************************************/
/*
 * echo "+dump, +read_retry, +ecc_count, +erase_count"  > debug
 */
static ssize_t dbgfs_debug_write(struct file *filp, const char __user *buffer,
				 size_t count, loff_t *ppos)
{
	char *options;
	size_t num = count;

	if (count > PAGE_SIZE)
		num = (PAGE_SIZE - 1);

	options = (char *)__get_free_page(GFP_TEMPORARY);
	if (!options)
		return -ENOMEM;

	if (copy_from_user(options, buffer, num)) {
		free_page((unsigned long) options);
		return -EFAULT;
	}

	options[num] = 0;

	dbgfs_debug_ops(options, hinfc504_dbg_inf);

	free_page((unsigned long) options);

	*ppos += count;
	return count;
}
/*****************************************************************************/

static struct file_operations dbgfs_debug_fops = {
	.owner = THIS_MODULE,
	.read  = dbgfs_debug_read,
	.write = dbgfs_debug_write,
};
/*****************************************************************************/

int hinfc504_dbgfs_debug_init(struct hinfc_host *host)
{
	struct dentry *dentry;

	if (dbgfs_root)
		return 0;

	dbgfs_root = debugfs_create_dir("nand", NULL);
	if (!dbgfs_root) {
		PR_ERR("Can't create 'nand' dir.\n");
		return -ENOENT;
	}

	dentry = debugfs_create_file("debug", S_IFREG | S_IRUSR | S_IWUSR,
				     dbgfs_root, NULL, &dbgfs_debug_fops);
	if (!dentry) {
		PR_ERR("Can't create 'debug' file.\n");
		goto fail;
	}

	dbgfs_host = host;

	if (nand_dbgfs_options)
		dbgfs_debug_ops(nand_dbgfs_options, hinfc504_dbg_inf);

	return 0;

fail:
	debugfs_remove_recursive(dbgfs_root);
	dbgfs_root = NULL;

	return -ENOENT;
}
/*****************************************************************************/

static int dbgfs_read_retry_notice_init(struct dentry *root,
					struct hinfc_host *host)
{
	if (!host->read_retry) {
		printk(KERN_WARNING
		       "read_retry_notice: The NAND not support this interface.\n");
		return -1;
	}

	return 0;
}

static void hinfc504_dbg_read_retry_notice(struct hinfc_host *host,
					      int index)
{
	printk(KERN_WARNING "Page 0x%08x do read retry (%d/%d) %s.\n",
	       GET_PAGE_INDEX(host), index, host->read_retry->count,
	       (GET_UC_ECC(host) ? "Fail" : "Success"));
}

struct hinfc504_dbg_inf_t hinfc504_dbg_inf_read_retry_notice = {
	"read_retry_notice", 0,
	dbgfs_read_retry_notice_init,
	NULL,
	NULL,
	NULL,
	NULL,
	hinfc504_dbg_read_retry_notice,
};
/*****************************************************************************/

static void hinfc504_dbg_ecc_notice_read(struct hinfc_host *host)
{
	if (GET_BAD_BLOCK(host) || GET_EMPTY_PAGE(host))
		return;

	if (GET_UC_ECC(host))
		printk(KERN_WARNING "page 0x%08x has uncorrect ecc.\n",
		       GET_PAGE_INDEX(host));
}

struct hinfc504_dbg_inf_t hinfc504_dbg_inf_ecc_notice = {
	"ecc_notice", 0,
	NULL,
	NULL,
	hinfc504_dbg_ecc_notice_read,
	NULL,
	NULL,
	NULL,
};
/*****************************************************************************/
