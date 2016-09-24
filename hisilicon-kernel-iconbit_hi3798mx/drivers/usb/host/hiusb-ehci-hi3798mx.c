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
#define USB2_OTG_UTMI_CKEN              (1<<3)
#define USB2_HST_PHY_CKEN               (1<<4)
#define USB2_UTMI0_CKEN                 (1<<5)
#define USB2_UTMI1_CKEN                 (1<<6)
#define USB2_BUS_SRST_REQ               (1<<12)
#define USB2_UTMI0_SRST_REQ             (1<<13)
#define USB2_UTMI1_SRST_REQ             (1<<14)
#define USB2_HST_PHY_SYST_REQ           (1<<16)
#define USB2_OTG_PHY_SRST_REQ           (1<<17)

#define PERI_CRG47                      __io_address(0xF8A22000 + 0xbc)
#define USB_PHY0_REF_CKEN               (1 << 0)
#define USB_PHY1_REF_CKEN               (1 << 1)
#define USB_PHY0_SRST_REQ               (1 << 8)
#define USB_PHY0_SRST_TREQ              (1 << 9)
#define USB_PHY1_SRST_REQ               (1 << 10)
#define USB_PHY1_SRST_TREQ              (1 << 11)

#define PERI_USB0                       __io_address(0xF8A20000 + 0x120)
#define WORDINTERFACE                   (1 << 0)
#define ULPI_BYPASS_EN_PORT0            (1 << 3)
#define SS_BURST16_EN                   (1 << 9)

#define PERI_USB1                       __io_address(0xF8A20000 + 0x124)

#define PERI_CRG100                     __io_address(0xF8A22000 + 0x190)
#define USB2_PHY2_REF_CKEN               (1 << 0)
#define USB2_PHY2_SRST_REQ               (1 << 8)
#define USB2_PHY2_SRST_TREQ              (1 << 9)

#define PERI_CRG102                     __io_address(0xF8A22000 + 0x198)
#define USB2_BUS_CKEN1                  (1<<0)
#define USB2_OHCI48M_CKEN1              (1<<1)
#define USB2_OHCI12M_CKEN1              (1<<2)
#define USB2_HST_PHY_CKEN1              (1<<4)
#define USB2_UTMI0_CKEN1                (1<<5)
#define USB2_BUS_SRST_REQ1              (1<<12)
#define USB2_UTMI0_SRST_REQ1            (1<<14)
#define USB2_HST_PHY_SYST_REQ1          (1<<16)

#define PERI_USB13                      __io_address(0xF8A20000 + 0x154)
#define PERI_USB14                      __io_address(0xF8A20000 + 0x158)
#define TEST_WRDATA                     (0xc)
#define TEST_ADDR                       (0x6 << 8)
#define TEST_WREN                       (1 << 13)
#define TEST_CLK                        (1 << 14)
#define TEST_RSTN                       (1 << 15)

static atomic_t dev_open_cnt = {
	.counter = 0,
};

static atomic_t dev_open_cnt1 = {
	.counter = 0,
};

void hiusb_start_hcd_hi3798mx(resource_size_t host_addr)
{
	int reg;

	if (CONFIG_HIUSB_EHCI1_IOBASE == host_addr ||
			CONFIG_HIUSB_OHCI1_IOBASE == host_addr) {
		if (atomic_add_return(1, &dev_open_cnt1) == 1) {
			/* reset enable */
			reg = readl(PERI_CRG102);
			reg |= (USB2_BUS_SRST_REQ1 //12th bit
				| USB2_UTMI0_SRST_REQ1 //14th bit
				| USB2_HST_PHY_SYST_REQ1);//16th bit
			writel(reg, PERI_CRG102);
			udelay(200);

			reg = readl(PERI_CRG100);
			reg |= (USB2_PHY2_SRST_REQ//8th bit
				| USB2_PHY2_SRST_TREQ);//9th bit
			writel(reg, PERI_CRG100);
			udelay(200);

#ifdef CFG_FPGA
			reg = readl(PERI_USB13);
			reg &= ~ULPI_BYPASS_EN_PORT0;     /* port2:ulpi */
			reg &= ~(WORDINTERFACE);         /*  port2 : 16bit*/
			reg &= ~(SS_BURST16_EN);
			writel(reg, PERI_USB13);
			udelay(100);
#else
			reg = readl(PERI_USB13);
			reg |= ULPI_BYPASS_EN_PORT0;  /* 1 ports utmi */
			reg &= ~(WORDINTERFACE);      /* 8bit */
			reg &= ~(SS_BURST16_EN);      /* 16 bit burst disable */
			writel(reg, PERI_USB13);
			udelay(100);
#endif

			/* open ref clk */
			reg = readl(PERI_CRG100);
			reg |= (USB2_PHY2_REF_CKEN);
			writel(reg, PERI_CRG100);
			udelay(300);

			/* cancel power on reset */
			reg = readl(PERI_CRG100);
			reg &= ~(USB2_PHY2_SRST_REQ);
			writel(reg, PERI_CRG100);
			udelay(300);

			/* config clk output */
			reg = TEST_WRDATA|TEST_ADDR|TEST_WREN|TEST_RSTN;
			writel(reg, PERI_USB14);
			udelay(500);

			reg = TEST_WRDATA|TEST_ADDR|TEST_WREN|TEST_RSTN|TEST_CLK;
			writel(reg, PERI_USB14);
			udelay(500);

			reg = TEST_WRDATA|TEST_ADDR|TEST_WREN|TEST_RSTN;
			writel(reg, PERI_USB14);
			udelay(500);
#ifdef CONFIG_S40_FPGA
			mdelay(1);
#else
			mdelay(10);
#endif

			/* solve 1p phy disconnect problem */
			writel(0xaa02, PERI_USB14);
			writel(0xea02, PERI_USB14);
			writel(0xaa02, PERI_USB14);
			udelay(500);

			/* cancel port reset */
			reg = readl(PERI_CRG100);
			reg &= ~(USB2_PHY2_SRST_TREQ);
			writel(reg, PERI_CRG100);
			udelay(300);

			/* cancel control reset */
			reg = readl(PERI_CRG102);
			reg &= ~(USB2_BUS_SRST_REQ1
				| USB2_UTMI0_SRST_REQ1
				| USB2_HST_PHY_SYST_REQ1);
			reg |= (USB2_BUS_CKEN1
				| USB2_OHCI48M_CKEN1
				| USB2_OHCI12M_CKEN1
				| USB2_HST_PHY_CKEN1
				| USB2_UTMI0_CKEN1);
			writel(reg, PERI_CRG102);
			udelay(200);
		}

	} else if (CONFIG_HIUSB_EHCI_IOBASE == host_addr ||
			CONFIG_HIUSB_OHCI_IOBASE == host_addr
#ifdef CONFIG_HIUSBUDC_REG_BASE_ADDRESS
			|| CONFIG_HIUSBUDC_REG_BASE_ADDRESS == host_addr
#endif
			) {
		if (atomic_add_return(1, &dev_open_cnt) == 1) {

			/* reset enable */
			reg = readl(PERI_CRG46);
			reg |= (USB2_BUS_SRST_REQ
				| USB2_UTMI0_SRST_REQ
				| USB2_UTMI1_SRST_REQ
				| USB2_HST_PHY_SYST_REQ
				| USB2_OTG_PHY_SRST_REQ);
			writel(reg, PERI_CRG46);
			udelay(200);

			reg = readl(PERI_CRG47);
			reg |= (USB_PHY0_SRST_REQ
				| USB_PHY1_SRST_REQ
				| USB_PHY0_SRST_TREQ
				| USB_PHY1_SRST_TREQ);
			writel(reg, PERI_CRG47);
			udelay(200);

#ifdef CONFIG_S40_FPGA
			reg = readl(PERI_USB0);
			reg |= ULPI_BYPASS_EN_PORT0;     /* port0:UTMI 16bit */
			reg |= (WORDINTERFACE);          /*  port0 : 16bit*/
			reg &= ~(SS_BURST16_EN);
			writel(reg, PERI_USB0);
			udelay(100);
#else
			reg = readl(PERI_USB0);
			reg |= ULPI_BYPASS_EN_PORT0;  /* 1 ports utmi */
			reg &= ~(WORDINTERFACE);      /* 8bit */
			reg &= ~(SS_BURST16_EN);      /* 16 bit burst disable */
			writel(reg, PERI_USB0);
			udelay(100);
#endif

			/* open ref clk */
			reg = readl(PERI_CRG47);
			reg |= (USB_PHY0_REF_CKEN
				| USB_PHY1_REF_CKEN);
			writel(reg, PERI_CRG47);
			udelay(300);

			/* cancel power on reset */
			reg = readl(PERI_CRG47);
			reg &= ~(USB_PHY0_SRST_REQ
			| USB_PHY1_SRST_REQ);
			writel(reg , PERI_CRG47);
			udelay(300);

			/* avoid inno phy error */
			/* 1. address 0x07, send data 0x3b */
			writel(0xa73b, PERI_USB1);
			writel(0xe73b, PERI_USB1);
			writel(0xa73b, PERI_USB1);
			udelay(500);
			/*
			 * >=1us later
			 * 2. address 0x0b, send data 0xc0
			 */
			writel(0xabc0, PERI_USB1);
			writel(0xebc0, PERI_USB1);
			writel(0xabc0, PERI_USB1);
			udelay(500);
			/*
			 * >=50us later
			 * 3. address 0x07, send data 0x3f
			 */
			writel(0xa73f, PERI_USB1);
			writel(0xe73f, PERI_USB1);
			writel(0xa73f, PERI_USB1);
			udelay(500);
			udelay(500);

			/*
			 * >=1ms later
			 * 4. address 0x0b, send data 0xe0
			 */
			writel(0xabe0, PERI_USB1);
			writel(0xebe0, PERI_USB1);
			writel(0xabe0, PERI_USB1);
			udelay(500);
			udelay(500);

			/* solve 2p phy disconnect issue */
			writel(0xaa02, PERI_USB1);
			writel(0xea02, PERI_USB1);
			writel(0xaa02, PERI_USB1);
			writel(0xba1e, PERI_USB1);
			writel(0xfa1e, PERI_USB1);
			writel(0xba1e, PERI_USB1);
			udelay(500);

			/* cancel port reset */
			reg = readl(PERI_CRG47);
			reg &=~(USB_PHY0_SRST_TREQ
				| USB_PHY1_SRST_TREQ);
			writel(reg, PERI_CRG47);
			udelay(300);

			/* cancel control reset */
			reg = readl(PERI_CRG46);
			reg &=~(USB2_BUS_SRST_REQ
				| USB2_UTMI0_SRST_REQ
				| USB2_UTMI1_SRST_REQ
				| USB2_HST_PHY_SYST_REQ
				| USB2_OTG_PHY_SRST_REQ);

			reg |= (USB2_BUS_CKEN
				| USB2_OHCI48M_CKEN
				| USB2_OHCI12M_CKEN
				| USB2_OTG_UTMI_CKEN
				| USB2_HST_PHY_CKEN
				| USB2_UTMI0_CKEN
				| USB2_UTMI1_CKEN);
			writel(reg, PERI_CRG46);
			udelay(200);
		}
	}
	return;
}

void hiusb_stop_hcd_hi3798mx(resource_size_t host_addr)
{
	int reg;

	if (CONFIG_HIUSB_EHCI1_IOBASE == host_addr ||
			CONFIG_HIUSB_OHCI1_IOBASE == host_addr) {
		if (atomic_sub_return(1, &dev_open_cnt1) == 0) {

			reg = readl(PERI_CRG102);
			reg |= (USB2_BUS_SRST_REQ1
				| USB2_UTMI0_SRST_REQ1
				| USB2_HST_PHY_SYST_REQ1);
			writel(reg, PERI_CRG102);
			udelay(200);

			reg = readl(PERI_CRG100);
			reg |= (USB2_PHY2_SRST_REQ
				| USB2_PHY2_SRST_TREQ);
			writel(reg, PERI_CRG100);
			udelay(100);
		}
	} else if (CONFIG_HIUSB_EHCI_IOBASE == host_addr ||
			CONFIG_HIUSB_OHCI_IOBASE == host_addr
#ifdef CONFIG_HIUSBUDC_REG_BASE_ADDRESS
			|| CONFIG_HIUSBUDC_REG_BASE_ADDRESS == host_addr
#endif
			) {

		if (atomic_sub_return(1, &dev_open_cnt) == 0) {
			reg = readl(PERI_CRG47);
			reg |=(USB_PHY0_SRST_REQ
				| USB_PHY1_SRST_REQ
				| USB_PHY0_SRST_TREQ
				| USB_PHY1_SRST_TREQ);
			writel(reg, PERI_CRG47);
			udelay(100);

			/* close clock  */
			reg = readl(PERI_CRG46);
			reg &=~(USB2_BUS_CKEN
				| USB2_HST_PHY_CKEN
				| USB2_UTMI0_CKEN
				| USB2_UTMI1_CKEN);
			writel(reg, PERI_CRG46);
			udelay(200);
		}
	}
}
