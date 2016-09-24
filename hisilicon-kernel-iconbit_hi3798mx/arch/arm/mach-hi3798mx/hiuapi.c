/******************************************************************************
 *    Copyright (C) 2014 Hisilicon STB Development Dept
 *    All rights reserved.
 * ***
 *    Create by Cai Zhiyong
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
******************************************************************************/

#define pr_fmt(fmt) "HIUAPI: " fmt
#include <linux/module.h>
#include <linux/init.h>
#include <asm/page.h>
#include <asm/setup.h>
#include <linux/errno.h>
#include <linux/hiuapi.h>

extern unsigned int get_cma_size(void);

static unsigned int get_ram_size(void)
{
	int i;
	u64 total = 0;
	struct meminfo *mi = &meminfo;

	for_each_bank (i, mi) {
		struct membank *bank = &mi->bank[i];
		total += bank_phys_size(bank);
	}

	/* unit is M */
	return (unsigned int)(total >> 20);
}
/******************************************************************************/

int get_mem_size(unsigned int *size, int flags)
{
	int ret = 0;

	switch (flags) {
	case HIUAPI_GET_RAM_SIZE:
	{
		static unsigned int ramsize = 0;
		if (!ramsize)
			ramsize = get_ram_size();
		if (size)
			*size = ramsize;
		break;
	}
	case HIUAPI_GET_CMA_SIZE:
	{
		static unsigned int cmasize = 0;
#ifdef CONFIG_CMA
		if (!cmasize)
			cmasize = get_cma_size();
#endif
		if (size)
			*size = cmasize;
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

EXPORT_SYMBOL(get_mem_size);

