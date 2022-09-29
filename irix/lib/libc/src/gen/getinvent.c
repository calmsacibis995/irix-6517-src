/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Hardware inventory library routines
 *
 */

#ident "$Revision: 1.11 $"
#ifdef __STDC__
	#pragma weak setinvent = _setinvent
	#pragma weak getinvent = _getinvent
	#pragma weak endinvent = _endinvent
	#pragma weak setinvent_r = _setinvent_r
	#pragma weak getinvent_r = _getinvent_r
	#pragma weak endinvent_r = _endinvent_r
#endif

#include "synonyms.h"
#include <sys/syssgi.h>
#include <invent.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>

#define DEFAULT_ITEMS 1000

static inv_state_t *state;

/*
 * endinvent - Release malloc's space
 */
void
endinvent_r(inv_state_t *s)
{
	if (!s)
		return;
	if (s->inv_table)
		free(s->inv_table);
	
	free(s);
}

void
endinvent(void)
{
	endinvent_r(state);
	state = NULL;
}

/*
 * setinvent -  initialize (using malloc) or rewind the inventory table
 *
 * This code attempts to read inventory records even if the kernel's
 * idea of how big the record is not the same as this module's.
 */
int
setinvent_r(inv_state_t **spp)
{
	int i_size, i_count = DEFAULT_ITEMS, count = DEFAULT_ITEMS;
	int j;
	char *inv_buffer = NULL;
	inv_state_t *s;

	if ((s = *spp) != NULL) {
		/* already initialized - rewind */
		s->inv_curr = 0;
		return 0;
	}

	/*
	 * Get size of inventory structure as compiled in the kernel.
	 * This may differ from sizeof(invent_t) if the kernel and
	 * this module have gotten out of sync.
	 */
	i_size = (int)syssgi(SGI_INVENT, SGI_INV_SIZEOF);
	if (i_size == -1)
		goto err;

	/*
	 * Iteratively get the maximum possible number of inventory
	 * entries 
	 */
	while (i_count == count*sizeof(inventory_t)/i_size)
	{
		if (inv_buffer) {
			free(inv_buffer);
			count *= 2;
		}
		inv_buffer = malloc(count*sizeof(inventory_t));
		if (inv_buffer == NULL)
			goto err;
		i_count = (int)syssgi(SGI_INVENT, SGI_INV_READ, inv_buffer,
				      count*sizeof(inventory_t));
		if (i_count == -1)
			goto err;
		i_count /= i_size;
	}
		
	s = malloc(sizeof(inv_state_t));
	s->inv_table = calloc((size_t)i_count, sizeof(inventory_t));

	if (!s->inv_table)
		goto err;

	/*
	 * If the size of the inventory structure has changed,
	 * return the minimum of the kernel's and program's size.
	 */
	if ((int)sizeof(inventory_t) < i_size)
		for (j = 0; j < i_count; j++)
			(void) memcpy(s->inv_table+j,
				inv_buffer+(j*i_size),(int)sizeof(inventory_t));
	else if ((int)sizeof(inventory_t) > i_size)
		for ( j = 0; j < i_count; j++ )
			(void) memcpy(s->inv_table+j,
					inv_buffer+(j*i_size), (size_t)i_size);
	else
		(void) memcpy(s->inv_table, inv_buffer,
			(size_t)(i_count)*sizeof(inventory_t));
	
	s->inv_curr = 0;
	s->inv_count = i_count;
	free(inv_buffer);
	*spp = s;

	return 0;
err:
	free(inv_buffer);
	free(s);
	return -1;
}

int
setinvent(void)
{
	return(setinvent_r(&state));
}

/*
 * getinvent - Return pointer to next inventory record
 */
inventory_t *
getinvent_r(inv_state_t *s)
{
	if (s->inv_curr < s->inv_count )
		return s->inv_table+s->inv_curr++;
	return NULL;
}

inventory_t *
getinvent(void)
{
	/* backward compatible auto-initialize */
	if (state == NULL && setinvent_r(&state) != 0)
		return NULL;
	return(getinvent_r(state));
}
