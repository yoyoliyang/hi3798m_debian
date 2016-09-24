#ifndef _ASM_HIBERNATE_H
#define _ASM_HIBERNATE_H

#include <linux/io.h>

#define HIBERNATE_HIBDRV_SIZE	0x00300000
#define USER_API_SIZE       0x00200000

#define HIBERNATE_HIBDRV_VIRT    (S40_IOCH1_VIRT - HIBERNATE_HIBDRV_SIZE - 0x100000)
#define USER_API_VIRT       (HIBERNATE_HIBDRV_VIRT - USER_API_SIZE - 0x100000)

#define HIBERNATE_CONSOLE	0
#define HIBERNATE_BPS	115200

#ifdef CONFIG_PM_HIBERNATE
#define HIBERNATE_ANDROID_MODE
#endif
#ifdef  HIBERNATE_ANDROID_MODE
#define HIBERNATE_UMOUNT_RW          "/cache", "/data",
#endif

#ifndef __ASSEMBLY__

int __hibernate_pfn_valid (unsigned long pfn);

#define hibernate_pfn_valid __hibernate_pfn_valid

#endif  /* __ASSEMBLY__ */

#define HIBERNATE_PFN_IS_NOSAVE
#endif /* _ASM_HIBERNATE_H */

