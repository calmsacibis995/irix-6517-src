/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __SYS_SWAP_H__
#define __SYS_SWAP_H__
#ident	"$Revision: 3.25 $"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL
#include <sys/ktypes.h>
#endif


/* The following are for the swapctl system call */

#define SC_ADD          1       /* add a specified resource for swapping */
#define SC_LIST         2       /* list all the swapping resources */
#define SC_REMOVE       3       /* remove the specified swapping resource */
#define SC_GETNSWP      4       /* get number of swapping resources configued */
#define SC_SGIADD	100	/* extentions to ADD */
#define SC_KSGIADD	101	/* kernel add */
#define SC_LREMOVE	102	/* delete based on logical swap # */
#define SC_GETFREESWAP	103	/* get # free blocks on all swap devices */
#define SC_GETSWAPMAX	104	/* get max # blocks on all swap devices */
#define SC_GETSWAPVIRT	105	/* get # virtual swap blocks */
#define SC_GETRESVSWAP	106	/* get # reserved logical swap blocks */
#define SC_GETSWAPTOT	107	/* get # physical swap blocks */
#define SC_GETLSWAPTOT	108	/* get # logical swap blocks */

typedef struct swapres {
        char    *sr_name;       /* pathname of the resource specified */
        off_t   sr_start;       /* starting offset of the swapping resource */
        off_t   sr_length;      /* length of the swap area (in blocks) */
} swapres_t;

#ifdef _KERNEL
typedef struct irix5_swapres {
	app32_ptr_t sr_name;	/* pathname of the resource specified */
	irix5_off_t sr_start;	/* starting offset of the swapping resource */
	irix5_off_t sr_length;	/* length of the swap area (in blocks) */
} irix5_swapres_t;

typedef struct irix5_n32_swapres {
	app32_ptr_t	sr_name;
	irix5_n32_off_t	sr_start;
	irix5_n32_off_t	sr_length;
} irix5_n32_swapres_t;
#endif	/* _KERNEL */

typedef struct xswapres {
        char    *sr_name;       /* pathname of the resource specified */
        off_t   sr_start;       /* starting offset of the swapping resource */
        off_t   sr_length;      /* length of the swap area (in blocks) */
	off_t	sr_maxlength;	/* grow to this maximum (blocks) */
	off_t	sr_vlength;	/* virtual length (blocks) */
	signed char sr_pri;	/* priority */
} xswapres_t;

#ifdef _KERNEL
typedef struct irix5_xswapres {
	app32_ptr_t sr_name;	/* pathname of the resource specified */
	irix5_off_t sr_start;	/* starting offset of the swapping resource */
	irix5_off_t sr_length;	/* length of the swap area (in blocks) */
	irix5_off_t sr_maxlength; /* grow to this maximum (blocks) */
	irix5_off_t sr_vlength;	/* virtual length (blocks) */
	signed char sr_pri;	/* priority */
} irix5_xswapres_t;

typedef struct irix5_n32_xswapres {
	app32_ptr_t	sr_name;
	irix5_n32_off_t	sr_start;
	irix5_n32_off_t	sr_length;
	irix5_n32_off_t	sr_maxlength;
	irix5_n32_off_t	sr_vlength;
	signed char	sr_pri;
} irix5_n32_xswapres_t;
#endif	/* _KERNEL */

typedef struct swapent {
        char    *ste_path;      /* get the name of the swap file */
        off_t   ste_start;      /* starting block for swapping */
        off_t   ste_length;     /* length of swap area (in blocks) */
        long    ste_pages;      /* numbers of pages for swapping */
        long    ste_free;       /* numbers of ste_pages free */
        long    ste_flags;      /* see below */
	long	ste_vpages;	/* virtual pages for swapping */
	long	ste_maxpages;	/* max pages swap device can grow */
	short	ste_lswap;	/* logical swap # */
	signed char ste_pri;	/* priority */
} swapent_t;

#ifdef _KERNEL
typedef struct irix5_swapent {
	app32_ptr_t	ste_path;	/* get the name of the swap file */
	irix5_off_t	ste_start;	/* starting block for swapping */
	irix5_off_t	ste_length;	/* length of swap area (in blocks) */
	app32_long_t	ste_pages;	/* numbers of pages for swapping */
	app32_long_t	ste_free;	/* numbers of ste_pages free */
	app32_long_t	ste_flags;	/* see below */
	app32_long_t	ste_vpages;	/* virtual pages for swapping */
	app32_long_t	ste_maxpages;	/* max pages swap device can grow */
	short		ste_lswap;	/* logical swap # */
	signed char	ste_pri;	/* priority */
} irix5_swapent_t;

typedef struct irix5_n32_swapent {
	app32_ptr_t	ste_path;	/* get the name of the swap file */
	irix5_n32_off_t	ste_start;	/* starting block for swapping */
	irix5_n32_off_t	ste_length;	/* length of swap area (in blocks) */
	app32_long_t	ste_pages;	/* numbers of pages for swapping */
	app32_long_t	ste_free;	/* numbers of ste_pages free */
	app32_long_t	ste_flags;	/* see below */
	app32_long_t	ste_vpages;	/* virtual pages for swapping */
	app32_long_t	ste_maxpages;	/* max pages swap device can grow */
	short		ste_lswap;	/* logical swap # */
	signed char	ste_pri;	/* priority */
} irix5_n32_swapent_t;
#endif

/* ste_flags values */

#define ST_INDEL        0x01            /* this file is in the process */
                                        /* of being deleted. Don't     */
                                        /* allocate from it. This can  */
                                        /* be turned of by swapadd'ing */
                                        /* this device again.          */
#define	ST_NOTREADY	0x02		/* This file is in the process 	*/
					/* of being allocated.  Don't	*/
					/* allocate from it.		*/
#define ST_STALE	0x04		/* file was NFS and was removed
					 * on server
					 */
#define ST_LOCAL_SWAP	0x08		/* swap resource is 'local' */
#define ST_IOERR	0x10		/* resource had an I/O err other
					 * than ESTALE, EACCES
					 */
#define ST_EACCES	0x20		/* got a EACCES when trying to 
					 * read/write to swap resource
					 */
#define ST_BOOTSWAP	0x40		/* this is the boot swap file */
/*
 * if any of the following bits is on, we no longer attempt to
 * allocate from the swap resource
 */
#define ST_OFFLINE	(ST_INDEL|ST_STALE|ST_EACCES|ST_NOTREADY)

typedef struct  swaptable {
        int     swt_n;                  /* number of swapents following */
        struct  swapent swt_ent[1];     /* array of swt_n swapents */
} swaptbl_t;


#ifdef _KERNEL

typedef struct  irix5_swaptable {
	int	swt_n;                  /* number of swapents following */
	struct	irix5_swapent swt_ent[1]; /* array of swt_n swapents */
} irix5_swaptbl_t;

typedef struct  irix5_n32_swaptable {
	int	swt_n;
	struct	irix5_n32_swapent swt_ent[1];
} irix5_n32_swaptbl_t;

#define MAXLSWAP	255	/* max swap files/devices */

/*
 * The following structure contains the data describing a swap file.
 */
typedef struct swapinfo {
	struct vnode *st_vp;	/* The swap device.		*/
	char	*st_bmap;	/* allocation bit map */
	long	*st_cksum;	/* Ptr to debug checksum array.	*/
	char 	*st_name;	/* name of swap file */
	struct swapinfo *st_list;/* next entry in priority list */
	off_t	st_start;	/* First block number on device	*/
				/* to be used for swapping.	*/
	off_t	st_length;	/* length in blocks */
	bitnum_t st_next;	/* next bitnum to start search at */
	pgno_t	st_swppglo;	/* st_start rounded up to page	*/
	pgno_t	st_npgs;	/* Number of pages allocated on file */
	pgno_t	st_nfpgs;	/* Nbr of free pages on file.	*/
	pgno_t	st_maxpgs;	/* max pgs swap file can grow to */
	pgno_t	st_vpgs;	/* virtual pages (availsmem) */
	ushort_t st_gen;	/* bit map generation #		*/
	ushort_t st_allocs;	/* # of sm_allocs() doing bit map search */
	short	st_flags;	/* Flags defined below.		*/
	char	st_pri;		/* priority of swap file */
	char	st_lswap;	/* logical swap device # */
} swapinfo_t;

/* st_flags values same as ste_flags */

/*
 * swaptab - list of swap entries - link listed by priority
 */
#define NSWAPPRI	8
typedef struct swaptab {
	swapinfo_t	*sw_list;	/* list of swapfile structs */
	swapinfo_t	*sw_next;	/* which to start alloc from */
} swaptab_t;
extern swapinfo_t *lswaptab[];	/* direct access via logical swap */

struct pglst;

/*
 * Swap Manager
 * sm_swaphandle_t defined in types.h
 */

/*
 * alloc contigous swap pages
 */
extern sm_swaphandle_t sm_alloc(int *, int);

/*
 * free up swap pages
 */
extern void sm_free(sm_swaphandle_t *, int);

/*
 * dealloc a block of swap pages
 */
extern void sm_dealloc(sm_swaphandle_t, int);

/*
 * swap in/out a collection of pages
 */
struct buf;
extern int sm_swap(struct pglst *, sm_swaphandle_t, int, int, void (*)(struct buf *));

/*
 * interpret logical next swap page given a swap handle
 * sm_swaphandle_t sm_next(sm_swaphandle_t);
 */
#define sm_next(x)	((sm_swaphandle_t)((__uint32_t)(x)+1))

/*
 * return logical swap # given a swap handle
 */
extern short sm_getlswap(sm_swaphandle_t);

/*
 * Add and delete swap.
 */
extern int swapadd(struct vnode *, xswapres_t *, char *, int *);
extern int swapdel(struct vnode *, off_t);

/*
 * is swap file deleted?
 */
extern int isswapdeleted(sm_swaphandle_t);

/*
 * Return number of free disk blocks on swap
 */
extern int nfreeswap(ulong *);

/*
 * Return max pages to grow to, virtual pages, and current size of swap
 */
extern void getmaxswap(pgno_t *, pgno_t *, pgno_t *);

/*
 * Given a dev and blkno construct a swap handle
 */
extern sm_swaphandle_t sm_makeswaphandle(dev_t, daddr_t);

#else

/* swapctl system call */
extern int swapctl(int, ...);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_SWAP_H__ */
