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

#ident	"$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>

#include "strtbl.h"

/*
** Very simple and dumb string table that supports only find/insert.
** In practice, if this table gets too large, we may need a more
** efficient data structure.   Also note that currently there is no 
** way to delete an item once it's added.
*/



/*
 * Initialize a string table.
 */
void
string_table_init(struct string_table *string_table)
{
	string_table->string_table_head = NULL;
	string_table->string_table_generation = 0;
	mrinit(&string_table->string_table_mrlock, "strtbl");

	return;
}


/*
 * Destroy a string table.
 */
void
string_table_destroy(struct string_table *string_table)
{
	struct string_table_item *item, *next_item;

	item = string_table->string_table_head;
	while (item) {
		next_item = item->next;

		STRTBL_FREE(item);
		item = next_item;
	}

	mrfree(&string_table->string_table_mrlock);

	return;
}



char *
string_table_insert(struct string_table *string_table, char *name)
{
	struct string_table_item *item, *new_item = NULL;

	mrlock(&string_table->string_table_mrlock, MR_ACCESS, PZERO);

again:
	item = string_table->string_table_head;
	while (item) {
		if (!strcmp(item->string, name)) {
			/*
			 * If we allocated space for the string and the found that
			 * someone else already entered it into the string table,
			 * free the space we just allocated.
			 */
			if (new_item)
				STRTBL_FREE(new_item);
			goto out;
		}
		item=item->next;
	}

	/*
	 * name was not found, so add it to the string table.
	 */
	if (new_item == NULL) {
		long old_generation = string_table->string_table_generation;

		mrunlock(&string_table->string_table_mrlock);
		new_item = STRTBL_ALLOC(strlen(name));
		ASSERT(new_item != NULL);

		strcpy(new_item->string, name);
		mrlock(&string_table->string_table_mrlock, MR_UPDATE, PZERO);

		/*
		 * While we allocated memory for the new string, someone else 
		 * changed the string table.
		 */
		if (old_generation != string_table->string_table_generation) {
			mrunlock(&string_table->string_table_mrlock);
			goto again;
		}
	}

	/*
	 * At this point, we're committed to adding new_item to the string table.
	 */
	new_item->next = string_table->string_table_head;
	item = string_table->string_table_head = new_item;
	string_table->string_table_generation++;

out:
	mrunlock(&string_table->string_table_mrlock);
	return(item->string);
}
