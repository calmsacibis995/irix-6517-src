/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:include/stak.h	1.1.4.1"

#ifndef STAK_SMALL
/*
 * David Korn
 * AT&T Bell Laboratories
 *
 * Interface definitions for a stack-like storage library
 *
 */

#if defined(__STDC__) || __cplusplus || c_plusplus
#   define __ARGS(args)	args
#else
#    define const	/*empty*/
#   define __ARGS(args)	()
#endif

typedef struct _stak_
{
	int		stakleft;	/* number of bytes left in frame */
	char		*staktop;	/* current stack location */
	char		*stakbot;	/* last returned stack location */
	short		stakref;	/* reference count */
#ifdef _STAK_PRIVATE
	_STAK_PRIVATE
#endif /* _STAK_PRIVATE */
} Stak_t;

#define STAK_SMALL	1	/* argument to stakcreate */

#if __cplusplus
    extern "C"
    {
#endif
	extern Stak_t	*stakcreate __ARGS((int));
	extern Stak_t	*stakinstall __ARGS((Stak_t*, char *(*)(int)));
	extern int	stakdelete __ARGS((Stak_t*));
	extern char	*stakalloc __ARGS((unsigned));
	extern char	*stakcopy __ARGS((const char*));
	extern char	*stakset __ARGS((char*, unsigned));
	extern char	*stakseek __ARGS((unsigned));
	extern int	stakputs __ARGS((const char*));
	extern char	*stakfreeze __ARGS((unsigned));
	extern char	*_stakgrow __ARGS((unsigned));
#if __cplusplus
    }
#endif

extern Stak_t		_stak_cur;	/* used by macros */

#define	staklink(sp)	((sp)->stakref++)
#define	stakptr(n)	(_stak_cur.stakbot+(n))
#define	staktell()	(_stak_cur.staktop-_stak_cur.stakbot)
#define stakputc(c)	((--_stak_cur.stakleft<0? _stakgrow(1):0), \
					*_stak_cur.staktop++=(c))

#endif /* STAK_SMALL */
