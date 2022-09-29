/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: xvnops_call.c,v 65.4 1998/03/23 16:43:27 gwehrman Exp $";
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
 * $Log: xvnops_call.c,v $
 * Revision 65.4  1998/03/23 16:43:27  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1998/01/20 17:18:19  lmc
 * Explicitly cast a parameter from a long to an integer to get rid
 * of the warning message.  Its the acl length, which better always
 * be less than 32bits.
 *
 * Revision 65.2  1997/11/06  20:01:04  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:41  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.631.1  1996/10/02  21:01:03  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:48:03  damon]
 *
 * Revision 1.1.626.3  1994/07/13  22:28:44  devsrc
 * 	merged with bl-10
 * 	[1994/06/29  11:41:39  devsrc]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  16:01:54  mbs]
 * 
 * Revision 1.1.626.2  1994/06/09  14:20:21  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:31:52  annie]
 * 
 * Revision 1.1.626.1  1994/02/04  20:30:08  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:18:04  devsrc]
 * 
 * Revision 1.1.624.1  1993/12/07  17:33:44  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:58:01  jaffe]
 * 
 * Revision 1.1.5.5  1993/01/21  15:20:46  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  15:59:55  cjd]
 * 
 * Revision 1.1.5.4  1992/11/24  19:41:59  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:25:52  bolinger]
 * 
 * Revision 1.1.5.3  1992/09/25  19:01:30  jaffe
 * 	Transarc delta: bab-ot5024-getacl-no-overflow-check 1.1
 * 	  Selected comments:
 * 	    The code that was calling the getacl vnode op was not checking whether
 * 	    or not it was overflowing the user's buffer before it called copyout to
 * 	    return the flat acl to the user.  Appropriate checks have now been added.
 * 	    ot 5024
 * 	    See above.
 * 	[1992/09/23  19:36:15  jaffe]
 * 
 * Revision 1.1.5.2  1992/08/31  21:14:51  jaffe
 * 	Transarc delta: bab-ot3539-syscall-error-propagation 1.6
 * 	  Selected comments:
 * 	    This delta unifies the error propagation model used by afs_syscall
 * 	    and the routines that it calls through the syscall switch.
 * 	    ot 3539
 * 	    This first version is simply a check-point so I can reopen another delta.
 * 	    Another check-point.
 * 	    Change the afssyscall calling convention for RS/6000 to correspond
 * 	    more closely to that of OSF/1.
 * 	    Another checkpoint.
 * 	    Debugging
 * 	    A little bit of cleanup.
 * 	    Corrected number of arguments to afscall_vnode_ops.  Surprisingly, it
 * 	    always worked when compiled with -g?!
 * 	    See above.
 * 	[1992/08/30  12:24:17  jaffe]
 * 
 * Revision 1.1.3.4  1992/05/22  20:21:57  garyf
 * 	rationalize out OSF1 conditionals
 * 	[1992/05/22  03:13:38  garyf]
 * 
 * Revision 1.1.3.3  1992/01/24  03:48:41  devsrc
 * 	Merging dfs6.3
 * 	[1992/01/24  00:19:31  devsrc]
 * 
 * Revision 1.1.3.2  1992/01/23  20:24:05  zeliff
 * 	Moving files onto the branch for dfs6.3 merge
 * 	[1992/01/23  18:39:53  zeliff]
 * 	Revision 1.1.1.3  1992/01/23  22:22:02  devsrc
 * 	Fixed logs
 * 
 * $EndLog$
*/
/*
 * Copyright (C) 1990 Transarc Corporation
 * All rights reserved.
 */

/*			Extended vnode ops
			Syscall Interface
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_uio.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/sysincludes.h>

#include <aclint.h>
#include <dcedfs/xvfs_vnode.h>

/*
 * Main fs_syscall kernel entry point
 */

#ifdef KERNEL

int afscall_vnode_ops (code, parm1, parm2, parm3, parm4, retvalP)
    long code, parm1, parm2, parm3, parm4;
    int * retvalP;
{
    struct dfs_acl *aclp;
    struct vnode *vp, *sourcevp;
    char *tp;
    int	acl_buffersize = 0;
    int rc;

    *retvalP = 0;	/* no "extra" info will be returned */
    tp = (char *) 0;
    aclp = (struct dfs_acl *) 0;
    rc =  osi_lookupname ((char *)parm1, OSI_UIOUSER, FOLLOW_LINK,
			    (struct vnode **)0, &vp);
    if (rc) goto bad2;

    switch (code) {

      case VNX_OPCODE_GETACL:		/* Get ACL */
	aclp = (struct dfs_acl *) osi_AllocBufferSpace();
	rc  = xvn_getacl (vp, aclp, parm4, osi_getucred());
	if (rc == 0) {
	  rc = osi_copyin((caddr_t)parm3,
			  (caddr_t)&acl_buffersize, sizeof (int));
	  if (rc == 0) {
	    if (aclp->dfs_acl_len <= acl_buffersize) {
	      rc = osi_copyout((caddr_t)&aclp->dfs_acl_len, (caddr_t)parm3,
				sizeof(aclp->dfs_acl_len));
	      if (rc == 0) {
		rc = osi_copyout((caddr_t)&aclp->dfs_acl_val[0], (caddr_t)parm2,
				aclp->dfs_acl_len);
	      }
	    } else {
	      /* if not enough room in buffer passed in */
	      rc = E2BIG;
	    }
	  }
	}

	break;

      case VNX_OPCODE_SETACL:		/* Set ACL */
	if (parm3 <= 0 || parm3 > MAXDFSACL) {
	    rc = EINVAL;
	    goto bad1;
	}
	aclp = (struct dfs_acl *) osi_AllocBufferSpace();
	aclp->dfs_acl_len = (int) parm3;
	rc = osi_copyin((caddr_t)parm2, (caddr_t)&aclp->dfs_acl_val[0], parm3);
	if (rc) goto bad1;
	rc = xvn_setacl (vp, aclp, 0, parm4, 0, osi_getucred());
	break;

      case VNX_OPCODE_COPYACL:	/* Copy ACL */
	/* Use parm3 as the dest acl slot and parm4 as the source acl slot. */
	rc = osi_lookupname ((char *)parm2, OSI_UIOUSER, FOLLOW_LINK,
				    (struct vnode **)0, &sourcevp);
	if (rc) goto bad1;
	rc = xvn_setacl (vp, 0, sourcevp, parm3, parm4, osi_getucred());
	break;

#ifdef notdef
	/* THIS IS OBSOLETE, RIGHT? -MLK */
      case VNX_OPCODE_QUOTA:	/* Get quota information */
	tp = osi_AllocBufferSpace();
	rc = osi_copyin((caddr_t)parm2, (caddr_t)tp, parm3);
	if (rc)
	    goto bad1;
	rc = xvn_quota (vp, tp, parm3, osi_getucred());
	if (rc) goto bad1;
	rc = osi_copyout((caddr_t)tp,
			 (caddr_t)parm2, parm3); /* in case it is a get */
	break;
#endif /* notdef */

      default:
	rc = EINVAL;
    }
  bad1:
    if (aclp)
	osi_FreeBufferSpace((struct osi_buffer *) aclp);
    OSI_VN_RELE(vp);
  bad2:
    if (tp) osi_FreeBufferSpace((struct osi_buffer *) tp);

    return (rc);
}

#endif /* KERNEL */
