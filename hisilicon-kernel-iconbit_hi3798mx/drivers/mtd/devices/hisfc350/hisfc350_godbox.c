/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-02-06
 *
******************************************************************************/

#include "hisfc350_os.h"
#include "hisfc350.h"

/*****************************************************************************/

/*periph hisfc CGR31 register*/
#define HISFC350_CRG31                                        (0x00BC)
#define HISFC350_CRG31_RST                                    (1 << 0)
#define HISFC350_CRG31_CLKEN                                  (1 << 8)
#define HISFC350_CRG31_CLK_25M                                0x00050000
#define HISFC350_CRG31_CLK_50M                                0x00040000
#define HISFC350_CRG31_CLK_75M                                0x00060000
#define HISFC350_CFG31_CLK_SRC_OFFSET                         16

/* periph hisfc CGR31 register*/
#define HI3712_HISFC350_CRG31                                 (0x00BC)
#define HI3712_HISFC350_CRG31_RST                             (1 << 0)
#define HI3712_HISFC350_CRG31_CLKEN                           (1 << 8)

#define HI3712_HISFC350_CRG31_CLK_24M                         0x00000000
#define HI3712_HISFC350_CRG31_CLK_50M                         0x00090000
#define HI3712_HISFC350_CRG31_CLK_99M                         0x00080000
#define HI3712_HISFC350_CRG31_CLK_198M                        0x000a0000
#define HI3712_HISFC350_CRG31_CLK_149M                        0x000c0000
#define HI3712_HISFC350_CFG31_CLK_SRC_OFFSET                  16

/*****************************************************************************/

void hisfc350_set_system_clock(struct hisfc_host *host,
			       struct spi_operation *op, int clk_en)
{
	long long chipid = get_chipid();

	if (chipid == _HI3712_V100) {
		unsigned int regval = HI3712_HISFC350_CRG31_CLK_24M;
		if (op && op->clock)
			regval = (op->clock & 0xf0000);

		if (clk_en)
			regval |= HI3712_HISFC350_CRG31_CLKEN;

		if (readl(host->sysreg + HI3712_HISFC350_CRG31 ) != regval)
			writel(regval, (host->sysreg + HI3712_HISFC350_CRG31));
	} else {
		unsigned int regval = HISFC350_CRG31_CLK_25M;
		if (op && op->clock)
			regval = (op->clock & 0x70000);

		if (clk_en)
			regval |= HISFC350_CRG31_CLKEN;

		if (readl(host->sysreg + HISFC350_CRG31) != regval)
			writel(regval, (host->sysreg + HISFC350_CRG31));
	}
}

/*****************************************************************************/

void hisfc350_get_best_clock(unsigned int * clock)
{
	int ix,clk;
	long long chipid = get_chipid();
	if (chipid == _HI3712_V100) {
		unsigned int sysclk[] = {
			24,  HI3712_HISFC350_CRG31_CLK_24M,
			50,  HI3712_HISFC350_CRG31_CLK_50M,
			99,  HI3712_HISFC350_CRG31_CLK_99M,
			149, HI3712_HISFC350_CRG31_CLK_149M,
			198, HI3712_HISFC350_CRG31_CLK_198M,
			0,0,
		};

		clk = HI3712_HISFC350_CRG31_CLK_24M; 
		for (ix = 0; sysclk[ix] != 0; ix += 2) {
			if ((*clock) < sysclk[ix])
				break;
			clk = sysclk[ix+1];
		}

		(*clock) = clk;
	} else {
		unsigned int sysclk[] = {
			25, HISFC350_CRG31_CLK_25M,
			50, HISFC350_CRG31_CLK_50M,
			75, HISFC350_CRG31_CLK_75M,
			0,0,
		};

		clk = HISFC350_CRG31_CLK_25M;
		for (ix = 0; sysclk[ix] != 0; ix += 2) {
			if ((*clock) < sysclk[ix])
				break;
			clk = sysclk[ix+1]; 
		}
		(*clock) = clk;
	}
}
/*****************************************************************************/
#ifdef CONFIG_HISFC350_SHOW_CYCLE_TIMING
char *hisfc350_get_clock_str(unsigned int clk_reg)
{
	int ix;
	long long chipid = get_chipid();
	static char buffer[40];

	if (chipid == _HI3712_V100) {
		unsigned int clk_str[] = {
			0, 24,
			1, 24,
			2, 24,
			3, 24,
			4, 24,
			5, 24,
			6, 24,
			7, 24,
			8, 99,
			9, 50,
			10, 198,
			11, 198,
			12, 149,
			13, 149,
			14, 149,
			15, 149,
		};

		clk_reg = (clk_reg & 0xf0000)
			>> HI3712_HISFC350_CFG31_CLK_SRC_OFFSET;

		for (ix = 0; clk_str[ix] < 16; ix += 2) {
			if (clk_reg == clk_str[ix]) {
				snprintf(buffer, sizeof(buffer), "%dM",
					clk_str[ix+1]);
				break;
			}
		}
		return buffer;
	} else {
		unsigned int clk_str[] = {
			4, 50,         /* 0x100 : 50M */
			5, 25,         /* 0x101 : 25M */
			6, 75,         /* 0x110 : 75M */
			7, 75,         /* 0x111 : 75M */
			0, 0,
		};

		clk_reg = ((clk_reg & 0x70000) >> 
			HISFC350_CFG31_CLK_SRC_OFFSET);

		for (ix = 0; clk_str[ix] != 0; ix += 2) {
			if (clk_reg == clk_str[ix]) {
				snprintf(buffer, sizeof(buffer), "%dM",
					 clk_str[ix+1]);
				break;
			}
		}
		return buffer;
	}
}
#endif /* CONFIG_HISFC350_SHOW_CYCLE_TIMING */
