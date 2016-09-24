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
#include <mach/cpu-info.h>
#include <mach/hardware.h>

#define PERI_CRG46                      __io_address(0xF8A22000 + 0xb8)
#define USB2_BUS_CKEN                   (1<<0)
#define USB2_OHCI48M_CKEN               (1<<1)
#define USB2_OHCI12M_CKEN               (1<<2)
#define USB2_HST_PHY_CKEN               (1<<4)
#define USB2_UTMI0_CKEN                 (1<<5)
#define USB2_UTMI1_CKEN                 (1<<6)
#define USB2_UTMI2_CKEN                 (1<<7)
#define USB2_BUS_SRST_REQ               (1<<12)
#define USB2_UTMI0_SRST_REQ             (1<<13)
#define USB2_UTMI1_SRST_REQ             (1<<14)
#define USB2_UTMI2_SRST_REQ             (1<<15)
#define USB2_HST_PHY_SYST_REQ           (1<<16)


#define PERI_CRG47                      __io_address(0xF8A22000 + 0xbc)
#define USB_PHY0_REF_CKEN               (1 << 0)
#define USB_PHY1_REF_CKEN               (1 << 1)
#define USB_PHY2_REF_CKEN               (1 << 2)
#define USB_PHY0_SRST_REQ               (1 << 8)
#define USB_PHY0_SRST_TREQ              (1 << 9)
#define USB_PHY1_SRST_REQ               (1 << 10)
#define USB_PHY1_SRST_TREQ              (1 << 11)
#define USB_PHY2_SRST_REQ               (1 << 12)
#define USB_PHY2_SRST_TREQ              (1 << 13)
#define USB_PHY0_REFCLK_SEL             (1 << 16)
#define USB_PHY1_REFCLK_SEL             (1 << 17)
#define USB_PHY2_REFCLK_SEL             (1 << 18)


#define PERI_USB0                       __io_address(0xF8A20000 + 0x120)
#define WORDINTERFACE                   (1 << 0)
#define ULPI_BYPASS_EN_PORT0            (1 << 3)
#define SS_BURST4_EN                    (1 << 7)
#define SS_BURST8_EN                    (1 << 8)
#define SS_BURST16_EN                   (1 << 9)

static atomic_t dev_open_cnt = {
	.counter = 0,
};

static void inno_phy_spec_config(void)
{
	writel(0x763d, __io_address(0xf8a20144));
	writel(0x763f, __io_address(0xf8a20144));
	writel(0x763d, __io_address(0xf8a20144));
	udelay(1);
	writel(0x1805d, __io_address(0xf8a20144));
	writel(0x1805f, __io_address(0xf8a20144));
	writel(0x1805d, __io_address(0xf8a20144));
	udelay(50);
	writel(0x7e3d, __io_address(0xf8a20144));
	writel(0x7e3f, __io_address(0xf8a20144));
	writel(0x7e3d, __io_address(0xf8a20144));
	mdelay(1);
	writel(0x1c05d, __io_address(0xf8a20144));
	writel(0x1c05f, __io_address(0xf8a20144));
	writel(0x1c05d, __io_address(0xf8a20144));
	mdelay(1);
}

void hiusb_start_hcd_s5(void)
{
	if (atomic_add_return(1, &dev_open_cnt) == 1) {

		int reg;

		/* reset enable */
		reg = readl(PERI_CRG46);
		reg |= (USB2_BUS_SRST_REQ
			| USB2_UTMI0_SRST_REQ
			| USB2_UTMI1_SRST_REQ
			| USB2_UTMI2_SRST_REQ
			| USB2_HST_PHY_SYST_REQ);

		writel(reg, PERI_CRG46);
		udelay(200);

		reg = readl(PERI_CRG47);
		reg |= (USB_PHY0_SRST_REQ
			| USB_PHY1_SRST_REQ
			| USB_PHY2_SRST_REQ
			| USB_PHY0_SRST_TREQ
			| USB_PHY1_SRST_TREQ
			| USB_PHY2_SRST_TREQ);
		writel(reg, PERI_CRG47);
		udelay(200);

#ifdef CONFIG_S5_FPGA
		reg = readl(PERI_USB0);
		reg |= ULPI_BYPASS_EN_PORT0;     /* port0:UTMI 16bit */
		reg |= (WORDINTERFACE);          /*  port0 : 16bit*/
		reg &= ~(SS_BURST16_EN);
		writel(reg, PERI_USB0);
		udelay(100);
#else
		reg = readl(PERI_USB0);
		reg |= ULPI_BYPASS_EN_PORT0;  /* 3 ports utmi */
		reg &= ~(WORDINTERFACE);      /* 8bit */
		reg &= ~(SS_BURST16_EN);      /* 16 bit burst disable */
		writel(reg, PERI_USB0);
		udelay(100);
#endif

		/* open clock for CTRL & PHY */
		reg = readl(PERI_CRG47);
		reg |= (USB_PHY0_REF_CKEN
			| USB_PHY1_REF_CKEN
			| USB_PHY2_REF_CKEN);
		writel(reg , PERI_CRG47);

		reg = readl(PERI_CRG46);
		reg |= (USB2_BUS_CKEN
			| USB2_OHCI48M_CKEN
			| USB2_OHCI12M_CKEN
			| USB2_HST_PHY_CKEN
			| USB2_UTMI0_CKEN
			| USB2_UTMI1_CKEN
			| USB2_UTMI2_CKEN);
		writel(reg, PERI_CRG46);
		udelay(300);

		/* cancel power on reset */
		reg = readl(PERI_CRG47);
		reg &= ~(USB_PHY0_SRST_REQ
			| USB_PHY1_SRST_REQ
			| USB_PHY2_SRST_REQ);
		writel(reg , PERI_CRG47);
		udelay(300);

		inno_phy_spec_config();
		/* cancel port reset */
		reg = readl(PERI_CRG47);
		reg &=~(USB_PHY0_SRST_TREQ
			| USB_PHY1_SRST_TREQ
			| USB_PHY2_SRST_TREQ);

		writel(reg, PERI_CRG47);
		udelay(300);

		/* cancel control reset */
		reg = readl(PERI_CRG46);
		reg &=~(USB2_BUS_SRST_REQ
			| USB2_UTMI0_SRST_REQ
			| USB2_UTMI1_SRST_REQ
			| USB2_UTMI2_SRST_REQ
			| USB2_HST_PHY_SYST_REQ);
		writel(reg, PERI_CRG46);
		udelay(200);

	}


}

void hiusb_stop_hcd_s5(void)
{
	if (atomic_sub_return(1, &dev_open_cnt) == 0) {

		int reg;

		reg = readl(PERI_CRG47);
		reg |=(USB_PHY0_SRST_REQ
			| USB_PHY1_SRST_REQ
			| USB_PHY2_SRST_REQ
			| USB_PHY0_SRST_TREQ
			| USB_PHY1_SRST_TREQ
			| USB_PHY2_SRST_TREQ);
		writel(reg, PERI_CRG47);
		udelay(100);

		/* close clock */
		reg = readl(PERI_CRG47);
		reg &=~ (USB_PHY0_REFCLK_SEL
			| USB_PHY1_REFCLK_SEL
			| USB_PHY2_REFCLK_SEL
			| USB_PHY0_REF_CKEN
			| USB_PHY1_REF_CKEN
			| USB_PHY2_REF_CKEN);
		writel(reg, PERI_CRG47);
		udelay(300);

		/* close clock  */
		reg = readl(PERI_CRG46);
		reg &=~(USB2_BUS_CKEN
			| USB2_OHCI48M_CKEN
			| USB2_OHCI12M_CKEN
			| USB2_HST_PHY_CKEN
			| USB2_UTMI0_CKEN
			| USB2_UTMI1_CKEN
			| USB2_UTMI2_CKEN);
		writel(reg, PERI_CRG46);
		udelay(200);
	}

}
