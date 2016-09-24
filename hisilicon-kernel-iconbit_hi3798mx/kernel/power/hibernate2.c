#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/syscalls.h>
#include <linux/console.h>
#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/hibernate_param.h>
#include <asm/uaccess.h>

#include <linux/mtd/partitions.h>
#include <linux/cmdline-parser.h>
#include <linux/tags.h>

#include <linux/hibernate.h>

#ifdef HIBERNATE_UMOUNT_RW
#include <linux/delay.h>
#include <linux/namei.h>
#include <linux/mount.h>
#endif
#ifdef HIBERNATE_ANDROID_MODE
#include <linux/inotify.h>
#endif

#ifdef CONFIG_MTD
#undef DEBUG
#include <linux/mtd/mtd.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#include <linux/freezer.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#include <asm/suspend.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
#include <linux/syscore_ops.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)
#include <linux/random.h>
#endif

#include "power.h"
#include "../../fs/mount.h"

#ifndef HIBERNATE_WORK_SIZE
#define HIBERNATE_WORK_SIZE          (128 * 1024)
#endif

#ifndef HIBERNATE_HD_SAVEAREA_SIZE
#define HIBERNATE_HD_SAVEAREA_SIZE   4096
#endif

#define ZONETBL_DEFAULT_NUM     8
#define EXTTBL_DEFAULT_NUM      8

#define ZONETBL_DEFAULT_SIZE    (ZONETBL_DEFAULT_NUM * \
								 sizeof(struct hibernate_savetbl))
#define EXTTBL_DEFAULT_SIZE     (EXTTBL_DEFAULT_NUM * \
								 sizeof(struct hibernate_savetbl64))

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
#define SHRINK_BITE				INT_MAX
#else
#define SHRINK_BITE				ULONG_MAX
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,33)
#ifndef HIBERNATE_SHRINK_REPEAT
#define HIBERNATE_SHRINK_REPEAT		1
#endif
#else
#ifndef HIBERNATE_SHRINK_REPEAT
#define HIBERNATE_SHRINK_REPEAT		10
#endif
#ifndef HIBERNATE_SHRINK_REPEAT2
#define HIBERNATE_SHRINK_REPEAT2	1
#endif
#ifndef HIBERNATE_SHRINK_REPEAT3
#define HIBERNATE_SHRINK_REPEAT3	2
#endif
#endif

#ifndef HIBERNATE_SHRINK_REPEAT_P1
#define HIBERNATE_SHRINK_REPEAT_P1		10000
#endif

#ifndef HIBERNATE_SHRINK_THRESHOLD
#define HIBERNATE_SHRINK_THRESHOLD   1
#endif

#ifndef HIBERNATE_SHRINK_THRESHOLD_COUNT
#define HIBERNATE_SHRINK_THRESHOLD_COUNT	100
#endif

#if defined(HIBERNATE_SUSPEND_ERR_RECOVER) && defined(HIBERNATE_SUSPEND_ERR_NO_RECOVER)
#error "duplicate define HIBERNATE_SUSPEND_ERR_RECOVER & HIBERNATE_SUSPEND_ERR_NO_RECOVER"
#endif

#if !defined(HIBERNATE_SUSPEND_ERR_RECOVER) && !defined(HIBERNATE_SUSPEND_ERR_NO_RECOVER)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define HIBERNATE_SUSPEND_ERR_RECOVER
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
#define pm_device_suspend(x)	dpm_suspend(x)
#define pm_device_resume(x) 	dpm_resume(x)
#else
#define pm_device_suspend(x)	dpm_suspend_start(x)
#define pm_device_resume(x)		dpm_resume_end(x)
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
#define pm_device_power_down(x)	dpm_suspend_end(x)
#define pm_device_power_up(x)	dpm_resume_start(x)
#else
#define pm_device_power_down(x)	dpm_suspend_noirq(x)
#define pm_device_power_up(x)	dpm_resume_noirq(x)
#endif
#else
#define pm_device_suspend(x)	device_suspend(x)
#define pm_device_power_down(x)	device_power_down(x)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define pm_device_resume(x)	device_resume(x)
#define pm_device_power_up(x)	device_power_up(x)
#else
#define pm_device_resume(x)	device_resume()
#define pm_device_power_up(x)	device_power_up()
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
#define STATE_FREEZE	PMSG_FREEZE
#else
#define STATE_FREEZE	PM_SUSPEND_DISK
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define STATE_RESTORE	(!hibernate_stat ? (ret ? PMSG_RECOVER : PMSG_THAW) : \
						 PMSG_RESTORE)
#else
#define STATE_RESTORE
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
#define hibernate       pm_suspend_disk
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
#define current_uid()	(current->uid)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
#define TOTAL_SWAPCACHE_PAGES	total_swapcache_pages
#define NR_SWAP_PAGES			nr_swap_pages
#else
#define TOTAL_SWAPCACHE_PAGES	total_swapcache_pages()
#define NR_SWAP_PAGES			get_nr_swap_pages()
#endif

#ifndef hibernate_pfn_valid
#define hibernate_pfn_valid(pfn)	pfn_valid(pfn)
#endif

#ifndef hibernate_flush_icache_range
#define hibernate_flush_icache_range(s, e)	flush_icache_range(s, e)
#endif

#ifndef HIBERNATE_AMP
#define hibernate_amp()				0
#define hibernate_amp_maincpu()		1
#endif

#define HIBERNATE_SAVEAREA_NUM		(sizeof(hibernate_savearea) / \
								 sizeof(struct hibernate_savearea))

struct hibernate_savearea hibernate_savearea[] = {
  /*   HIBERNATE_SAVEAREA */
	{0}
};

#ifdef HIBERNATE_UMOUNT_RW

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#define freeze_user_processes	freeze_processes
#else
int freeze_user_processes(void);
int freeze_kernel_threads(void);
void thaw_kernel_threads(void);
#endif

#define HIBERNATE_UMOUNT_RW_NUM (sizeof(hibernate_umount_rw) / sizeof(char *))

static const char *hibernate_umount_rw[] = {
	HIBERNATE_UMOUNT_RW
};

#endif

int pm_device_down;
int hibernate_shrink;
int hibernate_swapout_disable;
int hibernate_separate_pass;
int hibernate_canceled;
struct hibernate hibernate_param;

EXPORT_SYMBOL(hibernate_savearea);

EXPORT_SYMBOL(pm_device_down);
EXPORT_SYMBOL(hibernate_shrink);
EXPORT_SYMBOL(hibernate_swapout_disable);
EXPORT_SYMBOL(hibernate_separate_pass);
EXPORT_SYMBOL(hibernate_canceled);
EXPORT_SYMBOL(hibernate_param);

EXPORT_SYMBOL(hibdrv_snapshot);
EXPORT_SYMBOL(hibernate_set_savearea);
EXPORT_SYMBOL(hibernate_save_cancel);
EXPORT_SYMBOL(hibernate_register_machine);
EXPORT_SYMBOL(hibernate_unregister_machine);

static struct hibernate_ops *hibernate_ops;

static int hibernate_stat;
static int hibernate_error;
static int hibernate_retry;
static int hibernate_saved;
static int hibernate_saveno;
static int hibernate_loadno;
static int hibernate_separate;

static int hibernate_save_pages;
static void *hibernate_work;
static unsigned long hibernate_work_size;
static unsigned long hibernate_work_start, hibernate_work_end;
static unsigned long hibernate_nosave_area, hibernate_nosave_size;
static unsigned long hibernate_lowmem_nosave_area, hibernate_lowmem_nosave_size;

static struct hibernate_savetbl *zonetbl, *dramtbl;
static struct hibernate_savetbl64 *exttbl;
static unsigned long zonetbl_max, dramtbl_max, exttbl_max;

#ifdef HIBERNATE_HIBDRV_FLOATING
unsigned char *hibernate_hibdrv_buf;
#endif
unsigned char *hibernate_bootflag_buf;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22) && defined(HIBERNATE_PFN_IS_NOSAVE)

extern const void __nosave_begin, __nosave_end;

int pfn_is_nosave(unsigned long pfn)
{
	unsigned long nosave_begin_pfn, nosave_end_pfn;

	nosave_begin_pfn = __pa(&__nosave_begin) >> PAGE_SHIFT;
	nosave_end_pfn = PAGE_ALIGN(__pa(&__nosave_end)) >> PAGE_SHIFT;
	return (pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn);
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)

bool system_entering_hibernation(void)
{
	return pm_device_down != HIBERNATE_STATE_NORMAL;
}

EXPORT_SYMBOL(system_entering_hibernation);

#endif

#ifdef CONFIG_PM_HIBERNATE_DEBUG

void hibernate_putc(char c)
{
	if (hibernate_ops->putc) {
		if (c == '\n')
			hibernate_ops->putc('\r');
		hibernate_ops->putc(c);
	}
}

int hibernate_printf(const char *fmt, ...)
{
	int i, len;
	va_list args;
	char buf[256];

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	for (i = 0; i < len; i++)
		hibernate_putc(buf[i]);
	return len;
}

EXPORT_SYMBOL(hibernate_putc);
EXPORT_SYMBOL(hibernate_printf);

#endif

#ifdef CONFIG_MTD

static int hibernate_mtd_offs(struct mtd_info *mtd, loff_t *offs, loff_t end)
{
	while (*offs < end) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
		if (!mtd->block_isbad || !mtd->block_isbad(mtd, *offs))
#else
		if (!mtd->_block_isbad || !mtd->_block_isbad(mtd, *offs))
#endif
			return 0;
		*offs += mtd->erasesize;
	}
	return -EIO;
}

#endif

static struct file *hibernate_dev_open(const char *dev)
{
	struct file *f;
	char name[256];

	name[255] = '\0';
	if (*dev == '/') {
		strncpy(name, dev, 255);
	} else {
		strcpy(name, "/dev/");
		strncpy(name + 5, dev, 250);
	}
	f = filp_open(name, O_RDONLY, 0777);
	if (IS_ERR(f)) {
		if (*dev != '/') {
			/* for Android */
			strcpy(name, "/dev/block/");
			strncpy(name + 11, dev, 244);
			f = filp_open(name, O_RDONLY, 0777);
		}
		if (IS_ERR(f))
			printk("Can't open device %s (%d).\n", dev, (int)PTR_ERR(f));
	}

	return f;
}

#ifdef HIBERNATE_HIBDRV_FLOATING

#ifdef CONFIG_MTD

static int hibernate_load_drv_mtd(struct mtd_info *mtd, loff_t offs, loff_t end,
							 void *buf, size_t size)
{
	int ret;
	size_t read_size, req_size, remain;

	if (IS_ERR(mtd))
		return PTR_ERR(mtd);

	if ((ret = hibernate_mtd_offs(mtd, &offs, end)) < 0) {
		put_mtd_device(mtd);
		return ret;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
	ret = mtd->read(mtd, offs, 256, &read_size, buf);
#else
	ret = mtd->_read(mtd, offs, 256, &read_size, buf);
#endif
	if (ret >= 0) {
		if (HIBERNATE_ID(buf) != HIBERNATE_ID_DRIVER) {
			put_mtd_device(mtd);
			return 1;
		}
		if (size <= HIBERNATE_DRV_COPY_SIZE(buf)) {
			ret = -ENOMEM;
		} else {
			remain = HIBERNATE_DRV_COPY_SIZE(buf);
			for (;;) {
				if ((req_size = remain) > mtd->erasesize)
					req_size = mtd->erasesize;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
				ret = mtd->read(mtd, offs, req_size, &read_size, buf);
#else
				ret = mtd->_read(mtd, offs, req_size, &read_size, buf);
#endif
				if (ret < 0)
					break;
				if (req_size != read_size) {
					ret = -EIO;
					break;
				}
				remain -= req_size;
				offs += req_size;
				buf += req_size;
				if (remain <= 0)
					break;
				if ((ret = hibernate_mtd_offs(mtd, &offs, end)) < 0)
					break;
			}
		}
	}

	put_mtd_device(mtd);
	return ret;
}

int hibernate_load_drv_mtd_no(int mtdno, loff_t offs, loff_t end,
						 void *buf, size_t size)
{
	return hibernate_load_drv_mtd(get_mtd_device(NULL, mtdno), offs, end, buf, size);
}

EXPORT_SYMBOL(hibernate_load_drv_mtd_no);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)

int hibernate_load_drv_mtd_nm(const char *mtdname, loff_t offs, loff_t end,
						 void *buf, size_t size)
{
	return hibernate_load_drv_mtd(get_mtd_device_nm(mtdname), offs, end, buf, size);
}

EXPORT_SYMBOL(hibernate_load_drv_mtd_nm);

#endif

#endif  /* CONFIG_MTD */

#if HIBERNATE_HIBDRV_LOAD_MODE == HIBERNATE_LOAD_DEV

int hibernate_load_drv_dev(const char *dev, loff_t offs, void *buf, size_t size)
{
	int ret = 0;
	struct file *f;

	f = hibernate_dev_open(dev);
	if (IS_ERR(f))
		return PTR_ERR(f);

	if ((ret = kernel_read(f, offs, buf, PAGE_SIZE)) >= 0) {
		if (HIBERNATE_ID(buf) != HIBERNATE_ID_DRIVER) {
			filp_close(f, NULL);
			return 1;
		}
		if (size <= HIBERNATE_DRV_COPY_SIZE(buf)) {
			ret = -ENOMEM;
		} else {
			if (HIBERNATE_DRV_COPY_SIZE(buf) <= PAGE_SIZE ||
				(ret = kernel_read(f, offs + PAGE_SIZE, buf + PAGE_SIZE,
								   HIBERNATE_DRV_COPY_SIZE(buf) - PAGE_SIZE)) >= 0) {
				filp_close(f, NULL);
				return 0;
			}
		}
	}

	filp_close(f, NULL);
	return ret;
}

EXPORT_SYMBOL(hibernate_load_drv_dev);

#endif

#if HIBERNATE_HIBDRV_LOAD_MODE == HIBERNATE_LOAD_MEM

int hibernate_load_drv_mem(const void *hibdrv_addr, void *buf, size_t size)
{
	if (HIBERNATE_ID(hibdrv_addr) != HIBERNATE_ID_DRIVER)
		return 1;
	if (size <= HIBERNATE_DRV_COPY_SIZE(hibdrv_addr))
		return -ENOMEM;

	memcpy(buf, hibdrv_addr, HIBERNATE_DRV_COPY_SIZE(hibdrv_addr));
	return 0;
}

EXPORT_SYMBOL(hibernate_load_drv_mem);

#endif

static int hibernate_load_drv(size_t size)
{
	int ret = -EIO;

	if (hibernate_ops->drv_load) {
		ret = hibernate_ops->drv_load(hibernate_hibdrv_buf, size);
	} else {
#if HIBERNATE_HIBDRV_LOAD_MODE == HIBERNATE_LOAD_MEM
		ret = hibernate_load_drv_mem((const void *)HIBERNATE_HIBDRV_MEM,
								hibernate_hibdrv_buf, size);
#elif HIBERNATE_HIBDRV_LOAD_MODE == HIBERNATE_LOAD_DEV
		ret = hibernate_load_drv_dev(HIBERNATE_HIBDRV_DEV, HIBERNATE_HIBDRV_OFFSET,
								hibernate_hibdrv_buf, size);
#elif HIBERNATE_HIBDRV_LOAD_MODE == HIBERNATE_LOAD_MTD_NO
		ret = hibernate_load_drv_mtd_no(HIBERNATE_HIBDRV_MTD_NO, HIBERNATE_HIBDRV_OFFSET,
								   HIBERNATE_HIBDRV_OFFSET + HIBERNATE_HIBDRV_AREA_SIZE,
								   hibernate_hibdrv_buf, size);
#elif HIBERNATE_HIBDRV_LOAD_MODE == HIBERNATE_LOAD_MTD_NAME
		ret = hibernate_load_drv_mtd_nm(HIBERNATE_HIBDRV_MTD_NAME, HIBERNATE_HIBDRV_OFFSET,
								   HIBERNATE_HIBDRV_OFFSET + HIBERNATE_HIBDRV_AREA_SIZE,
								   hibernate_hibdrv_buf, size);
#endif
	}

	if (ret == 1) {
		printk("Can't find hibernation driver.\n");
		ret = -EIO;
	} else if (ret < 0) {
		printk("Can't load hibernation driver.\n");
	}
	return ret;
}

#endif  /* HIBERNATE_HIBDRV_FLOATING */

#ifdef CONFIG_MTD

static int hibernate_load_bf_mtd(struct mtd_info *mtd, loff_t offs, loff_t end,
							void *buf)
{
	int ret;
	size_t read_size;

	if (IS_ERR(mtd))
		return PTR_ERR(mtd);

	if ((ret = hibernate_mtd_offs(mtd, &offs, end)) < 0) {
		put_mtd_device(mtd);
		return ret;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
	ret = mtd->read(mtd, offs, HIBERNATE_BF_LEN, &read_size, buf);
#else
	ret = mtd->_read(mtd, offs, HIBERNATE_BF_LEN, &read_size, buf);
#endif
	put_mtd_device(mtd);
	return ret;
}

int hibernate_load_bf_mtd_no(int mtdno, loff_t offs, loff_t end, void *buf)
{
	return hibernate_load_bf_mtd(get_mtd_device(NULL, mtdno), offs, end, buf);
}

EXPORT_SYMBOL(hibernate_load_bf_mtd_no);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)

int hibernate_load_bf_mtd_nm(const char *mtdname, loff_t offs, loff_t end, void *buf)
{
	return hibernate_load_bf_mtd(get_mtd_device_nm(mtdname), offs, end, buf);
}

EXPORT_SYMBOL(hibernate_load_bf_mtd_nm);

#endif

#endif  /* CONFIG_MTD */

int hibernate_load_bf_dev(const char *dev, loff_t offs, void *buf)
{
	int ret;
	struct file *f;

	f = hibernate_dev_open(dev);
	if (IS_ERR(f))
		return PTR_ERR(f);
	ret = kernel_read(f, offs, buf, HIBERNATE_BF_LEN);
	filp_close(f, NULL);
	return ret;
}

EXPORT_SYMBOL(hibernate_load_bf_dev);

static int hibernate_load_bf(void)
{
	int ret = -EIO;
	struct hibernate_savearea *area = &hibernate_savearea[hibernate_loadno];

	if (hibernate_ops->bf_load) {
		ret = hibernate_ops->bf_load(hibernate_bootflag_buf, hibernate_loadno);
	} else if (area->bootflag_load_mode == HIBERNATE_LOAD_MEM) {
		memcpy(hibernate_bootflag_buf, (void *)area->bootflag_offset, HIBERNATE_BF_LEN);
		ret = 0;
	} else if (area->bootflag_load_mode == HIBERNATE_LOAD_DEV) {
		ret = hibernate_load_bf_dev(area->bootflag_part, area->bootflag_offset,
							   hibernate_bootflag_buf);
#ifdef CONFIG_MTD
	} else if (area->bootflag_load_mode == HIBERNATE_LOAD_MTD_NO) {
		ret = hibernate_load_bf_mtd_no((int)area->bootflag_part,
								  area->bootflag_offset,
								  area->bootflag_offset + area->bootflag_size,
								  hibernate_bootflag_buf);
	} else if (area->bootflag_load_mode == HIBERNATE_LOAD_MTD_NAME) {
		ret = hibernate_load_bf_mtd_nm(area->bootflag_part, area->bootflag_offset,
								  area->bootflag_offset + area->bootflag_size,
								  hibernate_bootflag_buf);
#endif
	}

	if (ret < 0) {
		printk("Can't load bootflag.\n");
	} else if (*(unsigned long *)hibernate_bootflag_buf != HIBERNATE_ID_BOOTFLAG) {
		printk("Can't find bootflag.\n");
		ret = -EIO;
	}
	return ret;
}

static int hibernate_work_alloc(void)
{
	int work_size;
	void *hd_savearea;

	for (work_size = HIBERNATE_WORK_SIZE; ; work_size >>= 1) {
		if (work_size < PAGE_SIZE) {
			printk("hibernate: Can't alloc work memory.\n");
			return -ENOMEM;
		}
		if ((hibernate_work = kmalloc(work_size, GFP_KERNEL)) != NULL)
			break;
	}
	hibernate_work_size = work_size;

	hd_savearea = kmalloc(HIBERNATE_HD_SAVEAREA_SIZE, GFP_KERNEL);
	if (hd_savearea == NULL) {
		printk("hibernate: Can't alloc HD savearea memory.\n");
		return -ENOMEM;
	}
	hibernate_param.hd_savearea = (unsigned long)hd_savearea;
	hibernate_param.hd_savearea_end = hibernate_param.hd_savearea + HIBERNATE_HD_SAVEAREA_SIZE;

	return 0;
}

static int hibernate_work_init(void)
{
	int table_size;
#ifdef HIBERNATE_HIBDRV_FLOATING
	int hibernate_hibdrv_size;
#endif
	struct hibernate_savetbl *savetbl;

#ifdef HIBERNATE_AMP
	if (hibernate_amp() && !hibernate_amp_maincpu()) {
		hibernate_work = (void *)HIBERNATE_AMP_WORK;
		hibernate_work_size = HIBERNATE_AMP_WORK_SIZE;
	} else
#endif
	{
#ifndef HIBERNATE_WORK_ALLOC_INIT
		int ret;
		if ((ret = hibernate_work_alloc()) < 0)
			return ret;
#endif
	}
	printk("Hibernate work area 0x%p-0x%p\n", hibernate_work, hibernate_work + hibernate_work_size);
	printk("Hibernate HD savearea 0x%p-0x%p\n",
		   (void *)(unsigned long)hibernate_param.hd_savearea,
		   (void *)(unsigned long)(hibernate_param.hd_savearea +
								   HIBERNATE_HD_SAVEAREA_SIZE));


#ifdef HIBERNATE_HIBDRV_FLOATING
#ifdef HIBERNATE_AMP
	if (hibernate_amp()) {
		hibernate_hibdrv_buf = (void *)HIBERNATE_AMP_WORK;
		hibernate_hibdrv_size = HIBERNATE_AMP_WORK_SIZE;
	} else
#endif
	{
		hibernate_hibdrv_buf = hibernate_work;
		hibernate_hibdrv_size = hibernate_work_size;
	}
	if (hibernate_amp_maincpu()) {
		int ret;
		if ((ret = hibernate_load_drv(hibernate_hibdrv_size)) < 0) {
#ifndef HIBERNATE_WORK_ALLOC_INIT
			kfree(hibernate_work);
#endif
			return ret;
		}
		hibernate_hibdrv_size = HIBERNATE_DRV_COPY_SIZE(hibernate_hibdrv_buf);
	}
	hibernate_flush_icache_range((unsigned long)hibernate_hibdrv_buf,
							(unsigned long)hibernate_hibdrv_buf + hibernate_hibdrv_size);
	if (hibernate_amp() && hibernate_amp_maincpu()) {
		savetbl = hibernate_work;
		table_size = hibernate_work_size;
	} else {
		savetbl = hibernate_work + HIBERNATE_DRV_COPY_SIZE(hibernate_hibdrv_buf);
		table_size = hibernate_work_size - HIBERNATE_DRV_COPY_SIZE(hibernate_hibdrv_buf);
	}
#else
	savetbl = hibernate_work;
	table_size = hibernate_work_size;
#endif

	if (hibernate_param.switch_mode) {
		int ret;
#ifndef HIBERNATE_HIBDRV_FLOATING
		if (hibernate_param.switch_mode == 1) {
			hibernate_bootflag_buf = (void *)HIBERNATE_HIBDRV_VIRT +
				HIBERNATE_DRV_COPY_SIZE(HIBERNATE_HIBDRV_VIRT);
		} else
#endif
		{
			hibernate_bootflag_buf = (void *)savetbl;
			savetbl = (void *)savetbl + HIBERNATE_BF_LEN;
			table_size -= HIBERNATE_BF_LEN;
		}
		if ((ret = hibernate_load_bf()) < 0) {
#ifndef HIBERNATE_WORK_ALLOC_INIT
			kfree(hibernate_work);
#endif
			return ret;
		}
	}

	zonetbl_max = ZONETBL_DEFAULT_NUM;
	exttbl_max = EXTTBL_DEFAULT_NUM;
	dramtbl_max = (table_size - ZONETBL_DEFAULT_SIZE - EXTTBL_DEFAULT_SIZE) /
		sizeof(struct hibernate_savetbl);
	dramtbl = savetbl;
	exttbl = (void *)(dramtbl + dramtbl_max);
	zonetbl = (void *)(exttbl + EXTTBL_DEFAULT_NUM);
	hibernate_param.zonetbl_num = 0;
	hibernate_param.dramtbl_num = 0;
	hibernate_param.exttbl_num = 0;
	hibernate_work_start = page_to_pfn(virt_to_page(hibernate_work));
	hibernate_work_end = hibernate_work_start + ((hibernate_work_size - 1) >> PAGE_SHIFT) + 1;
	hibernate_nosave_area = (unsigned long)-1;
	hibernate_nosave_size = 0;
	hibernate_lowmem_nosave_area = (unsigned long)-1;
	hibernate_lowmem_nosave_size = 0;
	hibernate_param.maxarea = 0;
	hibernate_param.maxsize = 0;
	hibernate_param.lowmem_maxarea = 0;
	hibernate_param.lowmem_maxsize = 0;
	hibernate_save_pages = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12)
	{
		struct timeval tv;
		do_gettimeofday(&tv);
		hibernate_param.snapshot_id = tv.tv_sec;
	}
#else
	hibernate_param.snapshot_id = get_random_int();
#endif

	return 0;
}

static void hibernate_work_free(void)
{
#ifndef HIBERNATE_WORK_ALLOC_INIT
	if (hibernate_amp_maincpu())
		kfree(hibernate_work);
#endif
}

static void hibernate_set_tbl(u32 start, u32 end,
						 struct hibernate_savetbl *tbl, int *num)
{
	if (*num > 0 && start == tbl[*num - 1].end) {
		tbl[*num - 1].end = end;
	} else if (start < end) {
		tbl[*num].start = start;
		tbl[*num].end = end;
		(*num)++;
	}
}

static void hibernate_set_tbl64(u64 start, u64 end,
						   struct hibernate_savetbl64 *tbl, int *num)
{
	if (*num > 0 && start == tbl[*num - 1].end) {
		tbl[*num - 1].end = end;
	} else if (start < end) {
		tbl[*num].start = start;
		tbl[*num].end = end;
		(*num)++;
	}
}

int hibernate_set_savearea(u64 start, u64 end)
{
	struct hibernate_savetbl64 *tbl;

	if ((start | end) & 3) {
		printk("hibernate_set_savearea: unsupported alignment\n");
		return -EINVAL;
	}

	if (hibernate_param.exttbl_num >= exttbl_max) {
		if (hibernate_param.dramtbl_num + EXTTBL_DEFAULT_NUM * 2 > dramtbl_max) {
			printk("hibernate: save table overflow\n");
			return -ENOMEM;
		}
		tbl = exttbl - EXTTBL_DEFAULT_NUM;
		memmove(tbl, exttbl, exttbl_max * sizeof(struct hibernate_savetbl64));
		exttbl = tbl;
		exttbl_max += EXTTBL_DEFAULT_NUM;
		dramtbl_max -= EXTTBL_DEFAULT_NUM;
	}
	hibernate_set_tbl64(start, end, exttbl, &hibernate_param.exttbl_num);
	return 0;
}

static int hibernate_set_save_zones(unsigned long start, unsigned long end)
{
	struct hibernate_savetbl64 *tbl;

	if (hibernate_param.zonetbl_num >= zonetbl_max) {
		if (hibernate_param.dramtbl_num + ZONETBL_DEFAULT_NUM > dramtbl_max) {
			printk("hibernate: save table overflow\n");
			return -ENOMEM;
		}
		tbl = (void *)exttbl - ZONETBL_DEFAULT_SIZE;
		memmove(tbl, exttbl,
				exttbl_max * sizeof(struct hibernate_savetbl64) +
				zonetbl_max * sizeof(struct hibernate_savetbl));
		exttbl = tbl;
		zonetbl -= ZONETBL_DEFAULT_NUM;
		zonetbl_max += ZONETBL_DEFAULT_NUM;
		dramtbl_max -= EXTTBL_DEFAULT_NUM;
	}
	hibernate_set_tbl(start, end, zonetbl, &hibernate_param.zonetbl_num);
	return 0;
}

static int hibernate_set_save_zone(unsigned long pfn)
{
	return hibernate_set_save_zones(pfn, pfn + 1);
}

static int hibernate_set_save_drams(unsigned long start, unsigned long end)
{
	if (hibernate_param.dramtbl_num >= dramtbl_max) {
		printk("hibernate: save table overflow\n");
		return -ENOMEM;
	}
	hibernate_set_tbl(start, end, dramtbl, &hibernate_param.dramtbl_num);
	return 0;
}

static int hibernate_set_save_dram(unsigned long pfn)
{
	return hibernate_set_save_drams(pfn, pfn + 1);
}

static void hibernate_set_nosave_dram(struct zone *zone, unsigned long pfn)
{
	if (pfn == hibernate_nosave_area + hibernate_nosave_size) {
		hibernate_nosave_size++;
		if (hibernate_param.maxsize < hibernate_nosave_size) {
			hibernate_param.maxarea = hibernate_nosave_area;
			hibernate_param.maxsize = hibernate_nosave_size;
		}
	} else {
		hibernate_nosave_area = pfn;
		hibernate_nosave_size = 1;
	}
	if (!is_highmem(zone)) {
		if (pfn == hibernate_lowmem_nosave_area + hibernate_lowmem_nosave_size) {
			hibernate_lowmem_nosave_size++;
			if (hibernate_param.lowmem_maxsize < hibernate_lowmem_nosave_size) {
				hibernate_param.lowmem_maxarea = hibernate_lowmem_nosave_area;
				hibernate_param.lowmem_maxsize = hibernate_lowmem_nosave_size;
			}
		} else {
			hibernate_lowmem_nosave_area = pfn;
			hibernate_lowmem_nosave_size = 1;
		}
	}
}

static int hibernate_make_save_table(void)
{
	int ret;
	struct zone *zone;
	unsigned long pfn, end;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
	unsigned long pfn2;
#endif

	for_each_zone (zone) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
		mark_free_pages(zone);
#endif
		end = zone->zone_start_pfn + zone->spanned_pages;
		for (pfn = zone->zone_start_pfn; pfn < end; pfn++) {
			if (!hibernate_pfn_valid(pfn) ||
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
				swsusp_page_is_forbidden(pfn_to_page(pfn)) ||
#endif
				(pfn >= hibernate_work_start && pfn < hibernate_work_end))
				continue;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
			pfn2 = pfn;
			if (swsusp_page_is_saveable(zone, &pfn2)) {
				hibernate_save_pages++;
				if ((ret = hibernate_set_save_zone(pfn)) < 0)
					return ret;
				if ((ret = hibernate_set_save_dram(pfn)) < 0)
					return ret;
			} else {
				pfn--;
				while (pfn < pfn2) {
					if ((ret = hibernate_set_save_zone(++pfn)) < 0)
						return ret;
					hibernate_set_nosave_dram(zone, pfn);
				}
			}
#else
			if ((ret = hibernate_set_save_zone(pfn)) < 0)
				return ret;
			if (swsusp_page_is_saveable(zone, pfn)) {
				hibernate_save_pages++;
				if ((ret = hibernate_set_save_dram(pfn)) < 0)
					return ret;
			} else {
				hibernate_set_nosave_dram(zone, pfn);
			}
#endif
		}
	}
	return 0;
}

#ifdef HIBERNATE_AMP

static int hibernate_merge_savearea(struct hibernate *hibernate_param1)
{
	int i, ret;
	struct hibernate_savetbl *p;
	struct hibernate_savetbl64 *p64;

	p = (void *)hibernate_param1->zonetbl;
	for (i = 0; i < hibernate_param1->zonetbl_num; i++, p++) {
		if ((ret = hibernate_set_save_zones(p->start, p->end)) < 0)
			return ret;
	}
	p = (void *)hibernate_param1->dramtbl;
	for (i = 0; i < hibernate_param1->dramtbl_num; i++, p++) {
		if ((ret = hibernate_set_save_drams(p->start, p->end)) < 0)
			return ret;
	}
	p64 = (void *)hibernate_param1->exttbl;
	for (i = 0; i < hibernate_param1->exttbl_num; i++, p64++) {
		if ((ret = hibernate_set_savearea(p64->start, p64->end)) < 0)
			return ret;
	}

	return 0;
}

#endif

#ifdef HIBERNATE_PRINT_SAVETBL

static void hibernate_print_savetbl32(struct hibernate_savetbl *tbl, int num, char *name)
{
	int i;

	for (i = 0; i < num; i++, tbl++) {
		printk("%4s area %4d: %08x - %08x  len: %08x\n",
			   name, i, tbl->start, tbl->end, tbl->end - tbl->start);
	}
}

static void hibernate_print_savetbl64(struct hibernate_savetbl64 *tbl,
								 int num, char *name)
{
	int i;

	for (i = 0; i < num; i++, tbl++) {
		printk("%4s area %4d: %016llx - %016llx  len: %016llx\n", name, i,
			   (unsigned long long)tbl->start, (unsigned long long)tbl->end,
			   (unsigned long long)(tbl->end - tbl->start));
	}
}

static void hibernate_print_savetbl(void)
{
	hibernate_print_savetbl32(zonetbl, hibernate_param.zonetbl_num, "Zone");
	hibernate_print_savetbl32(dramtbl, hibernate_param.dramtbl_num, "Dram");
	hibernate_print_savetbl64(exttbl, hibernate_param.exttbl_num, "Ext");
}

#endif

#ifdef HIBERNATE_PRINT_MEMINFO

#define K(x) ((x) << (PAGE_SHIFT - 10))

static void hibernate_print_meminfo(void)
{
	struct sysinfo si;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
	struct page_state ps;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
	unsigned long active, inactive, free;
#endif
	unsigned long buffers, cached, dirty, mapped;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
	get_zone_counts(&active, &inactive, &free);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
	get_page_state(&ps);
	dirty = ps.nr_dirty;
	mapped = ps.nr_mapped;
	cached = get_page_cache_size();
#else
	dirty = global_page_state(NR_FILE_DIRTY);
	mapped = global_page_state(NR_FILE_MAPPED);
	cached = global_page_state(NR_FILE_PAGES);
#endif
	buffers = nr_blockdev_pages();
	cached -= TOTAL_SWAPCACHE_PAGES + buffers;
	si_swapinfo(&si);

	printk("Buffers        :%8lu KB\n"
			"Cached         :%8lu KB\n"
			"SwapCached     :%8lu KB\n"
			"SwapUsed       :%8lu KB\n",
			K(buffers),
			K(cached),
			K(TOTAL_SWAPCACHE_PAGES),
			K(si.totalswap - si.freeswap));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
	printk("Active         :%8lu KB\n"
			"Inactive       :%8lu KB\n",
			K(active),
			K(inactive));
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	printk("Active         :%8lu KB\n"
		   "Inactive       :%8lu KB\n",
		   K(global_page_state(NR_ACTIVE)),
		   K(global_page_state(NR_INACTIVE)));
#else
	printk("Active(anon)   :%8lu KB\n"
			"Inactive(anon) :%8lu KB\n"
			"Active(file)   :%8lu KB\n"
			"Inactive(file) :%8lu KB\n",
			K(global_page_state(NR_ACTIVE_ANON)),
			K(global_page_state(NR_INACTIVE_ANON)),
			K(global_page_state(NR_ACTIVE_FILE)),
			K(global_page_state(NR_INACTIVE_FILE)));
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
	printk("AnonPages      :%8lu KB\n",
			K(global_page_state(NR_ANON_PAGES)));
#endif
	printk("Dirty          :%8lu KB\n"
			"Mapped         :%8lu KB\n",
			K(dirty),
			K(mapped));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
	printk("Slab           :%8lu kB\n"
			"PageTables     :%8lu kB\n",
			K(ps.nr_slab),
			K(ps.nr_page_table_pages));
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
	printk("Slab           :%8lu kB\n"
			"PageTables     :%8lu kB\n",
			K(global_page_state(NR_SLAB)),
			K(global_page_state(NR_PAGETABLE)));
#else
	printk("SReclaimable   :%8lu kB\n"
			"SUnreclaim     :%8lu kB\n"
			"PageTables     :%8lu kB\n",
			K(global_page_state(NR_SLAB_RECLAIMABLE)),
			K(global_page_state(NR_SLAB_UNRECLAIMABLE)),
			K(global_page_state(NR_PAGETABLE)));
#endif
}

#else

#define hibernate_print_meminfo()

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)

static void invalidate_filesystems(int invalidate)
{
	struct super_block *sb;
	struct block_device *bdev;

	spin_lock(&sb_lock);
restart:
	list_for_each_entry(sb, &super_blocks, s_list) {
		if (sb->s_bdev) {
			sb->s_count++;
			spin_unlock(&sb_lock);
			fsync_bdev(sb->s_bdev);
			if (invalidate)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)
				__invalidate_device(sb->s_bdev);
#else
				__invalidate_device(sb->s_bdev, 0);
#endif
		} else {
			sb->s_count++;
			spin_unlock(&sb_lock);
			bdev = bdget(sb->s_dev);
			fsync_bdev(bdev);
			fsync_super(sb);
			if (invalidate) {
				shrink_dcache_sb(sb);
				invalidate_inodes(sb);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
				invalidate_bdev(bdev);
#else
				invalidate_bdev(bdev, 0);
#endif
			}
		}
		spin_lock(&sb_lock);
		if (__put_super_and_need_restart(sb))
			goto restart;
	}
	spin_unlock(&sb_lock);
}

#endif

static int hibernate_shrink_memory(void)
{
	int i, pages, pages_sum, threshold_cnt;
	int shrink_sav = hibernate_shrink;
	int repeat = HIBERNATE_SHRINK_REPEAT;

	if (!hibernate_swapout_disable) {
		hibernate_shrink = HIBERNATE_SHRINK_ALL;
		repeat = HIBERNATE_SHRINK_REPEAT_P1;
	}

	if (hibernate_shrink != HIBERNATE_SHRINK_NONE) {
		hibernate_print_meminfo();
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
		invalidate_filesystems(1);
#endif
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,18) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
		if (hibernate_shrink == HIBERNATE_SHRINK_LIMIT1)
			repeat = HIBERNATE_SHRINK_REPEAT2;
		else if (hibernate_shrink == HIBERNATE_SHRINK_LIMIT2)
			repeat = HIBERNATE_SHRINK_REPEAT3;
#endif
		printk("Shrinking memory...  ");
		pages_sum = 0;
		threshold_cnt = 0;
		for (i = 0; i < repeat; i++) {
			pages = shrink_all_memory(SHRINK_BITE);
			if (pages <= HIBERNATE_SHRINK_THRESHOLD) {
				if (++threshold_cnt >= HIBERNATE_SHRINK_THRESHOLD_COUNT)
					break;
			} else {
				threshold_cnt = 0;
			}
			pages_sum += pages;
		}
		printk("\bdone (%d pages freed)\n", pages_sum);
	}
	hibernate_print_meminfo();

	hibernate_shrink = shrink_sav;
	return 0;
}

int hibdrv_snapshot(void)
{
	int ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
	drain_local_pages(NULL);
#else
	drain_local_pages();
#endif
	if ((ret = hibernate_make_save_table()) < 0)
		return ret;

	printk("dram save %d pages\n", hibernate_save_pages);
	printk("maxarea 0x%08x(0x%08x)  lowmem_maxarea 0x%08x(0x%08x)\n",
		   hibernate_param.maxarea, hibernate_param.maxsize,
		   hibernate_param.lowmem_maxarea, hibernate_param.lowmem_maxsize);
	printk("zonetbl %d  exttbl %d  dramtbl %d\n", hibernate_param.zonetbl_num,
		   hibernate_param.exttbl_num, hibernate_param.dramtbl_num);

	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_SAVE);

#ifdef HIBERNATE_AMP
	if (hibernate_amp() && hibernate_amp_maincpu()) {
		struct hibernate *hibernate_param1 = (struct hibernate *)HIBERNATE_AMP_PARAM;
		if ((ret = hibernate_amp_wait_subcpu_saveend()) >= 0) {
			hibernate_invalidate_dcache_range(HIBERNATE_AMP_PARAM,
										 HIBERNATE_AMP_PARAM + sizeof(struct hibernate));
			hibernate_invalidate_dcache_range(HIBERNATE_AMP_WORK,
										 HIBERNATE_AMP_WORK + HIBERNATE_AMP_WORK_SIZE);
			hibernate_merge_savearea(hibernate_param1);
			hibernate_param.subcpu_info = hibernate_param1->subcpu_info;
		} else if (ret == -ECANCELED) {
			hibernate_canceled = 1;
		}
	}
#endif

	if (hibernate_canceled) {
		ret = -ECANCELED;
	} else {
#ifdef HIBERNATE_PRINT_SAVETBL
		hibernate_print_savetbl();
#endif
		hibernate_param.zonetbl = __pa(zonetbl);
		hibernate_param.dramtbl = __pa(dramtbl);
		hibernate_param.exttbl = __pa(exttbl);
		if (hibernate_param.switch_mode == 1) {
			ret = HIBERNATE_DRV_SWITCH(HIBERNATE_HIBDRV_VIRT, &hibernate_param);
		} else if (hibernate_param.switch_mode == 2) {
			if ((ret = HIBERNATE_DRV_SNAPSHOT(HIBERNATE_HIBDRV_VIRT, &hibernate_param)) < 0) {
				if (ret == -ECANCELED)
					hibernate_canceled = 1;
			} else if (hibernate_param.stat == 0) {
#ifndef HIBERNATE_HIBDRV_FLOATING
				memcpy((void *)HIBERNATE_HIBDRV_VIRT +
					   HIBERNATE_DRV_COPY_SIZE(HIBERNATE_HIBDRV_VIRT),
					   hibernate_bootflag_buf, HIBERNATE_BF_LEN);
#endif
				ret = HIBERNATE_DRV_SWITCH(HIBERNATE_HIBDRV_VIRT, &hibernate_param);
			}
		} else {
			if ((ret = HIBERNATE_DRV_SNAPSHOT(HIBERNATE_HIBDRV_VIRT,
										 &hibernate_param)) == -ECANCELED)
				hibernate_canceled = 1;
		}
		if (ret < 0) {
			if (ret == -EIO)
				printk("hibdrv: I/O error\n");
			else if (ret == -ENOMEM)
				printk("hibdrv: Out of memory\n");
			else if (ret == -ENODEV)
				printk("hibdrv: No such device\n");
			else if (ret == -EINVAL)
				printk("hibdrv: Invalid argument\n");
			else if (ret == -ENOSPC)
				printk("hibdrv: No space left on snapshot device\n");
			else if (ret == -ETIMEDOUT)
				printk("hibdrv: Device timed out\n");
			else if (ret == -ECANCELED)
				printk("hibdrv: Operation Canceled\n");
			else
				printk("hibdrv: error %d\n", ret);
		}
	}

#ifdef HIBERNATE_AMP
	if (ret >= 0 && hibernate_amp()) {
		if (hibernate_amp_maincpu()) {
			if (hibernate_param.stat == 0)
				hibernate_amp_send_maincpu_saveend(0);
			else
				hibernate_amp_boot_subcpu();
		} else {
			if (hibernate_param.stat == 0) {
				memcpy((void *)HIBERNATE_AMP_PARAM, &hibernate_param,
					   sizeof(struct hibernate));
				hibernate_clean_dcache_range(HIBERNATE_AMP_PARAM,
										HIBERNATE_AMP_PARAM + sizeof(struct hibernate));
				hibernate_clean_dcache_range(HIBERNATE_AMP_WORK,
										HIBERNATE_AMP_WORK + HIBERNATE_AMP_WORK_SIZE);
				hibernate_amp_send_subcpu_saveend(0);
				if ((ret = hibernate_amp_wait_maincpu_saveend()) == -ECANCELED)
					hibernate_canceled = 1;
			}
		}
	}
#endif

	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_SAVEEND);

	return ret;
}

void hibernate_save_cancel(void)
{
	if (pm_device_down == HIBERNATE_STATE_SUSPEND || hibernate_separate_pass == 1) {
		hibernate_canceled = 1;
		if (hibernate_ops->progress)
			hibernate_ops->progress(HIBERNATE_PROGRESS_CANCEL);
	}
}

int hibernate(void)
{
	int ret;
#ifdef HIBERNATE_UMOUNT_RW
	int i, no, ret2;
	char *mnt_devname[HIBERNATE_UMOUNT_RW_NUM];
	char *mnt_type[HIBERNATE_UMOUNT_RW_NUM];
	unsigned long s_flags[HIBERNATE_UMOUNT_RW_NUM];
	char *mnt_options[HIBERNATE_UMOUNT_RW_NUM];
	int mnt_flags;
	mm_segment_t fs;
	struct path path;
#endif

	if (!hibernate_ops) {
		printk("Snapshot driver not found.\n");
		return -EIO;
	}

	if (hibernate_param.switch_mode == 2 && hibernate_saveno == hibernate_loadno) {
		printk("Switch error!! Snapshot saveno and loadno is same.\n");
		return -EINVAL;
	}

	BUG_ON(!hibernate_ops->snapshot);

	hibernate_stat = 0;
	hibernate_retry = 0;
	pm_device_down = HIBERNATE_STATE_SUSPEND;

#ifdef HIBERNATE_AMP
	if (hibernate_amp() && !hibernate_amp_maincpu())
		hibernate_canceled = 0;
#endif

	if (hibernate_separate == 0) {
		hibernate_separate_pass = 0;
		hibernate_swapout_disable = 1;
	} else if (hibernate_separate == 1) {
		hibernate_separate_pass = 0;
		hibernate_swapout_disable = 0;
	} else if (hibernate_separate == 2) {
		if (hibernate_separate_pass != 1) {
			hibernate_separate_pass = 1;
			hibernate_swapout_disable = 0;
		} else {
			hibernate_separate_pass = 2;
			hibernate_swapout_disable = 1;
		}
	}

	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_INIT);

	if (hibernate_separate_pass == 2 && hibernate_canceled) {
		hibernate_separate_pass = 0;
		ret = -ECANCELED;
		goto hibernate_work_init_err;
	}

	if ((ret = hibernate_work_init()) < 0) {
		hibernate_separate_pass = 0;
		goto hibernate_work_init_err;
	}

#ifdef HIBERNATE_AMP
	if (hibernate_amp() && hibernate_amp_maincpu() && hibernate_separate_pass != 1)
		hibernate_amp_save_subcpu();
#endif

	if (hibernate_ops->drv_init && (ret = hibernate_ops->drv_init()) < 0) {
		hibernate_separate_pass = 0;
		goto hibernate_drv_init_err;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,24)
	system_state = SYSTEM_SUSPEND_DISK;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	lock_system_sleep();
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	mutex_lock(&pm_mutex);
#endif

#ifdef HIBERNATE_PREPARE_CONSOLE
	pm_prepare_console();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
	if ((ret = pm_notifier_call_chain(PM_HIBERNATION_PREPARE))) {
		hibernate_separate_pass = 0;
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto pm_notifier_call_chain_err;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(3,4,0)
	if ((ret = usermodehelper_disable())) {
		hibernate_separate_pass = 0;
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto usermodehelper_disable_err;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	/* Allocate memory management structures */
	if ((ret = create_basic_memory_bitmaps())) {
		hibernate_separate_pass = 0;
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto create_basic_memory_bitmaps_err;
	}
#endif

#ifdef HIBERNATE_ANDROID_MODE
	printk("inotify_hibernate_wait Start.\n");
	inotify_hibernate_wait();
	printk("inotify_hibernate_wait Exit.\n");
#endif

	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_SYNC);

	printk("Syncing filesystems ... ");
	sys_sync();
	printk("done.\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,21)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
	if ((ret = disable_nonboot_cpus())) {
		hibernate_separate_pass = 0;
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto disable_nonboot_cpus_err;
	}
#else
	disable_nonboot_cpus();
#endif
#endif

	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_FREEZE);

	printk("%d\n",__LINE__);
#ifdef HIBERNATE_UMOUNT_RW
	for (no = 0; no < HIBERNATE_UMOUNT_RW_NUM; no++) {
		mnt_devname[no] = NULL;
		mnt_type[no] = NULL;
		mnt_options[no] = NULL;
	}
	fs = get_fs();
	for (no = 0; no < HIBERNATE_UMOUNT_RW_NUM; no++) {
		set_fs(KERNEL_DS);
		ret = user_path_at(AT_FDCWD, hibernate_umount_rw[no], 0, &path);
		set_fs(fs);
		if (ret < 0) {
			printk("user_path_at %s error %d\n", hibernate_umount_rw[no], ret);
			goto user_path_at_err;
		}
		mnt_devname[no] = kstrdup(real_mount(path.mnt)->mnt_devname, GFP_KERNEL);
		if (!mnt_devname[no]) {
			printk("kstrdup %s error %d\n", real_mount(path.mnt)->mnt_devname, ret);
			path_put(&path);
			goto user_path_at_err;
		}
		mnt_type[no] = kstrdup(path.mnt->mnt_sb->s_type->name, GFP_KERNEL);
		if (!mnt_type[no]) {
			printk("kstrdup %s error %d\n",
				   path.mnt->mnt_sb->s_type->name, ret);
			path_put(&path);
			goto user_path_at_err;
		}
		s_flags[no] = path.mnt->mnt_sb->s_flags;
		mnt_flags = path.mnt->mnt_flags;
		if (mnt_flags & MNT_NOSUID)
			s_flags[no] |= MS_NOSUID;
		if (mnt_flags & MNT_NODEV)
			s_flags[no] |= MS_NODEV;
		if (mnt_flags & MNT_NOEXEC)
			s_flags[no] |= MS_NOEXEC;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
		if (mnt_flags & MNT_NODIRATIME)
			s_flags[no] |= MS_NODIRATIME;
		if (mnt_flags & MNT_NOATIME)
			s_flags[no] |= MS_NOATIME;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
		else if (!(mnt_flags & MNT_RELATIME))
			s_flags[no] |= MS_STRICTATIME;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
		if (mnt_flags & MNT_RELATIME)
			s_flags[no] |= MS_RELATIME;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
		if (mnt_flags & MNT_READONLY)
			s_flags[no] |= MS_RDONLY;
#endif
		if (path.mnt->mnt_sb->s_op->show_options) {
			struct seq_file m;
			memset(&m, 0, sizeof(m));
			m.size = PAGE_SIZE - 1;
			m.buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
			if (!m.buf) {
				printk("kzalloc %s options error %d\n",
					   hibernate_umount_rw[no], ret);
				path_put(&path);
				goto user_path_at_err;
			}
			mnt_options[no] = m.buf;
			  if (path.mnt->mnt_sb->s_op->show_options(&m, path.dentry)) {
				printk("show_options %s error %d\n", hibernate_umount_rw[no], ret);
				path_put(&path);
				goto user_path_at_err;
			}
		}
		path_put(&path);
	}
	for (i = 0; i < 100; i++) {
		if ((ret = freeze_user_processes())) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
			ret = -EBUSY;
#endif
			break;
		}

		set_fs(KERNEL_DS);
		for (no = HIBERNATE_UMOUNT_RW_NUM - 1; no >= 0; no--) {
		   if (mnt_devname[no]) {
				printk("umount %s ... ", hibernate_umount_rw[no]);
				ret = sys_umount((void *)hibernate_umount_rw[no], 0);
				if (ret == -EINVAL) {           /* none mount */
					kfree(mnt_devname[no]);
					mnt_devname[no] = NULL;
					ret = 0;
					printk("skip.\n");
				} else if (ret < 0) {
					if (ret != -EBUSY)
						printk("error %d\n", ret);
					else
						printk("busy.\n");
					break;
				} else {
					printk("done.\n");
				}
			}
		}
		set_fs(fs);
		if (ret >= 0)
			break;
		ret2 = 0;
		set_fs(KERNEL_DS);
		while (++no < HIBERNATE_UMOUNT_RW_NUM) {
			if (mnt_devname[no]) {
				if ((ret2 = sys_mount(mnt_devname[no],
									  (void *)hibernate_umount_rw[no],
									  mnt_type[no], s_flags[no],
									  mnt_options[no])) < 0) {
					printk("mount %s %s error %d\n",
						   mnt_devname[no], hibernate_umount_rw[no], ret);
					break;
				}
			}
		}
		set_fs(fs);
		if (ret != -EBUSY)
			break;
		if (ret2 < 0) {
			ret = ret2;
			break;
		}
		thaw_processes();
		msleep(10);
	}
	if (ret < 0) {
		hibernate_separate_pass = 0;
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto freeze_processes_err;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
	if ((ret = freeze_kernel_threads())) {
		 hibernate_separate_pass = 0;
		 pm_device_down = HIBERNATE_STATE_RESUME;
		 goto umount_rw_err;
	 }
#endif
#else
	if (freeze_processes()) {
		ret = -EBUSY;
		hibernate_separate_pass = 0;
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto freeze_processes_err;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(3,2,0)
	if ((ret = dpm_prepare(PMSG_FREEZE))) {
		hibernate_separate_pass = 0;
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto dpm_prepare_err;
	}
#endif

	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_SHRINK);

	if (hibernate_param.switch_mode != 1) {
		/* Free memory before shutting down devices. */
		if ((ret = hibernate_shrink_memory()) || hibernate_separate_pass == 1 ||
			hibernate_canceled) {
			pm_device_down = HIBERNATE_STATE_RESUME;
			goto hibernate_shrink_memory_err;
		}
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
	if ((ret = freeze_kernel_threads())) {
		hibernate_separate_pass = 0;
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto freeze_kernel_threads_err;
	}

	if ((ret = dpm_prepare(PMSG_FREEZE))) {
		hibernate_separate_pass = 0;
		pm_device_down = HIBERNATE_STATE_RESUME;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
		thaw_kernel_threads();
#endif
		goto dpm_prepare_err;
	}
#endif

	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_SUSPEND);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
	suspend_console();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	ftrace_stop();
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
	disable_nonboot_cpus();
#endif

	if (hibernate_ops->device_suspend_early &&
		(ret = hibernate_ops->device_suspend_early())) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto hibernate_device_suspend_early_err;
	}

	if ((ret = pm_device_suspend(STATE_FREEZE))) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto pm_device_suspend_err;
	}

	if (hibernate_ops->device_suspend && (ret = hibernate_ops->device_suspend())) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto hibernate_device_suspend_err;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,30)
	if ((ret = disable_nonboot_cpus())) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto disable_nonboot_cpus_err;
	}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
	if ((ret = arch_prepare_suspend())) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto arch_prepare_suspend_err;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	if ((ret = pm_device_power_down(STATE_FREEZE))) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto pm_device_power_down_err;
	}
#endif

	if (hibernate_ops->pre_snapshot && (ret = hibernate_ops->pre_snapshot())) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto hibernate_pre_snapshot_err;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	if ((ret = disable_nonboot_cpus())) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto disable_nonboot_cpus_err;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,30)
	device_pm_lock();
#endif

	local_irq_disable();

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
	if ((ret = pm_device_power_down(STATE_FREEZE))) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto pm_device_power_down_err;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(3,0,0)
	if ((ret = sysdev_suspend(STATE_FREEZE))) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto sysdev_suspend_err;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
	if ((ret = syscore_suspend())) {
		pm_device_down = HIBERNATE_STATE_RESUME;
		goto syscore_suspend_err;
	}
#endif

	save_processor_state();

	hibernate_param.bootflag_dev  = hibernate_savearea[hibernate_saveno].bootflag_dev;
	hibernate_param.bootflag_area = hibernate_savearea[hibernate_saveno].bootflag_area;
	hibernate_param.bootflag_size = hibernate_savearea[hibernate_saveno].bootflag_size;
	hibernate_param.snapshot_dev  = hibernate_savearea[hibernate_saveno].snapshot_dev;
	hibernate_param.snapshot_area = hibernate_savearea[hibernate_saveno].snapshot_area;
	hibernate_param.snapshot_size = hibernate_savearea[hibernate_saveno].snapshot_size;

#ifdef CONFIG_SWAP
	if (!hibernate_param.oneshot && total_swap_pages > NR_SWAP_PAGES)
		hibernate_swapout_disable = 1;
	else
		hibernate_swapout_disable = 0;
#endif
	if (HIBERNATE_ID(HIBERNATE_HIBDRV_VIRT) != HIBERNATE_ID_DRIVER) {
		printk("Can't find hibernation driver.\n");
		ret = -EIO;
	} else {
		if ((ret = hibernate_ops->snapshot()) == 0) {
			hibernate_stat = hibernate_param.stat;
			hibernate_retry = hibernate_param.retry;
			hibernate_saved = 1;
		}
	}

	pm_device_down = HIBERNATE_STATE_RESUME;
	restore_processor_state();

	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_RESUME);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
	syscore_resume();
syscore_suspend_err:
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(3,0,0)
	sysdev_resume();
sysdev_suspend_err:
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
	pm_device_power_up(STATE_RESTORE);
pm_device_power_down_err:
#endif

	local_irq_enable();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,30)
	device_pm_unlock();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
disable_nonboot_cpus_err:
	enable_nonboot_cpus();
#endif

	if (hibernate_ops->post_snapshot)
		hibernate_ops->post_snapshot();
hibernate_pre_snapshot_err:

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	pm_device_power_up(STATE_RESTORE);
pm_device_power_down_err:
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
arch_prepare_suspend_err:
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,30)
disable_nonboot_cpus_err:
	enable_nonboot_cpus();
#endif

	if (hibernate_ops->device_resume)
		hibernate_ops->device_resume();
hibernate_device_suspend_err:

#ifdef HIBERNATE_SUSPEND_ERR_RECOVER
pm_device_suspend_err:
#endif
	pm_device_resume(STATE_RESTORE);
#ifndef HIBERNATE_SUSPEND_ERR_RECOVER
pm_device_suspend_err:
#endif

	if (hibernate_ops->device_resume_late)
		hibernate_ops->device_resume_late();
hibernate_device_suspend_early_err:

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
	enable_nonboot_cpus();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	ftrace_start();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
	resume_console();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
dpm_prepare_err:
	dpm_complete(STATE_RESTORE);

freeze_kernel_threads_err:
#endif

hibernate_shrink_memory_err:

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(3,2,0)
dpm_prepare_err:
	dpm_complete(STATE_RESTORE);

#endif

#ifdef HIBERNATE_UMOUNT_RW

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
umount_rw_err:
#endif
	thaw_kernel_threads();
	fs = get_fs();
	for (no = 0; no < HIBERNATE_UMOUNT_RW_NUM; no++) {
		if (mnt_devname[no]) {
			set_fs(KERNEL_DS);
			if ((ret2 = sys_mount(mnt_devname[no], (void *)hibernate_umount_rw[no],
								  mnt_type[no], s_flags[no],
								  mnt_options[no])) < 0) {
				printk("mount %s error %d\n", hibernate_umount_rw[no], ret);
				if (ret >= 0)
					ret = ret2;
			}
			set_fs(fs);
		}
	}
#endif

freeze_processes_err:
	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_THAW);
	thaw_processes();

#ifdef HIBERNATE_UMOUNT_RW
user_path_at_err:
	for (no = 0; no < HIBERNATE_UMOUNT_RW_NUM; no++) {
		if (mnt_devname[no])
			kfree(mnt_devname[no]);
		if (mnt_type[no])
			kfree(mnt_type[no]);
		if (mnt_options[no])
			kfree(mnt_options[no]);
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,21)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
disable_nonboot_cpus_err:
#endif
	enable_nonboot_cpus();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	free_basic_memory_bitmaps();
create_basic_memory_bitmaps_err:
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(3,4,0)
	usermodehelper_enable();
usermodehelper_disable_err:
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
	pm_notifier_call_chain(PM_POST_HIBERNATION);
pm_notifier_call_chain_err:
#endif

#ifdef HIBERNATE_PREPARE_CONSOLE
	pm_restore_console();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	unlock_system_sleep();
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	mutex_unlock(&pm_mutex);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16) && \
	LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,24)
	system_state = SYSTEM_RUNNING;
#endif

	if (hibernate_ops->drv_uninit)
		hibernate_ops->drv_uninit();

#ifdef HIBERNATE_ANDROID_MODE
	printk("inotify_hibernate_restart Start.\n");
	inotify_hibernate_restart();
	printk("inotify_hibernate_restart Exit.\n");
#endif

hibernate_drv_init_err:

	hibernate_work_free();
hibernate_work_init_err:

	if (hibernate_separate_pass == 2)
		hibernate_separate_pass = 0;

	pm_device_down = HIBERNATE_STATE_NORMAL;

	if (hibernate_ops->progress)
		hibernate_ops->progress(HIBERNATE_PROGRESS_EXIT);

	if (hibernate_canceled)
		ret = -ECANCELED;
	hibernate_error = ret;
	if (ret < 0) {
#ifdef HIBERNATE_AMP
		if (hibernate_amp()) {
			if (hibernate_amp_maincpu())
				hibernate_amp_send_maincpu_saveend(ret);
			else
				hibernate_amp_send_subcpu_saveend(ret);
		}
#endif
		printk(KERN_ERR "Hibernate!! error %d\n", ret);
	}
	return ret;
}

#ifdef CONFIG_PROC_FS

static struct proc_dir_entry *proc_hibernate;
static struct proc_dir_entry *proc_hibernate_stat;
static struct proc_dir_entry *proc_hibernate_error;
static struct proc_dir_entry *proc_hibernate_retry;
static struct proc_dir_entry *proc_hibernate_saved;
static struct proc_dir_entry *proc_hibernate_canceled;
static struct proc_dir_entry *proc_hibernate_saveno;
static struct proc_dir_entry *proc_hibernate_loadno;
static struct proc_dir_entry *proc_hibernate_switch;
static struct proc_dir_entry *proc_hibernate_compress;
static struct proc_dir_entry *proc_hibernate_shrink;
static struct proc_dir_entry *proc_hibernate_separate;
static struct proc_dir_entry *proc_hibernate_oneshot;
static struct proc_dir_entry *proc_hibernate_halt;
static struct proc_dir_entry *proc_hibernate_silent;

static int read_proc_hibernate(char __user *buffer, size_t count,
						  loff_t *offset, int value)
{
	int len, pos;
	char buf[16];

	pos = *offset;
	if (pos < 0 || (ssize_t)count < 0)
		return -EIO;

	len = sprintf(buf, "%d\n", value) - pos;
	if (len <= 0)
		return 0;

	if (copy_to_user(buffer, buf + pos, len))
		return -EFAULT;

	*offset = pos + len;
	return len;
}

static int write_proc_hibernate(const char __user *buffer, size_t count,
						   loff_t *offset, int max, const char *str)
{
	int val;
	char buf[16];

	if (current_uid() != 0)
		return -EACCES;
	if (count == 0 || count >= 16)
		return -EINVAL;
	if (*offset != 0)
		return -EIO;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	sscanf(buf, "%d", &val);

	if (val > max) {
		printk("hibernate: %s too large !!\n", str);
		return -EINVAL;
	}
	return val;
}

#define PROC_READ(name, var)                                            \
static int read_proc_hibernate_##name(struct file *file, char __user *buffer,\
								 size_t count, loff_t *offset)          \
{                                                                       \
	return read_proc_hibernate(buffer, count, offset, var);                  \
}                                                                       \
																		\
static const struct file_operations proc_hibernate_##name##_fops =		\
{																		\
	.read  = read_proc_hibernate_##name,								\
};

#define PROC_RW(name, var, str, max, func)								\
static int read_proc_hibernate_##name(struct file *file, char __user *buffer,\
								 size_t count, loff_t *offset)			\
{																		\
	return read_proc_hibernate(buffer, count, offset, var);				\
}																		\
																		\
static int write_proc_hibernate_##name(struct file *file,				\
									const char __user *buffer,			\
									size_t count, loff_t *offset)			\
{																		\
	int val;															\
																		\
	if ((val = write_proc_hibernate(buffer, count, offset, max, str)) < 0)   \
		return val;														\
	var = val;															\
	func();																\
	return count;														\
}																		\
																		\
static const struct file_operations proc_hibernate_##name##_fops =		\
{																		\
	.read  = read_proc_hibernate_##name,								\
	.write = write_proc_hibernate_##name,								\
};

static inline void dummy(void)
{
}

static inline void separate_pass_init(void)
{
	hibernate_separate_pass = 0;
}

PROC_READ(stat, hibernate_stat)
PROC_READ(error, hibernate_error)
PROC_READ(retry, hibernate_retry)
PROC_READ(saved, hibernate_saved)
PROC_RW(canceled, hibernate_canceled, "canceled", 1, dummy)
PROC_RW(saveno, hibernate_saveno, "saveno", HIBERNATE_SAVEAREA_NUM - 1, dummy)
PROC_RW(loadno, hibernate_loadno, "loadno", HIBERNATE_SAVEAREA_NUM - 1, dummy)
PROC_RW(switch, hibernate_param.switch_mode, "switch", 2, dummy)
PROC_RW(compress, hibernate_param.compress, "compress", 2, dummy)
PROC_RW(shrink, hibernate_shrink, "shrink", 3, dummy)
PROC_RW(separate, hibernate_separate, "separate", 2, separate_pass_init)
PROC_RW(oneshot, hibernate_param.oneshot, "oneshot", 1, dummy)
PROC_RW(halt, hibernate_param.halt, "halt", 1, dummy)
PROC_RW(silent, hibernate_param.silent, "silent", 3, dummy)

#endif

int hibernate_register_machine(struct hibernate_ops *ops)
{
	hibernate_ops = ops;
	return 0;
}

int hibernate_unregister_machine(struct hibernate_ops *ops)
{
	hibernate_ops = NULL;
	return 0;
}

static inline struct proc_dir_entry *hibernate_proc_create(
	const char *name, int rw, const struct file_operations *proc_fops)
{
	static struct proc_dir_entry *p;
	int mode = rw ? S_IRUGO | S_IWUSR : S_IRUGO;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	if ((p = create_proc_entry(name, mode, proc_hibernate)))
		p->proc_fops = proc_fops;
#else
	p = proc_create(name, mode, proc_hibernate, proc_fops);
#endif
	return p;
}
static int null_add_part(int slot, struct cmdline_subpart *subpart, void *param)
{
	return 0;
}


unsigned int get_hibernate_phyaddr(char *region)
{
	char hibernate_tag[128];
	char *ptr;
	unsigned int phyaddr;
	int tag_len;
	int ret = -EINVAL;

	memset(hibernate_tag, 0x0, sizeof(hibernate_tag));
	tag_len = get_param_data(region, hibernate_tag, sizeof(hibernate_tag));
	if (tag_len <= 0)
		return 0;

#define EQUAL_MARK "="
	/* Skip the first "=" */
	ptr = strstr(hibernate_tag, EQUAL_MARK);
	if (!ptr)
		goto error;

	ptr += sizeof(EQUAL_MARK) - 1;
	if (ptr >= hibernate_tag + tag_len)
		goto error;

	phyaddr = simple_strtoul(ptr, NULL, 16);

	if (phyaddr & 0xfffff) {
		printk(KERN_ERR "hibernate drv start addr is not 1MB aligned : %x\n",
			   phyaddr);
		goto error;
	}

	return phyaddr;
error:
	printk(KERN_ERR "Invalid hibernate tag, errno :%d\n", ret);
#undef EQUAL_MARK
	return 0;

}

static int __init hibernate_init(void)
{
	struct cmdline_parts *parts;
	struct cmdline_subpart *subpart;
	char *cmdline;
	u32 hibernateflag_offset, hibernateflag_size, hibernatedata_offset, hibernatedata_size;
	int check_flag = 0;
	unsigned int phyaddr, userapi_phyaddr;
	char *p = NULL;
	char backup = 0;

    hibernateflag_offset = hibernatedata_offset = 0;
    hibernateflag_size = hibernatedata_size = 0;

	phyaddr = get_hibernate_phyaddr("wpaddr");
	if (!phyaddr) {
		printk(KERN_INFO "Hibernate get drv phyaddr error\n");
		return 0;
	}

	userapi_phyaddr = get_hibernate_phyaddr("userapi");
	if (!userapi_phyaddr) {
		printk(KERN_INFO "Hibernate get drv userapi_phyaddr error\n");
		return 0;
	}

	cmdline = strstr(saved_command_line, "blkdevparts");
	if (!cmdline) {
		printk(KERN_INFO "Hibernate parse cmdline error\n");
		return 0;
	}

	p = cmdline;
	while (*p != ' ' && *p != '\0') {
		p++;
		continue;
	}
	backup = *p;
	*p = '\0';

	if (cmdline_parts_parse(&parts, cmdline)) {
		*p = backup;
		printk(KERN_INFO "hibernate parse partition error\n");
		return 0;
	}

	*p = backup;
	cmdline_parts_set(parts, (sector_t) (~0ULL), 0, null_add_part, NULL);

	subpart = parts->subpart;
	while (subpart) {
		if (!strcmp(subpart->name, "qbflag")) {
			hibernateflag_offset = subpart->from;
			hibernateflag_size = subpart->size;
			check_flag++;
		} else if (!strcmp(subpart->name, "qbdata")) {
			hibernatedata_offset = subpart->from;
			hibernatedata_size = subpart->size;
			check_flag++;
		} else if (!strcmp(subpart->name, "hibdrv")) {
			check_flag++;
		} else if (!strcmp(subpart->name, "userapi")) {
			check_flag++;
		}
		subpart = subpart->next_subpart;
	}

	cmdline_parts_free(&parts);

	if (check_flag != 4) {
		printk(KERN_INFO "check hibernate partition failed\n");
		return 0;
	}

#ifdef CONFIG_PROC_FS
	if ((proc_hibernate = proc_mkdir("hibernate", NULL))) {
		proc_hibernate_stat = hibernate_proc_create("stat", 0, &proc_hibernate_stat_fops);
		proc_hibernate_error = hibernate_proc_create("error", 0, &proc_hibernate_error_fops);
		proc_hibernate_retry = hibernate_proc_create("retry", 0, &proc_hibernate_retry_fops);
		proc_hibernate_saved = hibernate_proc_create("saved", 0, &proc_hibernate_saved_fops);
		proc_hibernate_canceled =
			hibernate_proc_create("canceled", 1, &proc_hibernate_canceled_fops);
		proc_hibernate_saveno =
			hibernate_proc_create("saveno", 1, &proc_hibernate_saveno_fops);
		proc_hibernate_loadno =
			hibernate_proc_create("loadno", 1, &proc_hibernate_loadno_fops);
		proc_hibernate_switch =
			hibernate_proc_create("switch", 1, &proc_hibernate_switch_fops);
		proc_hibernate_compress =
			hibernate_proc_create("compress", 1, &proc_hibernate_compress_fops);
		proc_hibernate_shrink =
			hibernate_proc_create("shrink", 1, &proc_hibernate_shrink_fops);
		proc_hibernate_separate =
			hibernate_proc_create("separate", 1, &proc_hibernate_separate_fops);
		proc_hibernate_oneshot =
			hibernate_proc_create("oneshot", 1, &proc_hibernate_oneshot_fops);
		proc_hibernate_halt = hibernate_proc_create("halt", 1, &proc_hibernate_halt_fops);
		proc_hibernate_silent = hibernate_proc_create("silent", 1, &proc_hibernate_silent_fops);
	}
#endif

	hibernate_saveno = CONFIG_PM_HIBERNATE_SAVENO;
	hibernate_loadno = CONFIG_PM_HIBERNATE_LOADNO;
	hibernate_param.switch_mode = 0;
	hibernate_param.compress = CONFIG_PM_HIBERNATE_COMPRESS;
	hibernate_param.silent = CONFIG_PM_HIBERNATE_SILENT;
	hibernate_shrink = CONFIG_PM_HIBERNATE_SHRINK;
	hibernate_separate = CONFIG_PM_HIBERNATE_SEPARATE;
#ifdef CONFIG_PM_HIBERNATE_ONESHOT
	hibernate_param.oneshot = 1;
#else
	hibernate_param.oneshot = 0;
#endif
#ifdef CONFIG_PM_HIBERNATE_HALT
	hibernate_param.halt = 1;
#else
	hibernate_param.halt = 0;
#endif
	hibernate_param.ver_major = HIBERNATE_PARAM_VER_MAJOR;
	hibernate_param.ver_minor = HIBERNATE_PARAM_VER_MINOR;

#ifdef HIBERNATE_PRELOAD_EXTTBL
	hibernate_param.preload_exttbl = 1;
#else
	hibernate_param.preload_exttbl = 0;
#endif

	hibernate_param.v2p_offset = PAGE_OFFSET - __pa(PAGE_OFFSET);
#ifdef HIBERNATE_HIBDRV_FLOATING
	hibernate_param.text_v2p_offset = hibernate_param.v2p_offset;
#else
	hibernate_param.text_v2p_offset = HIBERNATE_HIBDRV_VIRT - phyaddr;
	hibernate_param.text_pfn = phyaddr >> PAGE_SHIFT;
	hibernate_param.text_size = HIBERNATE_HIBDRV_SIZE;
#endif
	hibernate_param.lowmem_size = (unsigned long)high_memory - PAGE_OFFSET;
	hibernate_param.page_shift = PAGE_SHIFT;

	hibernate_param.console = HIBERNATE_CONSOLE;
	hibernate_param.bps = HIBERNATE_BPS;

	hibernate_swapout_disable = 0;
	hibernate_separate_pass = 0;
	hibernate_canceled = 0;
	hibernate_saved = 0;
#if 1
	hibernate_savearea[0].bootflag_load_mode = HIBERNATE_LOAD_DEV;
	hibernate_savearea[0].bootflag_offset = 0x00000000;
	hibernate_savearea[0].bootflag_part = "qbflag";
	hibernate_savearea[0].bootflag_dev = HIBERNATE_DEV(USER, 0, 0);
	hibernate_savearea[0].bootflag_area = hibernateflag_offset ;
	hibernate_savearea[0].bootflag_size = hibernateflag_size;
	hibernate_savearea[0].snapshot_dev = HIBERNATE_DEV(USER, 0, 0);
	hibernate_savearea[0].snapshot_area = hibernatedata_offset ;
	hibernate_savearea[0].snapshot_size = hibernatedata_size ;
#endif

	printk(KERN_INFO "Hibernate drv partition: offset 0x%X, size 0x%X\n",
		   hibernateflag_offset, hibernateflag_size);
	printk(KERN_INFO "Hibernate snapshot partition: offset 0x%X, size 0x%X\n",
		   hibernatedata_offset, hibernatedata_size);

#ifdef HIBERNATE_WORK_ALLOC_INIT
	if (hibernate_amp_maincpu()) {
		int ret;
		if ((ret = hibernate_work_alloc()) < 0)
			return ret;
	}
#endif
	printk(KERN_INFO "Hisilicon Pm!! module loaded\n");

	return 0;
}

static void __exit hibernate_exit(void)
{
#ifdef HIBERNATE_WORK_ALLOC_INIT
	if (hibernate_amp_maincpu())
		kfree(hibernate_work);
#endif

#ifdef CONFIG_PROC_FS
	if (proc_hibernate_stat) {
		remove_proc_entry("stat", proc_hibernate_stat);
		proc_hibernate_stat = NULL;
	}
	if (proc_hibernate_error) {
		remove_proc_entry("error", proc_hibernate_error);
		proc_hibernate_error = NULL;
	}
	if (proc_hibernate_retry) {
		remove_proc_entry("retry", proc_hibernate_retry);
		proc_hibernate_retry = NULL;
	}
	if (proc_hibernate_saved) {
		remove_proc_entry("saved", proc_hibernate_saved);
		proc_hibernate_saved = NULL;
	}
	if (proc_hibernate_canceled) {
		remove_proc_entry("canceled", proc_hibernate_canceled);
		proc_hibernate_canceled = NULL;
	}
	if (proc_hibernate_saveno) {
		remove_proc_entry("saveno", proc_hibernate_saveno);
		proc_hibernate_saveno = NULL;
	}
	if (proc_hibernate_loadno) {
		remove_proc_entry("loadno", proc_hibernate_loadno);
		proc_hibernate_loadno = NULL;
	}
	if (proc_hibernate_switch) {
		remove_proc_entry("switch", proc_hibernate_switch);
		proc_hibernate_switch = NULL;
	}
	if (proc_hibernate_compress) {
		remove_proc_entry("compress", proc_hibernate_compress);
		proc_hibernate_compress = NULL;
	}
	if (proc_hibernate_shrink) {
		remove_proc_entry("shrink", proc_hibernate_shrink);
		proc_hibernate_shrink = NULL;
	}
	if (proc_hibernate_separate) {
		remove_proc_entry("separate", proc_hibernate_separate);
		proc_hibernate_separate = NULL;
	}
	if (proc_hibernate_oneshot) {
		remove_proc_entry("oneshot", proc_hibernate_oneshot);
		proc_hibernate_oneshot = NULL;
	}
	if (proc_hibernate_halt) {
		remove_proc_entry("halt", proc_hibernate_halt);
		proc_hibernate_halt = NULL;
	}
	if (proc_hibernate_silent) {
		remove_proc_entry("silent", proc_hibernate_silent);
		proc_hibernate_silent = NULL;
	}
	if (proc_hibernate) {
		remove_proc_entry("hibernate", proc_hibernate);
		proc_hibernate = NULL;
	}
#endif
}

module_init(hibernate_init);
module_exit(hibernate_exit);

MODULE_LICENSE("GPL");
