#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/device.h>
#include <mach/hardware.h>

/* hisilicon sata reg */
#define HI_SATA_PHY0_CTLL       0x54
#define HI_SATA_PHY0_CTLH       0x58
#define HI_SATA_PHY1_CTLL       0x60
#define HI_SATA_PHY1_CTLH       0x64
#define HI_SATA_DIS_CLK     (1 << 12)
#define HI_SATA_OOB_CTL         0x6c
#define HI_SATA_PORT_PHYCTL     0x74

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hisilicon osdrv group");

#ifdef CONFIG_ARCH_GODBOX
static int phy_config = CONFIG_HI_SATA_PHY_CONFIG;
static int n_ports = CONFIG_HI_SATA_PORTS;
static int mode_3g = CONFIG_HI_SATA_MODE;

#ifdef MODULE
module_param(phy_config, uint, 0600);
MODULE_PARM_DESC(phy_config, "sata phy config (default:0x0e262709)");
module_param(n_ports, uint, 0600);
MODULE_PARM_DESC(n_ports, "sata port number (default:2)");
module_param(mode_3g, uint, 0600);
MODULE_PARM_DESC(mode_3g, "set sata 3G mode (0:1.5G(default);1:3G)");
#endif
#endif

#ifdef CONFIG_ARCH_GODBOX
#include "hi_ahci_sys_godbox_defconfig.c"
#endif/*CONFIG_ARCH_GODBOX*/

#ifdef CONFIG_ARCH_S40
#include "hi_ahci_sys_s40_defconfig.c"
#endif/*CONFIG_ARCH_S40*/

int hi_sata_init(struct device *dev, void __iomem *mmio)
{
#ifdef CONFIG_ARCH_S40
	hi_sata_init_s40(mmio);
#else
	unsigned int tmp;
	int i;

	hi_sata_poweron();
	msleep(20);
	hi_sata_clk_open();
	hi_sata_phy_clk_sel();
	hi_sata_unreset();
	msleep(20);
	hi_sata_phy_unreset();
	msleep(20);
	tmp = readl(mmio + HI_SATA_PHY0_CTLH);
	tmp |= HI_SATA_DIS_CLK;
	writel(tmp, (mmio + HI_SATA_PHY0_CTLH));
	tmp = readl(mmio + HI_SATA_PHY1_CTLH);
	tmp |= HI_SATA_DIS_CLK;
	writel(tmp, (mmio + HI_SATA_PHY1_CTLH));
	if (mode_3g) {
		tmp = 0x8a0ec888;
		phy_config = CONFIG_HI_SATA_3G_PHY_CONFIG;
	} else {
		tmp = 0x8a0ec788;
	}
	writel(tmp, (mmio + HI_SATA_PHY0_CTLL));
	writel(0x2121, (mmio + HI_SATA_PHY0_CTLH));
	writel(tmp, (mmio + HI_SATA_PHY1_CTLL));
	writel(0x2121, (mmio + HI_SATA_PHY1_CTLH));
	writel(0x84060c15, (mmio + HI_SATA_OOB_CTL));
	for (i = 0; i < n_ports; i++)
		writel(phy_config, (mmio + 0x100 + i*0x80
					+ HI_SATA_PORT_PHYCTL));

	hi_sata_phy_reset();
	msleep(20);
	hi_sata_phy_unreset();
	msleep(20);
	hi_sata_clk_unreset();
	msleep(20);
#endif
	return 0;
}
EXPORT_SYMBOL(hi_sata_init);

void hi_sata_exit(struct device *dev)
{
#if CONFIG_ARCH_S40
	hi_sata_exit_s40();
#else
	hi_sata_phy_reset();
	msleep(20);
	hi_sata_reset();
	msleep(20);
	hi_sata_clk_reset();
	msleep(20);
	hi_sata_clk_close();
	hi_sata_poweroff();
	msleep(20);
#endif
}
EXPORT_SYMBOL(hi_sata_exit);


