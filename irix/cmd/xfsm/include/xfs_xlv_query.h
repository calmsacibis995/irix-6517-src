#ifndef __XLV_QUERY_H__
#define __XLV_QUERY_H__

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
#ident "$Revision: 1.1 $"

/*
 *	Macros and routines to perform queries on XLV object references.
 */


/*
 *	Attribute Id(s) of the XLV object(s). The queries are based on
 *	these attributes.
 */

#define	XLV_QUERY_ATTR_RANGE_BEGIN	0

#define	XLV_QRY_VOLUME_ATTR_RANGE_BEGIN	(XLV_QUERY_ATTR_RANGE_BEGIN)
#define	XLV_VOLUME_NAME			(XLV_QRY_VOLUME_ATTR_RANGE_BEGIN + 1)
#define	XLV_VOLUME_FLAGS		(XLV_QRY_VOLUME_ATTR_RANGE_BEGIN + 2)
#define	XLV_VOLUME_STATE		(XLV_QRY_VOLUME_ATTR_RANGE_BEGIN + 3)
#define	XLV_QRY_VOLUME_ATTR_RANGE_END	(XLV_VOLUME_STATE)

#define	XLV_QRY_SUBVOL_ATTR_RANGE_BEGIN	(XLV_QRY_VOLUME_ATTR_RANGE_END + 1)
#define	XLV_SUBVOLUME_FLAGS		(XLV_QRY_SUBVOL_ATTR_RANGE_BEGIN + 0)
#define	XLV_SUBVOLUME_TYPE		(XLV_QRY_SUBVOL_ATTR_RANGE_BEGIN + 1)
#define	XLV_SUBVOLUME_SIZE		(XLV_QRY_SUBVOL_ATTR_RANGE_BEGIN + 2)
#define	XLV_SUBVOLUME_INL_RD_SLICE	(XLV_QRY_SUBVOL_ATTR_RANGE_BEGIN + 3)
#define	XLV_SUBVOLUME_NO_PLEX		(XLV_QRY_SUBVOL_ATTR_RANGE_BEGIN + 4)
#define	XLV_QRY_SUBVOL_ATTR_RANGE_END	(XLV_SUBVOLUME_NO_PLEX)

#define	XLV_QRY_PLEX_ATTR_RANGE_BEGIN	(XLV_QRY_SUBVOL_ATTR_RANGE_END + 1)
#define	XLV_PLEX_NAME			(XLV_QRY_PLEX_ATTR_RANGE_BEGIN + 0)
#define	XLV_PLEX_FLAGS			(XLV_QRY_PLEX_ATTR_RANGE_BEGIN + 1)
#define	XLV_PLEX_NO_VE			(XLV_QRY_PLEX_ATTR_RANGE_BEGIN + 2)
#define	XLV_PLEX_MAX_VE			(XLV_QRY_PLEX_ATTR_RANGE_BEGIN + 3)
#define	XLV_QRY_PLEX_ATTR_RANGE_END	(XLV_PLEX_MAX_VE)

#define	XLV_QRY_VE_ATTR_RANGE_BEGIN	(XLV_QRY_PLEX_ATTR_RANGE_END + 1)
#define	XLV_VE_NAME			(XLV_QRY_VE_ATTR_RANGE_BEGIN + 0)
#define	XLV_VE_TYPE			(XLV_QRY_VE_ATTR_RANGE_BEGIN + 1)
#define	XLV_VE_GRP_SIZE			(XLV_QRY_VE_ATTR_RANGE_BEGIN + 2)
#define	XLV_VE_STRIPE_UNIT_SIZE		(XLV_QRY_VE_ATTR_RANGE_BEGIN + 3)
#define	XLV_VE_STATE			(XLV_QRY_VE_ATTR_RANGE_BEGIN + 4)
#define	XLV_VE_START_BLK_NO		(XLV_QRY_VE_ATTR_RANGE_BEGIN + 5)
#define	XLV_VE_END_BLK_NO		(XLV_QRY_VE_ATTR_RANGE_BEGIN + 6)
#define	XLV_QRY_VE_ATTR_RANGE_END	(XLV_VE_END_BLK_NO)

#define	XLV_QUERY_ATTR_RANGE_END	(XLV_QRY_VE_ATTR_RANGE_END)


/*
 *	The xlv_query_set_t is the structure used to store the XLV
 *	object reference. It has a pointer to next XLV object reference
 *	for traversal.
 */


/* 
 *	Query all the XLV object references in the in_set for the attribute,
 *	value pair. The operator indicates the query condition. The references
 *	to the XLV object(s) which satisfy the query are stored in the
 *	out_set.
 */

extern int xlv_query (
	query_clause_t* query_clause,query_set_t *in_set,
	query_set_t **out_set);
 
/* 
 *	Query all the XLV volume objects in the in_set for the attribute,
 *	value pair. The operator indicates the query condition. The references
 *	to the XLV volume object(s) which satisfy the query are stored in the
 *	out_set.
 */

extern int xlv_query_volume (
	short attr_id, char *value, short oper,
	query_set_t *in_set, query_set_t **out_set);

/* 
 *	Query all the XLV subvolume objects in the in_set for the attribute,
 *	value pair. The operator indicates the query condition. The references
 *	to the XLV subvolume object(s) which satisfy the query are stored in the
 *	out_set.
 */

extern int xlv_query_subvolume (
	short attr_id, char *value, short oper,
	query_set_t *in_set, query_set_t **out_set);

/* 
 *	Query all the XLV plex objects in the in_set for the attribute,
 *	value pair. The operator indicates the query condition. The references
 *	to the XLV plex object(s) which satisfy the query are stored in the
 *	out_set.
 */

extern int xlv_query_plex (
	short attr_id, char *value, short oper,
	query_set_t *in_set, query_set_t **out_set);

/* 
 *	Query all the XLV ve objects in the in_set for the attribute,
 *	value pair. The operator indicates the query condition. The references
 *	to the XLV ve object(s) which satisfy the query are stored in the
 *	out_set.
 */

extern int xlv_query_ve (
	short attr_id, char *value, short oper,
	query_set_t *in_set, query_set_t **out_set);

/*
 *	Check if the attribute of the volume satisfies the given condition.
 *	Evaluation is done by comparing the volume's attribute with the
 *	given value. 
 *	The value 0 is returned if the condition is satisfied by
 *	the volume or else 1 is returned.
 */

extern short xlv_query_volume_eval(
	xlv_tab_vol_entry_t *volume,
	short attr_id,
	char *value,
	short oper);

/*
 *	Check if the attribute of the subvolume satisfies the given condition.
 *	Evaluation is done by comparing the subvolume's attribute with the
 *	given value. 
 *	The value 0 is returned if the condition is satisfied by
 *	the subvolume or else 1 is returned.
 */

extern short xlv_query_subvolume_eval(
	xlv_tab_subvol_t *sub_volume,
	short attr_id,
	char *value,
	short oper);

/*
 *	Check if the attribute of the plex satisfies the given condition.
 *	Evaluation is done by comparing the plex's attribute with the
 *	given value. 
 *	The value 0 is returned if the condition is satisfied by
 *	the plex or else 1 is returned.
 */

extern short xlv_query_plex_eval(
	xlv_tab_plex_t *plex,
	short attr_id,
	char *value,
	short oper);

/*
 *	Check if the attribute of the ve satisfies the given condition.
 *	Evaluation is done by comparing the ve's attribute with the
 *	given value. 
 *	The value 0 is returned if the condition is satisfied by
 *	the ve or else 1 is returned.
 */

extern short xlv_query_ve_eval(
	xlv_tab_vol_elmnt_t *ve,
	short attr_id,
	char *value,
	short oper);

extern void xlv_add_to_set(xlv_oref_t *oref, query_set_t **set);

extern void xlv_add_subvolume_to_set( xlv_tab_vol_entry_t *volume, xlv_tab_subvol_t *subvolume, query_set_t **set);

extern void xlv_add_plex_to_set(xlv_tab_vol_entry_t *volume,xlv_tab_subvol_t *subvolume,unsigned short plex_no,xlv_tab_plex_t *plex,query_set_t **set);

extern void xlv_add_ve_to_set2(xlv_tab_plex_t *plex,long ve_no, xlv_tab_vol_elmnt_t *ve,query_set_t **set);

extern void xlv_add_ve_to_set(xlv_tab_vol_entry_t *volume, xlv_tab_subvol_t *subvolume, unsigned short plex_no, long ve_no, xlv_tab_vol_elmnt_t *ve, query_set_t **set);


#endif /*  __XLV_QUERY_H__ */
