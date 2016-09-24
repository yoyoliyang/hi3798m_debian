/* linux/arch/arm/mach-godnet/platsmp.c
 *
 * clone form linux/arch/arm/mach-realview/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/unified.h>
#include <asm/smp_scu.h>
#include <mach/early-debug.h>
#include <asm/smp_plat.h>

#include "platsmp.h"
#include "hotplug.h"

extern unsigned long scureg_base;

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
//int __cpuinitdata pen_release = -1;

/* copy startup code to sram, and flash cache. */
static void prepare_slave_cores_boot(unsigned int start_addr, /* slave start phy address */
				     unsigned int jump_addr)  /* slave jump phy address */
{
	unsigned int *virtaddr;
	unsigned int *p_virtaddr;

	p_virtaddr = virtaddr = ioremap(start_addr, PAGE_SIZE);

	*p_virtaddr++ = 0xe51ff004; /* ldr  pc, [pc, #-4] */
	*p_virtaddr++ = jump_addr;  /* pc jump phy address */

	smp_wmb();
	__cpuc_flush_dcache_area((void *)virtaddr,
		(size_t)((char *)p_virtaddr - (char *)virtaddr));
	outer_clean_range(__pa(virtaddr), __pa(p_virtaddr));

	iounmap(virtaddr);
}

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static DEFINE_SPINLOCK(boot_lock);

/* relase pen and then the slave core run into our world */
static int __cpuinit hi3798mx_boot_secondary(unsigned int cpu,
					struct task_struct *idle)
{
	unsigned long timeout;

	prepare_slave_cores_boot(0xFFFF0000,
		(unsigned int)virt_to_phys(hi3798mx_secondary_startup));

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	slave_cores_power_up(cpu);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	timeout = jiffies + (5 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;
}
/*****************************************************************************/

static void __cpuinit hi3798mx_secondary_init(unsigned int cpu)
{
	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}
/*****************************************************************************/

static void __init hi3798mx_smp_init_cpus(void)
{
	unsigned int i, ncores, l2ctlr;

	asm volatile("mrc p15, 1, %0, c9, c0, 2\n" : "=r" (l2ctlr));
	ncores = ((l2ctlr >> 24) & 0x3) + 1;

	/* sanity check */
	if (ncores > NR_CPUS) {
		printk(KERN_WARNING
		       "Realview: no. of cores (%d) greater than configured "
		       "maximum of %d - clipping\n",
		       ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);
}

void slave_cores_power_up(int cpu)
{
	unsigned int regval, regval_bak;

	printk(KERN_DEBUG "CPU%u: powerup\n", cpu);

	/* select 400MHz before start slave cores */
	regval_bak = __raw_readl((void __iomem *)IO_ADDRESS(REG_BASE_CPU_LP));
	__raw_writel(0x306, (void __iomem *)IO_ADDRESS(REG_BASE_CPU_LP));
	__raw_writel(0x706, (void __iomem *)IO_ADDRESS(REG_BASE_CPU_LP));

	/* clear the slave cpu arm_por_srst_req reset */
	regval = __raw_readl((void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));
	regval &= ~(1 << (cpu + CPU_REG_ARM_POR_SRST));
	__raw_writel(regval, (void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));

	/* clear the slave cpu cluster_dbg_srst_req reset */
	regval = __raw_readl((void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));
	regval &= ~(1 << (cpu + CPU_REG_CLUSTER_DBG_SRST));
	__raw_writel(regval, (void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));

	/* clear the slave cpu reset */
	regval = __raw_readl((void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));
	regval &= ~(1 << (cpu + CPU_REG_ARM_SRST));
	__raw_writel(regval, (void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));

	/* restore cpu freq */
	regval = regval_bak & (~(1 << REG_CPU_LP_CPU_SW_BEGIN));
	__raw_writel(regval, (void __iomem *)IO_ADDRESS(REG_BASE_CPU_LP));
	__raw_writel(regval_bak, (void __iomem *)IO_ADDRESS(REG_BASE_CPU_LP));
}

static int hi3798mx_cpu_kill(unsigned int cpu)
{
	unsigned int regval;

	printk(KERN_DEBUG "CPU%u: killed\n", cpu);

	/* set the slave cpu reset */
	regval = __raw_readl((void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));
	regval |= (1 << (cpu + CPU_REG_ARM_SRST));
	__raw_writel(regval, (void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));

	/* set the slave cpu cluster_dbg_srst_req reset */
	regval = __raw_readl((void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));
	regval |= (1 << (cpu + CPU_REG_CLUSTER_DBG_SRST));
	__raw_writel(regval, (void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));

	/* set the slave cpu arm_por_srst_req reset */
	regval = __raw_readl((void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));
	regval |= (1 << (cpu + CPU_REG_ARM_POR_SRST));
	__raw_writel(regval, (void __iomem *)IO_ADDRESS(CPU_REG_BASE_RST));

	return 1;
}
/*****************************************************************************/

struct smp_operations hi3798mx_smp_ops __initdata = {
	.smp_init_cpus = hi3798mx_smp_init_cpus,
	.smp_secondary_init = hi3798mx_secondary_init,
	.smp_boot_secondary = hi3798mx_boot_secondary,
	.cpu_kill = hi3798mx_cpu_kill,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die = hi3798mx_cpu_die,
#endif
};
