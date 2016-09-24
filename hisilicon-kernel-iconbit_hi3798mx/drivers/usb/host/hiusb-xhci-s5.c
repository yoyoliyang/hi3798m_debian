#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <mach/hardware.h>

#include "xhci.h"
#include "hixhci.h"

MODULE_LICENSE("Dual MIT/GPL");

#define PERI_CRG123            __io_address(0xF8A22000 + 0x1ec)
#define USB3_VCC_SRST_REQ2     (1<<12)
#define USB3_PHY_SRST_REQ2       (0x1 << 13)
#define USB3_PHY_SRST_TREQ2     (0x1 << 14)

#define GTXTHRCFG             0xc108
#define GRXTHRCFG             0xc10c
#define REG_GCTL              0xc110

#define REG_GUSB2PHYCFG0         0xC200
#define BIT_UTMI_ULPI         (0x1 << 4)
#define BIT_UTMI_8_16         (0x1 << 3)

#define REG_GUSB3PIPECTL0	0xc2c0
#define PCS_SSP_SOFT_RESET	(0x1 << 31)


void hiusb3_start_hcd(void __iomem *base)
{
	unsigned long flags;
	unsigned int reg;

	local_irq_save(flags);

	/* de-assert usb3_vcc_srst_req */
	writel(0xFF4F000, PERI_CRG123);
	msleep(100);
	reg = readl(PERI_CRG123);
	reg &= ~(USB3_VCC_SRST_REQ2);
	writel(reg, PERI_CRG123);
	msleep(100);

	reg = readl(base + REG_GUSB3PIPECTL0);
	reg |= PCS_SSP_SOFT_RESET;
	writel(reg, base + REG_GUSB3PIPECTL0);

#ifdef CONFIG_S40_FPGA
	/* option1: configure for TMT, utmi 16bit interface */
	reg = readl(base + REG_GUSB2PHYCFG0);
	reg &= ~(0xf<<10);
	reg |= (0x9<<10);
	reg &= ~BIT_UTMI_8_16;
	reg &= ~(1<<6);       //disable suspend
	writel(reg, base + REG_GUSB2PHYCFG0);
	wmb();
	msleep(20);
#else
	/*step 3: USB2 PHY chose ulpi 8bit interface */
	reg = readl(base + REG_GUSB2PHYCFG0);
	reg &= ~BIT_UTMI_ULPI;
	reg &= ~(BIT_UTMI_8_16);
	writel(reg, base + REG_GUSB2PHYCFG0);
	wmb();
	msleep(20);
#endif

	reg = readl(base + REG_GCTL);
	reg &= ~(0x3<<12);
	reg |= (0x1<<12); /*[13:12] 01: Host; 10: Device; 11: OTG*/
	writel(reg, base + REG_GCTL);

	/* de-assert usb3phy hard-macro por */
	reg = readl(PERI_CRG123);
	reg &= ~USB3_PHY_SRST_REQ2;
	writel(reg, PERI_CRG123);
	msleep(100);

	reg = readl(PERI_CRG123);
	reg &= ~USB3_PHY_SRST_TREQ2; // de-assert hsp hard-macro port reset
	writel(reg, PERI_CRG123);

	reg = readl(base + REG_GUSB3PIPECTL0);
	reg &= ~(1<<17);       //disable suspend
	reg &= ~PCS_SSP_SOFT_RESET;
	writel(reg, base + REG_GUSB3PIPECTL0);
	msleep(100);

	writel(0x23100000, base + GTXTHRCFG);
	writel(0x23100000, base + GRXTHRCFG);
	msleep(20);



	local_irq_restore(flags);
}

EXPORT_SYMBOL(hiusb3_start_hcd);
void hiusb3_stop_hcd(void)
{
	unsigned long flags;
	unsigned int reg;

	local_irq_save(flags);

	reg = readl(PERI_CRG123);
	writel(reg | (USB3_VCC_SRST_REQ2), PERI_CRG123);
	msleep(500);
	local_irq_restore(flags);
}

EXPORT_SYMBOL(hiusb3_stop_hcd);

