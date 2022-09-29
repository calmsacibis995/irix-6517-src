/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: hyper.c,v 65.5 1998/04/01 14:16:39 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: hyper.c,v $
 * Revision 65.5  1998/04/01 14:16:39  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for sys/systm.h, kernel only.
 *
 * Revision 65.4  1998/03/23 16:32:28  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1998/03/19 23:47:26  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.2  1997/11/06  19:59:41  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:19:23  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.125.1  1996/10/02  21:13:16  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:04  damon]
 *
 * $EndLog$
*/
/* Copyright (C) 1996 Transarc Corporation - All rights reserved. */

#include <dcedfs/common_data.h>
#include <sys/param.h>
#include <sys/types.h>
#include <dcedfs/stds.h>
#if defined(SGIMIPS) && defined(KERNEL)
#include <sys/systm.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/util/RCS/hyper.c,v 65.5 1998/04/01 14:16:39 gwehrman Exp $")

#define MAX_WORD_VAL (4294967295)
#define MAX_WORD_VAL_BY_10 (429496729)

/* 
 * dfsh_StrToHyper -- Converts a number string to a hyper number.  Accepts
 *     either nnn oro nnn,,nnn formats, where nnn can be either a signed
 *     decimal number or and octal or hex number.  For example, "-1",
 *     "0xa5a5a5a5,,-1", "0,,010", "0x123456789abcdef0", or "-12345678901234".
 *
 *     Leading white space is skipped.  Parsing terminates on any character
 *     that would be an error.  If the number parsed is legal the endStr output
 *     argument is set to point to the first unused character and a zero is
 *     returned.  On any other arithmetic or parsing error a one is returned.
 *
 * RETURN CODES --
 *     0 on success
 *     1 on failure
 */
#ifdef SGIMIPS
int dfsh_StrToHyper(char *numString, afs_hyper_t *hyperP, char **endStr)
#else  /* SGIMIPS */
int dfsh_StrToHyper(const char *numString, afs_hyper_t *hyperP, char **endStr)
#endif /* SGIMIPS */
{
    int base;
    unsigned char digit;
    const char *c;
    char *sp, *ssp;
    int code;
    int hi;
    u_int lo;
    int negate;
    int left;
    afs_hyper_t lefthalf;

    /* Convert long possibly string by hand instead of using strtol (this
     * method also works in the kernel) */

    AFS_hzero(*hyperP);
    left = 0;
    negate = 0;
    base = 0;

    /* skip leading whitespace */
    c = numString;
    while (*c == ' ' || (*c >= 011 && *c <= 015))
	c++;				/* isspace doesn't work in kernel */

    for (; *c != 0; c++) {
	int digit;
	if (c[0] == ',' && c[1] == ',') {
	    if (!base)
		return 1;
	    if (AFS_hgethi(*hyperP))
		return 1;
	    if (negate)
		AFS_hnegate(*hyperP);
	    left = 1;
	    lefthalf = *hyperP;
	    AFS_hzero(*hyperP);
	    negate = 0;
	    base = 0;
	    c++;
	    continue;
	}
	if (!base) {
	    if (*c == '-') {
		negate = 1;
		continue;
	    }

	    if (c[0] == '0' && (c[1] == 'x' || c[1] == 'X')) {
		c++;
		base = 16;
		continue;
	    } else if (*c == '0') {
		base = 8;
		continue;
	    } else if (*c >= '0' && *c <= '9') {
		base = 10;
	    } else {
		/* terminate parsing */
		break;
	    }
	}
	if (*c >= '0' && *c <= '9')
	    digit = *c - '0';
	else if (*c >= 'a' && *c <= 'f')
	    digit = *c - 'a' + 10;
	else if (*c >= 'A' && *c <= 'F')
	    digit = *c - 'A' + 10;
	else {
	    /* terminate parsing */
	    break;
	}

	if (digit >= base)
	    return 1;
	if (base == 8) {
	    if (AFS_hgethi(*hyperP) & 0xe0000000)
		return 1;
	    AFS_hleftshift(*hyperP, 3); /* h*8 */
	} else if (base == 10) {
	    afs_hyper_t tmp;
	    if (AFS_hgethi(*hyperP) > MAX_WORD_VAL_BY_10) {
		/* overflow from high byte, bail out */		
		return 1;
	    }
	    tmp = *hyperP;
	    AFS_hleftshift(*hyperP, 1); /* h*2 */
	    AFS_hleftshift(tmp, 3); /* h*8 */
	    AFS_hadd(*hyperP, tmp); /* h*2 + h*8 == (h*10) */
	} else if (base == 16) {
	    if (AFS_hgethi(*hyperP) & 0xf0000000)
		return 1;
	    AFS_hleftshift(*hyperP, 4); /* h*16 */
	}
	AFS_hadd32(*hyperP, digit);
    }
    if (!base)
	return 1;
    if (left) {
	if (AFS_hgethi(*hyperP))
	    return 1;
	AFS_hset64 (*hyperP, AFS_hgetlo(lefthalf), AFS_hgetlo(*hyperP));
    }
    if (negate)
	AFS_hnegate(*hyperP);
    if (endStr)				/* report how far we got */
	*endStr = (char *)c;
    return 0;
}

char *dfsh_HyperToStr(afs_hyper_t *h, char *s)
{
    sprintf (s, "%u,,%u", AFS_HGETBOTH(*h));
    return s;
}
