/******************************************************************************
 *    COPYRIGHT (C) 2013 Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-12-19
 *
******************************************************************************/
#ifndef __PLATSMP__H__
#define __PLATSMP__H__

extern struct smp_operations s40_smp_ops;

void s40_scu_power_up(int cpu);
void s40_secondary_startup(void);
void s5_scu_power_up(int cpu);

#endif

