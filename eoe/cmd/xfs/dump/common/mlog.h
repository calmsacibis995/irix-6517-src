#ifndef MLOG_H
#define MLOG_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/mlog.h,v 1.9 1996/02/09 01:13:54 doucette Exp $"

/* mlog.[hc] - message logging abstraction
 */
#include <stdarg.h>

/* defined log levels - msg will be logged only if cmdline -v arg
 * is greater than or equal to its level.
 */
#define MLOG_NORMAL	0	/* OLD */
#define MLOG_SILENT	0	/* NEW */
#define MLOG_VERBOSE	1
#define MLOG_TRACE	2
#define MLOG_DEBUG	3
#define MLOG_NITTY	4
#define MLOG_LEVELMASK	0xff

/* modifier flags for the level - only the first is generally exported
 */
#define MLOG_BARE	0x010000	/* don't print preamble */
#define MLOG_NOTE	0x020000	/* use NOTE format */
#define MLOG_WARNING	0x040000	/* use WARNING format */
#define MLOG_ERROR	0x080000	/* use ERROR format */
#define MLOG_NOLOCK	0x100000	/* do not attempt to obtain mlog lock */

/* subsystem ids: allows per-subsystem control of log level.
 * use to index mlog_level_ss.
 */
#define MLOG_SS_GEN	0		/* default if no subsystem specified */
#define MLOG_SS_PROC	1		/* process/thread-related */
#define MLOG_SS_DRIVE	2		/* I/O-related */
#define MLOG_SS_MEDIA	3		/* media-related */
#define MLOG_SS_INV	4		/* media inventory */
#ifdef DUMP
#define MLOG_SS_INOMAP	5		/* dump inode number map */
#endif /* DUMP */
#ifdef RESTORE
#define MLOG_SS_TREE	5		/* restore tree */
#endif /* RESTORE */
#define MLOG_SS_CNT	6		/* NOTE! bump this when adding ss */	

#define MLOG_SS_SHIFT	8
#define MLOG_SS_MASK	( 0xff << MLOG_SS_SHIFT )

/* subsystem flags - NOTE! only one may be specified! use in mlog( first arg )
 */
#define MLOG_ALL	( MLOG_SS_GEN << MLOG_SS_SHIFT )
#define MLOG_PROC	( MLOG_SS_PROC << MLOG_SS_SHIFT )
#define MLOG_DRIVE	( MLOG_SS_DRIVE << MLOG_SS_SHIFT )
#define MLOG_MEDIA	( MLOG_SS_MEDIA << MLOG_SS_SHIFT )
#define MLOG_INV	( MLOG_SS_INV << MLOG_SS_SHIFT )
#ifdef DUMP
#define MLOG_INOMAP	( MLOG_SS_INOMAP << MLOG_SS_SHIFT )
#endif /* DUMP */
#ifdef RESTORE
#define MLOG_TREE	( MLOG_SS_TREE << MLOG_SS_SHIFT )
#endif /* RESTORE */

/* mlog_level - set during initialization, exported to facilitate
 * message logging decisions. one per subsystem (see above)
 */
extern intgen_t mlog_level_ss[ MLOG_SS_CNT ];

/* made external so main.c sigint dialog can change
 */
extern intgen_t mlog_showlevel;
extern intgen_t mlog_showss;
extern intgen_t mlog_timestamp;

/* mlog_ss_name - so main.c sigint dialog can allow changes
 */
extern char *mlog_ss_names[ MLOG_SS_CNT ];

/* initializes the mlog abstraction. split into two phases to
 * unravel some initialization sequencing problems.
 */
extern bool_t mlog_init1( intgen_t argc, char *argv[ ] );
extern bool_t mlog_init2( void );

/* post-initialization, to tell mlog how many streams
 */
extern void mlog_tell_streamcnt( size_t streamcnt );

/* vprintf-based message format
 */
extern void mlog( intgen_t level, char *fmt, ... );
extern void mlog_va( intgen_t levelarg, char *fmt, va_list args );

/* the following calls are exported ONLY to dlog.c!
 */
extern void mlog_lock( void );
extern void mlog_unlock( void );

#endif /* MLOG_H */
