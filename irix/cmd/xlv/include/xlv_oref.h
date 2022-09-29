#ifndef __XLV_OREF_H__
#define __XLV_OREF_H__

/**************************************************************************
 *                                                                        *
 *           Copyright (C) 1993, 1994 Silicon Graphics, Inc.              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.14 $"


/*
 *	Macros and routines to manipulate XLV object references.
 *	Objects can be, for example, entries in the xlv_tab.
 *
 *	A fully resolved object reference contains a pointer to
 * 	the object itself plus all the ancestor nodes.  A fully
 *	resolved disk part object, for example, contains pointers
 *	to the volume element, plex, subvol, and volume to which
 * 	it belongs.
 */


/*	
 *	Generic handle to an XLV object.
 *	Note that if the oref is not fully resolved, the obj_uuid
 *	field must be set.
 *	The obj_type field is set to indicate the level to which
 *	the object has been resolved.
 *
 *	From an object reference, it is trivial to find all the
 *	ancestors of a given object.  e.g., which volume owns 
 *	this plex, etc.
 */

typedef struct oref_s {
	struct oref_s		*next;
	xlv_obj_type_t		obj_type;
	xlv_tab_vol_entry_t	*vol;
	xlv_tab_subvol_t	*subvol;
	xlv_tab_plex_t		*plex;
	xlv_tab_vol_elmnt_t	*vol_elmnt;
	xlv_tab_disk_part_t	*disk_part;
	uuid_t			uuid;
	int			flag;
	char			nodename[XLV_NODENAME_LEN];
} xlv_oref_t;

/* 
 *	Search xlv_tab_vol and locate the object with the specified
 *	uuid.
 */

extern int xlv_oref_resolve (
	xlv_oref_t *oref, xlv_tab_vol_entry_t *tab_vol_entry);

extern int xlv_oref_resolve_from_list (
	xlv_oref_t *oref, xlv_oref_t *oref_list);


/*
 *	Given an object, return its string name in relative format.
 *	e.g. volume_foo.data.2 if it's the second plex of 
 *	volume_foo's data subvolume.
 */

extern void xlv_oref_to_string (xlv_oref_t *oref, char *str);

/*
 *	Prints out the object referenced by an oref.
 */

extern void xlv_oref_print (xlv_oref_t *oref, int p_mode);

/*
 *	An iterator that applies a user-supplied function and arg
 *	to every volume element that makes up an object.
 *	The user-supplied function returns 0 to continue with the
 *	iteration. Return 1 to stop.
 */
typedef int (*XLV_OREF_PF) (xlv_oref_t *, void *);

extern void xlv_for_each_ve_in_obj (
        xlv_oref_t	*oref,
        XLV_OREF_PF	operation,
        void		*arg);

extern void xlv_for_each_ve_in_vol (
        xlv_oref_t	*oref,
        XLV_OREF_PF	operation,
        void		*arg);

extern int xlv_for_each_ve_in_subvol (
        xlv_oref_t	*oref,
        XLV_OREF_PF	operation,
        void		*arg);

extern int xlv_for_each_ve_in_plex (
        xlv_oref_t	*oref,
        XLV_OREF_PF	operation,
        void		*arg);

/*
 *	Returns 1 if the oref is a null reference; i.e., it has just
 *	been XLV_OREF_INIT'ed and no new fields have been set.
 */
extern int xlv_oref_is_null (xlv_oref_t *oref);

/*
 * Similiar to xlv_check_tab_vol for standalone pieces
 */
extern int xlv_check_oref_pieces(xlv_oref_t **oref_pieces, int filter);

/*
 * Check if the oref is missing pieces.
 * Return zero if all defined pieces are present.
 */
extern int xlv_is_incomplete_oref(xlv_oref_t *oref);


/*
 *	See if the object has been resolved to the given level. 
 */

#define XLV_OREF_RESOLVED(oref, to_this_type) \
	((oref)->obj_type == (to_this_type))

#define XLV_OREF_INIT(oref) (bzero((oref), sizeof(xlv_oref_t)))

#define XLV_OREF_COPY(src_oref, dst_oref) \
	(bcopy((src_oref), (dst_oref), sizeof(xlv_oref_t)))

/*
 *	NOTE that these routines assume that they have been called
 *	top-down.  e.g., if you call XLV_OREF_SET_PLEX, you must
 *	have previously called _SET_SUBVOL and _SET_VOL.
 *
 *	XXX Is this true any more XXX    Wei 1/14/93
 */

#define XLV_OREF_SET_UUID(oref, uuid_p) \
	(bcopy(uuid_p, &((oref)->uuid), sizeof(uuid_t)))
	
#define XLV_OREF_SET_VOL(oref, I_vol) \
	{ (oref)->vol = (I_vol); (oref)->obj_type = XLV_OBJ_TYPE_VOL; }

#define XLV_OREF_SET_SUBVOL(oref, I_subvol) \
	{ (oref)->subvol = (I_subvol); (oref)->obj_type = XLV_OBJ_TYPE_SUBVOL; }

#define XLV_OREF_SET_PLEX(oref, I_plex) \
	{ (oref)->plex = (I_plex); (oref)->obj_type = XLV_OBJ_TYPE_PLEX; }

#define XLV_OREF_SET_VOL_ELMNT(oref, ve) \
	{ (oref)->vol_elmnt = (ve); (oref)->obj_type = XLV_OBJ_TYPE_VOL_ELMNT; }

#define XLV_OREF_SET_DISK_PART(oref, dp) \
	{ (oref)->disk_part = (dp); (oref)->obj_type = XLV_OBJ_TYPE_DISK_PART; }
	
#define XLV_OREF_TYPE(oref) ((oref)->obj_type)
#define XLV_OREF_VOL(oref) ((oref)->vol)
#define XLV_OREF_SUBVOL(oref) ((oref)->subvol)
#define XLV_OREF_PLEX(oref) ((oref)->plex)
#define XLV_OREF_VOL_ELMNT(oref) ((oref)->vol_elmnt)
#define XLV_OREF_DISK_PART(oref) ((oref)->disk_part)


#endif /*  __XLV_OREF_H__ */
