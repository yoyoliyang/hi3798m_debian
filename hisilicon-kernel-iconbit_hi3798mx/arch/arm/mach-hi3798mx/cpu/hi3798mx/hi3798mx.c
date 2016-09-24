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
#include <asm/system.h>
#include <mach/platform.h>
#include <asm/io.h>
#include <mach/clock.h>
#include <mach/io.h>
#include <mach/cpu.h>


extern struct device_resource hi3798mx_hinfc610_device_resource;
/*****************************************************************************/

int usb_dev_dma_fixed  = 1;
EXPORT_SYMBOL(usb_dev_dma_fixed);

static struct device_resource *hi3798mx_device_resource[] = {
	&hi3798mx_hinfc610_device_resource,
	NULL,
};
/*****************************************************************************/
extern struct clk hi3798mx_hinfc610_clk;

static struct clk hi3798mx_uart_clk = {
#ifdef CONFIG_S40_FPGA 
	.rate = 54000000,
#else
	.rate = 83300000,
#endif
};
static struct clk hi3798mx_uart1_clk = {
#ifdef CONFIG_S40_FPGA 
	.rate = 54000000,
#else
	.rate = 3000000,
#endif
};

static struct clk_lookup hi3798mx_lookups[] = {
	{
		.dev_id = "hinfc610",
		.clk    = &hi3798mx_hinfc610_clk,
	}, { /* UART0 */
		.dev_id		= "uart:0",
		.clk		= &hi3798mx_uart_clk,
	}, { /* UART1 */
		.dev_id		= "uart:1",
		.clk		= &hi3798mx_uart1_clk,
	}, { /* UART2 */
		.dev_id		= "uart:2",
		.clk		= &hi3798mx_uart_clk,
	}, 
};

/*****************************************************************************/

static void hi3798mx_cpu_init(struct cpu_info *info)
{
	info->clk_cpu    = 800000000;
	info->clk_timer  = 24000000;
	info->cpuversion = "";
	clkdev_add_table(hi3798mx_lookups,
		ARRAY_SIZE(hi3798mx_lookups));	
}
/*****************************************************************************/
/* Hi3798Mv100_a */
struct cpu_info hi3798mv100_a_cpu_info =
{
	.name = "Hi3798Mv100_a",
	.chipid = _HI3798MV100_A,
	.chipid_mask = _HI3798MV100_A_MASK,
	.resource = hi3798mx_device_resource,
	.init = hi3798mx_cpu_init,
};

/* Hi3798Mv100 */
struct cpu_info hi3798mv100_cpu_info =
{
	.name = "Hi3798Mv100",
	.chipid = _HI3798MV100,
	.chipid_mask = _HI3798MV100_MASK,
	.resource = hi3798mx_device_resource,
	.init = hi3798mx_cpu_init,
};

/* Hi3796Mv100 */
struct cpu_info hi3796mv100_cpu_info =
{
	.name = "Hi3796Mv100",
	.chipid = _HI3796MV100,
	.chipid_mask = _HI3798MV100_MASK,
	.resource = hi3798mx_device_resource,
	.init = hi3798mx_cpu_init,
};

