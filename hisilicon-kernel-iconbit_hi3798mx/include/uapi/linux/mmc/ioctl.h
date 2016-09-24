#ifndef LINUX_MMC_IOCTL_H
#define LINUX_MMC_IOCTL_H

#include <linux/types.h>

struct mmc_ioc_cmd {
	/* Implies direction of data.  true = write, false = read */
	int write_flag;

	/* Application-specific command.  true = precede with CMD55 */
	int is_acmd;

	__u32 opcode;
	__u32 arg;
	__u32 response[4];  /* CMD response */
	unsigned int flags;
	unsigned int blksz;
	unsigned int blocks;

	/*
	 * Sleep at least postsleep_min_us useconds, and at most
	 * postsleep_max_us useconds *after* issuing command.  Needed for
	 * some read commands for which cards have no other way of indicating
	 * they're ready for the next command (i.e. there is no equivalent of
	 * a "busy" indicator for read operations).
	 */
	unsigned int postsleep_min_us;
	unsigned int postsleep_max_us;

	/*
	 * Override driver-computed timeouts.  Note the difference in units!
	 */
	unsigned int data_timeout_ns;
	unsigned int cmd_timeout_ms;

	/*
	 * For 64-bit machines, the next member, ``__u64 data_ptr``, wants to
	 * be 8-byte aligned.  Make sure this struct is the same size when
	 * built for 32-bit.
	 */
	__u32 __pad;

	/* DAT buffer */
	__u64 data_ptr;
};
#define mmc_ioc_cmd_set_data(ic, ptr) ic.data_ptr = (__u64)(unsigned long) ptr

#define MMC_IOC_CMD _IOWR(MMC_BLOCK_MAJOR, 0, struct mmc_ioc_cmd)

/*
 * Since this ioctl is only meant to enhance (and not replace) normal access
 * to the mmc bus device, an upper data transfer limit of MMC_IOC_MAX_BYTES
 * is enforced per ioctl call.  For larger data transfers, use the normal
 * block device operations.
 */
#define MMC_IOC_MAX_BYTES  (512L * 256)

struct mmc_erase_cmd {
	unsigned int from; /* first sector to erase */
	unsigned int nr;   /* number of sectors to erase */

	/* erase command argument (SD supports only %MMC_ERASE_ARG) */
	unsigned int arg;
};
#define MMC_ERASE_ARG            0x00000000
#define MMC_SECURE_ERASE_ARG     0x80000000
#define MMC_TRIM_ARG             0x00000001
#define MMC_DISCARD_ARG          0x00000003
#define MMC_SECURE_TRIM1_ARG     0x80000001
#define MMC_SECURE_TRIM2_ARG     0x80008000
#define MMC_SECURE_ARGS          0x80000000
#define MMC_TRIM_ARGS            0x00008001

#define MMC_ERASE_CMD _IOW(MMC_BLOCK_MAJOR, 1, struct mmc_erase_cmd)

#endif /* LINUX_MMC_IOCTL_H */
