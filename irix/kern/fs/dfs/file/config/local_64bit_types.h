/* Copyright (C) 1996 Transarc Corporation - All rights reserved. */
/*
 * @DEC_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: local_64bit_types.h,v $
 * Revision 65.1  1997/10/20 19:21:49  jdoak
 * *** empty log message ***
 *
BINRevision 1.1.2.1  1996/10/02  17:14:42  damon
BIN	Newest DFS from Transarc
BIN
 * $EndLog$
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/config/RCS/local_64bit_types.h,v 65.1 1997/10/20 19:21:49 jdoak Exp $ */

/* This file defines in-memory types which represent RPC wire types that
 * contain 64bit quantities.  The RPC IDL compiler and marshalling code map
 * between the two so that the interface functions can use a platform dependent
 * 64bit type, while the wire-level definition is remains platform
 * independent.
 *
 * The mapping functions are defined in local_64bit_xlate.c. */

#ifndef _TRANSARC_LOCAL_64BIT_TYPES_
#define _TRANSARC_LOCAL_64BIT_TYPES_

/* Get the defintion of afs_hyper_t from config/stds.h. */
#include <dcedfs/param.h>
#include <dcedfs/stds.h>

typedef struct {
    afs_hyper_t tokenID;
    u_int32 expirationTime;
    afs_hyper_t type;
    afs_hyper_t beginRange;
    afs_hyper_t endRange;
} afs_token_t;

typedef struct {
    int16 l_type;
    int16 l_whence;
    afs_hyper_t l_start_pos;
    afs_hyper_t l_end_pos;
    u_int32 l_pid;
    u_int32 l_sysid;
    u_int32 l_fstype;
} afs_recordLock_t;

#endif /* _TRANSARC_LOCAL_64BIT_TYPES_ */
