/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_SYSGET_H__
#define __SYS_SYSGET_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Id: sysget.h,v 1.15 1997/10/14 06:45:19 scottl Exp $"

#include <sgidefs.h>

/*
 * sysget.h - defined/structs for sysget() - cell specific calls
 *
 * Format:
 *	sysget(name, buf, buflen, flags, cookie)
 */

/* system activities sub-opcodes. NOTE: these are the same as MPSA_xxxx opcodes
 * except for new ones that have been appended to the end of the list.
 */
#define SGT_SINFO	1	/* get added-up sysinfo */
#define SGT_MINFO	2	/* get minfo */
#define SGT_DINFO	3	/* get dinfo */
#define SGT_SERR	4	/* get syserr */
#define SGT_NCSTATS	5	/* get ncstats (pathname cache) */
#define SGT_EFS		6	/* get efs stats */
#define SGT_RMINFO	8	/* real memory info */
#define SGT_BUFINFO	9	/* buffer cache info */
#define SGT_RUNQ	10	/* runq root data structure */
#define SGT_DISPQ	11	/* a particular dispatching queue */
#define SGT_VOPINFO	13	/* vnode operations */
#define SGT_TCPIPSTATS  14	/* TCP/IP stats for one or more processors */
#define SGT_RCSTAT 	15	/* nfs rcstat for a processor */
#define SGT_CLSTAT 	16	/* nfs clstat for a processor */
#define SGT_RSSTAT 	17	/* nfs rsstat for a processor */
#define SGT_SVSTAT 	18	/* nfs svstat for a processor */
#define	SGT_XFSSTATS	20	/* get xfs stats */
#define	SGT_CLSTAT3	21	/* nfs3 clstat for a processor */
#ifdef TILES_TO_LPAGES
#define SGT_TILEINFO	22	/* kernel tile info */
#endif /* TILES_TO_LPAGES */
#define SGT_CFSSTAT	23	/* CacheFS stats */
#define SGT_SVSTAT3	24
#define SGT_NODE_INFO 	25	/* Per node info */
#define SGT_LPGSTATS	26	/* Large page statistics */
#define SGT_SHMSTAT	27	/* Shared memory stats */
#define SGT_KSYM 	28	/* Arbitrary kernel object */ 
#define SGT_MSGQUEUE	29	/* msgque list */
#define SGT_SOCKSTATS   31	/* socket stats for one or more processors */
#define SGT_SINFO_CPU   32	/* cpu portion of sysinfo */
#define SGT_STREAMSTATS 33	/* Stream stats for one or more processors */
#define	SGT_MAX		33	/* Max entries, >>End-of-Table<< */

/* flags */
#define SGT_INFO 	0x1	/* Return sgt_info struct (below)	*/
#define SGT_READ 	0x2	/* Return data to user buf		*/
#define SGT_WRITE	0x4	/* Write data from user buf to kernel	*/
#define SGT_SUM		0x8	/* Add data sets together 		*/
#define SGT_CPUS	0x10	/* Return data for each cpu		*/
#define SGT_NODES 	0x20	/* Return data for each node		*/
#define SGT_STAT	0x40	/* Return sgt_stat struct		*/
#ifdef _KERNEL
#define SGT_UADDR	0x1000  /* Buffer address is a user address	*/
#endif

/* structure returned when SGT_INFO flag specified */
struct sgt_info {
	int	si_size;	/* record (struct) size			*/
	int	si_num;		/* max number of records (>1 if array)	*/
	int	si_hiwater;	/* hi-water mark (current != numrec)	*/
};
typedef struct sgt_info sgt_info_t;

/* structure returned when SGT_STAT flag specified (One per cell) */
struct sgt_stat {
	sgt_info_t	info;
	cell_t	   	cellid;
	int		unused[8];
};
typedef struct sgt_stat sgt_stat_t;

/* The cookie allows the kernel to save info somewhat like a bookmark
 * so that the user can loop on the sysget call until all data has been
 * returned.  This eliminates the need for the user to know how many   
 * data items to expect ahead of time and allows a smaller buffer to be
 * used.   The cookie can also be used to seek to a specific object
 * among a group so that the caller doesn't have to loop through all
 * of them.
 */
typedef enum sc_status {
        SC_BEGIN,               /* first call - set by user */
        SC_CONTINUE,            /* keep calling */
        SC_DONE,                /* all done - set by kernel */
	SC_SEEK			/* seek to specified object */
} sc_status_t;

typedef enum sc_type {
	SC_CELLID,		/* ID is for a cell (cell_t) */
	SC_CNODEID,		/* ID is for a node (cnodeid_t) */
	SC_CPUID		/* ID is for a cpu (int) */
} sc_type_t;

#define SGT_OPAQUE_SZ	64

typedef struct sgt_cookie {
        sc_status_t    sc_status;	
	union {
		cell_t 		cellid;
		cnodeid_t	cnodeid;
		int		cpuid;
	} sc_id;
	sc_type_t 	sc_type;
	char		sc_opaque[SGT_OPAQUE_SZ];
} sgt_cookie_t;

typedef struct sgt_cookie_genop {
	cell_t	curcell;
	int	index;
	int	arg1;
	int	arg2;
	int	arg3;
	int	arg4;
} sgt_cookie_genop_t;

#define SGT_COOKIE_GENOP(_cp) ((sgt_cookie_genop_t *)_cp->sc_opaque)

/* sc_id cell special args */
#define SC_CELL_ALL	 -1	/* Return data from all cells		*/
#define SC_MYCELL	 -2  	/* Return data from local cell 		*/


/* Define default cookie init function - return all cells */
#define SGT_COOKIE_INIT(_cp)	{(_cp)->sc_status = SC_BEGIN;	\
                                 (_cp)->sc_id.cellid = SC_CELL_ALL;	\
                                 (_cp)->sc_type = SC_CELLID;}

/* Define cookie function for specific cell */
#define SGT_COOKIE_SET_CELL(_cp, _cellid)  {(_cp)->sc_id.cellid = _cellid; \
					    (_cp)->sc_status = SC_BEGIN; \
					    (_cp)->sc_type = SC_CELLID;}

/* Define cookie function for specific node */
#define SGT_COOKIE_SET_NODE(_cp, _nodeid)  {(_cp)->sc_id.cnodeid = _nodeid; \
					    (_cp)->sc_status = SC_BEGIN; \
					    (_cp)->sc_type = SC_CNODEID;}

/* Define cookie function for a physical cpu */
#define SGT_COOKIE_SET_CPU(_cp, _cpuid)  {(_cp)->sc_id.cpuid = _cpuid; \
					    (_cp)->sc_status = SC_BEGIN; \
					    (_cp)->sc_type = SC_CPUID;}

/* Define cookie function for a logical cpu of a cell */
#define SGT_COOKIE_SET_LCPU(_cp, _lcpuid) \
{ 	sgt_cookie_genop_t *_cgtp; 		\
	(_cp)->sc_status = SC_SEEK; 		\
	_cgtp = SGT_COOKIE_GENOP((_cp)); 	\
	_cgtp->index = (int)_lcpuid; \
}


/* Define cookie function for specific kernel symbol.  The kernel symbol
 * name is stored in the opaque section of the cookie after the area reserved
 * for continuation info.
 */
#define KSYM_LENGTH	(sizeof(((sgt_cookie_t *)0)->sc_opaque) - \
			 sizeof(cell_t))

#define SGT_COOKIE_SET_KSYM(_cp, _ksym) \
	strncpy(&(_cp)->sc_opaque[sizeof(cell_t)], _ksym, KSYM_LENGTH);
					  
/* SGT_KSYM symnames */
#define KSYM_VAR		"v"   		/* kernel vars */
#define KSYM_SWPLO		"swplo" 	/* swap lo */
#define	KSYM_SEMAMETER		"defaultsemameter" /* spin lock metering flag */
#define KSYM_TIME		"time"  	/* time */
#define	KSYM_SPLOCKMETER	"splockmeter" 	/* spin lock metering flag */
#define KSYM_AVENRUN		"avenrun" 	/* load average */
#define KSYM_PHYSMEM		"physmem" 	/* &physmem */
#define KSYM_KPBASE		"kpbase" 	/* &kpbase */
#define KSYM_PFDAT		"pfdat" 	/* &pfdat */
#define KSYM_FREEMEM		"freemem" 	/* &freemem */
#define KSYM_USERMEM		"usermem" 	/* &usermem */
#define KSYM_PDWRIMEM		"dchunkpages" 	/* &dchunkpages */
#define KSYM_BUFMEM		"bufmem" 	/* &bufmem */
#define KSYM_RUNQ		"Runq" 		/* Runq */
#define KSYM_PSET		"Bv" 		/* Bv */
#define KSYM_CHUNKMEM		"chunkpages" 	/* &chunkpages */
#define KSYM_MAXCLICK		"maxclick" 	/* &maxclick */
#define KSYM_PSTART		"physmem_start" /* &start-of-physical-memory */
#define KSYM_TEXT		"text" 		/* text[] */
#define KSYM_ETEXT		"etext" 	/* etext[] */
#define KSYM_EDATA		"edata" 	/* edata[] */
#define KSYM_END		"end" 		/* end[] */
#define KSYM_SYSSEGSZ		"syssegsz" 	/* &syssegsz */
#define KSYM_TUNENAME		"tunename" 	/* tunename[] */
#define KSYM_TUNETABLE		"tunetable" 	/* tunetable[] */
#define KSYM_VN_VNUMBER		"vn_vnumber" 	/* vn_vnumber */
#define KSYM_VN_EPOCH  		"vn_epoch" 	/* vn_epoch */
#define KSYM_VN_NFREE  		"vn_nfree" 	/* vn_nfree */
#define KSYM_MSGINFO		"msginfo"	/* msginfo */
#define KSYM_GFXINFO		"gfxinfo"	/* gfxinfo */
#define KSYM_MIN_FILE_PAGES	"min_file_pages"/* guess */
#define KSYM_MIN_FREE_PAGES	"min_free_pages"/* guess */

#ifdef _KERNEL
struct sgt_attr {
        int     size;           /* struct size */
        int     flags;          /* see below */
        int     (*func)(int, char *, int *, int, sgt_cookie_t *, sgt_info_t *);
                                /* call func to collect stats */
};
typedef struct sgt_attr sgt_attr_t;

#define SA_PERCPU	0x1     /* one object exists per cpu */
#define SA_SUMMABLE	0x2     /* object is summable across cells/nodes */
#define SA_RDWR		0x4     /* object can be written as well as read */
#define SA_LOCAL  	0x8	/* all objects can be read from local cell */
#define SA_PERNODE	0x10	/* one object exists per node */
#define SA_GLOBAL	0x20	/* UNIKERNEL only - global to all cells */

#define sysget_bcopy	bcopy	/* remote kernel copy. Needed for R2 */

extern sgt_attr_t sysget_attr[SGT_MAX];
extern int sysget_mpsa1(int, char *, int, int);
extern int sysget_mpsa(int, char *, int);
extern void ksym_add(char *, void *, int, int, int);
#else
#include <stddef.h>
extern int sysget(int, char *, int, int, sgt_cookie_t *);
#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_SYSGET_H__ */
