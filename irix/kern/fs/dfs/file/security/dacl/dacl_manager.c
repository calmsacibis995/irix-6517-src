/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_manager.c,v 65.4 1998/03/23 16:36:34 gwehrman Exp $";
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
 * $Log: dacl_manager.c,v $
 * Revision 65.4  1998/03/23 16:36:34  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1998/03/18 15:08:18  gwehrman
 * Removed dacl_WriteToDisk() prototype for user space builds.  Removed
 * definition of dacl_mgmt_InitSimpleManager().  It is not used.
 *
 * Revision 65.2  1997/11/06 20:00:36  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:16  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.12.1  1996/10/02  18:15:42  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:39  damon]
 *
 * Revision 1.1.7.3  1994/08/23  18:58:58  rsarbo
 * 	delegation
 * 	[1994/08/23  16:32:55  rsarbo]
 * 
 * Revision 1.1.7.2  1994/07/13  22:26:10  devsrc
 * 	merged with bl-10
 * 	[1994/06/28  21:11:03  devsrc]
 * 
 * 	delegation ACL support
 * 	[1994/06/07  14:04:59  delgado]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  16:01:14  mbs]
 * 
 * Revision 1.1.7.1  1994/06/09  14:18:42  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:30:44  annie]
 * 
 * Revision 1.1.3.3  1993/01/21  15:17:11  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  15:53:43  cjd]
 * 
 * Revision 1.1.3.2  1992/11/24  18:28:33  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:20:47  bolinger]
 * 
 * Revision 1.1  1992/01/19  02:52:44  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/*
 *	dacl_manager.c -- code to implement the generic ACL manager (& the simple one)
 *
 * Copyright (C) 1991 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

#include <epi_id.h>
#include <dce/id_base.h>
#include  <dce/id_epac.h>
#include  <dcedfs/fshs_deleg.h>
#include <dacl.h>
#include <dacl_debug.h>
#include <dacl_manager.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_manager.c,v 65.4 1998/03/23 16:36:34 gwehrman Exp $")

EXPORT long dacl_mgmt_SimpleRead(aclP, rockP, mgrUuidP)
     dacl_t *		aclP;
     char *		rockP;
     epi_uuid_t *	mgrUuidP;
{
  return dacl_ReadFromDisk(aclP, rockP, mgrUuidP);
}

EXPORT long dacl_mgmt_SimpleWrite(aclP, rockP, mgrUuidP)
     dacl_t *		aclP;
     char *		rockP;
     epi_uuid_t *	mgrUuidP;
{
#ifdef KERNEL
  extern long dacl_WriteToDisk(dacl_t *, char *);
#endif /* KERNEL */

  return dacl_WriteToDisk(aclP, rockP);
}

EXPORT long dacl_mgmt_InitManager(mgrP, mgrUuidP, readFuncP, writeFuncP, accessFuncP)
     dacl_manager_t *		mgrP;
     epi_uuid_t *		mgrUuidP;
     dacl_readwrite_fn_P	readFuncP;
     dacl_readwrite_fn_P	writeFuncP;
     dacl_accesscheck_fn_P	accessFuncP;
{
  long		rtnVal = 0;
  static char	routineName[] = "dacl_mgmt_InitManager";
  
  if ((mgrP != (dacl_manager_t *)NULL) && (mgrUuidP != (epi_uuid_t *)NULL) &&
      (readFuncP != (dacl_readwrite_fn_P)NULL) &&
      (readFuncP != (dacl_readwrite_fn_P)NULL) &&
      (accessFuncP != (dacl_accesscheck_fn_P)NULL)) {

    mgrP->managerUuid = *mgrUuidP;
    mgrP->readFunction = readFuncP;
    mgrP->writeFunction = writeFuncP;
    mgrP->accessFunction = accessFuncP;
  }
  else {
    if (mgrP == (dacl_manager_t *)NULL) {
      dacl_LogParamError(routineName, "mgrP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (mgrUuidP == (epi_uuid_t *)NULL) {
      dacl_LogParamError(routineName, "mgrUuidP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (readFuncP == (dacl_readwrite_fn_P)NULL) {
      dacl_LogParamError(routineName, "readFuncP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (writeFuncP == (dacl_readwrite_fn_P)NULL) {
      dacl_LogParamError(routineName, "writeFuncP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (accessFuncP == (dacl_accesscheck_fn_P)NULL) {
      dacl_LogParamError(routineName, "accessFuncP", dacl_debug_flag, __FILE__, __LINE__);
    }

    rtnVal = DACL_ERROR_PARAMETER_ERROR;
  }
  
  return rtnVal;
}

#ifdef notdef
EXPORT long dacl_mgmt_InitSimpleManager(mgrP, mgrUuidP)
     dacl_manager_t *	mgrP;
     epi_uuid_t *	mgrUuidP;
{
  long		rtnVal = 0;

  rtnVal = dacl_mgmt_InitManager(mgrP, mgrUuidP,
				 dacl_mgmt_SimpleRead, dacl_mgmt_SimpleWrite,
				 dacl_CheckAccessId);
  
  return rtnVal;
}
#endif /* notdef */











