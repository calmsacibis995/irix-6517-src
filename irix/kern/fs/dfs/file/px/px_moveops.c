/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: px_moveops.c,v 65.5 1998/04/01 14:16:09 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: px_moveops.c,v $
 * Revision 65.5  1998/04/01 14:16:09  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added an extern declaration for vol_SetActionProc().
 *
 * Revision 65.4  1998/03/23 16:24:34  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/12/16 17:05:40  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.2  1997/11/06  19:57:54  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:30  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.11.1  1996/10/02  18:13:03  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:38  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1993 Transarc Corporation
 *      All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/fshs.h>
#include <dcedfs/aggr.h>
#include <dcedfs/volume.h>
#include <dcedfs/xcred.h>
#include <dcedfs/xvfs_vnode.h>
#include <dcedfs/tkset.h>
#include <px.h>

RCSID ("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/px/RCS/px_moveops.c,v 65.5 1998/04/01 14:16:09 gwehrman Exp $")

#define	VOLCLEAN_DESTROY    0x0
#define	VOLCLEAN_TOKEN_DESTROY    0x1
#define	VOLCLEAN_SRC_FLDB    0x2
#define	VOLCLEAN_TGT_FLDB    0x3

struct volCleanup {
    struct volCleanup *next;
    afs_hyper_t volId;
    unsigned long cleanState;
    unsigned long startTime;
};

static int vc_inited = 0;
static int numVCs;
static struct lock_data vc_lock;
static struct volCleanup *VCs;

static tkm_tokenID_t openPreserve, openExecute, openAll;

#define	BASIC_RHYTHM	  5000	/* how often we get called */
#define	BASIC_RHYTHM_HORIZON	(2*BASIC_RHYTHM + 20) /* how far ahead to look */

#ifdef SGIMIPS
extern void vol_SetActionProc(long (*proch)());
#endif

/* 
 * Here are the main work for some of the replication-related RPC calls. 
 */

static long vc_Action(volp, actionh)
struct volume *volp;
int actionh;
{/* Called from the xvolume layer. */
    return 0;
}

static void vc_ckInit(void)
{
    lock_Init(&vc_lock);
    vol_SetActionProc(vc_Action);
    vc_inited = 1;
}

#define	CHECK_INIT  {if (vc_inited == 0) vc_ckInit();}

/* Called periodically from the PX daemon loop. */
long pxvc_Cleanups()
{
    long interv;

    icl_Trace0(px_iclSetp, PX_TRACE_PVC);
    CHECK_INIT;
    return BASIC_RHYTHM;
}
