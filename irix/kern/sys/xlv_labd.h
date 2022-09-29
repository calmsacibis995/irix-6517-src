#ifndef __XLV_LABD_H__
#define __XLV_LABD_H__

/**************************************************************************
 *                                                                        *
 *                Copyright (C) 1994, Silicon Graphics, Inc.              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.5 $"

/*
 * Format of request messages returned to xlv_labd by the
 * SGI_XLV_NEXT_RQST syssgi.
 */


#define XLV_LABD_RQST_OFFLINE		1
#define XLV_LABD_RQST_VES_ACTIVE	2
#define XLV_LABD_RQST_CONFIG_CHANGE     3

typedef struct {
	__uint32_t	operation;
	__uint32_t	num_vol_elmnts;
	uuid_t		vol_uuid;
	uuid_t		vol_elmnt_uuid [XLV_MAX_FAILURES];
} xlv_config_rqst_t;

typedef struct {
	__uint32_t	operation;
	__uint32_t	num_vol_elmnts;
	uuid_t		plex_uuid;
	uuid_t		vol_elmnt_uuid [XLV_MAX_VE_PER_PLEX];
} xlv_ves_active_rqst_t;


#endif /* __XLV_LABD_H__ */
