#include <linux/init.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <asm/byteorder.h>
#include <linux/io.h>
#include <asm/system.h>
#include <asm/unaligned.h>
#include <mach/hardware.h>

#define PERI_CRG36              (__io_address(0x101F5000 + 0xd0))
#define USB_CKEN                (1 << 8)
#define USB_CTRL_UTMI1_REG      (1 << 6)
#define USB_CTRL_UTMI0_REG      (1 << 5)
#define USB_CTRL_HUB_REG        (1 << 4)
#define USBPHY_PORT1_TREQ       (1 << 3)
#define USBPHY_PORT0_TREQ       (1 << 2)
#define USBPHY_REQ              (1 << 1)
#define USB_AHB_SRST_REQ        (1 << 0)
#define PERI_USB0               (__io_address(0x10200000 + 0x28))
#define WORDINTERFACE           (1 << 0)
#define ULPI_BYPASS_EN          (1 << 3)
#define SS_BURST4_EN            (1 << 7)
#define SS_BURST8_EN            (1 << 8)
#define SS_BURST16_EN           (1 << 9)

#define WDG_LOAD                (__io_address(0x10201000 + 0x0000))
#define WDG_CONTROL             (__io_address(0x10201000 + 0x0008))
#define WDG_LOCK                (__io_address(0x10201000 + 0x0c00))

void hiusb_start_hcd(void)
{
	int reg;
	unsigned long flags;

	local_irq_save(flags);

	reg = readl(PERI_CRG36);

	if (reg & USBPHY_REQ) {

		reg = readl(PERI_CRG36);
		reg |= USB_CKEN;
		reg |= USBPHY_REQ;
		reg &= ~(USBPHY_PORT1_TREQ);
		reg &= ~(USBPHY_PORT0_TREQ);
		reg |= USB_CTRL_UTMI1_REG;
		reg |= USB_CTRL_UTMI0_REG;
		reg |= USB_AHB_SRST_REQ;
		writel(reg, PERI_CRG36);
		udelay(20);

		reg = readl(PERI_USB0);
		reg |= ULPI_BYPASS_EN;
		reg &= ~(WORDINTERFACE);
		reg &= ~(SS_BURST16_EN);
		writel(reg, PERI_USB0);
		udelay(100);

		reg = readl(PERI_CRG36);
		reg &= ~(USBPHY_REQ);
		writel(reg, PERI_CRG36);
		udelay(100);

		reg = readl(PERI_CRG36);
		reg &= ~( USB_CTRL_UTMI1_REG);
		reg &= ~(USB_CTRL_UTMI0_REG);
		reg &= ~(USB_CTRL_HUB_REG);
		reg &= ~(USB_AHB_SRST_REQ);
		writel(reg, PERI_CRG36);
		udelay(10);

		reg = readl(WDG_CONTROL);
		if (!(reg & 0x01)) {
			writel(0x1ACCE551, WDG_LOCK);
			writel(0x5DC0, WDG_LOAD);
			writel(0x03, WDG_CONTROL);
			writel(0, WDG_LOCK);
			readl(IO_ADDRESS(0x60070000));
			writel(0x1ACCE551, WDG_LOCK);
			writel(0, WDG_CONTROL);
			writel(0, WDG_LOCK);
		}

	}

	local_irq_restore(flags);
}
EXPORT_SYMBOL(hiusb_start_hcd);

void hiusb_stop_hcd(void)
{
}
EXPORT_SYMBOL(hiusb_stop_hcd);
