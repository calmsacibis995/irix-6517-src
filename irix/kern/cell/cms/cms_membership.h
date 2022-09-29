#ifndef	_CMS_MEMBERSHIP_H
#define	_CMS_MEMBERSHIP_H
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
 * Quorum types.
 */
typedef enum {
	QUORUM_MAJORITY	=	1,	/* Majority quorum */
	QUORUM_MINORITY	=	2,	/* Minority quorum */
	QUORUM_TIE	=	3	/* Tie quorum      */
} cms_quorum_t;

typedef enum {
	QUORUM_TYPE_INIT,		/* Quorum nneded at init time */
	QUORUM_TYPE_RUN			/* Quorum after first membership */
} cms_quorum_type_t;

extern	uint_t		cms_quorum_get(cms_quorum_type_t);
extern	cms_quorum_t	cms_quorum_check(cms_quorum_type_t, cell_set_t *);
extern	void		cms_membership_compute(cell_set_t *, boolean_t);
extern	boolean_t	cms_comb_dynamic_init(void);
extern	cell_set_t *	cms_comb_find_dynamic(cell_set_t *, boolean_t);
extern	boolean_t	cms_membership_validate(cell_set_t *, boolean_t);


#endif /* _CMS_MEMBERSHIP_H */
