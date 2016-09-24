#ifndef __PLATSMP__H__
#define __PLATSMP__H__

extern struct smp_operations hi3798mx_smp_ops;

void hi3798mx_secondary_startup(void);

void slave_cores_power_up(int cpu);

#endif

