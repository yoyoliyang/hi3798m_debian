/******************************************************************************
 *    COPYRIGHT (C) 2013 Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-12-19
 *
******************************************************************************/
#ifndef L2CACHEH
#define L2CACHEH
/******************************************************************************/

#ifdef CONFIG_CACHE_L2X0
extern int hi_pm_disable_l2cache(void);
extern int hi_pm_enable_l2cache(void);
#else
#  define hi_pm_disable_l2cache()
#  define hi_pm_enable_l2cache()
#endif

/******************************************************************************/
#endif /* L2CACHEH */
