/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.22 $"

#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/uuid.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/kmem.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#else
#include <stdlib.h>
#include <stdio.h>
#include <bstring.h>
#endif
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#ifndef _KERNEL
#include <xlv_utils.h>
#endif


#define CAN_READ_WRITE_VE(ve) (((ve)->state != XLV_VE_STATE_OFFLINE) && \
			     ((ve)->state != XLV_VE_STATE_INCOMPLETE))

/*
 * Create a block map given an xlv_tab subvolume entry.
 * A block map and a ve array may be passed in to this routine;
 * this routine is responsible for freeing the block map if there
 * is an error; and for freeing the ve array.
 */
xlv_block_map_t *
#ifdef _KERNEL
xlv_tab_create_block_map (xlv_tab_subvol_t *sv,
			  xlv_block_map_t *block_map,
			  ve_entry_t *ve_array)
#else
xlv_tab_create_block_map (xlv_tab_subvol_t *sv)
#endif
{
#ifndef _KERNEL
        xlv_block_map_t         *block_map;
	ve_entry_t		*ve_array;
#endif
        xlv_block_map_entry_t   *block_map_entry;
        unsigned                max_block_map_entries;
	xlv_tab_plex_t		*plex;
	xlv_tab_vol_elmnt_t	*ve;
	ve_entry_t		*ve_array_entry;
	unsigned		ve_array_len, p, i;
	unsigned		ve_index, ve_array_index;
	__int64_t		ve_start_blkno, ve_array_start_blkno;
	unsigned		prev_read_plex_vector, prev_write_plex_vector;


#ifdef _KERNEL
        /*
         * We only create a block map if the subvolume is plexed.
         */
        ASSERT (sv->num_plexes > 1);
#endif

#if DISABLE_FIRST_PLEX
	/*
	 * Disable first plex completely.
	 */
	plex = sv->plex[0];
	if (plex) {
		for (ve_index=0; ve_index < plex->num_vol_elmnts; ve_index++) {
			ve = &plex->vol_elmnts[ve_index];
			ve->state = XLV_VE_STATE_OFFLINE;
		}
	}
#endif

#ifdef _KERNEL
	if (ve_array == NULL) {
		ve_array = (ve_entry_t *) kmem_zalloc (
			sizeof(ve_entry_t)*XLV_MAX_VE_PER_PLEX, KM_SLEEP);
	}
#else
	ve_array = (ve_entry_t *) malloc (
		sizeof(ve_entry_t)*XLV_MAX_VE_PER_PLEX);
	bzero (ve_array, sizeof(ve_entry_t)*XLV_MAX_VE_PER_PLEX);
#endif

	/*
	 * Go through all the plexes.  Insert an entry for every
	 * volume address range that's not already in ve_array.
	 */

	ve_array_len = 0;
        for (p = 0; p < XLV_MAX_PLEXES; p++) {

		plex = sv->plex[p];

		if (plex == NULL)
			continue;		/* plex is missing */


		/*
		 * for each vol elmnt in this plex...
		 */
                for (ve_index=0; ve_index < plex->num_vol_elmnts; ve_index++) {

			/*
			 * We initialize this to zero for now.
			 * XXX Make it remember where it left off.
			 */
			ve_array_index = 0;

			ve = &plex->vol_elmnts[ve_index];

			/*
			 * Skip this volume element if it's offline or
			 * incomplete.
			 */
			if ( ! CAN_READ_WRITE_VE(ve)) 
				continue;

			/*
			 * Handle the special-case where this is the very
			 * first valid vol element that we see.
			 */
			if (ve_array_len == 0) {
				ve_array[0].start_blkno = ve->start_block_no;
				ve_array[0].end_blkno = ve->end_block_no;

				/*
				 * We always write to all plexes.
				 * But only read from non-stale plexes.
				 * (assuming that we can read and write to
				 * the volume element, of course.)
				 */

				ve_array[0].write_plex_vector = 1 << p;
				if ((ve->state != XLV_VE_STATE_STALE) &&
				    (ve->state != XLV_VE_STATE_EMPTY))
					ve_array[0].read_plex_vector = 1 << p;
				else
					ve_array[0].read_plex_vector = 0;

				ve_array_len = 1;
				continue;
			}


			ve_start_blkno = ve->start_block_no;

			/*
                         * Find the ve_array entry within which
                         * this volume element falls.
                         */
			ve_array_start_blkno =
				ve_array[ve_array_index].start_blkno;
			while (ve_start_blkno > ve_array_start_blkno) {
				ve_array_index++;
				ve_array_start_blkno = 
					ve_array[ve_array_index].start_blkno;

				/*
				 * If we've run past the end of ve_array,
				 * then add this new block range to the
				 * end of ve_array.
				 */
				if (ve_array_index > ve_array_len) {
					ve_array[ve_array_len].start_blkno =
						ve->start_block_no;
					ve_array[ve_array_len].end_blkno =
						ve->end_block_no;

					ve_array[ve_array_len].
						write_plex_vector = 1 << p;
					if ((ve->state != XLV_VE_STATE_STALE) &&
					    (ve->state != XLV_VE_STATE_EMPTY)) {
                                        	ve_array[ve_array_len].
						    read_plex_vector = 1 << p;
					}
					else
						ve_array[ve_array_len].
                                                    read_plex_vector = 0;

					ve_array_len++;
					goto next_iteration;
				}
			}


			if (ve_start_blkno == ve_array_start_blkno) {

				/*
				 * Got a match, just update plex_vector
				 * for the block range to indicate that
				 * this plex has it too.
				 */
				ve_array[ve_array_index].write_plex_vector |=
					1 << p;

				if ((ve->state != XLV_VE_STATE_STALE) &&
				    (ve->state != XLV_VE_STATE_EMPTY)) {
					ve_array[ve_array_index].
						read_plex_vector |= 1 << p;
				}
				else 
					ve_array[ve_array_index].
						read_plex_vector &= ~(1<<p);
			}

			else if (ve_start_blkno < ve_array_start_blkno) {

				/*
				 * We've gone beyond this volume
				 * element's block range. This means
				 * that this block range did not exist
				 * in ve_array.
				 * We need to insert the block range.
				 */
				for (i = ve_array_len;
				     i > ve_array_index; i--) {
					ve_array[i] = ve_array[i-1];
				}

				ve_array[ve_array_index].start_blkno =
					ve_start_blkno;
				ve_array[ve_array_index].end_blkno =
					ve->end_block_no;
				ve_array[ve_array_index].write_plex_vector =
					1 << p;
				if ((ve->state != XLV_VE_STATE_STALE) &&
				    (ve->state != XLV_VE_STATE_EMPTY)) {
					ve_array[ve_array_index].
					    read_plex_vector = 1 << p;
				}
				else
					ve_array[ve_array_index].
                                                read_plex_vector = 0;

				ve_array_len++;

			}
			else {
				/*
				 * The while loop above should have
				 * handled this case.
				 */
				ASSERT (0);
			}

next_iteration:
			ve_array_index++;

		}	/* end for each vol elmnt in this plex... */
	}

	/*
	 * Now go through and make sure that we've covered the entire
	 * address range for this subvolume.
	 *
	 * If there were't any valid volume elements, return NULL 
	 *
	 * If we've got a hole, then return NULL.
	 */

	if (ve_array_len == 0 || ve_array[0].start_blkno != (__int64_t)0) {
#if defined(_KERNEL) && defined(DEBUG)
		if (ve_array[0].start_blkno)
			cmn_err (CE_WARN,
		    "xlv_tab_create_block_map: Hole at start of addr space");
#endif
#ifdef _KERNEL
		if (block_map) {
			kmem_free(block_map, 0);
		}
#endif
		block_map = NULL;
		goto done;
	}

	for (ve_array_index = 1; ve_array_index < ve_array_len;
	     ve_array_index++) {

		if (ve_array[ve_array_index].start_blkno !=
		    (ve_array[ve_array_index-1].end_blkno+1)) {
#if defined(_KERNEL) && defined(DEBUG)
			cmn_err (CE_WARN,
            "xlv_tab_create_block_map: Hole in middle of addr space");
#endif
#ifdef _KERNEL
			if (block_map) {
				kmem_free(block_map, 0);
			}
#endif
			block_map = NULL;
			goto done;
		}

	}

	/*
	 * Make sure that we've covered the end.
	 */
	if ((ve_array[ve_array_len-1].end_blkno+1) !=
#ifndef _KERNEL
	    xlv_tab_subvol_size(sv)
#else
	    sv->subvol_size
#endif
	    ) {
#if defined(_KERNEL) && defined(DEBUG)
		cmn_err (CE_WARN,
            "xlv_tab_create_block_map: Hole at end of address space");
#endif
#ifdef _KERNEL
		if (block_map) {
			kmem_free(block_map, 0);
		}
#endif
		block_map = NULL;
		goto done;
	}

#ifdef DEBUG
	/*
	 * Check to make sure that all the volume elements across a
	 * row are the same size.
	 */
	for (ve_array_index = 1; ve_array_index < ve_array_len;
	     ve_array_index++) {

                ve_array_entry = &ve_array[ve_array_index];

		for (p=0; p < XLV_MAX_PLEXES; p++) {

			plex = sv->plex[p];

			if (plex == NULL)
				continue;		/* plex is missing */

			for (ve_index = 0, ve = plex->vol_elmnts;
			     ve_index < plex->num_vol_elmnts;
			     ve_index++, ve++) {

				if (ve_array_entry->start_blkno ==
				    ve->start_block_no)
					ASSERT (ve_array_entry->end_blkno ==
					        ve->end_block_no);
			}
		}
	}
#endif /* DEBUG */


	/*
	 * Combine adjacent block ranges in the ve_array that are on
	 * the same set of plexes into a single, contiguous block range
	 * in the block map.
	 *
	 * We do this in 2 passes: count the number of entries we need
	 * and then populating the block map.
	 */

	prev_read_plex_vector = ve_array[0].read_plex_vector;
	prev_write_plex_vector = ve_array[0].write_plex_vector;
	max_block_map_entries = 1;

	for (ve_array_index = 1;
	     ve_array_index < ve_array_len; ve_array_index++) {

		ve_array_entry = &ve_array[ve_array_index];

		/*
		 * If the new block range is covered by a different
		 * set of plexes, we need to use a new block map
		 * entry.
		 */
		if ((ve_array_entry->read_plex_vector != prev_read_plex_vector)
 			|| (ve_array_entry->write_plex_vector !=
			    prev_write_plex_vector)) {

			max_block_map_entries++;
			prev_read_plex_vector =
				ve_array_entry->read_plex_vector;
			prev_write_plex_vector =
				ve_array_entry->write_plex_vector;
		}
	}

	/*
	 * Allocate the memory.
	 */
#ifdef _KERNEL
	if (block_map == NULL) {
		block_map = (xlv_block_map_t *) kmem_zalloc (
		    sizeof (xlv_block_map_t) + 
		    (max_block_map_entries-1) * sizeof (xlv_block_map_entry_t),
		    KM_SLEEP);
	}
	else {
		ASSERT(block_map->entries >= max_block_map_entries);
	}
#else
	block_map = (xlv_block_map_t *) malloc (
		sizeof (xlv_block_map_t) + 
		(max_block_map_entries-1) * sizeof (xlv_block_map_entry_t));
#endif
	block_map->entries = max_block_map_entries;

	/*
	 * Set up the block map itself.
	 * We combine adjoining volume elements into the same block
	 * map entry.
	 */
	block_map_entry = block_map->map;
	ve_array_entry = &ve_array[0];

	block_map_entry->read_plex_vector = prev_read_plex_vector =
		ve_array_entry->read_plex_vector;
	block_map_entry->write_plex_vector = prev_write_plex_vector =
		ve_array_entry->write_plex_vector;

	block_map_entry->end_blkno = ve_array_entry->end_blkno;

	
	for (ve_array_index = 1;
             ve_array_index < ve_array_len; ve_array_index++) {

                ve_array_entry = &ve_array[ve_array_index];

		/*
                 * If the new block range is covered by a different
                 * set of plexes, we need to use a new block map
                 * entry.
                 */
                if ((ve_array_entry->read_plex_vector != prev_read_plex_vector)
			|| (ve_array_entry->write_plex_vector !=
			    prev_write_plex_vector)) {

			block_map_entry++;

			block_map_entry->read_plex_vector =
				prev_read_plex_vector =
				ve_array_entry->read_plex_vector;
			block_map_entry->write_plex_vector =
				prev_write_plex_vector =
				ve_array_entry->write_plex_vector;

			block_map_entry->end_blkno = ve_array_entry->end_blkno;

		}
		else {
			/*
			 * If the set of plexes are the same, update
			 * the end_block_no to include current entry.
			 */
			block_map_entry->end_blkno = ve_array_entry->end_blkno;
		}
	}

done:

#ifdef _KERNEL
	kmem_free (ve_array, 0);
#else
	free (ve_array);
#endif

	return block_map;

}


/*
 * Print out a block map.
 */
void
xlv_tab_block_map_print (xlv_block_map_t *block_map)
{
        int     i;

        if (block_map == NULL)
                return;

        printf ("block map:  blocks    read_plex_vector  write_plex_vector\n");
        for (i=0; i<block_map->entries; i++) {
                printf ("            %d   0x%x       0x%x\n",
                        block_map->map[i].end_blkno,
                        block_map->map[i].read_plex_vector,
			block_map->map[i].write_plex_vector);
        }
}
