/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994 Silicon Graphics, Inc.  	          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef __XLV_ATTR_H__
#define __XLV_ATTR_H__

#ident  "$Revision: 1.1 $"

/*
 * Attributes which can be manipulated via the syssgi(2) system call.
 *
 * Commands:
 *
 *	SGI_XLV_ATTR_CURSOR
 *	SGI_XLV_ATTR_GET
 *	SGI_XLV_ATTR_SET
 */

/*
 * Attributes
 */
#define XLV_ATTR_TAB_SIZE	1	/* size of xlv_tab and xlv_tab_vol */
#define XLV_ATTR_VOL		2
#define XLV_ATTR_SUBVOL		3
#define XLV_ATTR_PLEX		4
#define XLV_ATTR_VE		5
#define XLV_ATTR_LOCKS		6	/* number of xlv locks allocated */
#define XLV_ATTR_SUPPORT	7	/* options supported: plexing */
#define XLV_ATTR_SV_MAXSIZE	8	/* subvolume maximum size */
#define XLV_ATTR_ROOTDEV	9	/* root device */
#define XLV_ATTR_PATH		10	/* cursor path */
#define XLV_ATTR_GENERATION	11	/* configuration generation */
#define XLV_ATTR_MEMORY		12	/* set memory pool parameters */
#define XLV_ATTR_STATS		13	/* statistics */
#define XLV_ATTR_FLAGS		14	/* flags */

/*
 * Command type
 */
#define XLV_ATTR_CMD_QUERY	1
#define XLV_ATTR_CMD_ADD	2
#define XLV_ATTR_CMD_MERGE	2
	/*
	 * An add operation is really a merge. The agrument to
	 * the call is the parent object which provides context
	 * information.
	 */
#define XLV_ATTR_CMD_REMOVE	3
#define XLV_ATTR_CMD_SET	4
#define XLV_ATTR_CMD_NOTIFY	5	/* config change notification */
#define XLV_ATTR_CMD_PLEXMEM	6	/* resize plex i/o pool */
#define XLV_ATTR_CMD_VEMEM	7	/* resize volume element i/o pool */


/*
 * Attribute Flags -- flag values
 */
#define XLV_FLAG_STAT	0x01		/* Collect statistics */

typedef struct xlv_attr_cursor {
	int	generation;
	/* The following provides context information */
	int	vol;		/* volume table index */
	int	subvol;		/* minor device number */
	int	plex;		/* which plex in subvolume */
	int	ve;		/* which volume element in plex */
} xlv_attr_cursor_t;

struct xlv_stat_s;

typedef struct xlv_attr_memory {
	int	size, slots;	/* number of buffers per slot and num slots */
	int	hits, misses;	/* stats on pool rates */
	int	waits, resized;	/* buffer queued; times pool has grown */
	int	scale; 		/* rate to grow memory */
	int	threshold;	/* miss rate when to grow memory */
} xlv_attr_memory_t;

typedef struct xlv_attr_req {
	int	attr;					/* attribute */
	int	cmd;					/* command type */
	union {
		void		    *aru_buf;		/* buffer address */
		struct {
			int tab;
			int tab_vol;
		} aru_size;
		xlv_tab_vol_entry_t *aru_vp;		/* XLV_ATTR_VOL */
		xlv_tab_subvol_t    *aru_svp;		/* XLV_ATTR_SUBVOL */
		xlv_tab_plex_t	    *aru_pp;		/* XLV_ATTR_PLEX */
		xlv_tab_vol_elmnt_t *aru_vep;		/* XLV_ATTR_VE */
		xlv_attr_cursor_t   *aru_path;		/* XLV_ATTR_PATH */
		xlv_attr_memory_t   *aru_mem;		/* XLV_ATTR_MEMORY */
		struct {
			int maxlocks;		/* allocated locks */
			int numlocks;		/* locks used (max subvol) */
		} aru_locks;
		struct {
			int plexing;		/* present of plexing support */
			int pad;		/* padding */
		} aru_support;
		__uint64_t	    aru_sv_maxsize;	/* max subvol size*/
		struct {
			dev_t rootdev;		/* root disk device */
			int   pad;		/* padding */
		} aru_rootdev;
		struct {
			int   generation;	/* config generation */
			int   pad;		/* padding */
		} aru_generation;
		struct xlv_stat_s *aru_statp;	/* statistics */
		struct {
			int flag1;		/* flags */
			int flag2;		/* more flags */
		} aru_flags;
	} ar_aru;

#define	ar_tab_size	ar_aru.aru_size.tab
#define	ar_tab_vol_size	ar_aru.aru_size.tab_vol
#define	ar_vp		ar_aru.aru_vp			/* volume */
#define	ar_svp		ar_aru.aru_svp			/* subvolume */
#define	ar_pp		ar_aru.aru_pp			/* plex */
#define	ar_vep		ar_aru.aru_vep			/* volume element */
#define	ar_path		ar_aru.aru_path			/* component path */
#define	ar_mem		ar_aru.aru_mem			/* memory statistics */
#define	ar_max_locks	ar_aru.aru_locks.maxlocks
#define	ar_num_locks	ar_aru.aru_locks.numlocks
#define	ar_supp_plexing	ar_aru.aru_support.plexing
#define	ar_sv_maxsize	ar_aru.aru_sv_maxsize
#define	ar_rootdev	ar_aru.aru_rootdev.rootdev
#define	ar_generation	ar_aru.aru_generation.generation
#define	ar_statp	ar_aru.aru_statp
#define	ar_flag1	ar_aru.aru_flags.flag1
#define	ar_flag2	ar_aru.aru_flags.flag2
} xlv_attr_req_t;


#ifdef _KERNEL
typedef struct irix5_xlv_attr_req {
	app32_int_t attr;	
	app32_int_t cmd;
	union {
		app32_ptr_t	aru_buf;		/* 32 bit pointer */
		struct {
			app32_int_t tab;
			app32_int_t tab_vol;
		} aru_size;
		app32_ptr_t	aru_vp;			/* 32 bit pointer */
		app32_ptr_t	aru_svp;		/* 32 bit pointer */
		app32_ptr_t	aru_pp;			/* 32 bit pointer */
		app32_ptr_t	aru_vep;		/* 32 bit pointer */
		struct {
			app32_int_t flag1;
			app32_int_t flag2;
		} aru_flags;
	} ar_aru;
} irix5_xlv_attr_req_t;
#endif


#ifdef _KERNEL
/*
 * Get a XLV attribute cursor (ticket) for attribute queries.
 */
extern int xlv_attr_get_cursor(xlv_attr_cursor_t *cursor);

/*
 * Return ESTALE if the cursor has expired due to configuration changes
 * by another process.
 */
extern int xlv_attr_get(xlv_attr_cursor_t *cursor, xlv_attr_req_t *req);
extern int xlv_attr_set(xlv_attr_cursor_t *cursor, xlv_attr_req_t *req);
#endif

#endif	/* __XLV_ATTR_H__ */
