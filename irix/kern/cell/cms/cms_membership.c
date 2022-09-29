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
#include "cms_base.h"
#include "cms_message.h"
#include "cms_info.h"
#include "cms_membership.h"
#include "cms_trace.h"

int cms_conn_matrix[MAX_CELLS][MAX_CELLS];

typedef enum {
        SET_DELAY_COMPLEMENT,
        SET_DELAY_MINIDX
} cms_set_delay_policy_t;

static void cms_conn_matrix_compute(void);
static uint_t cms_set_delay_minidx(cell_set_t *);
static uint_t cms_set_delay_complement(cell_set_t *);
static void cms_set_delay(cms_set_delay_policy_t, cell_set_t *);


void
cms_membership_compute(cell_set_t *membset, boolean_t initial)
{
	/* build connectivity matrix */
	cms_conn_matrix_compute();

   	cms_comb_find_dynamic(membset, initial);
}

/* ARGSUSED */
boolean_t
cms_membership_validate(cell_set_t *membset, boolean_t initial)
{
	cms_quorum_t		quorum;
	cms_quorum_type_t	quorum_type;

	quorum_type = (initial ? QUORUM_TYPE_INIT: QUORUM_TYPE_RUN);

	/*
	 * if the mcast set has majority, we have to accept
	 * any decision, even if that implies having the
	 * local node removed from the membership
	 */
	quorum = cms_quorum_check(quorum_type, membset);

	/*
	 * If the mcast set (consensus set) is minority
	 * no membership can be generated
	 */
	if (quorum == QUORUM_MINORITY) {
		
		cms_trace(CMS_TR_MEMB_MIN, *membset, 0, 0);

		return B_FALSE;
	}

	/*
	 * If the consensus set is a TIE
	 * delay the delivery of the membership
	 */
	if (quorum == QUORUM_TIE)
		cms_set_delay(SET_DELAY_COMPLEMENT, membset);

	/*
	 * The consensus set is a majority or a tie
	 * that has completed the delay cycle, go ahead
	 * and build the new membership. If the consensus
	 * set is only a tie, select the best combination
	 * containing this local node.
	 */
	return B_TRUE;
}

void
cms_conn_matrix_compute(void)
{
        cell_t          i, j;

        bzero(cms_conn_matrix, sizeof(cms_conn_matrix));
        for (i = 0; i < MAX_CELLS;  i++) {
                if (set_is_member(&set_universe, i) == B_FALSE)
                        continue;
                for (j = 0; j < MAX_CELLS;  j++) {
                        if (set_is_member(&cip->cms_recv_set[i], j))
                                cms_conn_matrix[i][j] = 1;
                }
        }
}

uint_t
cms_quorum_compute(cms_quorum_type_t type, uint_t count)
{
	uint_t	quorum;

	switch (type) {
	case QUORUM_TYPE_INIT:
		quorum = count;
		break;
	case QUORUM_TYPE_RUN:
		quorum = ((count % 2) == 0 ? (count/2) : (count/2 + 1));
		break;
	}
	return quorum;
}

uint_t
cms_quorum_get(cms_quorum_type_t type)
{
	uint_t	count, quorum;

	count = cip->cms_num_cells;
	quorum = cms_quorum_compute(type, count);
	return quorum;
}

/* ARGSUSED */
cms_quorum_t
cms_quorum_check(cms_quorum_type_t type, cell_set_t *memb_set)
{

#ifdef	CELL_IRIX
	if (set_is_member(memb_set, golden_cell))
		return QUORUM_MAJORITY;
	else	
		return QUORUM_MINORITY;
#else /* CELL_IRIX */
	uint_t	count, quorum, incount;

	incount = set_member_count(memb_set);
	count = cip->cms_num_cells;
	quorum = cms_quorum_compute(type, count);

	if (incount > quorum)
		return QUORUM_MAJORITY;

	if (incount == quorum) {
		if (incount == (count / 2))
			return QUORUM_TIE;
		else
			return QUORUM_MAJORITY;
	}

	return QUORUM_MINORITY;
#endif /* CELL_IRIX */
}

static void
cms_set_delay(cms_set_delay_policy_t policy, cell_set_t *set)
{
	uint_t	tie_delay;

	switch (policy) {
	case SET_DELAY_COMPLEMENT:
		tie_delay = cms_set_delay_complement(set);
		break;
	case SET_DELAY_MINIDX:
		tie_delay = cms_set_delay_minidx(set);
		break;
	}


	delay(tie_delay);
	cms_trace(CMS_TR_DELAY_FSTOP, tie_delay, 0, 0);
}

static uint_t
cms_set_delay_complement(cell_set_t *set)
{
	cell_t		leader;

	leader = cms_elect_leader(set);

	return leader*cip->cms_tie_timeout;
}

static uint_t
cms_set_delay_minidx(cell_set_t *set)
{
	/* return very high value if nothing is found */
	return (uint_t)set_member_min(set);
}
