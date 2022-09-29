/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <bstring.h>
#include <errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mac.h>
#include <sys/mac_label.h>

static int check_setvalue(unsigned short *, unsigned short);
static int b_equ(mac_b_label *, mac_b_label *);
static int subset(unsigned short *, int, unsigned short *, int);
static int tcsec_dom(msen_t, msen_t);
static int biba_dom(mint_t, mint_t);

/*
 *  The following routines operate on the Mandatory Sensitivity (MSEN),
 *  and Mandatory Integrity (MINT) components of the SGI MAC Label.
 *  Several of the routines operate by constructing a full mac label
 *  by substituting a wildcard for the missing label component, then
 *  invoking existing mac label routines.  It is anticipated that
 *  these routines will eventually replace the mac routines.
 */

/*
 *  Return a pointer to a mac label constructed from the given
 *  msen and mint labels.
 */
mac_t
mac_from_msen_mint(msen_t msenp, mint_t mintp)
{
	int i;
        mac_t lp;	/* pointer to a mac label structure */
	mac_t rp;

	if (!msen_valid(msenp) || !mint_valid(mintp))
		return NULL;

	lp = (mac_t) kmem_alloc(sizeof(mac_label), KM_NOSLEEP);
	if (lp == NULL)
		return NULL;

        /* Sensitivity component first */
        lp->ml_msen_type =      msenp->b_type;
        lp->ml_level =          msenp->b_hier;
        lp->ml_catcount =       msenp->b_nonhier;
        for (i = 0; i < lp->ml_catcount; i++)
                lp->ml_list[i] = msenp->b_list[i];

        /* Integrity component last */
        lp->ml_mint_type =      mintp->b_type;
        lp->ml_grade =          mintp->b_hier;
        lp->ml_divcount =       mintp->b_nonhier;
        for (i = 0; i < lp->ml_divcount; i++)
                lp->ml_list[i + lp->ml_catcount] = mintp->b_list[i];

	/* Check it */
	if (mac_invalid(lp)) {
		kmem_free(lp, sizeof(mac_label));
		return NULL;
	}

        rp = mac_add_label(lp);
	ASSERT(rp != NULL);

        kmem_free(lp, sizeof(mac_label));

        return rp;
}

/*
 *  Return a pointer to a mac label constructed from the given msen
 *  label, substituting MINT_EQUAL_LABEL for the needed mint component.
 */
mac_t
mac_from_msen(msen_t msenp)
{
	int i;
        mac_t lp;	/* pointer to a mac label structure */
        mac_t rp;	/* pointer to a mac label structure */

	if (!msen_valid(msenp))
		return NULL;

	lp = (mac_t) kmem_alloc(sizeof(mac_label), KM_NOSLEEP);
	if (lp == NULL)
		return NULL;

        /* Sensitivity component first */
        lp->ml_msen_type =      msenp->b_type;
        lp->ml_level =          msenp->b_hier;
        lp->ml_catcount =       msenp->b_nonhier;
        for (i = 0; i < lp->ml_catcount; i++)
                lp->ml_list[i] = msenp->b_list[i];

        /* Integrity component last */
        lp->ml_mint_type =      MINT_EQUAL_LABEL;
        lp->ml_grade =          0;
        lp->ml_divcount =       0;

	/* Check it */
	if (mac_invalid(lp)) {
		kmem_free(lp, sizeof(mac_label));
		return NULL;
	}

        rp = mac_add_label(lp);
	ASSERT(rp != NULL);

        kmem_free(lp, sizeof(mac_label));

        return rp;
}

/*
 *  Return a pointer to a mac label constructed from the given mint
 *  label, substituting MSEN_EQUAL_LABEL for the needed msen component.
 */
mac_t
mac_from_mint(mint_t mintp)
{
	int i;
        mac_t lp;	/* pointer to a mac label structure */
        mac_t rp;

	if (!mint_valid(mintp))
		return NULL;

	lp = (mac_t) kmem_alloc(sizeof(mac_label), KM_NOSLEEP);
	if (lp == NULL)
		return NULL;

        /* Sensitivity component first */
        lp->ml_msen_type =      MSEN_EQUAL_LABEL;
        lp->ml_level =          0;
        lp->ml_catcount =       0;

        /* Integrity component last */
        lp->ml_mint_type =      mintp->b_type;
        lp->ml_grade =          mintp->b_hier;
        lp->ml_divcount =       mintp->b_nonhier;
        for (i = 0; i < lp->ml_divcount; i++)
                lp->ml_list[i + lp->ml_catcount] = mintp->b_list[i];

	/* Check it */
	if (mac_invalid(lp)) {
		kmem_free(lp, sizeof(mac_label));
		return NULL;
	}

        rp = mac_add_label(lp);
	ASSERT(rp != NULL);

        kmem_free(lp, sizeof(mac_label));

        return rp;
}

/*
 *  Return a pointer to a msen label constructed from that component
 *  of the given mac label.
 */
msen_t 
msen_from_mac(mac_t lp)
{
        int i;
	msen_t msenp;
	msen_t rp;

	if (mac_invalid(lp))
		return NULL;

	msenp = (msen_t) kmem_alloc(sizeof(msen_label), KM_NOSLEEP);
	if (msenp == NULL)
		return NULL;

        msenp->b_type =    lp->ml_msen_type;
        msenp->b_hier =    lp->ml_level;
        msenp->b_nonhier = lp->ml_catcount;
        for (i = 0; i < lp->ml_catcount; i++)
                msenp->b_list[i] = lp->ml_list[i];

	rp = msen_add_label(msenp);
	ASSERT(rp);

	kmem_free(msenp, sizeof(msen_label));
	return rp;
}

/*
 *  Check the components of a mint label,
 *  return 1 if valid, 0 otherwise.
 */
int
msen_valid(msen_t msenp)
{

	if (msenp == NULL)
		return 0;

	/* Check number of categories */
	if (msenp->b_nonhier > (MAC_MAX_SETS/2))
		return 0;
		
	switch (msenp->b_type) {
		case MSEN_ADMIN_LABEL:
		case MSEN_EQUAL_LABEL:
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
			if (msenp->b_hier != 0 || msenp->b_nonhier > 0 )
				return 0;
			break;
		case MSEN_TCSEC_LABEL:
		case MSEN_MLD_LABEL:
			if (msenp->b_nonhier > 0)
				if (check_setvalue(msenp->b_list,
						   msenp->b_nonhier) == -1)
					return 0;
			break;
		case MSEN_UNKNOWN_LABEL:
		default:
			return 0;
	}
	return 1;
}

/*
 * Comapre two msen labels,
 *      Returns 1 iff the labels are equal
 *      Returns 0 otherwise.
 *      Returns -1 if errors.
 */
int
msen_equal(msen_t msen1, msen_t msen2)
{

	if (!msen_valid(msen1) || !msen_valid (msen2)) {
		return (-1);
	}

	if (msen1 == msen2)
		return (1);

	if (msen1->b_type == MSEN_EQUAL_LABEL ||
	    msen2->b_type == MSEN_EQUAL_LABEL)
		return 1;


	switch (msen1->b_type) {
		case MSEN_ADMIN_LABEL:
			switch (msen2->b_type) {
				case MSEN_ADMIN_LABEL:
					break;
				default:
					return (0);
			}
			break;
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
			switch (msen2->b_type) {
				case MSEN_HIGH_LABEL:
				case MSEN_MLD_HIGH_LABEL:
					break;
				default:
					return (0);
			}
			break;
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
			switch (msen2->b_type) {
				case MSEN_LOW_LABEL:
				case MSEN_MLD_LOW_LABEL:
					break;
				default:
					return (0);
			}
			break;
		case MSEN_MLD_LABEL:
		case MSEN_TCSEC_LABEL:
			switch (msen2->b_type) {
				case MSEN_TCSEC_LABEL:
				case MSEN_MLD_LABEL:
					if (!b_equ((mac_b_label *) msen1,
						   (mac_b_label *) msen2))
						return (0);
					break;
				default:
					return (0);
			}
			break;
		default:
			return (0);
	}
	return 1;
}

/*
 * msen_dom
 * 	Returns 1 iff the first label dominates the second label
 *	Returns 0 otherwise, including errors.
 */
int
msen_dom(msen_t lp1, msen_t lp2)
{

	if (!msen_valid(lp1) || !msen_valid (lp2)) {
		return (0);
	}

	/* Labels are same */
	if (lp1 == lp2)
		return (1);

	/* If either label is MSEN_EQUAL don't check sensitivity further. */
	if (lp1->b_type == MSEN_EQUAL_LABEL ||
	    lp2->b_type == MSEN_EQUAL_LABEL)
		return 1;

	switch (lp1->b_type) {
		case MSEN_ADMIN_LABEL:
			switch (lp2->b_type) {
				case MSEN_ADMIN_LABEL:
				case MSEN_LOW_LABEL:
				case MSEN_MLD_LOW_LABEL:
					return (1);
			}
			break;
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
			/* High always dominates.  */
			return (1);
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
			switch (lp2->b_type) {
				case MSEN_LOW_LABEL:
				case MSEN_MLD_LOW_LABEL:
					return (1);
			}
			break;
		case MSEN_MLD_LABEL:
		case MSEN_TCSEC_LABEL:
			switch (lp2->b_type) {
				case MSEN_LOW_LABEL:
				case MSEN_MLD_LOW_LABEL:
					return(1);
				case MSEN_TCSEC_LABEL:
				case MSEN_MLD_LABEL:
					if (!tcsec_dom(lp1, lp2))
						return (0);
					return(1);
			}
			break;
	}

	return (0);
}

/*
 *  msen_dup(lp)
 *
 *	Returns a copy of the passed label.
 *	In kernel only allocate enough room to hold the label.
 *	In user space allocate the max size of a label.
 *	User must free the resulting label when done.
 */

msen_t
msen_dup(msen_t lp)
{
	msen_t result;		/* result goes here */
	ssize_t size;		/* size of the result */

#ifdef	_KERNEL
	size = msen_size(lp);
	ASSERT(size != 0);
	result = (msen_t) kmem_alloc(size, KM_NOSLEEP);
#else	/* _KERNEL */
	if ((size = msen_size(lp)) == 0)
		return NULL;
	result = (msen_t) malloc(sizeof(msen_label));
#endif	/* _KERNEL */

	if (result == NULL)
		return NULL;

	bcopy((char *) lp, (char *) result, size);

	return (result);
}

ssize_t
msen_size(msen_t lp)
{
	/* if it is a invalid label, return size 0 */
	if (!msen_valid(lp))
		return(0);
	return (ssize_t) ((__psint_t)&lp->b_list[lp->b_nonhier] - (__psint_t)lp);
}

/*
 *  Return a pointer to a mint label constructed from that component
 *  of the given mac label.
 */
mint_t 
mint_from_mac(mac_t lp)
{
        int i;
	mint_t mintp;
	mint_t rp;

	if (mac_invalid(lp))
		return NULL;

	mintp = (mint_t) kmem_alloc(sizeof(mint_label), KM_SLEEP);
	if (mintp == NULL)
		return NULL;

        mintp->b_type =    lp->ml_mint_type;
        mintp->b_hier =    lp->ml_grade;
        mintp->b_nonhier = lp->ml_divcount;
        for (i = 0; i < lp->ml_divcount; i++)
                mintp->b_list[i] = lp->ml_list[lp->ml_catcount + i];

	rp = mint_add_label(mintp);
	ASSERT(rp);

	kmem_free(mintp, sizeof(mint_label));
	return rp;
}

/*
 * mint_equal
 *      Returns 1 iff the labels are equal
 *      Returns 0 otherwise.
 *      Returns -1 if errors.
 */
int
mint_equal(mint_t mintp1, mint_t mintp2)
{
	if (!mint_valid(mintp1) || !mint_valid(mintp2)) {
		return (-1);
	}

	if (mintp1 == mintp2)
		return (1);

	if (mintp1->b_type == MINT_EQUAL_LABEL ||
	    mintp2->b_type == MINT_EQUAL_LABEL)
		return 1;

	if (mintp1->b_type != mintp2->b_type)
		return 0;

	switch (mintp1->b_type) {
                case MINT_HIGH_LABEL:
                case MINT_LOW_LABEL:
                        return 1;
                case MINT_BIBA_LABEL:
                        return (b_equ(mintp1, mintp2));
                default:
                        break;
        }
	return 0;
}

/*
 *  Check the components of a mint label,
 *  return 1 if valid, 0 otherwise.
 */
int
mint_valid(mint_t mintp)
{

	if (mintp == NULL)
		return 0;

	if (mintp->b_nonhier > (MAC_MAX_SETS/2))
		return 0;
		
	/*
	 * check whether the minttype value is valid, and do they have
	 * appropriate grade, division association.
	 */
	switch (mintp->b_type) {
		case MINT_BIBA_LABEL:
			if (mintp->b_nonhier > 0)
				if (check_setvalue(mintp->b_list,
						   mintp->b_nonhier) == -1)
					return 0;
			break;
		case MINT_EQUAL_LABEL:
		case MINT_HIGH_LABEL:
		case MINT_LOW_LABEL:
			if (mintp->b_hier != 0 || mintp->b_nonhier > 0 )
				return 0;
			break;
		default:
			return 0;
	}

	return 1;
}

/*
 * mint_dom
 * 	Returns 1 iff the first label is dominated by the second label
 *	Returns 0 otherwise, including errors.
 */
int
mint_dom(mint_t lp1, mint_t lp2)
{
	if (!mint_valid(lp1) || !mint_valid (lp2)) {
		return (0);
	}

	/* Labels are same */
	if (lp1 == lp2)
		return (1);

	/*
	 * If either label is MINT_EQUAL we're done.
	 */
	if (lp1->b_type == MINT_EQUAL_LABEL ||
	    lp2->b_type == MINT_EQUAL_LABEL)
		return (1);

	switch (lp1->b_type) {
		case MINT_LOW_LABEL:
			return (lp2->b_type == MINT_LOW_LABEL);
		case MINT_BIBA_LABEL:
			switch (lp2->b_type) {
				case MINT_BIBA_LABEL:
					return (biba_dom(lp1, lp2));
				case MINT_LOW_LABEL:
					return (1);
				default:
					break;
			}
			break;
		case MINT_HIGH_LABEL:
			return (1);
	}
	return (0);
}

/*
 *  mint_dup(lp)
 *
 *	Returns a copy of the passed label.
 *	In kernel only allocate enough room to hold the label.
 *	In user space allocate the max size of a label.
 *	User must free the resulting label when done.
 */

mint_t
mint_dup(mint_t lp)
{
	mint_t result;		/* result goes here */
	ssize_t size;		/* size of the result */

#ifdef	_KERNEL
	size = mint_size(lp);
	ASSERT( size != 0 );
	result = (mint_t) kmem_alloc(size, KM_NOSLEEP);
#else	/* _KERNEL */
	if ((size = mint_size(lp)) == 0)
		return NULL;
	result = (mint_t) malloc(sizeof(mint_label));
#endif	/* _KERNEL */

	if (result == NULL)
		return NULL;

	bcopy((char *) lp, (char *) result, size);

	return (result);
}

ssize_t
mint_size(mint_t lp)
{
	/* if it is a invalid label, return size 0 */
	if (!mint_valid(lp))
		return(0);
	return (ssize_t) ((__psint_t)&lp->b_list[lp->b_nonhier] - (__psint_t)lp);
}


/*
 * Copy in a msen label.
 */
int
msen_copyin_label(msen_t src, msen_t *result)
{
	msen_t msen_buf;

	*result = NULL;

	msen_buf = (msen_t) kmem_alloc(sizeof(msen_label), KM_NOSLEEP);
	if (msen_buf == NULL)
		return (ENOMEM);

	if (copyin((caddr_t) src, (caddr_t) msen_buf, sizeof(msen_label))) {
		kmem_free(msen_buf, sizeof(msen_label));
		return (EFAULT);
	}

	if (!msen_valid(msen_buf)) {
		kmem_free(msen_buf, sizeof(msen_label));
		return (EINVAL);
	}
	*result = msen_add_label(msen_buf);
	ASSERT(*result);

	kmem_free(msen_buf, sizeof(msen_label));
	return (0);
}

/*
 * Copy in a mint label.
 */
int
mint_copyin_label(mint_t src, mint_t *result)
{
	mint_t mint_buf;

	*result = NULL;

	mint_buf = (mint_t) kmem_alloc(sizeof(mint_label), KM_NOSLEEP);
	if (mint_buf == NULL)
		return (ENOMEM);

	if (copyin((caddr_t) src, (caddr_t) mint_buf, sizeof(mint_label))) {
		kmem_free(mint_buf, sizeof(mint_label));
		return (EFAULT);
	}

	if (!mint_valid(mint_buf)) {
		kmem_free(mint_buf, sizeof(mint_label));
		return (EINVAL);
	}
	*result = mint_add_label(mint_buf);
	ASSERT(*result);

	kmem_free(mint_buf, sizeof(mint_label));
	return (0);
}

/*************************************************************************/
/*  Utility Routines							 */
/*************************************************************************/

/*
 * Check the category set value or division set value see whether they
 * are in ascending order and the value appear only once.
 */
static int
check_setvalue(unsigned short *list, unsigned short count)
{
        int i;

        for (i = 1; i < count ; i++)
                if (list[i] <= list[i - 1])
                        return -1;
        return 0;
}

/*
 * Compare the equality relationship between two 
 * labels of BIBA or TCSEC type.
 */
static int
b_equ(mac_b_label *bp1, mac_b_label *bp2)
{

	/*
	 * The trivial check.
	 */
	if (bp1 == bp2)
		return (1);

	/*
	 * Check the hierarchal component.
	 */
	if (bp1->b_hier != bp2->b_hier)
		return (0);

	/*
	 * Check the non-hierarchal sets.
	 */
	if (bp1->b_nonhier != bp2->b_nonhier)
		return (0);

	/*
 	 * Check that every element of l2 is in l1.
	 */
	if (bp1->b_nonhier == 0)
		return 1;
	return ! bcmp (bp1->b_list, bp2->b_list,
			  bp1->b_nonhier * sizeof(bp1->b_list[0]));
}

/*
 *  Given two arrays of unsigned short integers, l1 and l2, 
 *  of respective dimensions c1 and c2,
 *  each sorted in ascending sequence,
 *  return 1 iff l1 is a (proper or improper) subset of l2;
 *  That is, return 1 is every element of l1 is in l2.
 *  Return 0 otherwise.
 *  This routine requires, but does not check nor ASSERT, that both
 *  arrays be sorted.  
 *  It does not return correct results if lists are not sorted.
 */
static int
subset(unsigned short *l1, int c1, unsigned short *l2, int c2)
{
	unsigned short *l1e;
	unsigned short *l2e;
	unsigned int    i1;
	unsigned int    i2;

	if (c1 == 0)
		return 1;
	l1e = l1 + c1;
	l2e = l2 + c2;
	while (l2 < l2e) {
		if ((i2 = *l2++) == (i1 = *l1)) {
			if (++l1 < l1e)
				continue;
			return 1;
		}
		if (i2 > i1)		/* i1 is missing from l2 */
			return 0;
	}
	return 0;
}

/*
 * Compare the dominance relationship between two TCSEC labels
 */
static int
tcsec_dom(msen_t lp1, msen_t lp2)
{

	/* Check the heirarchical levels */
	if (lp1->b_hier < lp2->b_hier)
		return (0);

	/* Check the category sets */
	if (lp1->b_nonhier < lp2->b_nonhier)
		return (0);

	/*  return true (1) if lp2's categories are a subset of lp1's */
	return subset(lp2->b_list, lp2->b_nonhier, lp1->b_list,
		      lp1->b_nonhier );
}

/*
 * Compare the dominance relationship between two BIBA labels
 */
static int
biba_dom(mint_t lp1, mint_t lp2)
{

	/* The trivial check.  */
	if (lp2 == lp1)
		return (1);

	/*  Check the heirarchical grade */
	if (lp2->b_hier < lp1->b_hier)
		return (0);

	/*  Check the division sets */
	if (lp2->b_nonhier < lp1->b_nonhier)
		return (0);

	/*
 	 * Check that every element of lp1 is in lp2.
	 * It is not required that every element of lp2 be in lp1.
	 * return true (1) if lp1's divisions are a subset of lp2's
	 */
	return subset(lp1->b_list, lp1->b_nonhier, lp2->b_list,
		      lp2->b_nonhier);
}

/*
 * Data for memory resident msen and mint labels.
 */
struct msen_list {
	struct msen_list *mll_next;	/* Next in the list */
	msen_t	          mll_label;
};

struct mint_list {
	struct mint_list *mll_next;	/* Next in the list */
	mint_t	          mll_label;
};

static struct msen_list *msen_label_list;
static struct mint_list *mint_label_list;

/*
 * Add a label to the active list if it isn't already there.
 */
msen_t
msen_add_label(msen_t new)
{
	struct msen_list *lp;

	ASSERT(new);

	/* Check that the label is reasonable.  */
	if (!msen_valid(new)) {
#ifdef	DEBUG
		cmn_err(CE_PANIC, "Invalid msen label add attempted.(%x,%x)",
		    new->b_type, new->b_hier);
#endif	/* DEBUG */
		return (NULL);
	}

	/* Search for the label in the existing list. */
	for (lp = msen_label_list; lp != NULL; lp = lp->mll_next) {

		if (lp->mll_label->b_type != new->b_type)
			continue;
		/*
		 * The types have been found to match.
		 * If labels are equal return the existing one.
		 */
		if (msen_equal(new, lp->mll_label))
			return (lp->mll_label);
	}

	/*
	 *  Add new label to list and return a pointer to it.
	 */
	lp = (struct msen_list *) kmem_alloc(sizeof(struct msen_list),
					     KM_NOSLEEP);
	lp->mll_label = msen_dup(new);
	lp->mll_next = msen_label_list;
	msen_label_list = lp;
	return (lp->mll_label);

}

/*
 * Add a label to the active list if it isn't already there.
 */
mint_t
mint_add_label(mint_t new)
{
	struct mint_list *lp;

	ASSERT(new);

	/* Check that the label is reasonable.  */
	if (!mint_valid(new)) {
#ifdef	DEBUG
		cmn_err(CE_PANIC, "Invalid mint label add attempted.(%x,%x)",
		    new->b_type, new->b_hier);
#endif	/* DEBUG */
		return (NULL);
	}

	/* Search for the label in the existing list. */
	for (lp = mint_label_list; lp != NULL; lp = lp->mll_next) {

		if (lp->mll_label->b_type != new->b_type)
			continue;
		/*
		 * The types have been found to match.
		 * If labels are equal return the exsiting one.
		 */
		if (mint_equal(new, lp->mll_label))
			return (lp->mll_label);
	}

	/*
	 *  Add new label to list and return a pointer to it.
	 */
	lp = (struct mint_list *) kmem_alloc(sizeof(struct mint_list),
					     KM_NOSLEEP);
	lp->mll_label = mint_dup(new);
	lp->mll_next = mint_label_list;
	mint_label_list = lp;
	return (lp->mll_label);
}
