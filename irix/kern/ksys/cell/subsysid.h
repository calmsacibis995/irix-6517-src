/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_KSYS_SUBSYSID_H_
#define	_KSYS_SUBSYSID_H_	1
#ident "$Revision: 1.19 $"

#ifndef CELL
#error included by non-CELL configuration
#endif

/*
 * Subsystem id's - one needed for each idl file.
 * Remember to update SUBSYSID_LIST appropriately.
 */
#define DCSHM_SUBSYSID		1	/* shm client */
#define DSSHM_SUBSYSID		2	/* shm server */
#define DCVN_SUBSYSID		3	/* cfs client */
#define DSVN_SUBSYSID		4	/* cfs server */
#define DCVS_SUBSYSID		5	/* vsocket client */
#define DSVS_SUBSYSID		6	/* vsocket server */
#define DCPGRP_SUBSYSID		7	/* pgrp client */
#define DSPGRP_SUBSYSID		8	/* pgrp server */
#define DCVFS_SUBSYSID		9	/* dvfs client */
#define DSVFS_SUBSYSID		10	/* dvfs server */
#define DCPROC_SUBSYSID		11	/* proc client */
#define DSPROC_SUBSYSID		12	/* proc server */
#define OBJECT_SUBSYSID		13	/* object relocator */
#define DCFILE_SUBSYSID		14	/* vfile client */
#define DSFILE_SUBSYSID		15	/* vfile server */
#define WPDS_SUBSYSID		16	/* WP Domain mgr server */
#define WPSS_SUBSYSID		17	/* WP Domain server server */
#define WPSC_SUBSYSID		18	/* WP Domain server client */
#define DCHOST_SUBSYSID		19	/* host client */
#define DSHOST_SUBSYSID		20	/* host server */
#define DCSESSION_SUBSYSID	21	/* session client */
#define DSSESSION_SUBSYSID	22	/* session server */
#define DPID_SUBSYSID		23	/* distributed pid */
#define DHEART_BEAT_SUBSYSID	24	/* Heartbeat subsysid */
#define DCSPECFS_SUBSYSID	25	/* Specfs "local" client side */
#define DSSPECFS_SUBSYSID	26	/* Specfs "common" server side */
#define UCOPY_SUBSYSID		27	/* copyin/out */
#define	CRED_SVC_SUBSYSID	28	/* credentials service */
#define	CMS_SVC_SUBSYSID	29	/* Membership service */
#define	EXIM_SUBSYSID		30	/* Virtual Memory */
#define	DTHREAD_SUBSYSID	31	/* remote thread services */
#define DCXVN_SUBSYSID		32	/* cxfs client */
#define DSXVN_SUBSYSID		33	/* cxfs server */
#define	CELL_TEST_SUBSYSID	34	/* cell test subsysid */
#define CXFS_ARRAY_SUBSYSID	35	/* cxfs array mount services */
#define	UTILITY_SUBSYSID	36	/* utility stuff */

#define MAX_SUBSYSID		36	/* subsysid max */

/*
 * SUBSYSID_LIST defines the list of subsystems included in various
 * configurations.  This information can be used to build various
 * tables by defining SYBSYSID_FOR_{NONE,CELL,CELL_IRIX,CELL_ARRAY} as
 * desired before invoking SUBSYSID_LIST.
 *
 * The subsystem id's should be listed in ascending order with no
 * gaps allowed.
 */
#define SUBSYSID_LIST()				      		\
	SUBSYSID_FOR_CELL_IRIX(DCSHM_SUBSYSID, dcshm)		\
	SUBSYSID_FOR_CELL_IRIX(DSSHM_SUBSUSID, dsshm)		\
	SUBSYSID_FOR_CELL(DCVN_SUBSYSID, dcvn)    		\
	SUBSYSID_FOR_CELL(DSVN_SUBSYSID, dsvn)    		\
	SUBSYSID_FOR_CELL_IRIX(DCVS_SUBSYSID, dcvsock)		\
	SUBSYSID_FOR_CELL_IRIX(DSVS_SUBSYSID, dsvsock)		\
	SUBSYSID_FOR_CELL_IRIX(DCPGRP_SUBSYSID, dcpgrp)		\
	SUBSYSID_FOR_CELL_IRIX(DSPGRP_SUBSYSID, dspgrp)		\
	SUBSYSID_FOR_CELL(DCVFS_SUBSYSID, dcvfs)    		\
	SUBSYSID_FOR_CELL(DSVFS_SUBSYSID, dsvfs)    		\
	SUBSYSID_FOR_CELL_IRIX(DCPROC_SUBSYSID, dcproc)		\
	SUBSYSID_FOR_CELL_IRIX(DSPROC_SUBSYSID, dsproc)		\
	SUBSYSID_FOR_CELL(OBJECT_SUBSYSID, kore)    		\
	SUBSYSID_FOR_CELL_IRIX(DCFILE_SUBSYSID, dcfile)		\
	SUBSYSID_FOR_CELL_IRIX(DSFILE_SUBSYSID, dsfile)		\
	SUBSYSID_FOR_CELL(WPDS_SUBSYSID, wpds)		        \
	SUBSYSID_FOR_CELL(WPSS_SUBSYSID, wpss)	        	\
	SUBSYSID_FOR_CELL(WPSC_SUBSYSID, wpsc)  		\
	SUBSYSID_FOR_CELL_IRIX(DCHOST_SUBSYSID, dchost)		\
	SUBSYSID_FOR_CELL_IRIX(DSHOST_SUBSYSID, dshost)		\
	SUBSYSID_FOR_CELL_IRIX(DCSESSION_SUBSYSID, dcsession)	\
	SUBSYSID_FOR_CELL_IRIX(DSSESSION_SUBSYSID, dssession)	\
	SUBSYSID_FOR_CELL_IRIX(DPID_SUBSYSID, dpid)     	\
	SUBSYSID_FOR_CELL(DHEART_BEAT_SUBSYSID, cell_hb)	\
        SUBSYSID_FOR_CELL_IRIX(DCSPECFS_SUBSYSID, spec_dc)      \
        SUBSYSID_FOR_CELL_IRIX(DSSPECFS_SUBSYSID, spec_ds)      \
	SUBSYSID_FOR_CELL(UCOPY_SUBSYSID, ucopy)        	\
	SUBSYSID_FOR_CELL(CRED_SVC_SUBSYSID, creds)		\
	SUBSYSID_FOR_CELL(CMS_SVC_SUBSYSID, cms)		\
	SUBSYSID_FOR_CELL(EXIM_SUBSYSID, exim)		        \
	SUBSYSID_FOR_CELL(DTHREAD_SUBSYSID, thread)		\
	SUBSYSID_FOR_NONE(DCXVN_SUBSYSID, dcxvn)    		\
	SUBSYSID_FOR_NONE(DSXVN_SUBSYSID, dsxvn)    		\
	SUBSYSID_FOR_CELL_DEBUG(CELL_TEST_SUBSYSID, cell_test)  \
        SUBSYSID_FOR_CELL_ARRAY(CXFS_ARRAY_SUBSYSID, cxfs_array)\
	SUBSYSID_FOR_CELL_DEBUG(UTILITY_SUBSYSID, utility)

#endif /* !_KSYS_SUBSYSID_H_ */
