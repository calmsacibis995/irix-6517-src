/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, 1996 Silicon Graphics, Inc.	  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __CKPT_H__
#define __CKPT_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.108 $"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/procset.h>
#include <sys/proc.h>
#include <sys/stat.h>

/*
 * CPR Statefile Reversion Number
 */
#define	CKPT_REVISION_NONE	0	/* Null rev */
#define	CKPT_REVISION_10	1	/* CPR Rev 1.0 */
#define	CKPT_REVISION_11	2	/* CPR Rev 1.1 */
#define	CKPT_REVISION_12	3	/* CPR Rev 1.2 */
#define	CKPT_REVISION_CURRENT	CKPT_REVISION_12


/* 256 as file len is big enough for CPR purpose */
#ifdef FULLSIZE
#define	CPATHLEN		PATH_MAX
#else
#define	CPATHLEN		(PATH_MAX/4)
#endif /* FULLSIZE */

#define	CPR			"/usr/sbin/cpr"
#define	STATEF_SHARE		"share"
#define	STATEF_INDEX		"index"
#define	STATEF_STATE		"state"
#define	STATEF_IMAGE		"image"

#define	STATEF_PIPE		"pipe"
#define	STATEF_FILE		"file"
#define	STATEF_SHM		"shm"

#define DEVZERO			"/dev/zero"
#define DEVCCSYNC		"/dev/ccsync"
#define DEVTTY			"/dev/tty"
#define DEVCONSOLE		"/dev/console"
#define	DEVUSEMA		"/dev/usema"
#define	DEVUSEMACLONE		"/dev/usemaclone"

/*
 * CPR attributes control file saved in the home dir of THE USER WHO STARTS A CPR.
 */
#define	CKPT_ATTRFILE		".cpr"
#define	CKPT_ATTRFILE_GUI	"/tmp/.cpr_gui"

#define STRERR			strerror(oserror())


/*
 * POSIX definitions of arguments, types, etc.
 */
struct ckpt_args {
	unsigned long	ckpt_type;	/* data type */
	void 		*ckpt_data;	/* data */
};
/*
 * ckpt_type's
 */
#define	CKPT_TYPE_CALLBACK	1L

/*
 * Arg definitions
 *
 * CKPT_TYPE_CALLBACK
 */
typedef struct ckpt_arg_callback {
	unsigned long	callback_type;			/* see below */
	int		(*callback_func)(void *);	/* callback address */
	void		*callback_arg;			/* arg for callback */
} ckpt_arg_callback_t;

#define	CKPT_CALLBACK_HIERARCHY	1L	/* callback from initial proc in */
					/* process hierarchy */
/*
 * Steal 4 bits for passing control into ckpt_create() (assuming 32 bit long)
 *
 * CKPT_TYPE_NOEXEC is used to make sure that applications calling ckpt_xxx
 * don't exec other setuid program.
 */
#define CKPT_TYPE_NOEXEC	(1<<28)		/* don't exec for this request */
#define CKPT_TYPE_ARCLIENT	(1<<29)		/* create array client only */

#define CKPT_TYPE_MASK		0x0FFFFFFF
#define CKPT_REAL_TYPE(t)	((t) & CKPT_TYPE_MASK)

typedef	int64_t		ckpt_id_t;
typedef uint64_t	ckpt_type_t;

/*
 * To comply the latest POSIX definitions. Make sure there is no overlap between
 * P_SID... defs in procset.h and CPR ID extensions.
 */
#define	P_ASH		10	/* SGI specific */
#define	P_HID		11	/* checkpoint proc hierarchy */
#define	P_SGP		12	/* checkpoint share group */

#define	CKPT_PROCESS	P_PID
#define	CKPT_PGROUP	P_PGID
#define	CKPT_SESSION	P_SID
#define	CKPT_ARSESSION	P_ASH
#define	CKPT_HIERARCHY	P_HID
#define	CKPT_SGROUP	P_SGP

/*
 * Array Session related definitions
 */
#define CKPT_IS_ASH(t)		(CKPT_REAL_TYPE(t) == CKPT_ARSESSION)

#define CKPT_STOP_SYNC_POINT	"ckpt-stop-sync-point\n"
#define CKPT_DUMP_SYNC_POINT	"ckpt-dump-sync-point\n"
#define CKPT_ABORT_SYNC_POINT	"ckpt-abort-sync-point\n"
#define CKPT_RESTART_SYNC_POINT	"ckpt-restart-sync-point\n"
#define CKPT_GUI_SYNC_POINT	"ckpt-gui-sync-point\n"

#ifndef PSCOMSIZ
#define PSCOMSIZ	32
#endif
#ifndef PSARGSZ
#define PSARGSZ		80
#endif

typedef long long ckpt_rev_t;
/*
 * Information about a CPR statefile; One per process.
 * Callers have to free all the memory by following cs_next after use.
 */
typedef struct ckpt_stat {
	struct ckpt_stat	*cs_next;	/* next process */
	ckpt_rev_t		cs_revision;	/* CPR revision # */
	ckpt_id_t		cs_id;		/* POSIX CPR ID */
	ckpt_type_t		cs_type;	/* POSIX CPR type */
	pid_t			cs_pid;		/* proc pid */
	pid_t			cs_ppid;	/* proc parent pid */
	pid_t			cs_pgrp;	/* proc group leader */
	pid_t			cs_sid;		/* session id */
	struct stat		cs_stat;	/* see stat(2) */
	char            	cs_nfiles;	/* # of open files */
	char            	cs_comm[PSCOMSIZ]; /* process name */
	char            	cs_psargs[PSARGSZ]; /* and arguments */
	char            	cs_cdir[PATH_MAX]; /* current directory */
	int			cs_cpuboard;	/* board (sys/invent.h) */
	int			cs_cpu;		/* cpu (sys/invent.h) */
	int			cs_abi;		/* abi (sys/kabi.h) */
} ckpt_stat_t;

/*
 * Definitions for cpr_flags
 */
#define	CKPT_OPENFILE_DISTRIBUTE	0x1	/* Save openfiles where they are */
#define	CKPT_RESTART_INTERACTIVE	0x2	/* Restart with job control */
#define	CKPT_CHECKPOINT_UPGRADE		0x4	/* Save DSOs & Executables */
#define	CKPT_CHECKPOINT_CONT		0x8	/* Continue the ckpt'ed procs */
#define	CKPT_CHECKPOINT_KILL		0x10	/* Kill the ckpt'ed procs */
#define	CKPT_NQE			0x20	/* Use NQE semantics */
#define	CKPT_RESTART_CPRCMD		0x80000000	/*  CPR INTERNAL ONLY */
#define	CKPT_CHECKPOINT_STEP		0x40000000	/*  CPR INTERNAL ONLY */
#define	CKPT_BATCH			0x20000000	/*  CPR INTERNAL ONLY */
#define	CKPT_CHECKPOINT_CPRCMD		0x80000000	/*  CPR INTERNAL ONLY */
/*
 * ckpt_attr support
 */
typedef struct ckpt_attr {
	ckpt_id_t	ckpt_id;
	ckpt_type_t	ckpt_type;
	ckpt_rev_t	ckpt_revision;
} ckpt_attr_t;

#define	CKPT_ATTR_ID	0x1
#define	CKPT_ATTR_TYPE	0x2
#define	CKPT_ATTR_REV	0x4

extern int cpr_flags;
extern int cpr_debug;
extern int pipe_rc;

extern int ckpt_setup(struct ckpt_args *[], size_t);
extern int ckpt_create(const char *, ckpt_id_t, u_long, struct ckpt_args *[], size_t);
extern ckpt_id_t ckpt_restart(const char *, struct ckpt_args *[], size_t);
extern int ckpt_remove(const char *);
extern int ckpt_stat(const char *, ckpt_stat_t **);
extern void ckpt_reach_sync_point(char *, char);
extern void ckpt_error(const char *, ...);
extern void ckpt_notice(const char *, ...);
extern char *ckpt_type_str(ckpt_type_t);
extern char *ckpt_action_str(u_long);
extern char *rev_to_str(ckpt_rev_t);
extern int ckpt_getattr(const char *, int, ckpt_attr_t *);

#ifdef DEBUG

#define IFDEB1(a)		{if (cpr_debug & 0x1) (a);}
#define IFDEB2(a)		{if (cpr_debug & 0x2) (a);}

extern int cpr_line;
extern char cpr_file[];
#define cerror  strcpy(cpr_file, __FILE__);cpr_line=__LINE__;ckpt_error
#define cnotice strcpy(cpr_file, __FILE__);cpr_line=__LINE__;ckpt_notice

#else

#define IFDEB1(a)
#define IFDEB2(a)

#define cerror  ckpt_error
#define cnotice ckpt_notice

#endif /* DEBUG */

#ifdef __cplusplus
}
#endif
#endif /* !__CKPT_H__ */
