/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-03-07
 *
******************************************************************************/

#ifndef CPUINFOH
#define CPUINFOH
/******************************************************************************/


#define _HI3716CV200ES                           (0x0019400200LL)
#define _HI3716CV200ES_MASK                      (0xFFFFFFFFFFLL)

#define _HI3716CV200                             (0x0037160200LL)
#define _HI3716CV200_MASK                        (0xFFFFFFFFFFLL)

#define _HI3716HV200                             (0x0437160200LL)
#define _HI3719CV100                             (0x0837160200LL)
#define _HI3718CV100                             (0x1037160200LL)

#define _HI3716MV400                             (0x1C37160200LL)
#define _HI3716MV400_MASK                        (0xFEFFFFFFFFLL)

#define _HI3719MV100A                            (0x1E37160200LL)
#define _HI3719MV100A_MASK                       (0xFEFFFFFFFFLL)

#define _HI3719MV100                             (0x0037190100LL)
#define _HI3718MV100                             (0x0437190100LL)
#define _HI3718MV100_MASK                        (0xFEFFFFFFFFLL)

#define _HI3798CV100A                            (0x0019050100LL)
#define _HI3798CV100A_MASK                       (0xFFFFFFFFFFLL)

#define _HI3798CV100                             (0x1C19050100LL)
#define _HI3796CV100                             (0x1819050100LL)
#define _HI3798CV100_MASK                        (0xFFFFFFFFFFLL)

#define _HI3798MV100_A                           (0x0019400300LL)
#define _HI3798MV100_A_MASK                      (0xF0FFFFFFFFLL)

#define _HI3798MV100                             (0x0137980100LL)
#define _HI3796MV100                             (0x0037980100LL)
#define _HI3798MV100_MASK                        (0xF1FFFFFF0FLL)

void get_clock(unsigned int *cpu, unsigned int *timer);

long long get_chipid(void);

const char * get_cpu_name(void);

const char * get_cpu_version(void);

/******************************************************************************/
#endif /* CPUINFOH */
