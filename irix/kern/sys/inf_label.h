
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	__INF_LABEL_
#define	__INF_LABEL_

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.1 $"

/*
 * Attribute names known to PlanG and especially eag_parseattr().
 */
#define EAG_INF		"inf"		/* INF label of a file */
#define EAG_INF_NOPLANG	"inf-noplang"	/* INF label of PlanGless filesystem */
#define EAG_INF_PROC	"inf-process"	/* INF label of this process */

#define INF_MIN_LEVEL		0	/* No level less than this 	*/
#define INF_MAX_LEVEL		255	/* No level greater than this 	*/
#define INF_MIN_CATEGORY	0	/* No category greater than this */
#define INF_MAX_CATEGORY	65534	/* No category greater than this */
#define INF_MAX_SETS		250

typedef struct	inf_label {
	unsigned char	il_level;	/* Hierarchical level  */
	unsigned short	il_catcount;	/* Category count */
	unsigned short	il_list[INF_MAX_SETS];
} inf_label;

/*
 * Plan G Database: Two files:
 *	INF_EAG_INDEX - index and size of an entry in ...
 *	INF_EAG_LABEL - the label itself.
 */
#define INF_EAG_DIR		"attribute"
#define INF_EAG_INDEX_FILE	"inf_index"
#define INF_EAG_LABEL_FILE	"inf_label"
#define INF_EAG_INDEX		"attribute/inf_index"
#define INF_EAG_LABEL		"attribute/inf_label"

/*
 * If INF_EAG_DIRECT the label is stored in the index
 * entry rather than the label file.
 */
#define INF_EAG_DIRECT	0x00000001	/* Direct entry */
/*
 * The maximum size of data to put into a direct index file entry.
 * Use as much as possible, but leave the flags alone.
 */
#define INF_EAG_DIRECT_MAX	(sizeof (int) + sizeof (int))

/*
 * Structure of an entry in the INF_EAG_INDEX file.
 */
struct inf_eag_index_s {
	unsigned int	iei_index;	/* Index of label in the label file */
	unsigned int	iei_size;	/* Size of label, in bytes */
	unsigned int	iei_flags;	/* Flags reguarding this entry */
};
typedef struct inf_eag_index_s inf_eag_index_t;

#ifdef _KERNEL
/*
 * Structure of the in-core index to label pointer mapping.
 */
struct inf_eag_incore {
	struct inf_eag_incore	*iei_next;	/* Next in this list */
	struct inf_label	*iei_label;	/* Pointer to label table */
	unsigned int		iei_index;	/* Index in the label file */
	int			iei_size;	/* size of label */
};
typedef struct inf_eag_incore inf_eag_incore_t;

/*
 * Data to add to the vfs entry. Includes the vnode pointers for
 * INF_EAG_INDEX and INF_EAG_LABEL and the in-core index-label list.
 */
struct inf_vfs {
	struct vnode	*iv_index;      /* inode->data map file */
	struct vnode	*iv_label;      /* label data file */
	inf_eag_incore_t *iv_list;       /* in-core mapping */
	struct inf_label *iv_noplang;   /* use if not plang */
};
typedef struct inf_vfs inf_vfs_t;

#endif /* _KERNEL */

/* function prototypes */

extern int inf_equ(inf_label *, inf_label *);
extern int inf_dom(inf_label *, inf_label *);
extern int inf_size(inf_label *);
extern inf_label *inf_dup(inf_label *);


#ifdef _KERNEL
struct inf_vfs;
struct vattr;
struct vfs;

extern inf_label *inf_add_label( inf_label * );
extern void inf_loadattr( struct vfs * );
extern void inf_unloadattr( struct vfs * );
extern void inf_confignote(void);
extern void inf_init_cred(void);
extern void inf_eag_init(struct vfs *, char *);
extern void inf_eag_free(struct vfs *);

/* Define macros choosing stub functions or real functions here */
extern int inf_enabled;

#define _INF_LOADATTR(s)	((inf_enabled)? inf_loadattr(s): (void)0)
#define _INF_UNLOADATTR(s)	((inf_enabled)? inf_unloadattr(s): (void)0)
#define _INF_CONFIGNOTE()	((inf_enabled)? inf_confignote(): (void)0)
#define _INF_INIT_CRED()	((inf_enabled)? inf_init_cred(): (void)0)
#define _INF_EAG_INIT(v,e)	((inf_enabled)? inf_eag_init(v,e): (void)0)
#define _INF_EAG_FREE(v)	((inf_enabled)? inf_eag_free(v): (void)0)

#else	/* _KERNEL */
extern inf_label *inf_strtolabel( const char * );
#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* __INF_LABEL_ */
