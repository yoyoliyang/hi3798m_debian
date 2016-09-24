#include <mach/platform.h>

	.macro  addruart, rp, rv, tmp
	ldr	\rp, =CONFIG_DEFAULT_UART_BASE_ADDR @physical base
	ldr	\rv, =IO_ADDRESS(CONFIG_DEFAULT_UART_BASE_ADDR) @virtual base
	.endm

#include <asm/hardware/debug-pl01x.S>
