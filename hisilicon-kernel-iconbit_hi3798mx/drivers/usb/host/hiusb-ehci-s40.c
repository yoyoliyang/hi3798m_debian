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
#define USB2_UTMI2_CKEN                 (1<<7)
#define USB2_ADP_CKEN                   (1<<8)
#define USB2_BUS_SRST_REQ               (1<<12)
#define USB2_UTMI0_SRST_REQ             (1<<13)
#define USB2_UTMI1_SRST_REQ             (1<<14)
#define USB2_UTMI2_SRST_REQ             (1<<15)
#define USB2_HST_PHY_SYST_REQ           (1<<16)
#define USB2_OTG_PHY_SRST_REQ           (1<<17)
#define USB2_ADP_SRST_REQ               (1<<18)


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
#define ULPI_BYPASS_EN_PORT1_2          (1 << 4)
#define SS_BURST4_EN                    (1 << 7)
#define SS_BURST8_EN                    (1 << 8)
#define SS_BURST16_EN                   (1 << 9)
#define DWC_OTG_EN                      (1 << 28)

#define PERI_USB1                       __io_address(0xF8A20000 + 0x124)
#define PHY0_TXPREEMPAMPTUNE_MASK       ~(0x3 << 27)
#define PHY0_TXPREEMPAMPTUNE_VALUE      (0x3 << 27)
#define PHY0_SIDDQ_MASK                 ~(0x1 << 22)
#define PHY0_SIDDQ_VALUE                (0x1 << 22)

#define PERI_USB2                       __io_address(0xF8A20000 + 0x128)
#define PHY3_TXPREEMPAMPTUNE_MASK       ~(0x3 << 27)
#define PHY3_TXPREEMPAMPTUNE_VALUE      (0x3 << 27)
#define PHY3_TXVREFTUNE_MASK            ~(0xF << 0)
#define PHY3_TXVREFTUNE_VALUE           (0x8 << 0)
#define PHY3_SIDDQ_MASK                 ~(0x1 << 22)
#define PHY3_SIDDQ_VALUE                (0x1 << 22)

#define PERI_USB4                       __io_address(0xF8A20000 + 0x130)
#define PHY2_TXPREEMPAMPTUNE_MASK       ~(0x3 << 27)
#define PHY2_TXPREEMPAMPTUNE_VALUE      (0x3 << 27)
#define PHY2_TXVREFTUNE_MASK            ~(0xF << 0)
#define PHY2_TXVREFTUNE_VALUE           (0x8 << 0)
#define PHY2_SIDDQ_MASK                 ~(0x1 << 22)
#define PHY2_SIDDQ_VALUE                (0x1 << 22)

extern long long get_chipid(void);

static atomic_t dev_open_cnt = {
	.counter = 0,
};

void hiusb_start_hcd_s40(void)
{
	if (atomic_add_return(1, &dev_open_cnt) == 1) {

		int reg;

		/* power down phy for eye diagram */
		reg = readl(PERI_USB1);
		reg |= PHY0_SIDDQ_VALUE;     
		writel(reg, PERI_USB1);
		
		reg = readl(PERI_USB2);
		reg |= PHY3_SIDDQ_VALUE;    
		writel(reg, PERI_USB2);
		
		reg = readl(PERI_USB4);
		reg |= PHY2_SIDDQ_VALUE;   
		writel(reg, PERI_USB4);
		udelay(2000);
		
		/* reset enable */
		reg = readl(PERI_CRG46);
		reg |= (USB2_BUS_SRST_REQ
			| USB2_UTMI0_SRST_REQ
			| USB2_UTMI1_SRST_REQ
			| USB2_UTMI2_SRST_REQ
			| USB2_HST_PHY_SYST_REQ
			| USB2_OTG_PHY_SRST_REQ);

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

#ifdef CONFIG_S40_FPGA
		reg = readl(PERI_USB0);
		reg |= ULPI_BYPASS_EN_PORT0;     /* port0:UTMI 16bit */
		reg &= ~ULPI_BYPASS_EN_PORT1_2;  /*  port1,port2:ulpi*/
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
		/* for ssk usb storage ok */
		msleep(10);
		
		/* cancel power on reset */
		reg = readl(PERI_CRG47);
		reg &= ~(USB_PHY0_SRST_REQ
			| USB_PHY1_SRST_REQ
			| USB_PHY2_SRST_REQ);
		writel(reg , PERI_CRG47);
		udelay(300);

		if ((_HI3719MV100 == get_chipid()) || (_HI3718MV100 == get_chipid())) {
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
		}

		/* cancel port reset */
		reg = readl(PERI_CRG47);
		reg &=~(USB_PHY0_SRST_TREQ
			| USB_PHY1_SRST_TREQ
			| USB_PHY2_SRST_TREQ);
		if (_HI3716CV200ES == get_chipid()) {
			reg |= (USB_PHY0_REFCLK_SEL
				| USB_PHY1_REFCLK_SEL
				| USB_PHY2_REFCLK_SEL
				| USB_PHY0_REF_CKEN
				| USB_PHY1_REF_CKEN
				| USB_PHY2_REF_CKEN);
		} else {
			reg |= (USB_PHY0_REF_CKEN
				| USB_PHY1_REF_CKEN
				| USB_PHY2_REF_CKEN);
		}
		writel(reg, PERI_CRG47);
		udelay(300);

		/* cancel control reset */
		reg = readl(PERI_CRG46);
		reg &=~(USB2_BUS_SRST_REQ
			| USB2_UTMI0_SRST_REQ
			| USB2_UTMI1_SRST_REQ
			| USB2_UTMI2_SRST_REQ
			| USB2_HST_PHY_SYST_REQ
			| USB2_OTG_PHY_SRST_REQ);

		reg |= (USB2_BUS_CKEN
			| USB2_OHCI48M_CKEN
			| USB2_OHCI12M_CKEN
			| USB2_OTG_UTMI_CKEN
			| USB2_HST_PHY_CKEN
			| USB2_UTMI0_CKEN
			| USB2_UTMI1_CKEN
			| USB2_UTMI2_CKEN
			| USB2_ADP_CKEN);
		writel(reg, PERI_CRG46);
		udelay(200);

		/* Hi3719MV100 and Hi3718MV100 need not set PERI_USB1/2/4 */
		if ((_HI3719MV100 == get_chipid()) || (_HI3718MV100 == get_chipid())) {
			goto out;
		}

		reg = readl(PERI_USB1);
		if (_HI3716CV200ES == get_chipid()) {
			reg &= PHY0_TXPREEMPAMPTUNE_MASK;
			reg |= PHY0_TXPREEMPAMPTUNE_VALUE;
		}
		reg &= PHY0_SIDDQ_MASK;
		reg &= ~PHY0_SIDDQ_VALUE;
		writel(reg, PERI_USB1);
		udelay(200);

		reg = readl(PERI_USB2);
		if (_HI3716CV200ES == get_chipid()) {
			reg &= PHY3_TXPREEMPAMPTUNE_MASK;
			reg |= PHY3_TXPREEMPAMPTUNE_VALUE;
		}
		reg &= PHY3_SIDDQ_MASK;
		reg &= ~PHY3_SIDDQ_VALUE;
		writel(reg, PERI_USB2);
		udelay(200);

		if (_HI3716CV200ES == get_chipid()) {
			reg = readl(PERI_USB2);
			reg &= PHY3_TXVREFTUNE_MASK;
			reg |= PHY3_TXVREFTUNE_VALUE;
			writel(reg, PERI_USB2);
			udelay(200);
		}

		if (_HI3716CV200ES == get_chipid()) {
			/* Fix the disconnect issue Of Inno Phy */
			writel(0xa207, PERI_USB4);
			writel(0xe207, PERI_USB4);
			writel(0xa207, PERI_USB4);
			writel(0xa1a6, PERI_USB4);
			writel(0xe1a6, PERI_USB4);
			writel(0xa1a6, PERI_USB4);
			writel(0xaa1b, PERI_USB4);
			writel(0xea1b, PERI_USB4);
			writel(0xaa1b, PERI_USB4);
		} else {
			reg = readl(PERI_USB4);
			reg &= PHY0_SIDDQ_MASK;
			reg &= ~PHY0_SIDDQ_VALUE;
			writel(reg, PERI_USB4);
			udelay(200);
		}

	}
out:
	return;
}

void hiusb_stop_hcd_s40(void)
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
			| USB2_OTG_UTMI_CKEN
			| USB2_HST_PHY_CKEN
			| USB2_UTMI0_CKEN
			| USB2_UTMI1_CKEN
			| USB2_UTMI2_CKEN
			| USB2_ADP_CKEN);
		writel(reg, PERI_CRG46);
		udelay(200);
	}

}
