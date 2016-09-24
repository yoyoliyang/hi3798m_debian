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
#include <mach/cpu.h>

extern struct device_resource hi3716cv200es_hinfc504_device_resource;
/*****************************************************************************/

static struct device_resource *hi3716cv200es_device_resource[] = {
	&hi3716cv200es_hinfc504_device_resource,
	NULL,
};
/*****************************************************************************/
extern struct clk hi3716cv200es_hinfc504_clk;

static struct clk hi3716cv200es_uart_clk = {
#ifdef CONFIG_S40_FPGA 
	.rate = 54000000,
#else
	.rate = 85700000,
#endif
};
static struct clk hi3716cv200es_uart1_clk = {
#ifdef CONFIG_S40_FPGA 
	.rate = 54000000,
#else
	.rate = 3000000,
#endif
};

static struct clk_lookup hi3716cv200es_lookups[] = {
	{
		.dev_id = "hinfc504",
		.clk    = &hi3716cv200es_hinfc504_clk,
	}, { /* UART0 */
		.dev_id		= "uart:0",
		.clk		= &hi3716cv200es_uart_clk,
	}, { /* UART1 */
		.dev_id		= "uart:1",
		.clk		= &hi3716cv200es_uart1_clk,
	}, { /* UART2 */
		.dev_id		= "uart:2",
		.clk		= &hi3716cv200es_uart_clk,
	}, { /* UART3 */
		.dev_id		= "uart:3",
		.clk		= &hi3716cv200es_uart_clk,
	}, { /* UART4 */
		.dev_id		= "uart:4",
		.clk		= &hi3716cv200es_uart_clk,
	}, 
};

/*****************************************************************************/

static void hi3716cv200es_cpu_init(struct cpu_info *info)
{
	info->clk_cpu    = 800000000;
	info->clk_timer  = 24000000;
	info->cpuversion = "";
	clkdev_add_table(hi3716cv200es_lookups,
		ARRAY_SIZE(hi3716cv200es_lookups));

}
/*****************************************************************************/

struct cpu_info hi3716cv200es_cpu_info =
{
	.name = "Hi3716Cv200es",
	.chipid = _HI3716CV200ES,
	.chipid_mask = _HI3716CV200ES_MASK,
	.resource = hi3716cv200es_device_resource,
	.init = hi3716cv200es_cpu_init,
};
