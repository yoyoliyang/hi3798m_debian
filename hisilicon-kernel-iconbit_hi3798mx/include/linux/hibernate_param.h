#ifndef _LINUX_HIBERNATE_PARAM_H
#define _LINUX_HIBERNATE_PARAM_H

#include <linux/hibernate.h>
#include <asm/hibernate.h>

#define HIBERNATE_PARAM_VER_MAJOR    0x0003
#define HIBERNATE_PARAM_VER_MINOR    0x0001

#define HIBERNATE_HEADER_ID          0x00
#define HIBERNATE_HEADER_COPY_SIZE   0x04
#define HIBERNATE_HEADER_DRV_SIZE    0x08
#define HIBERNATE_HEADER_VERSION     0x0c
#define HIBERNATE_HEADER_SNAPSHOT    0x20
#define HIBERNATE_HEADER_HIBERNATE   0x28
#define HIBERNATE_HEADER_SWITCH      0x30

#define HIBERNATE_ID_DRIVER          0x44483457      /* W4HD */
#define HIBERNATE_ID_BOOTFLAG        0x46423457      /* W4BF */

#define HIBERNATE_PART_SHIFT         0
#define HIBERNATE_LUN_SHIFT          8
#define HIBERNATE_DEV_SHIFT          16

#define HIBERNATE_PART_MASK          (0xff << HIBERNATE_PART_SHIFT)
#define HIBERNATE_LUN_MASK           (0xff << HIBERNATE_LUN_SHIFT)
#define HIBERNATE_DEV_MASK           (0xff << HIBERNATE_DEV_SHIFT)

#define HIBERNATE_DEV_NOR            (0x01 << HIBERNATE_DEV_SHIFT)
#define HIBERNATE_DEV_NAND           (0x02 << HIBERNATE_DEV_SHIFT)
#define HIBERNATE_DEV_ATA            (0x03 << HIBERNATE_DEV_SHIFT)
#define HIBERNATE_DEV_SD             (0x04 << HIBERNATE_DEV_SHIFT)
#define HIBERNATE_DEV_MEM            (0x05 << HIBERNATE_DEV_SHIFT)
#define HIBERNATE_DEV_USER           (0x7e << HIBERNATE_DEV_SHIFT)
#define HIBERNATE_DEV_EXT            (0x7f << HIBERNATE_DEV_SHIFT)

#define HIBERNATE_DEV(dev, lun, part)        (HIBERNATE_DEV_##dev | \
										 ((lun) << HIBERNATE_LUN_SHIFT) | \
										 ((part) << HIBERNATE_PART_SHIFT))

#ifndef HIBERNATE_LUN_CONV
#define HIBERNATE_LUN_CONV(dev)      (dev)
#endif

#define HIBERNATE_DEV_TO_LUN(dev)    HIBERNATE_LUN_CONV(((dev) & HIBERNATE_LUN_MASK) >> \
											  HIBERNATE_LUN_SHIFT)
#define HIBERNATE_DEV_TO_PART(dev)   (((dev) & HIBERNATE_PART_MASK) >> HIBERNATE_PART_SHIFT)

#define HIBERNATE_LOAD_MEM           0
#define HIBERNATE_LOAD_DEV           1
#define HIBERNATE_LOAD_MTD_NO        2
#define HIBERNATE_LOAD_MTD_NAME      3

#define HIBERNATE_BF_LEN             0x100

#ifdef HIBERNATE_HIBDRV_FLOATING
#define HIBERNATE_HIBDRV_VIRT        hibernate_hibdrv_buf
#endif



#ifndef __ASSEMBLY__

#if BITS_PER_LONG == 64
#define ptr_to_u64(x)           ((u64)(x))
#define u64_to_ptr(x)           ((void *)(x))
#else
#define ptr_to_u64(x)           ((u64)(u32)(x))
#define u64_to_ptr(x)           ((void *)(u32)(x))
#endif

enum hibernate_progress {
	HIBERNATE_PROGRESS_INIT,
	HIBERNATE_PROGRESS_SYNC,
	HIBERNATE_PROGRESS_FREEZE,
	HIBERNATE_PROGRESS_SHRINK,
	HIBERNATE_PROGRESS_SUSPEND,
	HIBERNATE_PROGRESS_SAVE,
	HIBERNATE_PROGRESS_SAVEEND,
	HIBERNATE_PROGRESS_RESUME,
	HIBERNATE_PROGRESS_THAW,
	HIBERNATE_PROGRESS_EXIT,
	HIBERNATE_PROGRESS_CANCEL,
};

struct hibernate_savetbl {
	u32         start;
	u32         end;
};

struct hibernate_savetbl64 {
	u64         start;
	u64         end;
};

struct hibernate {
	u16         ver_major;
	u16         ver_minor;
	s16         page_shift;
	s16         switch_mode;
	s16         compress;
	s16         oneshot;
	s16         halt;
	s16         silent;
	s32         console;
	s32         bps;
	u32         bootflag_dev;
	u32         bootflag_area;
	u32         bootflag_size;
	u32         snapshot_dev;
	u32         snapshot_area;
	u32         snapshot_size;
	u64         v2p_offset;
	u64         hd_savearea;
	u64         hd_savearea_end;
	u64         subcpu_info;
	u64         zonetbl;
	u64         dramtbl;
	u64         exttbl;
	s32         zonetbl_num;
	s32         dramtbl_num;
	s32         exttbl_num;
	s32         preload_exttbl;
	u32         lowmem_size;
	u32         maxarea;
	u32         maxsize;
	u32         lowmem_maxarea;
	u32         lowmem_maxsize;
	u32         snapshot_id;
	u32         private[4];
	u64         text_v2p_offset;
	u32         text_pfn;
	u32         text_size;
	u32         reserve[4];
	u32         stat;
	u32         retry;
};

struct hibernate_savearea {
	int         bootflag_load_mode;
	u32         bootflag_offset;
	char        *bootflag_part;
	u32         bootflag_dev;
	u32         bootflag_area;
	u32         bootflag_size;
	u32         snapshot_dev;
	u32         snapshot_area;
	u32         snapshot_size;
};

struct hibernate_ops {
	int (*drv_load)(void *buf, size_t size);
	int (*bf_load)(void *buf, int loadno);
	int (*drv_init)(void);
	int (*device_suspend_early)(void);
	int (*device_suspend)(void);
	int (*pre_snapshot)(void);
	int (*snapshot)(void);
	void (*post_snapshot)(void);
	void (*device_resume)(void);
	void (*device_resume_late)(void);
	void (*drv_uninit)(void);
	void (*putc)(char c);
	void (*progress)(int val);
};

int swsusp_page_is_saveable(struct zone *zone, unsigned long pfn);

int hibdrv_snapshot(void);
int hibernate_register_machine(struct hibernate_ops *ops);
int hibernate_unregister_machine(struct hibernate_ops *ops);

#ifdef CONFIG_MTD
int hibernate_mtd_load(int mtdno, void *buf, size_t size);
int hibernate_mtd_load_nm(const char *mtdname, void *buf, size_t size);
int hibernate_mtd_bf_load(int mtdno, void *buf, loff_t offs, size_t size);
int hibernate_mtd_bf_load_nm(const char *mtdname, void *buf,
						loff_t offs, size_t size);
#endif

#ifdef HIBERNATE_HIBDRV_DEV_LOAD
int hibernate_dev_load(const char *dev, void *buf, size_t size);
#endif

int hibernate_dev_bf_load(const char *dev, void *buf, loff_t offs, size_t size);

extern struct hibernate hibernate_param;

extern struct hibernate_savearea hibernate_savearea[];

extern unsigned char *hibernate_hibdrv_buf;
extern unsigned char *hibernate_bootflag_buf;
extern unsigned long userapi_addr;

#ifdef HIBERNATE_DEBUG_NO_HIBDRV

#define HIBERNATE_ID(drv)                    HIBERNATE_ID_DRIVER
#define HIBERNATE_DRV_COPY_SIZE(drv)         0x00008000
#define _HIBERNATE_DRV_SNAPSHOT(drv, x)      0
#define _HIBERNATE_DRV_SWITCH(drv, x)        0

#else   /* HIBERNATE_DEBUG_NO_HIBDRV */

#define HIBERNATE_ID(drv)            (*(u32 *)((void *)(drv) + HIBERNATE_HEADER_ID))
#define HIBERNATE_DRV_COPY_SIZE(drv) \
	(*(u32 *)((void *)(drv) + HIBERNATE_HEADER_COPY_SIZE))

#define _HIBERNATE_DRV_SNAPSHOT(drv, x) \
	((int (*)(void *))((void *)(drv) + HIBERNATE_HEADER_SNAPSHOT))(x)
#define _HIBERNATE_DRV_SWITCH(drv, x) \
	((int (*)(void *))((void *)(drv) + HIBERNATE_HEADER_SWITCH))(x)

#endif  /* HIBERNATE_DEBUG_NO_HIBDRV */

#ifndef HIBERNATE_DRV_SNAPSHOT
#define HIBERNATE_DRV_SNAPSHOT(drv, x)       _HIBERNATE_DRV_SNAPSHOT(drv, x)
#endif

#ifndef HIBERNATE_DRV_SWITCH
#define HIBERNATE_DRV_SWITCH(drv, x)         _HIBERNATE_DRV_SWITCH(drv, x)
#endif

#endif  /* __ASSEMBLY__ */

#endif  /* _LINUX_HIBERNATE_PARAM_H */
