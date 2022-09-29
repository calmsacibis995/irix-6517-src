/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: volc.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1997/12/16 17:05:47  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:20:03  jdoak
 * *** empty log message ***
 *
 * $EndLog$
 */
/*
 * (C) Copyright 1996, 1991 Transarc Corporation.
 * All Rights Reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/userInt/RCS/volc.h,v 65.3 1999/07/21 19:01:14 gwehrman Exp $ */

#ifndef _VOLC_H_
#define _VOLC_H_
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/osi_net.h>
#include <assert.h>
#include <dcedfs/ftserver_data.h>

/*
 * fts_NeedAggrType() mask asking for any extended file system (LFS, VxFS)
 */
#define AG_TYPE_ANY_EFS_MASK	((1 << AG_TYPE_EPI) | (1 << AG_TYPE_VXFS) | (1 << AG_TYPE_DMEPI))

extern char programName[];

extern char *AggrType[];
extern int verbose;

extern char currentCellName[];
extern char localCellName[];
extern afs_hyper_t localCellID;
extern char remoteCellName[];
extern afs_hyper_t remoteCellID;

extern int volc_noEnumOnVldbEntryCreate;

IMPORT int useNoAuth;
IMPORT int useLocalAuth;

#define VOL_ERR_BITS (VOL_BUSY|VOL_DELONSALVAGE|VOL_OUTOFSERVICE)

/* Pipe state to facilitate detecting pipe errors when calling FTSERVER_Dump
   and FTSERVER_Restore */
typedef struct ftsPipeState {
  char *data;		/* opaque data */
  unsigned long error;	/* error code */
} ftsPipeState;


/*
 * Exported globals/functions of volc_vprocs.c
 */
extern long VC_CreateVolume(), VC_DeleteVolume(), VC_BackupVolume(), VC_DumpVolume();
extern long VC_RestoreVolume(), VC_MoveVolume(), VC_ReleaseVolume(), VC_ListAggregates();
extern long VC_ListVolumes(), VC_SyncVldb(), VC_RenameVolume(), VC_SyncServer();
extern long VC_VolserStatus(), VC_VolumeZap(), VC_ZapVolumeClones();
extern long VC_GenerateVolumeClones(), VC_VolumeStatus(), VC_EndTrans();
extern long VC_SetQuota(), VC_SetStateBits();

extern int vols_AggregateInfo();
extern long fts_DeleteVolume _TAKES((rpc_binding_handle_t,
			   unsigned long,
			   afs_hyper_t *,
			   long,
			   struct sockaddr *));
extern long fts_VolumeExists _TAKES((rpc_binding_handle_t,
			   unsigned long,
			   afs_hyper_t *));
extern long CacheServerConnection _TAKES((struct sockaddr *,
					   rpc_binding_handle_t,
					   int));



/*
 * Exported globals/functions of volc_vldbsubr.c
 */
extern char *volTypes[MAXTYPES];

/*
 * Exported globals/functions of volc_volsint.c
 */
extern int vols_CreateTrans _TAKES((register rpc_binding_handle_t,
				     afs_hyper_t *,
				     unsigned long,
				     unsigned long,
#ifdef SGIMIPS
				     signed32 *,
#else
				     long *,
#endif /* SGIMIPS */
				     int));
extern int vols_AbortTrans _TAKES((register rpc_binding_handle_t,
				     long));
extern int vols_DeleteTrans _TAKES((register rpc_binding_handle_t,
				     long));
extern int vols_CreateVolume _TAKES((register rpc_binding_handle_t,
				      unsigned long,
				      char *,
				      long,
				      unsigned long,
				      afs_hyper_t *,
				      afs_hyper_t *,
#ifdef SGIMIPS
				      signed32 *));
#else
				      long *));
#endif /* SGIMIPS */
extern int vols_DeleteVolume _TAKES((register rpc_binding_handle_t,
				      long));

/* pipe transport protocol selector bits */
#define PIPE_NONE	0	/* use DCE pipes only */
#define PIPE_UNIX	1	/* use UNIX sockets for local servers */
#define PIPE_TCP	2	/* use TCP sockets for remote servers */

extern int vols_Dump _TAKES((register rpc_binding_handle_t,
			      long,
			      ftserver_Date *,
			      pipe_t *,
			      unsigned));
extern int vols_Restore _TAKES((register rpc_binding_handle_t,
				 long,
				 unsigned long,
				 pipe_t *,
				 unsigned));
extern int vols_Forward _TAKES((register rpc_binding_handle_t,
				 register rpc_binding_handle_t,
				 long,
				 struct ftserver_Date *,
				 struct ftserver_dest *,
				 long,
				 int));
extern int vols_Clone _TAKES((register rpc_binding_handle_t,
			       long,
			       long,
			       char *,
			       afs_hyper_t *));
extern int vols_ReClone _TAKES((register rpc_binding_handle_t,
				 long,
				 afs_hyper_t *));
extern int vols_GetFlags _TAKES((register rpc_binding_handle_t,
				  long,
#ifdef SGIMIPS
				  unsigned32 *));
#else
				  unsigned long *));
#endif /* SGIMIPS */
extern int vols_SetFlags _TAKES((register rpc_binding_handle_t,
				  long,
				  unsigned long));
extern int vols_GetStatus _TAKES((register rpc_binding_handle_t,
				   long,
				   struct ftserver_status *));
extern int vols_SetStatus _TAKES((register rpc_binding_handle_t,
				    long,
				    unsigned long,
				    struct ftserver_status *));
extern int vols_ListVolumes _TAKES((register rpc_binding_handle_t,
				     unsigned long,
				     long,
				     long,
				     long *,
				     struct ftserver_statEntries *));
extern int vols_ListAggregates _TAKES((register rpc_binding_handle_t,
					struct sockaddr *,
					ftserver_iterator *,
					ftserver_iterator *,
					struct ftserver_aggrEntries *));
extern int vols_AggregateInfo _TAKES((register rpc_binding_handle_t,
				       struct sockaddr *,
				       unsigned long,
				       struct ftserver_aggrInfo *,
				       int));
extern int vols_Monitor _TAKES((register rpc_binding_handle_t,
				 struct ftserver_transEntries *));
extern int vols_GetOneVolStatus _TAKES((register rpc_binding_handle_t,
					 afs_hyper_t *,
					 unsigned long,
					 struct ftserver_status *,
					 int));
extern int vols_SwapIDs _TAKES((register rpc_binding_handle_t,
				 long,
				 long));
extern long fts_GetAllAggrs _TAKES((rpc_binding_handle_t,
				     struct sockaddr *,
				     long *,
				     ftserver_aggrList **));
extern void DisplayVolumes _TAKES((struct sockaddr *,
				    unsigned long,
				    ftserver_status *,
				    long, long, long));
extern void VolumeStats _TAKES((ftserver_status *,
				 struct vldbentry *,
				 struct sockaddr *,
				 unsigned long,
				 long));
extern void PrintVolserStatus _TAKES((ftserver_transStatus *));
extern void fts_InitDecoder _TAKES((void));

/* Misc exports */
extern char *MapSockAddr();
extern char *ExtractSuffix _TAKES((char *));
extern u_long GetVolumeID _TAKES((char *, afs_hyper_t *));
extern char *fts_ivToString _TAKES((unsigned long));
extern int fts_strToIv _TAKES((char *, unsigned long *));
extern int fts_uuidToStr _TAKES((afsUUID *, char *, int));
extern int fts_strToUuid _TAKES((char *, afsUUID *));
extern int GetAggr _TAKES ((char *,
			     struct sockaddr *,
			     long,
#ifdef SGIMIPS
			     unsigned32 *,
#else
			     unsigned long *,
#endif /* SGIMIPS */
			     char *));
#define	GETAGGR_NUMERICONLY	0
#define	GETAGGR_VALIDATE	1
#define	GETAGGR_NUMERICOK	2

extern rpc_binding_handle_t connToServer _TAKES((struct sockaddr *, u_char *,
						 int));
#define	SERVERKIND_ANY	0
#define	SERVERKIND_FT	1
#define	SERVERKIND_REP	2
extern long fts_UseThisTimeout _TAKES((rpc_binding_handle_t, int));

IMPORT int fts_GetToken _TAKES((struct afsNetAddr *,
				 unsigned char *,
				 afs_hyper_t *,
				 long,
				 unsigned long,
				 int *));

IMPORT int fts_ReleaseToken _TAKES((int));

extern long fts_WaitForRepHostUpdates _TAKES((struct vldbentry *,
					      afs_hyper_t *,
					      afs_hyper_t *,
					      afsNetAddr *,
					      int));
#endif /* _VOLC_H_ */
