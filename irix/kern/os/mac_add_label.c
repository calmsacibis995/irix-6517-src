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

#ident	"$Revision: 1.12 $"

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/mac_label.h"

/*
 * Label constants. Use the pseudo_label type to save space.
 */
struct pseudo_label {
	char	bytes[8];
};

/*
 * In each case here, put the Moldy label directly after the
 * non-Moldy label. This is necessary so that mac_unmold() can
 * find the non-Moldy label quickly.
 */

static struct pseudo_label pseudo_labels[] = {
	{ MSEN_HIGH_LABEL,	MINT_HIGH_LABEL, },	/* 0 */
	{ MSEN_MLD_HIGH_LABEL,	MINT_HIGH_LABEL, },	/* 1 */
	{ MSEN_LOW_LABEL,	MINT_HIGH_LABEL, },	/* LOW_HIGH */
	{ MSEN_MLD_LOW_LABEL,	MINT_HIGH_LABEL, },	/* 3 */
	{ MSEN_EQUAL_LABEL,	MINT_HIGH_LABEL, },	/* 4 */
	{ MSEN_ADMIN_LABEL,	MINT_HIGH_LABEL, },	/* ADMIN_HIGH */

	{ MSEN_HIGH_LABEL,	MINT_LOW_LABEL, },	/* HIGH_LOW */
	{ MSEN_MLD_HIGH_LABEL,	MINT_LOW_LABEL, },	/* 7 */
	{ MSEN_LOW_LABEL,	MINT_LOW_LABEL, },	/* 8 */
	{ MSEN_MLD_LOW_LABEL,	MINT_LOW_LABEL, },	/* 9 */
	{ MSEN_EQUAL_LABEL,	MINT_LOW_LABEL, },	/* 10 */
	{ MSEN_ADMIN_LABEL,	MINT_LOW_LABEL, },	/* ADMIN_LOW */

	{ MSEN_HIGH_LABEL,	MINT_EQUAL_LABEL, },	/* HIGH_EQUAL */
	{ MSEN_MLD_HIGH_LABEL,	MINT_EQUAL_LABEL, },	/* 13 */
	{ MSEN_LOW_LABEL,	MINT_EQUAL_LABEL, },	/* 14 */
	{ MSEN_MLD_LOW_LABEL,	MINT_EQUAL_LABEL, },	/* 15 */
	{ MSEN_EQUAL_LABEL,	MINT_EQUAL_LABEL, },	/* EQUAL_EQUAL */
	{ MSEN_ADMIN_LABEL,	MINT_EQUAL_LABEL, },	/* 17 */
};
#define PSEUDO_SIZE ((sizeof (pseudo_labels)) / sizeof (struct pseudo_label))

#define	PL_LOW_HIGH	2
#define	PL_ADMIN_HIGH	5
#define	PL_HIGH_LOW	6
#define	PL_ADMIN_LOW	11
#define	PL_HIGH_EQUAL	12
#define	PL_EQUAL_EQUAL	16

/*
 * Exported label constants.
 */
mac_label *mac_high_low_lp = (mac_label *)&pseudo_labels[PL_HIGH_LOW];
mac_label *mac_low_high_lp = (mac_label *)&pseudo_labels[PL_LOW_HIGH];
mac_label *mac_equal_equal_lp = (mac_label *)&pseudo_labels[PL_EQUAL_EQUAL];
mac_label *mac_high_equal_lp = (mac_label *)&pseudo_labels[PL_HIGH_EQUAL];
mac_label *mac_admin_high_lp = (mac_label *)&pseudo_labels[PL_ADMIN_HIGH];
mac_label *mac_admin_low_lp = (mac_label *)&pseudo_labels[PL_ADMIN_LOW];

/*
 * Data for memory resident labels.
 * mac_label_list is the list of labels.
 *
 */

struct list {
	struct list	*mll_next;	/* Next in the list */
	mac_label	*mll_plain;	/* Normal form */
	mac_label	*mll_moldy;	/* Equivalent Moldy label */
};

static struct list *mac_label_list;

/*
 * Add a label to the active list if it isn't already there.
 * Allocate space for the label and it's moldy counterpart.
 */

mac_label *
mac_add_label(mac_label *new)
{
	struct list *lp;
	int i;
	int new_is_moldy;
	mac_label *plp;

	ASSERT(new);
	/*
	 * Check that the label is reasonable.
	 */
	if (mac_invalid(new)) {
		cmn_err_tag(1791, CE_WARN, "Invalid label add attempted.");
		return (NULL);
	}

	/*
	 * Check the constants in pseudo_labels.
	 */
	for (i = 0; i < PSEUDO_SIZE; i++) {
		plp = (mac_label *) &pseudo_labels[i];
		if (plp->ml_msen_type == new->ml_msen_type &&
		    plp->ml_mint_type == new->ml_mint_type)
			return (plp);
	}

	/*
	 * Search for the label in the existing list.
	 * Allocate space for the new labels if necessary.
	 */
	new_is_moldy = mac_is_moldy(new);

	for (lp = mac_label_list; lp != NULL; lp = lp->mll_next) {
		/*
		 * Performance shortcut. The integrity type will
		 * be the same reguardless of moldyness.
		 */
		if (lp->mll_plain->ml_mint_type != new->ml_mint_type)
			continue;

		if (new_is_moldy) {
			if (lp->mll_moldy->ml_msen_type != new->ml_msen_type)
				continue;
		}
		else {
			if (lp->mll_plain->ml_msen_type != new->ml_msen_type)
				continue;
		}
		/*
		 * The types have been found to match.
		 * Leave the loop if the labels are equal.
		 */
		if (mac_equ(new, lp->mll_plain))
			return (new_is_moldy ? lp->mll_moldy : lp->mll_plain);
	}

	/*
	 * The label is not in the list. Add both the plain
	 * and moldy values.
	 */
	lp = (struct list *)kern_malloc(sizeof(struct list));

	lp->mll_plain = mac_demld(new);
	lp->mll_moldy = mac_set_moldy(new);
	
	lp->mll_next = mac_label_list;
	mac_label_list = lp;
	return (new_is_moldy ? lp->mll_moldy : lp->mll_plain);
}

mac_label *
mac_unmold(mac_label *mlp)
{
	mac_label *plp;
	struct list *lp;
	int i;

	/*
	 * Bet this check gets hit more often than we'd like.
	 */
	if (!mac_is_moldy(mlp))
		return (mlp);
	/*
	 * Check the constants in pseudo_labels.
	 * If the label is found return the preceeding entry in the
	 * pseudo list. Yuck.
	 */
	for (i = 0; i < PSEUDO_SIZE; i++) {
		plp = (mac_label *) &pseudo_labels[i];
		if (plp->ml_msen_type == mlp->ml_msen_type &&
		    plp->ml_mint_type == mlp->ml_mint_type)
			return ((mac_label *) &pseudo_labels[i - 1]);
	}

	/*
	 * The label should be on the list already.
	 */
	for (lp = mac_label_list; lp != NULL; lp = lp->mll_next)
		if (lp->mll_moldy == mlp)
			return (lp->mll_plain);

	/*
	 * This is not good.
	 */
	cmn_err(CE_PANIC, "mac_unmold failed to find label");
	/*NOTREACHED*/
	return((mac_label *)0);
}
