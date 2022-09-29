/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _KSYS_VOP_BACKDOOR_H
#define _KSYS_VOP_BACKDOOR_H

#ident	"$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/vnode.h>

/*
 * Header file defining vop interface backdoor structure
 * 
 * This header file defines a mechanism whereby VOP's (which inherently
 * may go remote) may request that the ultimate caller perform certain
 * operations that are only reasonably done in the context of the original
 * requestor.  This currently includes ioctl's which perform operations
 * on the callers file descriptor set but might also be used to implement
 * VOP_OPEN on fdfs.
 */

/*
 * The vopbd_req_t defines the operation requested.
 */
typedef enum {
	VOPBDR_NIL,		/* Do nothing */
        VOPBDR_ASSIGN,          /* Assign a vnode to a file descriptor. */
        VOPBDR_DUP              /* Duplictae an existing file descriptor. */
} vopbd_req_t;

struct vnode;
typedef struct {
	vnode_t	        *vopbda_vp;     /* Pointer to vnode to reference. */
        vnumber_t       vopbda_vnum;    /* Vnode gen for checking. */
        int             vopbda_cpi;     /* Checkpoint info for ckpt_setfd. */
} vopbd_assign_t;

struct vfile;     
typedef struct {
	struct vfile	*vopbdd_vfp;    /* Pointer to vnode to reference. */
        int             vopbdd_srcfd;   /* Source fd for dup. */
} vopbd_dup_t;
        
typedef union {
        vopbd_assign_t  vopbdp_assign;  /* Parms for assign request. */
        vopbd_dup_t     vopbdp_dup;     /* Parms for dup request. */
} vopbd_parm_t;

typedef struct vopbd {
        vopbd_req_t	vopbd_req;	/* The request code. */
        vopbd_parm_t    vopbd_parm;     /* One of the corresponding parameter
					   structures. */
} vopbd_t;

#endif /* _KSYS_VOP_BACKDOOR_H */
