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

#ifdef __STDC__
	#pragma weak mac_from_msen_mint = _mac_from_msen_mint
	#pragma weak mac_from_msen = _mac_from_msen
	#pragma weak mac_from_mint = _mac_from_mint
	#pragma weak msen_from_text = _msen_from_text
	#pragma weak msen_to_text = _msen_to_text
	#pragma weak msen_from_mac = _msen_from_mac
	#pragma weak msen_free = _msen_free
	#pragma weak msen_valid = _msen_valid
	#pragma weak msen_equal = _msen_equal
	#pragma weak msen_dom = _msen_dom
	#pragma weak msen_size = _msen_size
	#pragma weak mint_from_text = _mint_from_text
	#pragma weak mint_to_text = _mint_to_text
	#pragma weak mint_from_mac = _mint_from_mac
	#pragma weak mint_free = _mint_free
	#pragma weak mint_valid = _mint_valid
	#pragma weak mint_equal = _mint_equal
	#pragma weak mint_dom = _mint_dom
	#pragma weak mint_size = _mint_size
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include "mac_util.h"

/*
 *  The following routines operate on the Mandatory Sensitivity (MSEN),
 *  and Mandatory Integrity (MINT) components of the SGI MAC Label.
 *  Several of the routines operate by constructing a full mac label
 *  by subtituting a wildcard for the missing label component, then
 *  invoking existing mac label routines.  It is anticipated that
 *  these routines will eventually replace the mac routines.
 */

static mac_t
mac_from_internal(msen_t msenp, mint_t mintp)
{
	int i;
	mac_t lp;

	if ((lp = (mac_t) malloc(sizeof(struct mac_label))) == NULL)
		return(NULL);

	/* sensitivity component first */
	if (msenp == NULL)
	{
		lp->ml_msen_type =	MSEN_EQUAL_LABEL;
		lp->ml_level =		0;
		lp->ml_catcount =	0;
	}
	else
	{
		lp->ml_msen_type =	msenp->b_type;
		lp->ml_level =		msenp->b_hier;
		lp->ml_catcount =	msenp->b_nonhier;
		for (i = 0; i < lp->ml_catcount; i++)
			lp->ml_list[i] = msenp->b_list[i];
	}

	/* Integrity component last */
	if (mintp == NULL)
	{
		lp->ml_mint_type =	MINT_EQUAL_LABEL;
		lp->ml_grade =		0;
		lp->ml_divcount =	0;
	}
	else
	{
		lp->ml_mint_type =	mintp->b_type;
		lp->ml_grade =		mintp->b_hier;
		lp->ml_divcount =	mintp->b_nonhier;
		for (i = 0; i < lp->ml_divcount; i++)
			lp->ml_list[i + lp->ml_catcount] = mintp->b_list[i];
	}

	if (mac_valid(lp) <= 0)
	{
		free(lp);
		return(NULL);
	}

	return(lp);
}

typedef int (*mbl_convert)(label_segment_p, char *);

static mac_b_label *
from_text_internal(const char *str, mbl_convert convert)
{
	char *tmpstr;
	mac_b_label *mp;
	label_segment ls;

	/* reject bogus arg */
	if (str == NULL)
		return(NULL);

	/* copy input argument */
	tmpstr = strdup(str);
	if (tmpstr == NULL)
		return(NULL);

	/* allocate mac_b_label structure */
	if ((mp = (mac_b_label *) malloc(sizeof(*mp))) == NULL)
	{
		free(tmpstr);
		return(NULL);
	}

	/* look up appropriate type */
	ls.type = 0;
	ls.level = 0;
	ls.count = 0;
	ls.list = mp->b_list;
	if ((*convert)(&ls, tmpstr) == -1)
	{
		free(tmpstr);
		free(mp);
		return(NULL);
	}
	mp->b_type = ls.type;
	mp->b_hier = ls.level;
	mp->b_nonhier = ls.count;

	free(tmpstr);
	return(mp);
}

static char *
to_text_internal(mac_b_label *mp, size_t *len_p, mbl_convert convert)
{
	char *buf;
	label_segment ls;

	/* allocate buffer */
	buf = (char *) malloc((size_t) 8192);
	if (buf == NULL)
		return(NULL);

	/* get text representation */
	ls.type = mp->b_type;
	ls.level = mp->b_hier;
	ls.count = mp->b_nonhier;
	ls.list = mp->b_list;
	if ((*convert)(&ls, buf) == -1)
	{
		free(buf);
		return(NULL);
	}

	/* reallocate buffer to smallest size */
	buf = realloc(buf, strlen(buf) + 1);
	if (buf == NULL)
		return(NULL);
	
	/* success! */
	if (len_p != NULL)
		*len_p = strlen(buf);
	return(buf);
}

/*
 *  Return a pointer to a mac label constructed from the given
 *  msen and mint labels.
 */
mac_t
mac_from_msen_mint(msen_t msenp, mint_t mintp)
{
	if (!msen_valid(msenp) || !mint_valid(mintp))
		return NULL;

	return(mac_from_internal(msenp, mintp));
}

/*
 *  Return a pointer to a mac label constructed from the given msen
 *  label, substituting MINT_EQUAL_LABEL for the needed mint component.
 */
mac_t
mac_from_msen(msen_t msenp)
{
	if (!msen_valid(msenp))
		return NULL;

	return(mac_from_internal(msenp, (mint_t) NULL));
}

/*
 *  Return a pointer to a mac label constructed from the given mint
 *  label, substituting MSEN_EQUAL_LABEL for the needed msen component.
 */
mac_t
mac_from_mint(mint_t mintp)
{
	if (!mint_valid(mintp))
		return NULL;

	return(mac_from_internal((msen_t) NULL, mintp));
}

/*
 *  Return the binary msen label described in the ascii text, null if label
 *  is invalid.
 */

static int
msen_from_mac_text(label_segment_p ls, char *str)
{
	mac_t mp;

	mp = mac_from_text(str);
	if (mp != NULL)
	{
		ls->type = mp->ml_msen_type;
		ls->level = mp->ml_level;
		ls->count = mp->ml_catcount;
		memcpy(ls->list, mp->ml_list,
		       ls->count * sizeof(*mp->ml_list));
		mac_free(mp);
		return(0);
	}
	return(-1);
}

msen_t 
msen_from_text(const char *str)
{
	msen_t mp;

	mp = from_text_internal(str, __msen_from_text);
	if (mp == NULL)
		return(from_text_internal(str, msen_from_mac_text));
	return(mp);
}

/*
 *  Return a pointer to the ascii representation of a mandatory
 *  sensitivity (msen) label.
 */
char *
msen_to_text(msen_t mp, size_t *len_p)
{
	if (!msen_valid(mp))
		return(NULL);
	return(to_text_internal(mp, len_p, __msen_to_text));
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

	if (mac_valid(lp) <= 0)
		return NULL;

	if ((msenp = malloc(sizeof(msen_label))) == NULL)
		return NULL;

	msenp->b_type =    lp->ml_msen_type;
	msenp->b_hier =    lp->ml_level;
	msenp->b_nonhier = lp->ml_catcount;
	for (i = 0; i < lp->ml_catcount; i++)
		msenp->b_list[i] = lp->ml_list[i];

	return msenp;
}

/*
 *  Free a msen label.
 */
int
msen_free(void *msenp)
{
        if (msenp)
                free(msenp);
        return 0;
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
		if (__check_setvalue(msenp->b_list, msenp->b_nonhier) == -1)
			return 0;
		break;
	case MSEN_UNKNOWN_LABEL:
	default:
		return 0;
	}
	return 1;
}

/*
 * Compare two msen labels,
 *      Returns 1 iff the labels are equal
 *      Returns 0 otherwise.
 *      Returns -1 if errors.
 */
int
msen_equal(msen_t msen1, msen_t msen2)
{
	label_segment ls1, ls2;

	if (msen_valid(msen1) <= 0 || msen_valid (msen2) <= 0) {
		setoserror(EINVAL);
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
			ls1.type = msen1->b_type;
			ls1.level = msen1->b_hier;
			ls1.count = msen1->b_nonhier;
			ls1.list = msen1->b_list;

			ls2.type = msen2->b_type;
			ls2.level = msen2->b_hier;
			ls2.count = msen2->b_nonhier;
			ls2.list = msen2->b_list;

			if (!__segment_equal(&ls1, &ls2))
				return(0);
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
	label_segment ls1, ls2;

	if (msen_valid(lp1) <= 0 || msen_valid (lp2) <= 0) {
		setoserror(EINVAL);
		return (0);
	}

	/* Labels are same */
	if (lp1 == lp2)
		return (1);

	/* If either label is MSEN_EQUAL don't check sensitivity further.  */
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
			ls1.type = lp1->b_type;
			ls1.level = lp1->b_hier;
			ls1.count = lp1->b_nonhier;
			ls1.list = lp1->b_list;

			ls2.type = lp2->b_type;
			ls2.level = lp2->b_hier;
			ls2.count = lp2->b_nonhier;
			ls2.list = lp2->b_list;
		
			if (!__tcsec_dominate(&ls1, &ls2))
				return (0);
			return(1);
		}
	}

	return (0);
}

ssize_t
msen_size(msen_t lp)
{
	/* if it is a invalid label, return size 0 */
	if (!msen_valid(lp)) {
		setoserror(EINVAL);
		return((ssize_t) -1);
	}
	return((ssize_t) sizeof(struct mac_b_label));
}

/*
 *  Return the binary mint label described in the ascii text, null if label
 *  is invalid.
 */
static int
mint_from_mac_text(label_segment_p ls, char *str)
{
	mac_t mp;

	mp = mac_from_text(str);
	if (mp != NULL)
	{
		ls->type = mp->ml_mint_type;
		ls->level = mp->ml_grade;
		ls->count = mp->ml_divcount;
		memcpy(ls->list, mp->ml_list + mp->ml_catcount,
		       ls->count * sizeof(*mp->ml_list));
		mac_free(mp);
		return(0);
	}
	return(-1);
}

mint_t 
mint_from_text(const char *str)
{
	mint_t mp;

	mp = from_text_internal(str, __mint_from_text);
	if (mp == NULL)
		return(from_text_internal(str, mint_from_mac_text));
	return(mp);
}

/*
 *  Return a pointer to the ascii representation of a mandatory
 *  sensitivity (mint) label.
 */
char *
mint_to_text(mint_t mp, size_t *len_p)
{
	if (!mint_valid(mp))
		return(NULL);
	return(to_text_internal(mp, len_p, __mint_to_text));
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

	if (mac_valid(lp) <= 0)
		return NULL;

	if ((mintp = malloc(sizeof(mint_label))) == NULL)
		return NULL;

	mintp->b_type =    lp->ml_mint_type;
	mintp->b_hier =    lp->ml_grade;
	mintp->b_nonhier = lp->ml_divcount;
	for (i = 0; i < lp->ml_divcount; i++)
		mintp->b_list[i] = lp->ml_list[lp->ml_catcount + i];

	return mintp;
}

/*
 *  Free a mint label.
 */
int
mint_free(void *mintp)
{
        if (mintp)
                free(mintp);
        return 0;
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
	label_segment ls1, ls2;

	if (mint_valid(mintp1) <= 0 || mint_valid(mintp2) <= 0) {
		setoserror(EINVAL);
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
			ls1.type = mintp1->b_type;
			ls1.level = mintp1->b_hier;
			ls1.count = mintp1->b_nonhier;
			ls1.list = mintp1->b_list;

			ls2.type = mintp2->b_type;
			ls2.level = mintp2->b_hier;
			ls2.count = mintp2->b_nonhier;
			ls2.list = mintp2->b_list;

			return(__segment_equal(&ls1, &ls2));
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
		if (__check_setvalue(mintp->b_list, mintp->b_nonhier) == -1)
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
	label_segment ls1, ls2;

	if (mint_valid(lp1) <= 0 || mint_valid (lp2) <= 0) {
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
					ls1.type = lp1->b_type;
					ls1.level = lp1->b_hier;
					ls1.count = lp1->b_nonhier;
					ls1.list = lp1->b_list;

					ls2.type = lp2->b_type;
					ls2.level = lp2->b_hier;
					ls2.count = lp2->b_nonhier;
					ls2.list = lp2->b_list;

					return(__biba_dominate(&ls1, &ls2));
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

ssize_t
mint_size(mint_t lp)
{
	/* if it is a invalid label, return size 0 */
	if (!mint_valid(lp)) {
		setoserror(EINVAL);
		return((ssize_t) -1);
	}
	return((ssize_t) sizeof(struct mac_b_label));
}
