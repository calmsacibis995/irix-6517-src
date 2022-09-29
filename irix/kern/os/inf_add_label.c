/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.2 $"

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/inf_label.h"

/*
 * Label constants. Use the pseudo_label type to save space.
 */
struct pseudo_label {
	char	bytes[8];
};

/*
 * These constants are assumed (below) to have 0 categories.
 * (this is an artifact of re-using the MAC code.
 * we may wish to degeneralize this sometime soon)
 */

static struct pseudo_label pseudo_labels[] = {
	{ 0,	0, },	/* level 0, no categories */
};
#define PSEUDO_SIZE ((sizeof (pseudo_labels)) / sizeof (struct pseudo_label))

#define	PL_LOW	0

/*
 * Exported label constants.
 */
inf_label *inf_low_lp = (inf_label *)&pseudo_labels[PL_LOW];

/*
 * Data for memory resident labels.
 * inf_label_list is the list of labels.
 */

struct list {
	struct list	*ill_next;	/* Next in the list */
	inf_label	*ill_plain;	/* Normal form */
};

static struct list *inf_label_list;

/*
 * Add a label to the active list if it isn't already there.
 * Allocate space for the label if it's new.
 */

inf_label *
inf_add_label(inf_label *new)
{
	struct list *lp;
	int i;
	inf_label *plp;

	ASSERT(new);
	/*
	 * Check that the label is reasonable.
	 */
	if (new->il_catcount > INF_MAX_SETS) {
		cmn_err_tag(1790, CE_WARN, "Invalid information label add attempted.");
		return (NULL);
	}

	/*
	 * Check the constants in pseudo_labels.
	 */
	if (new->il_catcount == 0)
		for (i = 0; i < PSEUDO_SIZE; i++) {
			plp = (inf_label *) &pseudo_labels[i];
			if (plp->il_level == new->il_level)
				return (plp);
		}

	for (lp = inf_label_list; lp != NULL; lp = lp->ill_next)
		/*
		 * Leave the loop if the labels are equal.
		 */
		if (inf_equ(new, lp->ill_plain))
			return (lp->ill_plain);

	/*
	 * The label is not in the list.
	 */
	lp = (struct list *)kern_malloc(sizeof(struct list));

	lp->ill_plain = inf_dup(new);
	lp->ill_next = inf_label_list;
	inf_label_list = lp;

	return (lp->ill_plain);
}
