#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <asm/memory.h>
#include <mach/early-debug.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/syscalls.h>
#include <asm/mach/time.h>
#include <linux/slab.h>
#include <asm/hardware/arm_timer.h>
#include <linux/kmemleak.h>
#include <linux/device.h>
#include <linux/irqchip/arm-gic.h>
#include <mach/sram.h>
#include <mach/hardware.h>

#include "l2cache.h"

void __iomem *hi_sc_virtbase;
void __iomem *hi_crg_virtbase;
void __iomem *hi_scu_virtbase;

/*ddr address for save cpu context*/
extern unsigned int hi_pm_ddrbase;
extern unsigned int hi_pm_phybase;

unsigned long saved_interrupt_mask[128];
unsigned long saved_cpu_target_mask[128];

asmlinkage void hi_pm_sleep(void);

extern void __iomem *s40_gic_cpu_base_addr;
static void __iomem *gic_dist_virt_base_addr = IOMEM(CFG_GIC_DIST_BASE);

#ifdef CONFIG_CA_WARKUP_CHECK
extern unsigned int _ddr_wakeup_check_code_begin;
extern unsigned int _ddr_wakeup_check_code_end;
extern unsigned int hi_sram_virtbase;

unsigned int ddr_wakeup_check_code[] = {
0xea000006,0xe59ff034,0xe59ff034,0xe59ff034,0xe59ff034,0xe59ff034,0xe59ff034,0xe59ff034,
0xe59f0034,0xe1a0d000,0xee111f10,0xe3c11b06,0xe3c11007,0xee011f10,0xeb0000fe,0xeafffffe,
0xeafffffe,0xeafffffe,0xeafffffe,0xeafffffe,0xeafffffe,0xeafffffe,0xeafffffe,0xffff0e00,
0xeb000000,0xeb00001c,0xe28f002c,0xe8900c00,0xe08aa000,0xe08bb000,0xe24a7001,0xe15a000b,
0x1a000000,0xeb000014,0xe8ba000f,0xe24fe018,0xe3130001,0x1047f003,0xe12fff13,0x00000994,
0x000009a4,0xe3b03000,0xe3b04000,0xe3b05000,0xe3b06000,0xe2522010,0x28a10078,0x8afffffc,
0xe1b02e82,0x28a10030,0x45813000,0xe12fff1e,0xf000b51f,0xbd1feca8,0xbd10b510,0xeb000218,
0xe1a01002,0xfafffff9,0xe59fc01c,0xe08cc00f,0xe31c0001,0x128fe00d,0x01a0e00f,0xe12fff1c,
0xe28fc001,0xe12fff1c,0xfc4cf000,0x00000344,0xfafffff0,0xeb00023b,0xe92d4ffc,0xe1a03000,
0xe1a04001,0xe1a07002,0xe1a01004,0xe1a0c004,0xe1a01004,0xe1a0e004,0xe1a01004,0xe1a05004,
0xe1a01004,0xe1a06004,0xe1a01004,0xe1a08004,0xe1a01004,0xe1a09004,0xe1a01004,0xe58d4004,
0xe1a01004,0xe58d4000,0xe1a02007,0xe2477001,0xe1a00003,0xe0877003,0xe320f000,0xe1a0a003,
0xe283b008,0xe88b0360,0xe8835000,0xe59db004,0xe583b018,0xe59db000,0xe5a3b01c,0xe2833004,
0xe1530007,0x8a000000,0xeafffff3,0xe8bd8ffc,0xe1a02000,0xe1a01002,0xea000000,0xe2811001,
0xe5d10000,0xe3500000,0x1afffffb,0xe0410002,0xe12fff1e,0xe92d4070,0xe1a03000,0xe1a04001,
0xea000001,0xe4d45001,0xe4c35001,0xe1b05002,0xe2422001,0x1afffffa,0xe8bd8070,0xe92d41f0,
0xe24dd030,0xe1a04000,0xe3a02030,0xe28f1f5f,0xe1a0000d,0xeb0001ab,0xe1a0000d,0xebffffe3,
0xe1a05000,0xe1a07004,0xe3a08401,0xe3a06000,0xea000004,0xe1a02005,0xe1a0100d,0xe1a00006,
0xebffffe3,0xe0866008,0xe1560007,0x3afffff8,0xe28dd030,0xe8bd81f0,0xe92d4070,0xe3a02000,
0xe5923000,0xe3a04000,0xe3a05000,0xe59f014c,0xe5900000,0xe3500000,0x0a000002,0xe59f013c,
0xe5900000,0xe8bd8070,0xe2821401,0xea00000c,0xe59f012c,0xe5820000,0xe5914000,0xe1a00080,
0xe5820000,0xe5915000,0xe1540005,0x0a000003,0xe0410002,0xe59f6104,0xe5860000,0xea000003,
0xe2811401,0xe2820102,0xe1510000,0x9affffef,0xe320f000,0xe5823000,0xe59f00e0,0xe5900000,
0xeaffffe7,0xe92d4ff8,0xe59f40d8,0xe5945060,0xe7e20255,0xe280600b,0xe2050007,0xe2807008,
0xe7e00455,0xe2800001,0xe1a08100,0xe3a0a000,0xe59fb0b4,0xebffffcf,0xe58d0000,0xe3a01000,
0xe1a00001,0xe59d2000,0xebffff7a,0xe3a00000,0xe1c41000,0xe5810004,0xe3a09801,0xe320f000,
0xe2490001,0xe1b09000,0x1afffffc,0xe3a00001,0xe59f1070,0xe5810004,0xe8bd8ff8,0xe3a01001,
0xe59f2060,0xe5821000,0xea000005,0xe59f1054,0xe5910294,0xe2001001,0xe3510001,0x1a000000,
0xea000000,0xeafffff8,0xe320f000,0xe12fff1e,0x23232323,0x69736968,0x6f63696c,0x6f6c206e,
0x6f702077,0x20726577,0x65646f6d,0x6d617220,0x756c6620,0x66206873,0x2367616c,0x00232323,
0xffff0a40,0xa9a9a9a9,0xf8a31000,0x12345678,0xe3a0333e,0xe5932ee0,0xe2423565,0xe2533c02,
0x1a000008,0xe3500000,0x0a000001,0xe3a03003,0xe5c03000,0xe3510000,0x0a00000d,0xe3a03c02,
0xe1c130b0,0xea00000a,0xe59f319c,0xe1520003,0x1a000007,0xe3500000,0x0a000001,0xe3a03002,
0xe5c03000,0xe3510000,0x0a000001,0xe3a03c02,0xe1c130b0,0xe12fff1e,0xe92d47f0,0xe24dd020,
0xe3a05001,0xe3a00000,0xe58d001c,0xe3a09000,0xe59f0158,0xe5906080,0xe7e08256,0xe5906084,
0xe7e07c56,0xe590612c,0xe20690ff,0xe3580001,0x1a000004,0xe3590001,0x1a000002,0xebffff98,
0xebffffb5,0xea000040,0xe3570001,0x1a00003e,0xe59f011c,0xe590a000,0xe5900004,0xe58d001c,
0xe59f0110,0xe59000c4,0xe3800010,0xe59f1104,0xe58100c4,0xe3a04ffa,0xe320f000,0xe1b00004,
0xe2444001,0x1afffffc,0xe59f00e8,0xe59000c4,0xe3c00010,0xe59f10dc,0xe58100c4,0xe28d2018,
0xe28d101c,0xe3a00000,0xeb000035,0xe3a04000,0xea000003,0xe1a0000a,0xe59d101c,0xeb000051,
0xe2844001,0xe1540005,0x3afffff9,0xe59d001c,0xe59d1018,0xeb000077,0xe28d1004,0xe3a00000,
0xeb0000c1,0xe59f008c,0xe59000c4,0xe3800010,0xe59f1080,0xe58100c4,0xe3a04ffa,0xe320f000,
0xe1b00004,0xe2444001,0x1afffffc,0xe59f0064,0xe59000c4,0xe3c00010,0xe59f1058,0xe58100c4,
0xe3a01000,0xe59d0004,0xe5010100,0xe59d0008,0xe50100fc,0xe59d000c,0xe50100f8,0xe59d0010,
0xe50100f4,0xe59d0014,0xe50100f0,0xe3a00001,0xe59f1024,0xe5810000,0xe59f0020,0xe3a0133e,
0xe58100bc,0xe320f000,0xeafffffe,0x37160200,0xf8ab0000,0xf840d000,0xf8a22000,0xf840f000,
0x80510001,0xe92d40f0,0xe5916000,0xe3c66003,0xe5816000,0xe5916000,0xe2866048,0xe3c6303f,
0xe5916000,0xe206403f,0xe3540038,0x2a000001,0xe2646037,0xea000000,0xe2646077,0xe5826000,
0xe320f000,0xe59f62b8,0xe5966008,0xe3160001,0x0afffffb,0xe59f62a8,0xe5863000,0xe3a06000,
0xe59f729c,0xe5876004,0xe1c76006,0xe596500c,0xe3c55006,0xe1856080,0xe587600c,0xe3a06001,
0xe5876010,0xe8bd80f0,0xe92d4010,0xe3a02000,0xe59f326c,0xe593300c,0xe3c33001,0xe59f4260,
0xe584300c,0xe320f000,0xe59f3254,0xe5933008,0xe3130008,0x0afffffb,0xe59f3244,0xe5830014,
0xe5831018,0xe0822001,0xe320f000,0xe59f3230,0xe5933020,0xe1530002,0x1afffffb,0xe8bd8010,
0xe92d4070,0xe1a02000,0xe59f5214,0xe595500c,0xe3855001,0xe59f6208,0xe586500c,0xe1a01121,
0xe3a03000,0xea000008,0xe7924103,0xe320f000,0xe59f51ec,0xe5955008,0xe3150008,0x0afffffb,
0xe59f51dc,0xe585401c,0xe2833001,0xe1530001,0x3afffff4,0xe8bd8070,0xe92d403c,0xe1a0500d,
0xe3a02000,0xe1a03002,0xe885000c,0xe320f000,0xe59f31ac,0xe5933008,0xe3130008,0x0afffffb,
0xe59f319c,0xe593300c,0xe3833001,0xe59f5190,0xe585300c,0xe3a04000,0xea000005,0xe1a03180,
0xe2645007,0xe1a05185,0xe1a03533,0xe7cd3004,0xe2844001,0xe3540008,0x3afffff7,0xe320f000,
0xe59f315c,0xe5933008,0xe3130008,0x0afffffb,0xe3a02080,0xe59f3148,0xe583201c,0xe3a04003,
0xea000008,0xe320f000,0xe59f3134,0xe5933008,0xe3130008,0x0afffffb,0xe3a03000,0xe59f5120,
0xe585301c,0xe2844004,0xe1540001,0x3afffff4,0xe320f000,0xe59f3108,0xe5933008,0xe3130008,
0x0afffffb,0xe5dd3003,0xe1a03c03,0xe5dd5002,0xe1833805,0xe5dd5001,0xe1833405,0xe5dd5000,
0xe1832005,0xe59f30d8,0xe583201c,0xe320f000,0xe59f30cc,0xe5933008,0xe3130008,0x0afffffb,
0xe5dd3007,0xe1a03c03,0xe5dd5006,0xe1833805,0xe5dd5005,0xe1833405,0xe5dd5004,0xe1832005,
0xe59f309c,0xe583201c,0xe8bd803c,0xe92d4070,0xe3500000,0x1a000001,0xe3a05014,0xea000000,
0xe3a05020,0xe1a04005,0xe320f000,0xe59f5070,0xe5955008,0xe3150001,0x0afffffb,0xe59f5060,
0xe5955008,0xe31500f0,0x0a000002,0xe3a050ee,0xe5c15000,0xe8bd8070,0xe3a03000,0xea00000d,
0xe28354fa,0xe245585f,0xe5952030,0xe7c12003,0xe7e75452,0xe2836001,0xe7c15006,0xe7e75852,
0xe2836002,0xe7c15006,0xe1a05c22,0xe2836003,0xe7c15006,0xe2833004,0xe1530004,0x3affffef,
0xeaffffeb,0xf9a10000,0xe92d41f0,0xe2522020,0x3a00000b,0xe3520080,0x3a000005,0xf5d1f080,
0xe8b151f8,0xe2422020,0xe3520080,0xe8a051f8,0x2afffff9,0xe8b151f8,0xe2522020,0xe8a051f8,
0x2afffffb,0xe1b0ce02,0x28b15018,0x28a05018,0x48b10018,0x48a00018,0xe8bd41f0,0xe1b0cf02,
0x24913004,0x24803004,0x012fff1e,0xe1b02f82,0x20d130b2,0x44d12001,0x20c030b2,0x44c02001,
0xe12fff1e,0xe1a0500e,0xeb00002b,0xe1a0e005,0xe1b05000,0xe1a0100d,0xe1a0300a,0xe3c00007,
0xe1a0d000,0xe28dd060,0xe92d4020,0xeb00000f,0xe8bd4020,0xe3a06000,0xe3a07000,0xe3a08000,
0xe3a0b000,0xe3c11007,0xe1a0c005,0xe8ac09c0,0xe8ac09c0,0xe8ac09c0,0xe8ac09c0,0xe1a0d001,
0xe12fff1e,0xf3af0004,0x00208000,0xebb0f7ff,0xe92d4000,0xe3a00016,0xe24dd014,0xe1a0100d,
0xe28d2004,0xe5812000,0xef123456,0xe59d0004,0xe3500000,0x059f001c,0x02800007,0x03c00007,
0xe59d100c,0xe59d2008,0xe59d3010,0xe28dd014,0xe8bd8000,0x0000002c,0xffff0aa4,0xe59f0000,
0xe12fff1e,0xffff0a44,0xe59f100c,0xe3a00018,0xef123456,0xe12fff1e,0x00000008,0x00020026,
0xe12fff1e,0xe3a00403,0xeee10a10,0xe12fff1e,0xffff0a44,0xffff0a44,0x00000060,0xffff00a4,
0x00000000,0x00000000,
};
#endif /* CONFIG_CA_WARKUP_CHECK */

static int hi_pm_save_gic(void)
{
	unsigned int max_irq, i;
	unsigned int intack;

	/* disable gic dist */
	writel(0, gic_dist_virt_base_addr + GIC_DIST_CTRL);

	/*
	 * Find out how many interrupts are supported.
	 */
	max_irq = readl(gic_dist_virt_base_addr + GIC_DIST_CTR) & 0x1f;
	max_irq = (max_irq + 1) * 32;

	/*
	 * The GIC only supports up to 1020 interrupt sources.
	 * Limit this to either the architected maximum, or the
	 * platform maximum.
	 */
	max_irq = max_t(unsigned int, max_irq, max(1020, NR_IRQS));

	/* save Dist target */
	for (i = 32; i < max_irq; i += 4) {
		saved_cpu_target_mask[i / 4] =
		    readl(gic_dist_virt_base_addr + GIC_DIST_TARGET + i * 4 / 4);
	}

	/* save mask irq */
	for (i = 0; i < max_irq; i += 32) {
		saved_interrupt_mask[i / 32] =
		    readl(gic_dist_virt_base_addr + GIC_DIST_ENABLE_SET +
			  i * 4 / 32);
	}

	/* clear all interrupt */
	for (i = 0; i < max_irq; i += 32) {
		writel(0xffffffff, gic_dist_virt_base_addr + GIC_DIST_ENABLE_CLEAR + i * 4 / 32);
	}

	/* read INT_ACK in CPU interface, until result is 1023 */
	for (i = 0; i < max_irq; i++) {
		intack = readl(s40_gic_cpu_base_addr + 0x0c);
		if (1023 == intack) {
			break;
		}
		writel(intack, s40_gic_cpu_base_addr + 0x10);
	}

#if 0   /* comment off wakeup intr, cause we will go directly to deepsleep */
	/* enable softinterrupt mask */
	writel(0xffff, gic_dist_virt_base_addr + GIC_DIST_ENABLE_SET);

	/* enable KPC/TBC/RTC interrupt */
	writel(GET_IRQ_BIT(IRQ_KPC) | GET_IRQ_BIT(IRQ_TBC) | GET_IRQ_BIT(IRQ_RTC),
	       gic_dist_virt_base_addr + GIC_DIST_ENABLE_SET + 4);

	writel(0, gic_dist_virt_base_addr + GIC_DIST_ENABLE_SET + 8);

	/* enable all gpio interrupt */
	writel(0x3fffff, gic_dist_virt_base_addr + GIC_DIST_ENABLE_SET + 0xc);
#endif

	return 0;
}

static int hi_pm_retore_gic(void)
{
	unsigned int max_irq, i;

	/* PRINT OUT the GIC Status */
	unsigned int irq_status[5];

	for (i = 0; i < 5; i++)
		irq_status[i] = readl(gic_dist_virt_base_addr + 0xd00 + i * 4);

	writel(0, gic_dist_virt_base_addr + GIC_DIST_CTRL);
	writel(0, s40_gic_cpu_base_addr + GIC_CPU_CTRL);

	/*
	 * Find out how many interrupts are supported.
	 */
	max_irq = readl(gic_dist_virt_base_addr + GIC_DIST_CTR) & 0x1f;
	max_irq = (max_irq + 1) * 32;

	/*
	 * The GIC only supports up to 1020 interrupt sources.
	 * Limit this to either the architected maximum, or the
	 * platform maximum.
	 */
	max_irq = max_t(unsigned int, max_irq, max(1020, NR_IRQS));

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < max_irq; i += 16) {
		writel(0, gic_dist_virt_base_addr + GIC_DIST_CONFIG + i * 4 / 16);
	}

	/*
	 * Set all global interrupts to this CPU only.
	 */
	for (i = 32; i < max_irq; i += 4) {
		writel(saved_cpu_target_mask[i / 4],
		       gic_dist_virt_base_addr + GIC_DIST_TARGET + i * 4 / 4);
	}

	/*
	 * Set priority on all interrupts.
	 */
	for (i = 0; i < max_irq; i += 4) {
		writel(0xa0a0a0a0,
		       gic_dist_virt_base_addr + GIC_DIST_PRI + i * 4 / 4);
	}

	/*
	 * Disable all interrupts.
	 */
	for (i = 0; i < max_irq; i += 32) {
		writel(0xffffffff,
		       gic_dist_virt_base_addr + GIC_DIST_ENABLE_CLEAR + i * 4 / 32);
	}

	for (i = 0; i < max_irq; i += 32) {
		writel(saved_interrupt_mask[i / 32],
		       gic_dist_virt_base_addr + GIC_DIST_ENABLE_SET + i * 4 / 32);
	}

	writel(1, gic_dist_virt_base_addr + GIC_DIST_CTRL);

	/* set the BASE priority 0xf0 */
	writel(0xf0, s40_gic_cpu_base_addr + GIC_CPU_PRIMASK);

	writel(1, s40_gic_cpu_base_addr + GIC_CPU_CTRL);

	return 0;
}
/*****************************************************************************/

static int hi_pm_suspend(void)
{
	/* int ret = 0; */
	unsigned long flage = 0;

	/* disable irq */
	local_irq_save(flage);

	/* save gic */
	hi_pm_save_gic();

	hi_pm_disable_l2cache();

	sram_suspend();

	hi_pm_sleep();

	sram_resume();

	hi_pm_enable_l2cache();

	/* restore gic */
	hi_pm_retore_gic();

	/* enable irq */
	local_irq_restore(flage);

	return 0;
}

#ifdef CONFIG_PM_HIBERNATE
void hi_pm_hibernate_suspend (void)
{
	/* save gic */
	hi_pm_save_gic();

	/*save & disable l2 cache */
#ifdef CONFIG_CACHE_L2X0
	hi_pm_disable_l2cache();
#endif

#ifdef CONFIG_SUPPORT_SRAM_MANAGER
	sram_suspend();
#endif

	/* save & diable timer0_1 */
}

void hi_pm_hibernate_resume (void)
{
	/* restore & enable timer0_1 */

#ifdef CONFIG_SUPPORT_SRAM_MANAGER
	sram_resume();
#endif

	/*restore & enable l2 cache */
#ifdef CONFIG_CACHE_L2X0
	hi_pm_enable_l2cache();
#endif

	/* restore gic */
	hi_pm_retore_gic();
}
#endif

static int hi_pm_enter(suspend_state_t state)
{
	int ret = 0;
	switch (state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		ret = hi_pm_suspend();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

int hi_pm_valid(suspend_state_t state)
{
	return 1;
}

static const struct platform_suspend_ops hi_pm_ops = {
	.enter = hi_pm_enter,
	.valid = hi_pm_valid,
};
/*****************************************************************************/

static int __init hi_pm_init(void)
{
	hi_sc_virtbase = (void __iomem *)IO_ADDRESS(REG_BASE_SCTL);
	hi_crg_virtbase = (void __iomem *)IO_ADDRESS(REG_BASE_CRG);

	hi_pm_ddrbase = (unsigned int)kzalloc(1024, GFP_DMA | GFP_KERNEL);
	hi_pm_phybase = __pa(hi_pm_ddrbase);
	/*
	 * Because hi_pm_ddrbase is saved in .text of hi_pm_sleep.S, the kmemleak,
	 * which not check the .text, reports a mem leak here ,
	 * so we suppress kmemleak messages.
	 */
	kmemleak_not_leak((void *)hi_pm_ddrbase);

	suspend_set_ops(&hi_pm_ops);

#ifdef CONFIG_CA_WARKUP_CHECK
	hi_sram_virtbase = (unsigned int)ioremap_nocache(SRAM_BASE_ADDRESS, sizeof(ddr_wakeup_check_code));
	_ddr_wakeup_check_code_begin =
	    (unsigned int)kzalloc(sizeof(ddr_wakeup_check_code), GFP_DMA | GFP_KERNEL);
	/*
	 * Because _ddr_wakeup_check_code_begin is saved in .text of hi_pm_sleep.S, the kmemleak,
	 * which not check the .text, reports a mem leak here ,
	 * so we suppress kmemleak messages.
	 */
	kmemleak_not_leak((void *)_ddr_wakeup_check_code_begin);

	memcpy((void*)_ddr_wakeup_check_code_begin, ddr_wakeup_check_code, sizeof(ddr_wakeup_check_code));
	_ddr_wakeup_check_code_end = _ddr_wakeup_check_code_begin + sizeof(ddr_wakeup_check_code);
#endif	/* CONFIG_CA_WARKUP_CHECK */
	return 0;
}

module_init(hi_pm_init);
