/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: local_64bit_xlate.c,v 65.4 1998/03/23 16:47:04 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/* Copyright (C) 1996 Transarc Corporation - All rights reserved. */
/*
 * @DEC_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: local_64bit_xlate.c,v $
 * Revision 65.4  1998/03/23 16:47:04  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1998/03/02 22:29:42  bdr
 * Added proper function definitions.
 *
 * Revision 65.2  1997/11/06  20:01:49  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:21:49  jdoak
 * *** empty log message ***
 *
 * $EndLog$
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/common_data.h>
#include <dcedfs/osi.h>

#if defined(AFS_AIX31_ENV) && defined(KERNEL)
/* need to map malloc to xmalloc */
#include <sys/malloc.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/config/RCS/local_64bit_xlate.c,v 65.4 1998/03/23 16:47:04 gwehrman Exp $")

/***********************************************
 ** Translation routines for afsHyper / hyper **
 ***********************************************/

/* Allocate memory using the native memory manager since we are called from the
 * RPC system, which doesn't depend on DFS services.  In particular, on
 * AIX3.2.5 we are called with preemption enabled. */

#ifdef SGIMIPS
void afsHyper_from_local(
  afs_hyper_t *local_object,
  afsHyper **net_object)
#else
void afsHyper_from_local(local_object, net_object)
  afs_hyper_t *local_object;
  afsHyper **net_object;
#endif
{
    *net_object = osi_kalloc_r(sizeof(afsHyper));

    (*net_object)->low = AFS_hgetlo(*local_object);
    (*net_object)->high = AFS_hgethi(*local_object);
}

#ifdef SGIMIPS
void afsHyper_to_local(
  afsHyper *net_object,
  afs_hyper_t *local_object)
#else
void afsHyper_to_local(net_object, local_object)
  afsHyper *net_object;
  afs_hyper_t *local_object;
#endif
{
    AFS_hset64(*local_object, net_object->high, net_object->low);
}

#ifdef SGIMIPS
void afsHyper_free_local(
  afs_hyper_t *local_object)
#else
void afsHyper_free_local(local_object)
  afs_hyper_t *local_object;
#endif
{
    /* Not Used? */
}

#ifdef SGIMIPS
void afsHyper_free_inst(
  afsHyper *net_object)
#else
void afsHyper_free_inst(net_object)
  afsHyper *net_object;
#endif
{
    osi_kfree_r(net_object, sizeof(afsHyper));
}



/*************************************************
 ** Translation routines for afsToken / token_t **
 *************************************************/

#ifdef SGIMIPS
void afsToken_from_local(
  afs_token_t *local_object,
  afsToken **net_object)
#else
void afsToken_from_local(local_object, net_object)
  afs_token_t *local_object;
  afsToken **net_object;
#endif
{
    *net_object = osi_kalloc_r(sizeof(afsToken));

    (*net_object)->tokenID = local_object->tokenID;
    (*net_object)->expirationTime = local_object->expirationTime;
    (*net_object)->type = local_object->type;
    (*net_object)->beginRange = AFS_hgetlo(local_object->beginRange);
    (*net_object)->beginRangeExt = AFS_hgethi(local_object->beginRange);
    (*net_object)->endRange = AFS_hgetlo(local_object->endRange);
    (*net_object)->endRangeExt = AFS_hgethi(local_object->endRange);
}

#ifdef SGIMIPS
void afsToken_to_local(
  afsToken *net_object,
  afs_token_t *local_object)
#else
void afsToken_to_local(net_object, local_object)
  afsToken *net_object;
  afs_token_t *local_object;
#endif
{
    local_object->tokenID = net_object->tokenID;
    local_object->expirationTime = net_object->expirationTime;
    local_object->type = net_object->type;
    AFS_hset64(local_object->beginRange,
	       net_object->beginRangeExt, net_object->beginRange);
    AFS_hset64(local_object->endRange,
	       net_object->endRangeExt, net_object->endRange);
}

#ifdef SGIMIPS
void afsToken_free_local(
  afs_token_t *local_object)
#else
void afsToken_free_local(local_object)
  afs_token_t *local_object;
#endif
{
    /* Not Used? */
}

#ifdef SGIMIPS
void afsToken_free_inst(
  afsToken *net_object)
#else
void afsToken_free_inst(net_object)
  afsToken *net_object;
#endif
{
    osi_kfree_r(net_object, sizeof(afsToken));
}


/*********************************************************** 
 ** Translation routines for afsRecordLock / recordLock_t **
 ***********************************************************/

#ifdef SGIMIPS
void afsRecordLock_from_local(
  afs_recordLock_t *local_object,
  afsRecordLock **net_object)
#else
void afsRecordLock_from_local(local_object, net_object)
  afs_recordLock_t *local_object;
  afsRecordLock **net_object;
#endif
{
    *net_object = osi_kalloc_r(sizeof(afsRecordLock));

    (*net_object)->l_type = local_object->l_type;
    (*net_object)->l_whence = local_object->l_whence;
    (*net_object)->l_start_pos = AFS_hgetlo(local_object->l_start_pos);
    (*net_object)->l_start_pos_ext = AFS_hgethi(local_object->l_start_pos);
    (*net_object)->l_end_pos = AFS_hgetlo(local_object->l_end_pos);
    (*net_object)->l_end_pos_ext = AFS_hgethi(local_object->l_end_pos);
    (*net_object)->l_pid = local_object->l_pid;
    (*net_object)->l_sysid = local_object->l_sysid;
    (*net_object)->l_fstype = local_object->l_fstype;
}

#ifdef SGIMIPS
void afsRecordLock_to_local(
  afsRecordLock *net_object,
  afs_recordLock_t *local_object)
#else
void afsRecordLock_to_local(net_object, local_object)
  afsRecordLock *net_object;
  afs_recordLock_t *local_object;
#endif
{
    local_object->l_type = net_object->l_type;
    local_object->l_whence = net_object->l_whence;
    AFS_hset64(local_object->l_start_pos,
	       net_object->l_start_pos_ext, net_object->l_start_pos);
    AFS_hset64(local_object->l_end_pos,
	       net_object->l_end_pos_ext, net_object->l_end_pos);
    local_object->l_pid = net_object->l_pid;
    local_object->l_sysid = net_object->l_sysid;
    local_object->l_fstype = net_object->l_fstype;
}

#ifdef SGIMIPS
void afsRecordLock_free_local(
  afs_recordLock_t *local_object)
#else
void afsRecordLock_free_local(local_object)
  afs_recordLock_t *local_object;
#endif
{
    /* Not Used? */
}

#ifdef SGIMIPS
void afsRecordLock_free_inst(
  afsRecordLock *net_object)
#else
void afsRecordLock_free_inst(net_object)
  afsRecordLock *net_object;
#endif
{
    osi_kfree_r(net_object, sizeof(afsRecordLock));
}
