#include <linux/module.h>
#include <linux/hibernate_param.h>
#include <mach/hardware.h>
#include <asm/smp_scu.h>

#define UART_ADDR	uart_addrs[HIBERNATE_CONSOLE]
#define UART_NUM	5

extern void hi_pm_hibernate_suspend (void);
extern void hi_pm_hibernate_resume (void);

static unsigned long uart_addrs[] = {
	(unsigned long)IO_ADDRESS(REG_BASE_UART0),
	(unsigned long)IO_ADDRESS(REG_BASE_UART1),
	(unsigned long)IO_ADDRESS(REG_BASE_UART3),
};

static void __iomem *scu_base_addr
	= (void __iomem *)IO_ADDRESS(REG_BASE_A9_PERI + REG_A9_PERI_SCU);

#ifdef CONFIG_PM_HIBERNATE_DEBUG

static void hibernate_putchar (char c)
{
	while ((*((volatile unsigned short *)(UART_ADDR + 0x18)) & 0x20))
		;
	*((volatile unsigned short *)(UART_ADDR + 0x00)) = c;
}

#endif /* CONFIG_PM_HIBERNATE_DEBUG */

static int hibernate_snapshot (void)
{
	int ret = 0;

	/* UserAPI */
	hibernate_param.private[0] = userapi_addr;

	/* GPIO */

	/* IOMUX */

	hi_pm_hibernate_suspend();

	/* Clock */

	/* call hibernation driver */
	ret = hibdrv_snapshot();

#ifdef CONFIG_HAVE_ARM_SCU
	scu_enable(scu_base_addr);
#endif

	/* Clock */

	hi_pm_hibernate_resume ();

	/* IOMUX */

	/* GPIO */

	return ret;
}

static struct hibernate_ops hibernate_machine_ops = {
	.snapshot = hibernate_snapshot,
#ifdef CONFIG_PM_HIBERNATE_DEBUG
	.putc = hibernate_putchar,
#endif
};

static int __init hibernate_machine_init (void)
{
	return hibernate_register_machine(&hibernate_machine_ops);
}

static void __exit hibernate_machine_exit (void)
{
	hibernate_unregister_machine(&hibernate_machine_ops);
}

module_init(hibernate_machine_init);
module_exit(hibernate_machine_exit);

