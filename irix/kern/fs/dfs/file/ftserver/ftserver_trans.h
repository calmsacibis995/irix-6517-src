/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: ftserver_trans.h,v $
 * Revision 65.1  1997/10/20 19:20:10  jdoak
 * *** empty log message ***
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/ftserver/RCS/ftserver_trans.h,v 65.1 1997/10/20 19:20:10 jdoak Exp $ */

#ifndef _FTSERVER_TRANSH_
#define _FTSERVER_TRANSH_ 1
#include <dcedfs/volume.h>

/*
 * Per transaction structure
 */
struct ftserver_trans {
    struct ftserver_trans *t_next;	/* next ptr in active trans list */
    long t_transId;	    		/* transaction id */
    long t_aggrId;			/* volume's aggregate */
    afs_hyper_t t_volId;		/* volume's Id */
    long t_vDesc;			/* Descriptor to VOL_* calls */
    long t_lastTime;	    		/* time trans was last active (for timeouts) */
    afsTimeval t_creationTime;	/* time the transaction started */
    long t_returnCode;			/* transaction error code */
    long t_states;			/* Transaction's state bits */
    short t_refCount;			/* reference count on this structure */
    unsigned long t_accStatus;		/* hold the fileset accStatus here */
    unsigned long t_accError;		/* hold the accError here (for monitor) */
    afs_hyper_t t_baseType;		/* the underlying type of aggregate */
    unsigned long t_flags;		/* flags for transaction state */
    unsigned long t_lastSyscall;		/* when the most recent syscall was made */

#if !defined(OSF_NO_SOCKET_DUMP)
    void *t_srvrSockP;			/* server socket descriptor */
    void *t_clntSockP;			/* client socket descriptor */
#endif /* OSF_NO_SOCKET_DUMP */

};
#define	FTSERVER_FLAGS_WASINCONSISTENT	0x001
#define	FTSERVER_FLAGS_RESTORESTOPPED	0x002
#define	FTSERVER_FLAGS_DELETED		0x004

/*
 * Exported globals/functions of vols_trans.c
 */
extern struct ftserver_trans *ftserver_trans;

extern trans_Init(), ftserver_DeleteTrans(), ftserver_PutTrans(), ftserver_GCTrans();
extern ftserver_SwapTransStates _TAKES((struct ftserver_trans *,
					 struct ftserver_trans *));
extern void ftserver_KeepTransAlive _TAKES((struct ftserver_trans *,
					    unsigned long));
extern struct ftserver_trans *ftserver_NewTrans _TAKES((afs_hyper_t *,
							 long,
							 long,
							 long *));
extern struct ftserver_trans *ftserver_FindTrans _TAKES((register long));
extern struct ftserver_trans *ftserver_FindTransById _TAKES((afs_hyper_t *));
extern struct ftserver_trans *ftserver_FindTransByDesc _TAKES((long));

#endif /* _FTSERVER_TRANSH_ */
