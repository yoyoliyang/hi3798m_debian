/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-03-15
 *
******************************************************************************/

#ifndef MACHCPU_CPUH
#define MACHCPU_CPUH

#include <mach/cpu-info.h>
#include <asm/mach/resource.h>

struct cpu_info {
	const char *name;
	long long chipid;
	long long chipid_mask;
	struct device_resource **resource;

	unsigned int clk_cpu;
	unsigned int clk_timer;
	char *cpuversion;

	void (*init)(struct cpu_info *info);
};

void arch_cpu_init(void);

#endif /* MACHCPU_CPUH */
