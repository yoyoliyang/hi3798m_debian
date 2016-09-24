/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-08-15
 *
******************************************************************************/
#ifndef MATCHH
#define MATCHH
/******************************************************************************/

struct match_t {
	int type;
	int reg;
	void *data;
};

#define MATCH_SET_TYPE_REG(_type, _reg)   {(_type), (_reg), (void *)0}
#define MATCH_SET_TYPE_DATA(_type, _data) {(_type), 0, (void *)(_data)}
#define MATCH_SET(_type, _reg, _data)     {(_type), (_reg), (void *)(_data)}

int match_reg_to_type(struct match_t *table, int nr_table, int reg, int def);

int match_type_to_reg(struct match_t *table, int nr_table, int type, int def);

int match_data_to_type(struct match_t *table, int nr_table, char *data, int size,
		       int def);

void *match_type_to_data(struct match_t *table, int nr_table, int type,
			 void *def);

/******************************************************************************/
#endif /* MATCHH */
