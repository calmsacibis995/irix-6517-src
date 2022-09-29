/**************************************************************************
 *									  *
 * Copyright (C) 1990-1993 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.36 $ $Author: joecd $"

#ifndef __US_H__
#define __US_H__

#include <signal.h>		/* for sigset_t */
#include <sgidefs.h>		/* for sigset_t */
#include <ulocks.h>
#include <sys/types.h>
#include <semaphore_internal.h>

/*
 * Version 3 - 4.0/4.1
 * Version 4 - 5.0 - new amalloc, r4k locks
 * Version 5 - 5.x - removal of R3K support
 * Version 6 - 6.5 - removed user-level queuing
 */
#define _USVERSION		6

typedef struct tid_entry_s {
	pid_t	t_pid;
	short	t_next;
} tid_entry_t;

typedef struct ushdr_s {
	long		u_magic;
	long		u_version;
	tid_entry_t	*u_tidmap;
	short		*u_tidmaphash;
	int		u_tidmaphashmask;
	void		*u_arena;	/* usmalloc arena header */
	char		u_mfile[PATH_MAX];
	off_t		u_mmapsize;	/* used for usconfig(GETSIZE) */
	char		*u_mapaddr;	/* where we're attached at */
	int		u_maxtidusers;  /* max users for this arena */
	/* these are for history logging */
	int		u_histon;
	hist_t		*u_histfirst;
	hist_t		*u_histlast;
	int		u_histcount;
	int		u_histerrors;
	unsigned	u_histsize;	/* max # entries */
	ulock_t		u_histlock;
	/* the following are to get coherent dumps printed */
	ulock_t		u_dumpsemalock;
	ulock_t		u_dumplocklock;
	int		u_locktype; 	/* lock type */
	int		u_shflags;	/* type of arena */

	/* special single word of transfer via usget/put info */
	void		*u_info;
	usema_t		*u_openpollsema;/* thread openpolls */
	unsigned	u_spdev;	/* regular sema dev */
	pid_t		u_lastinit;	/* pid of last initializer */
	pid_t		u_lasttidzap;	/* pid of last one to zap a pid */
	char		*u_logfile;	/* arena log file */
} ushdr_t;

/*
 * History logging flags
 */
#define _USSEMAHIST  0x0004

/*
 * Mapped memory control block, allocated at the first location in shared
 * memory.  Contains any control information needed.
 */
#define _USACCESS_LCK 0		/* byte 0 in arena */

#define _USMAGIC32		0xdeadbabe
#define _USMAGIC64		0xdeadbabf
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define _USMAGIC		_USMAGIC64
#else
#define _USMAGIC		_USMAGIC32
#endif

/*
 * System type - all systems assumed to support ll/sc - all we need
 * is whether its UP or MP
 */
#define US_UP		0x1
#define US_MP		0x2
#define US_ISMP(x)	((x) & US_MP)

extern int _us_systype;	/* are we on UP/MP */

/* prototypes for local functions */
extern int _usany_tids_left(ushdr_t *);
extern int _uprint(int, char *, ...);
extern void _usr4klocks_init(int, int);
extern short _usgettid(ushdr_t *);
extern pid_t _usget_pid(ushdr_t *, short);
extern short __usfastadd(ushdr_t *);
extern int _usclean_tidmap(ushdr_t *);
extern void __usblockallsigs(sigset_t *);
extern void __usunblockallsigs(sigset_t *);
extern int _ussemaspace(void);
extern int _seminit(void);
extern void _semadd(void);
extern int _getsystype(void);
extern int _usfsplock(int, int);
extern int _usfreadlock(int, int);
extern int _usfunsplock(int, int);
extern int _usfgetlock(int, int);

/* Arena specific spin lock hook routines */
extern int _sctllock(ulock_t, int, ...);
extern int _spin_arena_init(ulock_t);
extern ulock_t _spin_arena_alloc(usptr_t *us_ptr);

/* Override locking decision:
 * When locking is activated (__multi_thread is set) these values,
 * set through usconfig control the real flags, the __us_rsthread* flags.
 */
extern int __us_sthread_stdio;
extern int __us_sthread_misc;
extern int __us_sthread_malloc;
extern int __us_sthread_pmq;
extern int __multi_thread;

#endif /* __US_H__ */
