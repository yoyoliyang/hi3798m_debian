#ifdef CONFIG_ARCH_GODBOX

#  include <mach/cpu-info.h>
#  include "hieth.h"
#  include "mdio.h"
#  include "mac.h"
#  include "ctrl.h"
#  include "glb.h"

#  define HIETH_SYSREG_BASE                     (IO_ADDRESS(0x101F5000))
#  define HIETH_SYSREG_REG               (0xa8)

#  define HI3716MV300_ETH_SYSREG_RST_BIT     (1 << 1)

#  define HI3716MV300_ETH_GPIO_REG      (0x016c)
#  define HI3716MV300_ETH_GPIO_BASE             (IO_ADDRESS(0x10203000))

#  define HI3712_ETH_SYSREG_RST_BIT     (1 << 1)

#  define HI3712_ETH_GPIO_REG           (0x28)
#  define HI3712_ETH_GPIO_BASE                  (IO_ADDRESS(0x10203000))

static void hieth_set_regbit(unsigned long addr, int bit, int shift)
{
	unsigned long reg;
	reg = readl(addr);
	bit = bit ? 1 : 0;
	reg &= ~(1 << shift);
	reg |= bit << shift;
	writel(reg, addr);
}

static void hieth_reset(int rst)
{
	unsigned long flags;
	local_irq_save(flags);
	hieth_set_regbit(HIETH_SYSREG_BASE + HIETH_SYSREG_REG, rst, 0);
	local_irq_restore(flags);
	msleep(1);
}

static inline void hieth_clk_ena(void)
{
	unsigned long flags;
	unsigned int val;
	local_irq_save(flags);
	val = readl(HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
	val |= (1 << 8);
	writel(val, HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
	local_irq_restore(flags);
}

static inline void hieth_clk_dis(void)
{
	unsigned long flags;
	unsigned int val;
	local_irq_save(flags);
	val = readl(HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
	val &= ~(1 << 8);
	writel(val, HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
	local_irq_restore(flags);
}

static void hieth_internal_phy_reset(void)
{
	/* godbox not support internal phy */
}

static void hieth_external_phy_reset(void)
{
	unsigned long flags;
	unsigned int val;
	long long chipid;

	chipid = get_chipid();
	if (chipid == _HI3712_V100) {
		unsigned int old;

		local_irq_save(flags);

		/* config pin re-use to miirst */
		old = val = readl(HI3712_ETH_GPIO_BASE + HI3712_ETH_GPIO_REG);
		val |= 0x1;
		writel(val, HI3712_ETH_GPIO_BASE + HI3712_ETH_GPIO_REG);

		/* do phy reset */
		val = readl(HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
		val |= HI3712_ETH_SYSREG_RST_BIT;
		writel(val, HIETH_SYSREG_BASE + HIETH_SYSREG_REG);

		local_irq_restore(flags);

		msleep(20);

		local_irq_save(flags);
		val = readl(HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
		val &= ~HI3712_ETH_SYSREG_RST_BIT;
		writel(val, HIETH_SYSREG_BASE + HIETH_SYSREG_REG);

		local_irq_restore(flags);

		msleep(20);

		/* restore pin re-use to old value */
		writel(old, HI3712_ETH_GPIO_BASE + HI3712_ETH_GPIO_REG);
	} else if (chipid == _HI3716M_V300) {
		unsigned int old;

		local_irq_save(flags);

		/* config pin re-use to miirst */
		old = val =
		    readl(HI3716MV300_ETH_GPIO_BASE + HI3716MV300_ETH_GPIO_REG);
		val |= 0x1;
		writel(val,
		       HI3716MV300_ETH_GPIO_BASE + HI3716MV300_ETH_GPIO_REG);

		/* do phy reset */
		val = readl(HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
		val |= HI3716MV300_ETH_SYSREG_RST_BIT;
		writel(val, HIETH_SYSREG_BASE + HIETH_SYSREG_REG);

		local_irq_restore(flags);

		msleep(20);

		local_irq_save(flags);
		val = readl(HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
		val &= ~HI3716MV300_ETH_SYSREG_RST_BIT;
		writel(val, HIETH_SYSREG_BASE + HIETH_SYSREG_REG);

		local_irq_restore(flags);

		msleep(20);

		/* restore pin re-use to old value */
		writel(old,
		       HI3716MV300_ETH_GPIO_BASE + HI3716MV300_ETH_GPIO_REG);

	} else {
#  ifdef CONFIG_HIETH_RESET_HELPER_EN
		local_irq_save(flags);
		/*gpiox[x] set to reset, then sleep 200ms */
		val =
		    readb(IO_ADDRESS(CONFIG_HIETH_RESET_HELPER_GPIO_BASE) +
			  0x400);
		val |= (1 << CONFIG_HIETH_RESET_HELPER_GPIO_BIT);
		writeb(val,
		       IO_ADDRESS(CONFIG_HIETH_RESET_HELPER_GPIO_BASE) + 0x400);
		writeb(0, (IO_ADDRESS(CONFIG_HIETH_RESET_HELPER_GPIO_BASE)
			   + (4 << CONFIG_HIETH_RESET_HELPER_GPIO_BIT)));
		local_irq_restore(flags);
		msleep(200);
		local_irq_save(flags);
		/*then,cancel reset,and should sleep 50ms */
		val =
		    readb(IO_ADDRESS(CONFIG_HIETH_RESET_HELPER_GPIO_BASE) +
			  0x400);
		val |= (1 << CONFIG_HIETH_RESET_HELPER_GPIO_BIT);
		writeb(val,
		       IO_ADDRESS(CONFIG_HIETH_RESET_HELPER_GPIO_BASE) + 0x400);
		writeb((1 << (CONFIG_HIETH_RESET_HELPER_GPIO_BIT)),
		       (IO_ADDRESS(CONFIG_HIETH_RESET_HELPER_GPIO_BASE)
			+ (4 << CONFIG_HIETH_RESET_HELPER_GPIO_BIT)));

		local_irq_restore(flags);
		msleep(50);
#  endif
	}
}

static void hieth_phy_suspend(void)
{
	/* FIXME: phy power down */
}

static void hieth_phy_resume(void)
{
	/* FIXME: phy power up */
	hieth_internal_phy_reset();
	hieth_external_phy_reset();
}

static void hieth_funsel_config(void)
{
}

static void hieth_funsel_restore(void)
{
}

int hieth_port_reset(struct hieth_netdev_local *ld, int port)
{
	long long chipid = get_chipid();

	hieth_assert(port == ld->port);

	/*soft reset */
	if (chipid == _HI3712_V100) {
		hieth_writel_bits(ld, 1, GLB_SOFT_RESET,
				  HI3712_BITS_ETH_SOFT_RESET);
		msleep(1);
		hieth_writel_bits(ld, 0, GLB_SOFT_RESET,
				  HI3712_BITS_ETH_SOFT_RESET);
		msleep(1);
		hieth_writel_bits(ld, 1, GLB_SOFT_RESET,
				  HI3712_BITS_ETH_SOFT_RESET);
		msleep(1);
		hieth_writel_bits(ld, 0, GLB_SOFT_RESET,
				  HI3712_BITS_ETH_SOFT_RESET);
	} else {
		if (ld->port == UP_PORT) {
			/* Note: sf ip need reset twice */
			hieth_writel_bits(ld, 1, GLB_SOFT_RESET,
					  BITS_ETH_SOFT_RESET_UP);
			msleep(1);
			hieth_writel_bits(ld, 0, GLB_SOFT_RESET,
					  BITS_ETH_SOFT_RESET_UP);
			msleep(1);
			hieth_writel_bits(ld, 1, GLB_SOFT_RESET,
					  BITS_ETH_SOFT_RESET_UP);
			msleep(1);
			hieth_writel_bits(ld, 0, GLB_SOFT_RESET,
					  BITS_ETH_SOFT_RESET_UP);
		} else if (ld->port == DOWN_PORT) {

			/* Note: sf ip need reset twice */
			hieth_writel_bits(ld, 1, GLB_SOFT_RESET,
					  BITS_ETH_SOFT_RESET_DOWN);
			msleep(1);
			hieth_writel_bits(ld, 0, GLB_SOFT_RESET,
					  BITS_ETH_SOFT_RESET_DOWN);
			msleep(1);
			hieth_writel_bits(ld, 1, GLB_SOFT_RESET,
					  BITS_ETH_SOFT_RESET_DOWN);
			msleep(1);
			hieth_writel_bits(ld, 0, GLB_SOFT_RESET,
					  BITS_ETH_SOFT_RESET_DOWN);
		} else
			BUG();
	}

	return 0;
}

#endif /*CONFIG_ARCH_GODBOX_V1 */

/* vim: set ts=8 sw=8 tw=78: */
