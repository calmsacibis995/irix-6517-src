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

#ident	"$Revision: 1.9 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/strtbl.h>
#include <sys/systm.h>

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
	struct string_table_item *item, *new_item = NULL, *last_item = NULL;

again:
	mraccess(&string_table->string_table_mrlock);
	item = string_table->string_table_head;
	last_item = NULL;

	while (item) {
		if (!strcmp(item->string, name)) {
			/*
			 * If we allocated space for the string and the found that
			 * someone else already entered it into the string table,
			 * free the space we just allocated.
			 */
			if (new_item)
				STRTBL_FREE(new_item);


			/*
			 * Search optimization: move the found item to the head
			 * of the list.
			 */
			if (last_item != NULL &&
			    mrtrypromote(&string_table->string_table_mrlock)) {
				last_item->next = item->next;
				item->next = string_table->string_table_head;
				string_table->string_table_head = item;
				mrunlock(&string_table->string_table_mrlock);
			} else
				mraccunlock(&string_table->string_table_mrlock);
			goto out;
		}
		last_item = item;
		item=item->next;
	}

	/*
	 * name was not found, so add it to the string table.
	 */
	if (new_item == NULL) {
		long old_generation = string_table->string_table_generation;

		mraccunlock(&string_table->string_table_mrlock);
		new_item = STRTBL_ALLOC(strlen(name));
		ASSERT(new_item != NULL);

		strcpy(new_item->string, name);
		mrupdate(&string_table->string_table_mrlock);

		/*
		 * While we allocated memory for the new string, someone else 
		 * changed the string table.
		 */
		if (old_generation != string_table->string_table_generation) {
			mrunlock(&string_table->string_table_mrlock);
			goto again;
		}
	} else {
		/* At this we only have the string table lock in access mode.
		 * Promote the access lock to an update lock for the string
		 * table insertion below.
		 */
		if (!mrtrypromote(&string_table->string_table_mrlock)) {
			long old_generation = 
				string_table->string_table_generation;
			mraccunlock(&string_table->string_table_mrlock);
			mrupdate(&string_table->string_table_mrlock);

			/*
			 * After we did the unlock and wer waiting for update
			 * lock someone could have potentially updated
			 * the string table. Check the generation number
			 * for this case. If it is the case we have to
			 * try all over again.
			 */
			if (old_generation != 
			    string_table->string_table_generation) {
				mrunlock(&string_table->string_table_mrlock);
				goto again;
			}
		}
	}

	/*
	 * At this point, we're committed to adding new_item to the string table.
	 */
	new_item->next = string_table->string_table_head;
	item = string_table->string_table_head = new_item;
	string_table->string_table_generation++;
	mrunlock(&string_table->string_table_mrlock);

out:
	return(item->string);
}
