/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-08-15
 *
******************************************************************************/

#include <linux/string.h>
#include <linux/match.h>

/*****************************************************************************/

int match_reg_to_type(struct match_t *table, int nr_table, int reg, int def)
{
	while (nr_table-- > 0) {
		if (table->reg == reg)
			return table->type;
		table++;
	}
	return def;
}

int match_type_to_reg(struct match_t *table, int nr_table, int type, int def)
{
	while (nr_table-- > 0) {
		if (table->type == type)
			return table->reg;
		table++;
	}
	return def;
}

int match_data_to_type(struct match_t *table, int nr_table, char *data, int size,
		      int def)
{
	while (nr_table-- > 0) {
		if (!memcmp(table->data, data, size))
			return table->type;
		table++;
	}
	return def;
}

void *match_type_to_data(struct match_t *table, int nr_table, int type,
			 void *def)
{
	while (nr_table-- > 0) {
		if (table->type == type)
			return table->data;
		table++;
	}
	return def;
}
