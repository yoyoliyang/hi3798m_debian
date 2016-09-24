#ifdef CONFIG_ARCH_S40

#include <mach/cpu-info.h>
#include "hieth.h"
#include "mdio.h"
#include "mac.h"
#include "ctrl.h"
#include "glb.h"

#define HIETH_SYSREG_BASE               ((void __iomem *)IO_ADDRESS(0xF8A22000))
#define HIETH_SYSREG_REG                0x00CC
#define HIETHPHY_SYSREG_REG             0x0120
#define INTERNAL_FEPHY_ADDR             ((void __iomem *)IO_ADDRESS(0xF8A20118))
#define HIETH_FEPHY_SELECT              ((void __iomem *)IO_ADDRESS(0xF8A20008))
#define HIETH_FEPHY_LDO_CTRL            ((void __iomem *)IO_ADDRESS(0xF8A20844))

/* DEFAULT external phy reset pin */
#define HIETH_FEPHY_RST_BASE            ((void __iomem *)IO_ADDRESS(0xF8A22168))
#define HIETH_FEPHY_RST_BIT             1

extern int hisf_phy_addr_up;
extern struct hisf_gpio  hisf_gpio_up;
extern struct hisf_gpio  hisf_gpio_down;

static void hieth_set_regbit(void __iomem * addr, int bit, int shift)
{
	unsigned long reg;
	reg = readl(addr);
	bit = bit ? 1 : 0;
	reg &= ~(1<<shift);
	reg |= bit<<shift;
	writel(reg, addr);
}

static void hieth_reset(int rst)
{
	hieth_set_regbit(HIETH_SYSREG_BASE + HIETH_SYSREG_REG, rst, 0);
	udelay(100);
}

static inline void hieth_clk_ena(void)
{
	unsigned int val;

	/* SF */
	val = readl(HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
	val |= (1 << 1);
	writel(val, HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
}

static inline void hieth_clk_dis(void)
{
	unsigned int val;

	/* SF */
	val = readl(HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
	val &= ~(1 << 1);
	writel(val, HIETH_SYSREG_BASE + HIETH_SYSREG_REG);
}

static void hieth_internal_phy_reset(void)
{
	unsigned int val;

	val = readl(HIETH_FEPHY_SELECT);
	if ((val & (1 << 8)) != 0)
		return;/* if not use fephy, leave it's clk disabled */

	writel(0x68, HIETH_FEPHY_LDO_CTRL);/* LDO output 1.1V */

	/* FEPHY enable clock */
	val = readl(HIETH_SYSREG_BASE + HIETHPHY_SYSREG_REG);
	val |= (1);
	writel(val, HIETH_SYSREG_BASE + HIETHPHY_SYSREG_REG);	

	/* set FEPHY address */
	val = readl(INTERNAL_FEPHY_ADDR);
	val &= ~(0x1F);
	val |= (hisf_phy_addr_up & 0x1F);
	writel(val, INTERNAL_FEPHY_ADDR);
	
	/* FEPHY set reset */
	val = readl(HIETH_SYSREG_BASE + HIETHPHY_SYSREG_REG);
	val |= (1 << 4);
	writel(val, HIETH_SYSREG_BASE + HIETHPHY_SYSREG_REG);
	
	udelay(10);
	
	/* FEPHY cancel reset */
	val = readl(HIETH_SYSREG_BASE + HIETHPHY_SYSREG_REG);
	val &= ~(1 << 4);
	writel(val, HIETH_SYSREG_BASE + HIETHPHY_SYSREG_REG);

	msleep(20); /* delay at least 15ms for MDIO operation */
}

void hieth_gpio_reset(void __iomem * gpio_base, u32 gpio_bit)
{
	u32 v;

#define RESET_DATA      (1)

	if (!gpio_base)
		return;

	gpio_base = IO_ADDRESS(gpio_base);

	/* config gpio[x] dir to output */
	v = readb(gpio_base + 0x400);
	v |= (1 << gpio_bit);
	writeb(v, gpio_base + 0x400);

	/* output 1--0--1 */
	writeb(RESET_DATA << gpio_bit,
			gpio_base + (4 << gpio_bit));
	msleep(10);
	writeb((!RESET_DATA) << gpio_bit,
			gpio_base + (4 << gpio_bit));
	msleep(10);
	writeb(RESET_DATA << gpio_bit,
			gpio_base + (4 << gpio_bit));
	msleep(10);
}

static void hieth_external_phy_reset(void)
{
	unsigned int val;

	/* if use internal fephy, return */
	val = readl(HIETH_FEPHY_SELECT);
	if ((val & (1 << 8)) == 0)
		return;

	/************************************************/
	/* reset external phy with default reset pin */
	val = readl(HIETH_FEPHY_RST_BASE);
	val |= (1 << HIETH_FEPHY_RST_BIT);
	writel(val, HIETH_FEPHY_RST_BASE);

	msleep(20);

	/* then, cancel reset, and should delay 200ms */
	val &= ~(1 << HIETH_FEPHY_RST_BIT);
	writel(val, HIETH_FEPHY_RST_BASE);

	msleep(20);
	val |=  1 << HIETH_FEPHY_RST_BIT;
	writel(val, HIETH_FEPHY_RST_BASE);

	/************************************************/
	/* reset external phy with gpio */	
	hieth_gpio_reset(hisf_gpio_up.gpio_base,hisf_gpio_up.gpio_bit);

	/************************************************/

	/* add some delay in case mdio cann't access now! */
	msleep(30);

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

	hieth_assert(port == ld->port);

	if (ld->port == UP_PORT) {
		/* Note: sf ip need reset twice */
		hieth_writel_bits(ld, 1, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_UP);
		msleep(1);
		hieth_writel_bits(ld, 0, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_UP);
		msleep(1);
		hieth_writel_bits(ld, 1, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_UP);
		msleep(1);
		hieth_writel_bits(ld, 0, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_UP);
	} else if (ld->port == DOWN_PORT) {
		/* Note: sf ip need reset twice */
		hieth_writel_bits(ld, 1, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_DOWN);
		msleep(1);
		hieth_writel_bits(ld, 0, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_DOWN);
		msleep(1);
		hieth_writel_bits(ld, 1, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_DOWN);
		msleep(1);
		hieth_writel_bits(ld, 0, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_DOWN);
	} else
		BUG();

	return 0;
}


#endif/*CONFIG_NET_HISFV300_GODBOX*/

/* vim: set ts=8 sw=8 tw=78: */
