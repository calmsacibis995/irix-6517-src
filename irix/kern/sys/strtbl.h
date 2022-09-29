/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef _STRTBL_H
#define _STRTBL_H

#ident	"$Revision: 1.2 $"

/*
** Overly-simplistic (for now) string table that supports find and insert.
*/


struct string_table_item {
	struct string_table_item	*next;
	char				string[1];
};


struct string_table {
	struct string_table_item	*string_table_head;
	long				string_table_generation;
	mrlock_t			string_table_mrlock;
};

#define STRTBL_BASIC_SIZE ((size_t)(((struct string_table_item *)0)->string))
#define STRTBL_ITEM_SIZE(str_length) (STRTBL_BASIC_SIZE + (str_length) + 1)

#define STRTBL_ALLOC(str_length) \
	((struct string_table_item *)kern_malloc(STRTBL_ITEM_SIZE(str_length)))

#define STRTBL_FREE(ptr) kern_free(ptr)

extern void string_table_init(struct string_table *);
extern char *string_table_insert(struct string_table *, char *);
extern void string_table_destroy(struct string_table *);
#endif	/* _STRTBL_H */
