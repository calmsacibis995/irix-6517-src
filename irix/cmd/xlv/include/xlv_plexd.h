#ifndef __XLV_PLEXD_H_
#define __XLV_PLEXD_H_

/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1994, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.7 $"

/*------------------------------------------------------------------
 * This defines the interface between the client library and the
 * xlv plexd. 
 */

/*
 * FIFO used for issuing requests.
 */
#define XLV_PLEXD_DIR	    	"/etc/"

#define XLV_PLEXD_RQST_FIFO 	XLV_PLEXD_DIR ".xlv_plexd_request_fifo"
#define XLV_PLEXD_LOG		XLV_PLEXD_DIR "xlv_plexd.log"

/*
 * Each request may contain up to PLEX_CPY_CMDS_PER_REQUEST
 * subvolumes that need to be revived.
 */
#define PLEX_CPY_CMDS_PER_REQUEST	20

typedef struct {
        xlv_name_t      subvol_name;
	dev_t		device;
        __int64_t       start_blkno;
        __int64_t       end_blkno;
} xlv_plexd_request_entry_t;

typedef struct {
        unsigned int                 	num_requests;
        xlv_plexd_request_entry_t	request[PLEX_CPY_CMDS_PER_REQUEST];
} xlv_plexd_request_t;

/*------------------------------------------------------------------
 * The following defines some local procedures used to format and
 * issue requests to the xlv plexd.
 *
 * These should only be called from the client side.
 */
typedef struct {
        int                     fd;
        xlv_plexd_request_t     plexd_request;
} xlv_revive_rqst_t;

extern void xlv_init_revive_request(xlv_revive_rqst_t *revive_request);

extern int xlv_issue_revive_request (
	xlv_revive_rqst_t	*revive_request,
        xlv_tab_subvol_t	*xlv_p,
	__int64_t		first_blkno,
	__int64_t		last_blkno);

extern int xlv_end_revive_request (xlv_revive_rqst_t *revive_request);


extern int xlv_tab_revive_subvolume (xlv_tab_subvol_t *xlv_p);


#endif /* __XLV_PLEXD_H_ */
