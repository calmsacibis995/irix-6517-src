#ifndef __STRINGS_H__
#define __STRINGS_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Copyright (C) 1989-1996 Silicon Graphics, Inc. All rights reserved.
 */
#ident "$Revision: 1.12 $"

/*
 * WARNING - this is an XPG4 header
 */
#include <standards.h>
#include <sys/types.h>

#if _SGIAPI
/* backward compat - we defined ffs/strcasecmp in string.h */
#include <string.h>

#else
int		ffs(int);
/* Case-insensitive comparision routines from 4.3BSD. Used in XPG4 also */
extern int	strcasecmp(const char *, const char *);
extern int	strncasecmp(const char *, const char *, size_t);
#endif	/* _SGIAPI */

extern int	bcmp(const void *, const void *, size_t);
extern void	bcopy(const void *, void *, size_t);
extern void	bzero(void *, size_t);
extern char	*index(const char *, int);
extern char	*rindex(const char *, int);

#ifdef __INLINE_INTRINSICS
/* The functions made intrinsic here can be activated by adding the 
** option -D__INLINE_INTRINSICS
*/
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 721))
#pragma intrinsic (bcopy)
#pragma intrinsic (bzero)
#endif /* COMPILER_VERSION >= 721 */
#endif /* __INLINE_INTRINSICS */


#ifdef __cplusplus
}
#endif


#endif /* !__STRINGS_H__ */
