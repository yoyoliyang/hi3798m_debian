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

extern long long get_chipid(void);
#ifdef CONFIG_ARCH_S40
extern void hiusb_start_hcd_s5(void);
extern void hiusb_start_hcd_s40(void);
extern void hiusb_stop_hcd_s5(void);
extern void hiusb_stop_hcd_s40(void);
#endif

#ifdef CONFIG_ARCH_HI3798MX
extern void hiusb_start_hcd_hi3798mx(resource_size_t host_addr);
extern void hiusb_stop_hcd_hi3798mx(resource_size_t host_addr);
#endif

void hiusb_start_hcd(resource_size_t host_addr)
{
#ifdef CONFIG_ARCH_S40
	long long chipid = get_chipid();
	switch (chipid) {
		case _HI3798CV100A:
		case _HI3798CV100:
		case _HI3796CV100:
			hiusb_start_hcd_s5();
			break;
		default:
			hiusb_start_hcd_s40();
			break;
	}
#endif

#ifdef CONFIG_ARCH_HI3798MX
	hiusb_start_hcd_hi3798mx(host_addr);
#endif
}

EXPORT_SYMBOL(hiusb_start_hcd);

void hiusb_stop_hcd(resource_size_t host_addr)
{
#ifdef CONFIG_ARCH_S40
	long long chipid = get_chipid();
	switch (chipid) {
		case _HI3798CV100A:
		case _HI3798CV100:
		case _HI3796CV100:
			hiusb_stop_hcd_s5();
			break;
		default:
			hiusb_stop_hcd_s40();
			break;
	}
#endif

#ifdef CONFIG_ARCH_HI3798MX
	hiusb_stop_hcd_hi3798mx(host_addr);
#endif
}
EXPORT_SYMBOL(hiusb_stop_hcd);
