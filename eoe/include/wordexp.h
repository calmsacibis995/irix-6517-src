#ifndef _WORDEXP_H
#define _WORDEXP_H
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.3 $"
/*
*
* Copyright 1995, Silicon Graphics, Inc.
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
*/

#include <sys/types.h>

#define WRDE_APPEND	0001	/* Append pathnames */
#define WRDE_DOOFFS	0002	/* Specify how many null pointers to add */
#define WRDE_NOCMD	0004	/* Fail if command substitution is requested */
#define WRDE_REUSE	0010	/* Reuse old wordexp */
#define WRDE_SHOWERR	0020	/* Do not redirect stderr to /dev/null */
#define WRDE_UNDEF	0040	/* Error if shell variable is unset */

#define WRDE_NOSYS	(-1)	/* unsuported */
#define WRDE_BADCHAR	(-2)	/* Special char in bad context */
#define WRDE_BADVAL	(-3)	/* Reference to unset shell variable */
#define WRDE_CMDSUB	(-4)	/* Command substitution requested */
#define WRDE_NOSPACE	(-5)	/* An attempt to allocate memory failed */
#define WRDE_SYNTAX	(-6)	/* Shell syntax error */

typedef struct
{
	size_t	we_wordc;	/* count of paths matched by words */
	char	**we_wordv;	/* pointer to list of exapnded words */
	size_t	we_offs;	/* slots to reserve at we_wordv[] start */
} wordexp_t;

extern int	wordexp(const char *,wordexp_t *, int);
extern void	wordfree(wordexp_t *);

#ifdef __cplusplus
}
#endif

#endif /*_WORDEXP_H*/
