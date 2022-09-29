/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __GETPAGES_H__
#define __GETPAGES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ksys/as.h>
#include <sys/sema.h>

#ident	"$Revision: 3.30 $"

/*	The following list is used to keep track of regions locked
 *	by getpages.
 */
typedef struct {
	struct pregion *gp_prp; /* Pointer to the pregion.	*/
	pgno_t gp_count;	/* Number of pages from this	*/
				/* region in lists		*/
} gprgl_t;

/*	The following structure is used to maintain a list of
 *	pages to be stolen by getpages.
 */
typedef struct pglst {
	struct pas_s 	*gp_pas;	/* Address space of the page   */
	union pde	*gp_ptptr;	/* Ptr to page table entry.	*/
	gprgl_t		*gp_rlptr;	/* Ptr to region list entry.	*/
	pgno_t		 gp_apn;	/* anon logical page number	*/
	struct pfdat	*gp_pfd;	/* pfdat			*/
	sm_swaphandle_t	 gp_sh;		/* swap handle			*/
	char		 gp_status;	/* status of swap xfer		*/
	char		 gp_flags;	/* flags of swap xfer		*/
} pglst_t;

/* gp_flags definition. */
#define	SANON_PAGE	1	/* Marks entry as an SANON page */

#define MAXPGLST	1000	/* max size for page lists */
/*
 * set of lists vhand manipulates
 */
#define	SPGLST0	0		/* pages to be written to swap */
#define	SPGLST1	1		/* pages to be written to swap */
#define FPGLST	2		/* pages to be freed w/ no I/O */
#define DPGLST	3		/* pages to be written back to files */
#define NPGLSTS	4

struct region;
struct pglsts {
	int	index;
	int	limit;
	int	(*cleaner)(struct region *);
	int	ntran;		/* # async transactions */
	int	ndone;		/* # async transactions completed */
	struct region	*lockedreg;
	pglst_t *list;
};

extern sema_t vhandsema;	/* wakeup semaphore for vhand */
extern int rsskicklim;
extern int rsswaitcnt;
extern lock_t vhandwaitlock;
extern sv_t rsswaitsema;

struct pregion;
struct buf;
struct pas_s;
struct ppas_s;
extern int getmpages(struct pas_s *, struct pregion *, int, caddr_t, pgno_t);
extern void swapdone(struct buf *);

/*	Flags for getpages et al	*/

#define GTPGS_HARD	0x01	/* toss all pages in region */
#define GTPGS_TOSS	0x02	/* flush and free pages */
#define GTPGS_NOLOCKS	0x04	/* error out on locked pages */
#define GTPGS_NOWRITE	0x40	/* just steal, don't write page */
#define GTPGS_NOMFILE	0x80	/* do NOT do DBD_MFILE */

#define GTPGS_MOSTRECENT 7	/* most recently accessed page */

/*
 * This structure is used to build the binary tree which sorts the address
 * spaces in the system in a fixed order for the paging daemon to scan.
 */
typedef struct prilist_s {
	struct vas_s		*vas;
	as_shake_t		sargs;
	struct prilist_s	*parent;
	struct prilist_s	*left;
	struct prilist_s	*right;
} prilist_t;
/* values for state these must be sorted most imporant == highest number */
#define	GTPGS_DEFUNCT		1	/* No address space for this process */
#define GTPGS_WEIGHTLESS	2	/* batch, etc */
#define GTPGS_NORM		3	/* normal TS process */
#define GTPGS_PRI_NORM		4	/* priority scheduled - normal */
#define GTPGS_PRI_HI		5	/* priority scheduled - hi */
#define GTPGS_ISO		10	/* isolated pseudo-state */

#ifdef _KERNEL
extern pglst_t *pglst;
extern gprgl_t *gprglst;
extern int maxpglst;

#endif

#ifdef __cplusplus
}
#endif

#endif /* __GETPAGES_H__ */
