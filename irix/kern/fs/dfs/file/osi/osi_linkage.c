/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_linkage.c,v 65.3 1998/03/23 16:26:40 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1993, 1995 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to force references to exported symbols in the
 * DFS core module that are not otherwise used internally.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_linkage.c,v 65.3 1998/03/23 16:26:40 gwehrman Exp $ */

#include <dcedfs/osi.h>
#include <dcedfs/osi_ufs.h>
#include <dcedfs/lock.h>
#include <dcedfs/osi_uio.h>
#include <dcedfs/osi_dfserrors.h>
#include <dcedfs/osi_ufs.h>
#include <dcedfs/osi_net.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)

#ifndef AFS_SUNOS54_ENV
/* initialized later */
void *osi_ufs_symbol_linkage[OSI_UFS_SYMBOL_LINKS];
#endif /* AFS_SUNOS54_ENV */

void *osi_symbol_linkage[] = {
    /*
     * symbols from osi.h
     */
    (void *)osi_getvdev,
    (void *)osi_Open,
    (void *)osi_Stat,
    (void *)osi_Close,
    (void *)osi_Truncate,
    (void *)osi_Read,
    (void *)osi_Write,
    (void *)osi_RDWR,
    (void *)osi_MapStrategy,
    (void *)osi_Invisible,
    (void *)osi_CallProc,
    (void *)osi_CancelProc,
    (void *)osi_Time,
    (void *)osi_SetTime,
    (void *)osi_Free,
    (void *) 0,	/* used to be osi_Dump */
    (void *)osi_FreeBufferSpace,
    (void *)osi_printIPaddr,
    (void *)osi_cv2string,
    (void *)osi_Alloc,
    (void *)osi_AllocBufferSpace,
    (void *)osi_GetBuf,
    (void *)osi_ReleaseBuf,
    (void *)osi_InitWaitHandle,
    (void *)osi_Wait,
    (void *)osi_CancelWait,
    (void *)osi_genpag,
    (void *)osi_getpag,
    (void *)osi_GetPagFromCred,
    (void *)osi_SetPagInCred,
    (void *)&afsdb_osi,
    (void *)afsl_InitTraceControl,
    (void *)afsl_TracePrintProlog,
    (void *)afsl_TracePrint,
    (void *)&afsl_tr_global,
    (void *)&afsl_AddPackage,
    /*
     * symbols from osi_ufs.h
     */
    (void *)osi_CantCVTVP,
    /*
     * symbols from osi_uio.h
     */
    (void *)osi_uio_copy,
    (void *)osi_uio_trim,
    (void *)osi_uio_skip,
    /*
     * symbols from osi_dfserrors.h
     */
    (void *)err_DFSToLocal,
    (void *)err_localToDFS,
    (void *)osi_initDecodeTable,
    (void *)osi_initEncodeTable,
    /*
     * symbols from osi_port_mach.h
     */
    (void *)osi_PreemptionOff,
    (void *)osi_RestorePreemption,
    (void *)osi_GetMachineName,
    (void *)osi_Sleep,
    (void *)osi_SleepInterruptably,
    (void *)osi_Wakeup,
    (void *)osi_NewProc,
    (void *)osi_vptofid,
    /*
     *symbols from osi_net_mach.h
     */
    (void *)osi_SameHost,
    (void *)osi_SameSubNet,
    (void *)osi_SameNet,
    /*
     * symbols from osi_lock_mach.h
     */
    (void *)lock_ObtainWriteNoBlock,
    (void *)lock_ObtainSharedNoBlock,
    (void *)osi_SleepR,
    (void *)osi_SleepW,
    (void *)osi_SleepS,
    (void *)osi_Sleep2,
    (void *)osi_SleepRI,
    (void *)osi_SleepWI,
    (void *)osi_SleepSI,

    /* symbols from lock.h */
    (void *)lock_Obtain,
    (void *)lock_Release,
    (void *)lock_ObtainNoBlock,

    /* symbols from osi_cred_mach.h */
    (void *)osi_crequal,
    
    /* symbols from osi_misc.c */
    (void *)osi_Zalloc, 
    (void *)osi_Zalloc_r, 
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
