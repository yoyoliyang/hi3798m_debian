/******************************************************************************
 *    COPYRIGHT (C) 2013 Chwubin. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Chwubin 2014-05-29
 *
******************************************************************************/

#include <linux/module.h>
#include <linux/signal.h>
#include <linux/spinlock.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/kallsyms.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/seq_file.h>
#include <asm/atomic.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/traps.h>
#include <linux/semaphore.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/io.h>
#include <asm/pgalloc.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/statfs.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/namei.h>
#include <mach/cpu-info.h>
#include "sha1.h"

/*NB!: customer may be insert module with large size. 
In this case, you should increase the macro value */
#define MODULE_RESERVED_DDR_LENGTH                    0x600000	//size for runtime check area
#define MAX_BUFFER_LENGTH                             (0x20000)	//128k

#define SC_SYS_BASE                                   IO_ADDRESS(0xF8000000)
#define SC_SYSRES                                     (SC_SYS_BASE + 0x0004)
#define SC_GEN15                                      (SC_SYS_BASE + 0x00BC)	//This register is set by the fastboot, indicated the C51 code is loaded.

#define MCU_START_REG                                 0xf840f000
#define C51_CODE_LOAD_FLAG                            0x80510002
#define RUNTIME_CHECK_EN_REG_ADDR                     0xF8AB0084	//OTP:runtime_check_en indicator :0xF8AB0084[20]
#define MAX_IOMEM_SIZE                                0x400

#define C51_BASE                                      0xf8400000
#define C51_SIZE                                      0x10000
#define C51_DATA                                      0xe000

#define HI_REG_WRITE32(addr, val)                     (*(volatile unsigned int*)(addr) = (val))
#define HI_REG_READ32(addr, val)                      ((val) = *(volatile unsigned int*)(addr))

static struct task_struct *module_copy_thread = NULL;
static struct task_struct *fs_check_thread = NULL;
static unsigned int module_vir_addr = 0, module_phy_addr = 0;
static unsigned int c51_check_vector_vir_addr = 0;
static int runtime_check_init = false;
static int c51_code_loaded = false;
static int runtime_check_enable = false;

int module_copy_thread_proc(void *argv);
int fs_check_thread_proc(void *argv);
int get_kernel_info(unsigned int *start_addr, unsigned int *end_addr);
int calc_kernel_hash(unsigned int hash[5]);
int calc_fs_hash(unsigned int hash[5]);
int calc_ko_hash(unsigned int hash[5]);
int store_check_vector(void);
int get_bootargs_info(void);

extern long long get_chipid(void);

static int get_runtimecheck_enable_flag(int *runtime_check_flag)
{
	unsigned int *reg_vir_addr = NULL;

	if (NULL == runtime_check_flag)
		return -1;

	*runtime_check_flag = false;

	reg_vir_addr = (unsigned int *)ioremap_nocache(RUNTIME_CHECK_EN_REG_ADDR, 32);
	if (reg_vir_addr == NULL) {
		return -1;
	}

	if (*reg_vir_addr & 0x100000) {
		*runtime_check_flag = true;
	}
	iounmap((void *)reg_vir_addr);

	return 0;
}

int runtime_module_init(void)
{
	long long chipid;

	chipid = get_chipid();
	if (chipid == _HI3716CV200ES || chipid == _HI3716CV200
	    || chipid == _HI3719CV100 || chipid == _HI3719MV100A
	    || chipid == _HI3719MV100 || chipid == _HI3716MV400
	    || chipid == _HI3798CV100A || chipid == _HI3798CV100
	    || chipid == _HI3719CV100) {
		get_runtimecheck_enable_flag(&runtime_check_enable);
	} else {
		runtime_check_enable = false;
	}

	if (NULL == module_copy_thread) {
		module_copy_thread = kthread_create(module_copy_thread_proc, NULL,
				   "module_copy_thread");
		if (NULL == module_copy_thread) {
			return -1;
		}
		wake_up_process(module_copy_thread);
	}

	if (NULL == fs_check_thread) {
		fs_check_thread = kthread_create(fs_check_thread_proc, NULL,
				   "fs_check_thread");
		if (NULL == fs_check_thread) {
			return -1;
		}
		wake_up_process(fs_check_thread);
	}

	return 0;
}

int module_copy_thread_proc(void *argv)
{
	struct module *p = NULL;
	struct module *mod;
	static int is_first_copy = true;
	struct list_head *modules = NULL;
	int ret = 0;
	unsigned int addr;

	msleep(10000);

	printk("\n******** Runtime Check Initial ***********\n");

	(void)get_bootargs_info();
	HI_REG_READ32(SC_GEN15, c51_code_loaded);

	printk("runtime_check_enable=0x%x c51_code_loaded=0x%x module_phy_addr=0x%x\n\n",
	     runtime_check_enable, c51_code_loaded, module_phy_addr);

	if (c51_code_loaded != C51_CODE_LOAD_FLAG) {
		kthread_stop(fs_check_thread);	//step the fs check thread
		return -1;
	}
	runtime_check_init = true;

	if ((module_phy_addr == 0) || (!runtime_check_enable)) {	//runtime check is disable
		writel(0x1, IO_ADDRESS(MCU_START_REG));	//start MCU
		return 0;
	}

	module_vir_addr = (unsigned int)ioremap_nocache(module_phy_addr,
					  MODULE_RESERVED_DDR_LENGTH);
	memset((void *)module_vir_addr, 0x0, MODULE_RESERVED_DDR_LENGTH);

	module_get_pointer(&modules);
	while (1) {
		addr = module_vir_addr;
		list_for_each_entry_rcu(mod, modules, list) {
			p = find_module(mod->name);
			if (p) {
				memcpy((void *)addr, (void *)(p->module_core),
				       p->core_text_size);
				addr += p->core_text_size;
			}
		}

		if (is_first_copy) {
			store_check_vector();
			writel(0x1, IO_ADDRESS(MCU_START_REG));	//start MCU
			//writel(0x3,IO_ADDRESS(0xf8ab0000 + 0x184));//lpc_ram_wr_disable, lpc_rst_disable
			is_first_copy = false;
		}

		msleep(1000);
	}

	return ret;
}

int fs_check_thread_proc(void *argv)
{
	int ret = 0;
	unsigned int hash[5];
	unsigned int fs_ref_hash[5];
	unsigned int i;
	unsigned int *vir_addr = NULL;
	static int is_first_cal = true;

	while (!runtime_check_init) {
		msleep(10);
	}

	while (runtime_check_enable) {
		calc_fs_hash(hash);
		if (is_first_cal) {
			for (i = 0; i < 5; i++) {
				fs_ref_hash[i] = hash[i];
			}

			is_first_cal = false;
		}

		ret = memcmp(fs_ref_hash, hash, sizeof(hash));
		if (ret) {
			/* error process: reset the whole chipset */
			vir_addr = (void *)ioremap_nocache(SC_SYSRES, 0x100);
			*(unsigned int *)vir_addr = 0x1;
			iounmap((void *)vir_addr);
		}

		msleep(10000);
	}

	return 0;
}

int get_kernel_info(unsigned int *start_addr, unsigned int *end_addr)
{
	unsigned char *tmp_buf = NULL;
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	char kernel_text_seg_name[32];
	char *pstr = NULL;
	long long chipid;

	chipid = get_chipid();
	if (chipid == _HI3716CV200ES || chipid == _HI3716CV200
	    || chipid == _HI3719CV100 || chipid == _HI3719MV100A
	    || chipid == _HI3719MV100 || chipid == _HI3716MV400
	    || chipid == _HI3798CV100A || chipid == _HI3798CV100
	    || chipid == _HI3719CV100) {
		memset(kernel_text_seg_name, 0, sizeof(kernel_text_seg_name));
		strncpy(kernel_text_seg_name, "Kernel code", strlen("Kernel code"));
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	set_fs(fs);

	/* get file handle */
	fp = filp_open("/proc/iomem", O_RDONLY | O_LARGEFILE, 0644);
	if (IS_ERR(fp))
		return -1;

	tmp_buf = kmalloc(MAX_IOMEM_SIZE, GFP_TEMPORARY);
	if (tmp_buf == NULL) {
		filp_close(fp, NULL);
		return -1;
	}
	memset(tmp_buf, 0, MAX_IOMEM_SIZE);

	/* get file content */
	pos = 0;
	fs = get_fs();
	set_fs(KERNEL_DS);
	vfs_read(fp, tmp_buf, MAX_IOMEM_SIZE, &pos);
	set_fs(fs);

	pstr = strstr(tmp_buf, kernel_text_seg_name);
	if (pstr == NULL) {
		kfree(tmp_buf);
		filp_close(fp, NULL);
		return -1;
	}
	pos = pstr - (char *)tmp_buf;

	memset(kernel_text_seg_name, 0, sizeof(kernel_text_seg_name));
	memcpy(kernel_text_seg_name, tmp_buf + pos - 20, 8);
	*start_addr = simple_strtoul(kernel_text_seg_name, 0, 16);

	memset(kernel_text_seg_name, 0, sizeof(kernel_text_seg_name));
	memcpy(kernel_text_seg_name, tmp_buf + pos - 11, 8);
	*end_addr = simple_strtoul(kernel_text_seg_name, 0, 16);

	kfree(tmp_buf);
	/* close file handle */
	filp_close(fp, NULL);

	return 0;
}

int calc_kernel_hash(unsigned int hash_output[5])
{
	unsigned int tmp_value;
	unsigned int kernel_start_addr, kernel_end_addr, kernel_size;
	int ret = 0;
	sha1_context ctx;
	unsigned char hash[20];
	unsigned int kernel_vir_addr;
	unsigned int i;
	unsigned int one_time_len;
	unsigned int left_len;
	unsigned int offset = 0;

	ret = get_kernel_info(&kernel_start_addr, &kernel_end_addr);
	if (ret != 0)
		return -1;

	kernel_size = kernel_end_addr - kernel_start_addr;
	kernel_vir_addr = (unsigned int)phys_to_virt(kernel_start_addr);

	hi_sha1_starts(&ctx);

	left_len = kernel_size;
	while (left_len > 0) {
		one_time_len = left_len > MAX_BUFFER_LENGTH ? MAX_BUFFER_LENGTH : left_len;
		hi_sha1_update(&ctx, (unsigned char *)kernel_vir_addr + offset, one_time_len);
		left_len -= one_time_len;
		offset += one_time_len;
		msleep(10);
	}

	hi_sha1_finish(&ctx, hash);

	for (i = 0; i < 20;) {
		tmp_value = ((unsigned int)hash[i++]) << 24;
		tmp_value |= ((unsigned int)hash[i++]) << 16;
		tmp_value |= ((unsigned int)hash[i++]) << 8;
		tmp_value |= ((unsigned int)hash[i++]);
		hash_output[i / 4 - 1] = tmp_value;
	}

	return 0;
}

int calc_ko_hash(unsigned int hash_output[5])
{
	unsigned int tmp_value;
	sha1_context ctx;
	unsigned char hash[20];
	unsigned int i;
	unsigned int one_time_len;
	unsigned int left_len;
	unsigned int offset = 0;

	hi_sha1_starts(&ctx);
	left_len = MODULE_RESERVED_DDR_LENGTH;
	while (left_len > 0) {
		one_time_len = left_len > MAX_BUFFER_LENGTH ? MAX_BUFFER_LENGTH : left_len;
		hi_sha1_update(&ctx, (unsigned char *)module_vir_addr + offset, one_time_len);
		left_len -= one_time_len;
		offset += one_time_len;
		msleep(10);
	}
	hi_sha1_finish(&ctx, hash);

	for (i = 0; i < 20;) {
		tmp_value = ((unsigned int)hash[i++]) << 24;
		tmp_value |= ((unsigned int)hash[i++]) << 16;
		tmp_value |= ((unsigned int)hash[i++]) << 8;
		tmp_value |= ((unsigned int)hash[i++]);

		hash_output[i / 4 - 1] = tmp_value;
	}

	return 0;
}

int calc_fs_hash(unsigned int hash_output[5])
{
	int ret = 0;
	unsigned char hash[20];
	unsigned int i;
	unsigned int tmp_value;
	static unsigned char *tmp_buf = NULL;
	sha1_context ctx;
	struct file *fp;
	mm_segment_t fs;
	struct path path;
	struct kstatfs st;
	loff_t pos;
	unsigned int read_len;
	unsigned int left_len;

	fs = get_fs();
	set_fs(KERNEL_DS);
	set_fs(fs);

	ret = user_path("/", &path);
	if (ret == 0) {
		ret = vfs_statfs(&path, &st);
		path_put(&path);
	}

	/* get file handle */
	fp = filp_open("/dev/ram0", O_RDONLY | O_LARGEFILE, 0644);
	if (IS_ERR(fp))
		return -1;

	if (tmp_buf == NULL) {
		tmp_buf = vmalloc(MAX_BUFFER_LENGTH);
		if (tmp_buf == NULL) {
			filp_close(fp, NULL);
			return -1;
		}
	}

	/* get file content */
	pos = 0;
	left_len = st.f_bsize * st.f_blocks;
	hi_sha1_starts(&ctx);
	while (left_len > 0) {
		read_len = left_len >= MAX_BUFFER_LENGTH ? MAX_BUFFER_LENGTH : left_len;
		left_len -= read_len;
		fs = get_fs();
		set_fs(KERNEL_DS);
		vfs_read(fp, tmp_buf, read_len, &pos);
		set_fs(fs);
		hi_sha1_update(&ctx, tmp_buf, read_len);
		msleep(100);
	}
	hi_sha1_finish(&ctx, hash);

	for (i = 0; i < 20;) {
		tmp_value = ((unsigned int)hash[i++]) << 24;
		tmp_value |= ((unsigned int)hash[i++]) << 16;
		tmp_value |= ((unsigned int)hash[i++]) << 8;
		tmp_value |= ((unsigned int)hash[i++]);

		hash_output[i / 4 - 1] = tmp_value;
	}

	//vfree(tmp_buf);
	/* close file handle */
	filp_close(fp, NULL);

	return 0;
}

int store_check_vector(void)
{
	unsigned int hash[5];
	unsigned int *ptr = NULL;
	unsigned int i;
	unsigned int sec_num = 2;	//kernel, KO
	unsigned int kernel_start_addr, kernel_end_addr, kernel_size;

	c51_check_vector_vir_addr = (unsigned int)ioremap_nocache(C51_BASE + C51_DATA + 0xE00,
					  C51_SIZE - C51_DATA - 0xE00);
	memset((void *)c51_check_vector_vir_addr, 0x0, C51_SIZE - C51_DATA - 0xE00);

	//kernel hash value
	calc_kernel_hash(hash);
	for (i = 0; i < 5; i++) {
		ptr = (unsigned int *)(((unsigned char *)c51_check_vector_vir_addr) + 0x14) + i;
		HI_REG_WRITE32(ptr, hash[i]);
	}

	//ko hash value
	calc_ko_hash(hash);
	for (i = 0; i < 5; i++) {
		ptr = (unsigned int *)(((unsigned char *)c51_check_vector_vir_addr) + 0x28) + i;
		HI_REG_WRITE32(ptr, hash[i]);
	}

	//check section number, address, length
	ptr = (unsigned int *)(((unsigned char *)c51_check_vector_vir_addr) + 0x50);
	HI_REG_WRITE32(ptr, sec_num);

	//kernel address, length
	get_kernel_info(&kernel_start_addr, &kernel_end_addr);
	kernel_size = kernel_end_addr - kernel_start_addr;
	ptr = (unsigned int *)(((unsigned char *)c51_check_vector_vir_addr) + 0x54);
	HI_REG_WRITE32(ptr, kernel_start_addr);

	ptr = (unsigned int *)(((unsigned char *)c51_check_vector_vir_addr) + 0x58);
	HI_REG_WRITE32(ptr, kernel_size);

	//KO address, length
	ptr = (unsigned int *)(((unsigned char *)c51_check_vector_vir_addr) + 0x5c);
	HI_REG_WRITE32(ptr, module_phy_addr);

	ptr = (unsigned int *)(((unsigned char *)c51_check_vector_vir_addr) + 0x60);
	HI_REG_WRITE32(ptr, MODULE_RESERVED_DDR_LENGTH);

	iounmap((void *)c51_check_vector_vir_addr);

	return 0;
}

int get_bootargs_info(void)
{
	unsigned char *tmp_buf = NULL;
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	char mmz_start_addr_str[16];
	unsigned int mmz_start_addr;
	char *pstr = NULL;

	fs = get_fs();
	set_fs(KERNEL_DS);
	set_fs(fs);

	/* get file handle */
	fp = filp_open("/proc/cmdline", O_RDONLY | O_LARGEFILE, 0644);
	if (IS_ERR(fp))
		return -1;

	tmp_buf = vmalloc(MAX_IOMEM_SIZE);
	if (tmp_buf == NULL) {
		filp_close(fp, NULL);
		return -1;
	}
	memset(tmp_buf, 0, MAX_IOMEM_SIZE);

	/* get file content */
	pos = 0;
	fs = get_fs();
	set_fs(KERNEL_DS);
	vfs_read(fp, tmp_buf, MAX_IOMEM_SIZE, &pos);
	set_fs(fs);

	pstr = strstr(tmp_buf, "mem=");
	if (pstr == NULL) {
		kfree(tmp_buf);
		filp_close(fp, NULL);
		return -1;
	}
	pos = pstr - (char *)tmp_buf;

	memset(mmz_start_addr_str, 0, sizeof(mmz_start_addr_str));
	memcpy(mmz_start_addr_str, tmp_buf + pos + 4, 10);
	mmz_start_addr = simple_strtoul(mmz_start_addr_str, 0, 10);
	module_phy_addr = mmz_start_addr * 1024 * 1024;

	vfree(tmp_buf);
	/* close file handle */
	filp_close(fp, NULL);

	return 0;
}

void runtime_module_exit(void)
{
	if (NULL != module_copy_thread) {
		kthread_stop(module_copy_thread);
	}

	if (NULL != fs_check_thread) {
		kthread_stop(fs_check_thread);
	}

	if (module_vir_addr != NULL) {
		iounmap((void *)module_vir_addr);
	}

	if (c51_check_vector_vir_addr != NULL) {
		iounmap((void *)c51_check_vector_vir_addr);
	}
}

module_init(runtime_module_init);
module_exit(runtime_module_exit);

MODULE_AUTHOR("HISILICON");
MODULE_LICENSE("GPL");
