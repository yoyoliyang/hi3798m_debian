#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include "hieth.h"
#include "mdio.h"
#include "mac.h"
#include "ctrl.h"
#include "glb.h"
#include "sys.h"

#ifdef CONFIG_ARCH_GODBOX
#  include "sys-godbox.c"
#endif

#ifdef CONFIG_ARCH_S40
#  include "sys-s40.c"
#endif

#ifdef CONFIG_ARCH_HI3798MX
#  include "sys-hi3798mx.c"
#endif
/********************************************************************************/

void hieth_sys_allstop(void)
{
	hieth_reset(1);
	hieth_clk_dis();
}

void hieth_sys_suspend(void)
{
	hieth_phy_suspend();
	hieth_clk_dis();
}

void hieth_sys_resume(void)
{
	hieth_phy_resume();
	hieth_clk_ena();
	hieth_reset(0);
}

void hieth_sys_init(void)
{
	hieth_funsel_config();
	hieth_sys_allstop();
	hieth_reset(0);
	hieth_clk_ena();
	msleep(100);
	hieth_internal_phy_reset();
	hieth_external_phy_reset();
}

void hieth_sys_exit(void)
{
	hieth_funsel_restore();
	hieth_sys_allstop();
}

/* vim: set ts=8 sw=8 tw=78: */
