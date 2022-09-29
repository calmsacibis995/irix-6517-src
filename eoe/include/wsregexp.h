#ifndef __WSREGEXP_H__
#define __WSREGEXP_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 * Internationalized regular expressions
 *	on wchar_t basis
 *	by frank@ceres.esd.sgi.com Dec 3 1992
 */
#ident  "$Revision: 1.1 $"

/*
 * command codes
 */
#define	STAR	0x00000001	/* star * */
#define	CBRA	0x00000002	/* \( */
#define	RNGE	0x00000003	/* \{m,n\} */

#define	CCHR	0xffffff50	/* c */
#define	CDOT	0xffffff54	/* . */
#define	CDOL	0xffffff58	/* $ */
#define	CCL	0xffffff5c	/* [s] */
#define	CCEOF	0xffffff60	/* end of regexp */
#define	CKET	0xffffff64	/* \) */
#define	CBACK	0xffffff68	/* \digit */
#define	ICLASS	0xffffff6c	/* [:class:] */
#define	EQCLASS	0xffffff70	/* [=c=] */
#define	COLLSYM	0xffffff74	/* [.cc.] */
#define	CNEG	0xffffff80	/* [^s] */
#define	CRNGE	0xffffffff	/* c-c */

#define	RE_NBRA		32		/* maximal number of \( \) */
#define	RE_MAX_RANGE	(1 << 30)	/* maximal numbers for \{m,n\} */

/*
 * rex exp data structure
 * 'sed', 'str' and 'locs' must be initialized by the user
 */
struct rexdata {
	short	sed;		/* flag for sed */
	short	nbra;		/* number of brackets/braces */
	wchar_t	*str;		/* string to be wsrecompile()'d */
	wchar_t	*locs;		/* break-out point */
	int	err;		/* returned error code, 0 = no error */
	int	circf;
	int	nodelim;	/* no delimiter flag */
	wchar_t	*loc1;		/* returned by step() */
	wchar_t	*loc2;		/* returned by advance() */
	wchar_t	*brsl[RE_NBRA];	/* start ptr of n'th regexp */
	wchar_t	*brel[RE_NBRA];	/* end ptr of n'th regexp */
};

/*
 * error codes
 */
#define	ERR_NORMBR	1	/* no remembered search string */
#define	ERR_REOVFLOW	2	/* regexp overflow */
#define	ERR_BRA		3	/* \( \) imbalance */
#define	ERR_DELIM	4	/* illegal or missing delimiter */
#define	ERR_NBR		5	/* bad number in \{ \} */
#define	ERR_2MNBR	6	/* more than 2 numbers given in \{ \} */
#define	ERR_DIGIT	7	/* ``\digit'' out of range */
#define	ERR_2MLBRA	8	/* too many \( */
#define	ERR_RANGE	9	/* range number too large */
#define	ERR_MISSB	10	/* } expected after \ */
#define	ERR_BADRNG	11	/* first number exceeds second in \{ \} */
#define	ERR_SIMBAL	12	/* [ ] imbalance */
#define	ERR_SYNTAX	13	/* illegal regular expression */
#define	ERR_ILLCLASS	14	/* illegal [:class:] */
#define	ERR_EQUIL	15	/* illegal [=c=] */
#define	ERR_COLL	16	/* illegal [.cc.] */

#define	ERR_MAX		16
#define	ERR_SIZE	-1	/* wchar_t or * > long */

/*
 * parameter prototyping
 */
long	*wsrecompile(struct rexdata *, long *, long *, wchar_t seof);
int	wsrestep(struct rexdata *, wchar_t *, long *);
int	wsrematch(struct rexdata *, wchar_t *, long *);
char	*wsreerr(int);

#ifdef __cplusplus
}
#endif
#endif /* !__WSREGEXP_H__ */

