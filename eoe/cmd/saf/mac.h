/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_MAC_H
#define	_MAC_H

#ident	"@(#)head.usr:mac.h	1.2.7.4"
#ifndef sgi
#include <sys/mac.h>
#endif

/*
 * Following are definitions for the LTDB and history log files.
 */

#define	LTF_ALIAS	"/etc/security/mac/ltf.alias"
#define	HIST_ALIAS_ADD	"/etc/security/mac/hist.alias.add"
#define	HIST_ALIAS_DEL	"/etc/security/mac/hist.alias.del"

#define	LTF_CAT		"/etc/security/mac/ltf.cat"
#define	HIST_CAT_ADD	"/etc/security/mac/hist.cat.add"
#define	HIST_CAT_DEL	"/etc/security/mac/hist.cat.del"

#define	LTF_CLASS	"/etc/security/mac/ltf.class"
#define	HIST_CLASS_ADD	"/etc/security/mac/hist.class.add"
#define	HIST_CLASS_DEL	"/etc/security/mac/hist.class.del"

#define	LTF_LID		"/etc/security/mac/lid.internal"
#define	HIST_LID_ADD	"/etc/security/mac/hist.lid.add"
#define	HIST_LID_DEL	"/etc/security/mac/hist.lid.del"

/*
 * Following are the maximum values for classifications, categories,
 * and level identifiers.  These user defined values must be within
 * the boundary of the maximum allowable values for the implementation,
 * defined in the system mac.h header file.
 */

#define	CLASS_MAX	MAXCLASSES
#define	CAT_MAX		MAXCATS
#define	LID_MAX		MAXLIDS

/*
 * Following are defines used for manipulation of the mac_level
 * structure elements.
 * NB_LONG - number of bits in a ulong
 * LONG_SHIFT - used to shift the catsig array 
 */
#define NB_LONG		((sizeof(ulong)*NBBY))
#define LONG_SHIFT	5

/*
 * Following is the short hand definition for all categories.
 */

#define	LVL_ALLCATS	"ALL"

/*
 * Following are some well-defined level alias names.
 */

#define	SYS_PUBLIC	"SYS_PUBLIC"
#define	SYS_PRIVATE	"SYS_PRIVATE"
#define	SYS_RANGE_MAX	"SYS_RANGE_MAX"	/* dominates all levels */
#define USER_PUBLIC	"USER_PUBLIC"
#define USER_LOGIN	"USER_LOGIN"
#define	SYS_AUDIT	"SYS_AUDIT"	
#define	SYS_OPERATOR	"SYS_OPERATOR"	
#define	SYS_RANGE_MIN	"SYS_RANGE_MIN"	/* dominated by all levels */

/*
 * Following are LTF name constants.
 */

#define	LVL_MAXNAMELEN	30	/* limit on name length */
#define	LVL_NAMESIZE	32	/* fixed length LTF name records */
#define	LVL_NAMESHIFT	5

/*
 * The following structure defines the format of an LTF record
 * for ltf.class, ltf.cat, and ltf.alias.
 */

struct ltf_rec {
	char ltf_name[LVL_NAMESIZE];	/* class/cat/alias name */
};

/*
 * Following are definitions of requests for alias and fully qualified
 * level names (lvlout(3)).
 */

#define	LVL_ALIAS	1
#define	LVL_FULL	2

/*
 * The following structure defines the format of an LTF history record.
 */

#define	LVL_NUMSIZE	11	/* max space for decimal representation */
				/* of a 32 byte number */

struct ltdbhist_rec {
	char	lth_namelen[LVL_NUMSIZE];/* length of name plus terminator */
	char	lth_val[LVL_NUMSIZE];	/* class, cat, or LID value */
	char	lth_mtime[LVL_NUMSIZE];	/* modification time */
	char	lth_name[1];		/* new name */
};

/*
 * Device allocation structure used by devalloc(3X)
 */

struct dev_alloca {
	ushort		state;		/* device state        */
	ushort		mode;		/* device mode         */
#ifdef sgi
	dev_t		level;		/* device level        */
	dev_t		hilevel;	/* device range:lolevel*/
	dev_t		lolevel;	/* device range:lolevel*/
#else
	level_t		level;		/* device level        */
	level_t		hilevel;	/* device range:lolevel*/
	level_t		lolevel;	/* device range:lolevel*/
#endif
	uid_t		uid;		/* device user         */
	ushort		relflag;	/* device release flag */
	ushort		pad;		/* unused padding field*/
};

#if defined (__STDC__)

extern int devstat(const char *, int, struct devstat *);
extern int fdevstat(int, int, struct devstat *);
extern int flvlfile(int, int, level_t *);
extern int lvldom(const level_t *, const level_t *);
extern int lvlequal(const level_t *, const level_t *);
extern int lvlfile(const char *, int, level_t *);
extern int lvlipc(int, int, int, level_t *);
extern int lvlproc(int,  level_t *);
extern int mkmld(const char *, const mode_t);
extern int mldmode(int);

extern int lvlin(const char *, level_t *);
extern int lvlintersect(const level_t *, const level_t *, level_t *);
extern int lvlout(const level_t *, char *, int, int);
extern int lvlunion(const level_t *, const level_t *, level_t *);
extern int lvlvalid(const level_t *);

#else	/* !defined (__STDC__) */

extern int devstat();
extern int fdevstat();
extern int flvlfile();
extern int lvldom();
extern int lvlequal();
extern int lvlfile();
extern int lvlipc();
extern int lvlproc();
extern int mkmld();
extern int mldmode();

extern int lvlin();
extern int lvlintersect();
extern int lvlout();
extern int lvlunion();
extern int lvlvalid();

#endif	/* defined (__STDC__) */

#endif	/* _MAC_H */
