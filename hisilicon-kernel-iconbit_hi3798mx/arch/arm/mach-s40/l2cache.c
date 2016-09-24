/******************************************************************************
 *    COPYRIGHT (C) 2013 Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-12-19
 *
******************************************************************************/

#define pr_fmt(fmt) "l2cache: " fmt

#include <linux/init.h>
#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/hardware.h>
#include <mach/cpu-info.h>
#include "l2cache.h"

static void __iomem *l2x0_virt_base = __io_address(REG_BASE_L2CACHE);

struct l2cache_data_t {
	u32 aux;
	u32 latency;
	u32 prefetch;
};

/*****************************************************************************/

#ifdef CONFIG_PM

static struct l2cache_data_t l2cache_data;

/*
 *  hi_pm_disable_l2cache()/hi_pm_enable_l2cache() is designed to
 *  disable and enable l2-cache during Suspend-Resume phase
 */
int hi_pm_disable_l2cache(void)
{
	/* backup aux control register value */
	l2cache_data.aux = readl_relaxed(l2x0_virt_base + L2X0_AUX_CTRL);
	l2cache_data.latency = readl_relaxed(l2x0_virt_base +
		L2X0_DATA_LATENCY_CTRL);
	l2cache_data.prefetch = readl_relaxed(l2x0_virt_base +
		L2X0_PREFETCH_CTRL);

	outer_flush_all();

	/* disable l2x0 cache */
	writel_relaxed(0, l2x0_virt_base + L2X0_CTRL);

	/* barrier */
	dmb();

	pr_info("l2x0 cache disabled.\r\n");

	return 0;
}
/*****************************************************************************/

int hi_pm_enable_l2cache(void)
{
	/*enable dynamic clk gating and standby mode*/
	writel_relaxed((L2X0_DYNAMIC_CLK_GATING_EN | L2X0_STNDBY_MODE_EN),
		       (l2x0_virt_base + L2X0_POWER_CTRL));

	/* disable cache */
	writel_relaxed(0, l2x0_virt_base + L2X0_CTRL);

	/* restore aux control register */
	writel_relaxed(l2cache_data.aux, l2x0_virt_base + L2X0_AUX_CTRL);
	writel_relaxed(l2cache_data.latency, l2x0_virt_base +
		L2X0_DATA_LATENCY_CTRL);
	writel_relaxed(l2cache_data.prefetch, l2x0_virt_base +
		L2X0_PREFETCH_CTRL);

	/* invalidate l2x0 cache */
	outer_inv_all();

	/* enable l2x0 cache */
	writel_relaxed(1, l2x0_virt_base + L2X0_CTRL);

	mb();

	return 0;
}
#endif /* CONFIG_PM */
/*****************************************************************************/

static int __init l2_cache_init(void)
{
	u32 val;
	/*
	 * Bits  Value Description
	 * [31]    0 : SBZ
	 * [30]    1 : Double linefill enable (L3)
	 * [29]    1 : Instruction prefetching enable
	 * [28]    1 : Data prefetching enabled
	 * [27]    0 : Double linefill on WRAP read enabled (L3)
	 * [26:25] 0 : SBZ
	 * [24]    1 : Prefetch drop enable (L3)
	 * [23]    0 : Incr double Linefill enable (L3)
	 * [22]    0 : SBZ
	 * [21]    0 : Not same ID on exclusive sequence enable (L3)
	 * [20:5]  0 : SBZ
	 * [4:0]   0 : use the Prefetch offset values 0.
	 */
	writel_relaxed(0x71000000, l2x0_virt_base + L2X0_PREFETCH_CTRL);

	val = __raw_readl(l2x0_virt_base + L2X0_AUX_CTRL);
	val |= (1 << 30); /* Early BRESP enabled */
	val |= (1 << 0);  /* Full Line of Zero Enable */
	writel_relaxed(val, l2x0_virt_base + L2X0_AUX_CTRL);

	if ((get_chipid() == _HI3719MV100) || (get_chipid() == _HI3718MV100)) {
		val = readl_relaxed(l2x0_virt_base + L2X0_TAG_LATENCY_CTRL);
		val &=0xfffff888;
		writel_relaxed(val, l2x0_virt_base + L2X0_TAG_LATENCY_CTRL);

		val = readl_relaxed(l2x0_virt_base + L2X0_DATA_LATENCY_CTRL);
		val &=0xfffff888;
		writel_relaxed(val, l2x0_virt_base + L2X0_DATA_LATENCY_CTRL);

		l2x0_init(l2x0_virt_base, 0x00440000, 0xFFB0FFFF);
	} else if (get_chipid() == _HI3798CV100A || get_chipid() == _HI3798CV100
			|| (get_chipid() == _HI3796CV100)) {
		/* L2cache is 1M( 64KB * 16 Way = 1M ) bytes */
		l2x0_init(l2x0_virt_base, 0x00470000, 0xFFB0FFFF);
	} else 
		l2x0_init(l2x0_virt_base, 0x00450000, 0xFFB0FFFF);

	/*
	 * 2. enable L2 prefetch hint                  [1]a
	 * 3. enable write full line of zeros mode.    [3]a
	 *   a: This feature must be enabled only when the slaves
	 *      connected on the Cortex-A9 AXI master port support it.
	 */
	asm volatile (
	"	mrc	p15, 0, r0, c1, c0, 1\n"
	"	orr	r0, r0, #0x02\n"
	"	mcr	p15, 0, r0, c1, c0, 1\n"
	  :
	  :
	  : "r0", "cc");

	return 0;
}
early_initcall(l2_cache_init);
