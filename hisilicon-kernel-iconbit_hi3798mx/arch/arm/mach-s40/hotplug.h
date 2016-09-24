/******************************************************************************
 *    COPYRIGHT (C) 2013 Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-12-18
 *
******************************************************************************/
#ifndef HOTPLUGH
#define HOTPLUGH
/******************************************************************************/

void s40_cpu_die(unsigned int cpu);

void set_scu_boot_addr(unsigned int start_addr, unsigned int jump_addr);

void s40_scu_power_up(int cpu);
void s5_scu_power_up(int cpu);

/******************************************************************************/
#endif /* HOTPLUGH */
