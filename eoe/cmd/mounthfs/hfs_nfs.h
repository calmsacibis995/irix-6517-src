/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __HFS_NFS_H__
#define __HFS_NFS_H__

#ident "$Revision: 1.2 $"


/* File handle definition */

typedef struct fhandle fhandle_t;

#include <sys/fs/nfs_clnt.h>

#define NFS_FHSIZE 32
#define FHPADSIZE       (NFS_FHSIZE - 2*sizeof(u_short)\
                         - sizeof(struct loopfid))

struct fhandle{
  u_short             fh_fsid;                /* File system id */
  u_short             fh_align;               /* alignment, must be zero */
  struct loopfid      fh_fid;                 /* see <sys/fs/nfs_clnt.h> */
  char                fh_pad[FHPADSIZE];      /* padding, must be zeros */
};


#include "nfs_prot.h"

#endif

