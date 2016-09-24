/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-02-07
 *
******************************************************************************/

#include <linux/err.h>
#include <linux/completion.h>
#include <linux/kernel.h>
#include <asm/mach/resource.h>
#include <mach/clock.h>
#include <asm/setup.h>
#include <mach/hardware.h>

/*****************************************************************************/

#define HI3716CV200_HINFC610_PERI_CRG_REG_BASE  __io_address(REG_BASE_CRG)
#define HI3716CV200_PERI_CRG24                  (HI3716CV200_HINFC610_PERI_CRG_REG_BASE + 0x60)
#define HI3716CV200_PERI_CRG24_CLK_EN           (0x1U << 0)
#define HI3716CV200_PERI_CRG24_CLK_SEL_MASK     (0x7U << 8)
#define HI3716CV200_PERI_CRG24_CLK_SEL_24M      (0x0U << 8)
#define HI3716CV200_PERI_CRG24_CLK_SEL_200M     (0x4U << 8)
#define HI3716CV200_PERI_CRG24_CLK_SEL_150M     (0x5U << 8)
#define HI3716CV200_PERI_CRG24_CLK_SEL_125M     (0x6U << 8)
#define HI3716CV200_PERI_CRG24_CLK_SEL_100M     (0x7U << 8)
#define HI3716CV200_PERI_CRG24_NF_SRST_REQ      (0x1U << 4)

static int hi3716mv300_hinfc610_enable(struct clk *clk)
{
	unsigned long reg_val;

	reg_val = readl(HI3716CV200_PERI_CRG24);
	reg_val &= ~HI3716CV200_PERI_CRG24_CLK_SEL_MASK;
	reg_val |= (HI3716CV200_PERI_CRG24_CLK_EN
		| HI3716CV200_PERI_CRG24_CLK_SEL_200M);
	writel(reg_val, HI3716CV200_PERI_CRG24);

	return 0;
}
/*****************************************************************************/

static void hi3716mv300_hinfc610_disable(struct clk *clk)
{
	unsigned long reg_val;

	reg_val = readl(HI3716CV200_PERI_CRG24);
	reg_val &= ~HI3716CV200_PERI_CRG24_CLK_SEL_MASK;
	reg_val &= ~HI3716CV200_PERI_CRG24_CLK_EN;
	writel(reg_val, HI3716CV200_PERI_CRG24);
}
/*****************************************************************************/

static struct clk_ops hi3716cv200_hinfc610_clk_ops = {
	.enable = hi3716mv300_hinfc610_enable,
	.disable = hi3716mv300_hinfc610_disable,
	.set_rate = NULL,
	.get_rate = NULL,
	.round_rate = NULL,
};
/*****************************************************************************/

struct clk hi3716cv200_hinfc610_clk = {
	.ops  = &hi3716cv200_hinfc610_clk_ops,
};
/*****************************************************************************/

static struct resource hi3716cv200_hinfc610_resources[] = {
	{
		.name   = "base",
		.start  = 0xF9810000,
		.end    = 0xF9810000 + 0x100,
		.flags  = IORESOURCE_MEM,
	}, {
		.name   = "buffer",
		.start  = 0xFE000000,
		.end    = 0xFE000000 + 2048 + 128,
		.flags  = IORESOURCE_MEM,
	},
};
/*****************************************************************************/

struct device_resource hi3716cv200_hinfc610_device_resource = {
	.name           = "hinfc610",
	.resource       = hi3716cv200_hinfc610_resources,
	.num_resources  = ARRAY_SIZE(hi3716cv200_hinfc610_resources),
};
/*****************************************************************************/
