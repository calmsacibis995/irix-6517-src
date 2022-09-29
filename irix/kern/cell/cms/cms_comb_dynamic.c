/*
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
/* 
 * This file contains routines to generate membership sets
 * based of dynamic combination generation. No static
 * generation or prioritization of membership sets is
 * necessary. This code has been borrowed from failsafe membership services 
 * code. Some modifications have been made especially in the area of renaming
 * variables and comments. No algorithmic change has been made.
 */

#include "cms_base.h"
#include "cms_message.h"
#include "cms_info.h"
#include "cms_membership.h"
#include "cms_trace.h"

/*
 * Data structure used to store nCr combination of cells.
 */
typedef struct cms_nCr_s {
	int 		n;
	int 		r;
	cell_set_t	*setmasks;
	int 		*cells;
	int 		*current;
} cms_nCr_t;

typedef struct cms_pcell_s {
	int 	cellid;
	int	priority[2];
} cms_pcell_t;

typedef void * nCr_handle_t;

static void cms_nCr_new(int, int, cms_pcell_t *);
static cell_set_t *cms_nCr_next(cell_set_t *);
static int cms_pcell_cmp(const void *, const void *);

static cms_pcell_t 	*pcells;
static int 		num_conn_per_cell[MAX_CELLS], 
			num_cells_per_conn_size[MAX_CELLS+1];
static cms_nCr_t	*nCr_handle;
extern int 		cms_conn_matrix[MAX_CELLS][MAX_CELLS];

extern void qsort(void *, size_t, size_t, int (*)(const void *, const void *));


/*
 * cms_pcell_cmp:
 * Sort comparison routine. Sorts cells according to priority which in our
 * case is the age of the cell.
 */
static int 
cms_pcell_cmp(const void *elm1, const void *elm2)
{
	cms_pcell_t *pcell1, *pcell2;

	pcell1 = (cms_pcell_t *) elm1;
	pcell2 = (cms_pcell_t *) elm2;

	/* We want the maximum element to appear first */

	if (pcell1->priority[0] == pcell2->priority[0]) {
		if (pcell1->priority[1] == pcell2->priority[1])
			return 0;
		else if (pcell1->priority[1] < pcell2->priority[1])
			return 1;
		else return -1;
	}
	else if (pcell1->priority[0] < pcell2->priority[0])
		return 1;
	else
		return -1;
}

/*
 * cms_nCr_new:
 * Initialize the nCr_handle with the sorted set of cells.
 * The cells are sorted according to age and thus when the combinations
 * are generated the oldest cells are used first.
 */
static void
cms_nCr_new(int n, int r, cms_pcell_t *pcells)
{
	int i;

	if (r < 0 || n < r || pcells == NULL) {
		cmn_err(CE_PANIC, "Invalid sizes for n %d and r %d\n", n, r);
		return; 

	} else {
		for (i=0; i<r; i++)
			nCr_handle->current[i] = -1;
	}

	nCr_handle->n = n;
	nCr_handle->r = r;

	qsort(pcells, n, sizeof(cms_pcell_t), cms_pcell_cmp);

	for (i=0; i<n; i++) {
		set_init(nCr_handle->setmasks + i);
		set_add_member(nCr_handle->setmasks + i, pcells[i].cellid);
		nCr_handle->cells[i] = pcells[i].cellid;
	}

}


/*
 * cms_nCr_next:
 * Return the next entry in the combination. This is called repeatedly
 * until we return NULL.
 */
static cell_set_t *
cms_nCr_next(cell_set_t *returnset)
{
	int first = 0;
	int i, j;

	for (i=0; i<nCr_handle->r; i++)
		if (nCr_handle->current[i] == -1) {
			first = 1;
			break;
		}

	if (first)
		for (i=0; i<nCr_handle->r; i++)
			nCr_handle->current[i] = i;
	else {
		for (i=nCr_handle->r - 1; i>=0; i--) {
			if (nCr_handle->current[i] < nCr_handle->n - 
						(nCr_handle->r - i)) {
				nCr_handle->current[i]++;
				if (i != nCr_handle->r - 1)
					for (j=i+1; j < nCr_handle->r; j++)
						nCr_handle->current[j] = 
							nCr_handle->current[i] 
								+ j - i;
				break;
			}
		}
		if (i < 0)
			return NULL;
	}

	set_init(returnset);
	for (i=0; i<nCr_handle->r; i++)
		set_add_member(returnset, 
				nCr_handle->cells[nCr_handle->current[i]]);
	return returnset;
}


/*
 * cms_comb_dynamic_init:
 * Initialize memory for nCr_handles and the array of cells and their age.
 */
boolean_t 
cms_comb_dynamic_init(void)
{
	size_t	pcell_size = set_member_count(&set_universe) * 
					sizeof(cms_pcell_t);

	pcells = (cms_pcell_t *) kmem_zalloc(pcell_size, KM_SLEEP);

	if ((nCr_handle = (cms_nCr_t *)kmem_zalloc(sizeof(cms_nCr_t), KM_SLEEP))
	    == NULL) {
		kmem_free(pcells, pcell_size);
		cmn_err(CE_PANIC, "CMS:Cannot allocate nCr_handle\n");
		return B_FALSE;
	}

	if ((nCr_handle->setmasks = (cell_set_t *)
    		kmem_zalloc(MAX_CELLS * sizeof(cell_set_t), KM_SLEEP)) == NULL) {
		kmem_free(pcells, pcell_size);
		kmem_free(nCr_handle, sizeof(cms_nCr_t));
		cmn_err(CE_PANIC, "CMS:Cannot allocate nCr_handle setmasks\n");
		return B_FALSE;
	}

	if ((nCr_handle->cells = (int *) 
		kmem_zalloc(MAX_CELLS*sizeof(int), KM_SLEEP)) == NULL) {
		kmem_free(pcells, pcell_size);
		kmem_free(nCr_handle->setmasks, MAX_CELLS * sizeof(cell_set_t));
		kmem_free(nCr_handle, sizeof(cms_nCr_t));
		cmn_err(CE_PANIC, "CMS:Cannot allocate nCr_handle cells\n");
		return B_FALSE;
	}

	if ((nCr_handle->current  = 
		(int *)kmem_zalloc(MAX_CELLS*sizeof(int), KM_SLEEP)) == NULL) {
		kmem_free(pcells, pcell_size);
		kmem_free(nCr_handle->setmasks, MAX_CELLS * sizeof(cell_set_t));
		kmem_free(nCr_handle->cells, MAX_CELLS*sizeof(int));
		kmem_free(nCr_handle, sizeof(cms_nCr_t));
		cmn_err(CE_PANIC, "CMS:Cannot allocate nCr_handle current\n");
		return B_FALSE;
	}
	return B_TRUE;
}

/*
 * cms_comb_find_dynamic:
 * Routine used to find the best clique (set of directly connected cells)
 * in a network. It uses the cms_conn_matrix to generate the combination.
 * The algorithm follows,
 * a. Compute the number of cells each cell is connected to. This is in
 *	num_conn_per_cell[].
 * b. Compute the number of cells that have a given number of connections
 *  This is in the array num_cells_per_conn_size[].
 * Based on this the max possible set size for the membership is determined.
 * The max possible set size is 'n' is where n or more cells have a 'n' 
 * connections. This is kept in max_candidate_set_size.
 * candidate_size = max_candidate_set_size
 * candiate_universe = (set of all cells).
 * loop:
 * Remove all cells from the candidate universe which are not fully 
 * connected to other members of the candidate universe.
 * if (set_count(&candiate_universe) == candidate_size) membership is found.
 * if (set_count(&candiate_universe) < candidate_size) {
 * 	candidate_size--;
 *	goto loop;
 * }
 * If (set_count(&candiate_universe) > candidate size) {
 * 	compute the various combinations of sets from the candidate universe.
 * 	choose the combination with the oldest set of cells to form the 
 *      membership.
 * }
 */
cell_set_t *
cms_comb_find_dynamic(cell_set_t *retset, boolean_t mustself)
{
	int 		ntotal;
	int 		i, j;
	int 		found, change;
	int 		max_candidate_set_size;
	cell_set_t 	candidate_set, candidate_universe;
	int 		candidate_set_size, candidate_universe_size;
	int 		candidate_cell_count, bad_candidate_size;
	int 		not_a_clique;
	cell_t 		cell;
	int		num_cells_with_min_conn_size;

	*retset = 0;

	ntotal = set_member_count(&set_universe);
	for (i=0; i<ntotal; i++) {
		num_conn_per_cell[i] = 0;
		for (j=0; j<ntotal; j++)
			if (cms_conn_matrix[i][j] && cms_conn_matrix[j][i])
				num_conn_per_cell[i]++;
	}
	for (i=0; i<=ntotal; i++)
		num_cells_per_conn_size[i] = 0;
	for (i=0; i<=ntotal; i++)
		num_cells_per_conn_size[num_conn_per_cell[i]]++;

	max_candidate_set_size = 0;

	/*
	 * Compute the max possible membership set size.
 	 * If there are n cells with n connections or more than n
	 * is a possible membership set size.
	 */
	num_cells_with_min_conn_size = 0;
	for (i = ntotal; i > 0; i--) {
		num_cells_with_min_conn_size += num_cells_per_conn_size[i];
		if (num_cells_with_min_conn_size >= i) {
			max_candidate_set_size = i;
			break;
		}
	}

	/*
	 * The maximum membership set size is max_candidate_set_size,
         * as there aren't at least max_candidate_set_size + 1 cells
         * that are connected to atleast max_candidate_set_size + 1
         * cells. Iterate over the range max_candidate_set_size .. 1
         * to determine a membership set of the maximum size. 
	 */

	for (candidate_set_size = max_candidate_set_size;  candidate_set_size; 
	    candidate_set_size--) {

		set_assign(&candidate_universe, &set_universe);

		/*
		 * First we narrow down our universe to which members
		 * of the membership set can belong too. The criterion
		 * used is that cells belonging to this universe must
		 * be connected to at least candidate_set_size cells in 
		 * *this* universe. Cells that do not satisfy this 
		 * criterion are eliminated, in turn possibly reducing
		 * the connectivity of other cells also. The process
		 * is repeated until no more cells can be eliminated
		 * from the universe. If at any stage number of cells
		 * in this universe becomes less than candidate_set_size,
		 * we give up looking for a membership set of this 
		 * size, as none exists. 
		 */
		bad_candidate_size = 0;
		change = 1;
		while (change) {
			for (i=0; i<ntotal; i++) {
				num_conn_per_cell[i] = 0;
				if (!set_is_member(&candidate_universe, i))
					continue;
				for (j=0; j<ntotal; j++)
					if (set_is_member(&candidate_universe, 
							j) &&
					    cms_conn_matrix[i][j] && 
					    cms_conn_matrix[j][i]) {
						num_conn_per_cell[i]++;
					}
			}

			change = 0;
			for (i=0; i<ntotal; i++)
				if (set_is_member(&candidate_universe, i) && 
				    num_conn_per_cell[i] < candidate_set_size) {
					change = 1;
					set_del_member(&candidate_universe, i);
				}

			candidate_cell_count = 0;
			for (i=0; i<ntotal; i++)
				candidate_cell_count += (num_conn_per_cell[i] 
						>= candidate_set_size);
			if (candidate_cell_count < candidate_set_size) {
				bad_candidate_size = 1;
				break;
			}
		}

		if (bad_candidate_size)
			continue;

		/*
		 * If the universe size if the same as the currently desired
		 * size of the membership set, the universe is the membership
		 * set. 
		 */
		candidate_universe_size = set_member_count(&candidate_universe);
		if (candidate_set_size == candidate_universe_size) {
			if (!mustself || 
			    set_is_member(&candidate_universe, cellid())) {
				set_assign(retset, &candidate_universe);
				goto returngoodval;
			} else
				continue;
		}

		/*
		 * We are going to generate combinations of size
		 * candidate_set_size from the cell universe of size
		 * candidate_universe_size one by one and check whether
		 * they form a clique. The first such clique will
		 * be returned as the membership set. Each cell
		 * is assigned a priority before the combination
		 * initialization so the the combination routine
		 * returns sets with cells with maximum age and
		 * maximum connectivity (in that order) first. 
		 */
		j = 0;
		cell = NULL;

		for (cell = 0; cell < MAX_CELLS; cell++) {
			if (!set_is_member(&candidate_universe, cell))
				continue;
			pcells[j].cellid = cell;
			pcells[j].priority[0] = cip->cms_age[cell];
			pcells[j].priority[1] = num_conn_per_cell[cell];
			j++;
		}

		/* Initialize combinations */
		cms_nCr_new(candidate_universe_size, 
					candidate_set_size, pcells);

		/* Get one candidate set at a time and examine if it is a
		 * clique. If we have found a clique, our serach is over,
		 * else try other membership sets of this size and then
		 * of smaller sizes. 
		 */
		found = 0;
		while (cms_nCr_next(&candidate_set)) {
			if (mustself && 
				!set_is_member(&candidate_set, cellid()))
				continue;
			not_a_clique = 0;
			for (i=0; i<ntotal; i++) {
				if (not_a_clique)
					break;
				if (set_is_member(&candidate_set, i))
					for (j=0; j<ntotal; j++)
						if (set_is_member(
							&candidate_set, j) &&
						    !cms_conn_matrix[i][j]) {
							not_a_clique = 1;
							break;
						}
			}
			if (not_a_clique)
				continue;

			found = 1;
			break;
		}

		if (found) {
			set_assign(retset, &candidate_set);
			goto returngoodval;
		}
	}

	return NULL;

returngoodval:
	return retset;
}
