/******************************************************************************
 *    COPYRIGHT (C) 2013 Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-12-19
 *
******************************************************************************/
#ifndef __MACH_SRAM_H
#define __MACH_SRAM_H

/* ARBITRARY:  SRAM allocations are multiples of this 2^N size */
#define SRAM_GRANULARITY	512

#ifdef CONFIG_SUPPORT_SRAM_MANAGER

extern void *sram_alloc(size_t len, dma_addr_t *dma);
extern void sram_free(void *addr, size_t len);
extern int sram_suspend(void);
extern int sram_resume(void);

#else
#  define sram_alloc(_len, _dma)   NULL
#  define sram_free(_addr, _len)
#  define sram_suspend()   while(0);
#  define sram_resume()    while(0);
#endif

#endif /* __MACH_SRAM_H */
