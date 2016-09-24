/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-03-15
 *
******************************************************************************/

#ifndef __TAGS_H__
#define __TAGS_H__

/*
 * name         param name
 * buf          param buffer pointer;
 * buflen       param length;
 *
 * return       -1:    not found this param;
 *              -2:    input parameter bad;
 *              other: parameter real length;
 */
int get_param_data(const char *name, char *buf, unsigned int buflen);

int set_param_data(const char *name, char *buf, unsigned int buflen);

#endif  /* __TAGS_H__ */
