/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_mgruuids.c,v 65.4 1998/04/01 14:16:14 gwehrman Exp $";
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
 * $Log: dacl_mgruuids.c,v $
 * Revision 65.4  1998/04/01 14:16:14  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added includes for sys/types.h and sys/systm.h.
 *
 * Revision 65.3  1998/03/23 16:36:30  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:00:34  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:16  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.12.1  1996/10/02  18:15:44  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:40  damon]
 *
 * Revision 1.1.7.2  1994/07/13  22:26:13  devsrc
 * 	merged with bl-10
 * 	[1994/06/28  21:11:05  devsrc]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  16:01:16  mbs]
 * 
 * Revision 1.1.7.1  1994/06/09  14:18:45  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:30:46  annie]
 * 
 * Revision 1.1.3.3  1993/01/21  15:17:17  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  15:53:54  cjd]
 * 
 * Revision 1.1.3.2  1992/11/24  18:28:51  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:20:59  bolinger]
 * 
 * Revision 1.1  1992/01/19  02:52:46  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/*
 *	dacl_mgruuids.c -- the routines that keep track of properties of relevant
 * ACL managers
 *
 * Copyright (C) 1991 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

#include <epi_id.h>

#include <dacl_mgruuids.h>
#ifdef SGIMIPS
#include <sys/types.h>
#include <sys/systm.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_mgruuids.c,v 65.4 1998/04/01 14:16:14 gwehrman Exp $")

EXPORT epi_uuid_t episodeAclMgmtUuid = {
  /* d076c532-0a1d-11ca-953d-02608c2ea96e */
  0xd076c532,
  0x0a1d,
  0x11ca,
  {0x95, 0x3d, 0x02, 0x60, 0x2e, 0xa9, 0x6e, 0x00}
};

EXPORT epi_uuid_t bosserverAclMgmtUuid = {
  /* 09a9ec46-0a1c-11ca-83a5-02608c2ea96e */
  0x09a9ec46,
  0x0a1c,
  0x11ca,
  {0x83, 0xa5, 0x02, 0x60, 0x2e, 0xa9, 0x6e, 0x00}
};


EXPORT int dacl_AreObjectEntriesRequired(mgrUuidP)
     epi_uuid_t *	mgrUuidP;
{
  int		rtnVal = 0;

  if (bcmp((char *)mgrUuidP, (char *)(&episodeAclMgmtUuid),
	   sizeof(epi_uuid_t)) == 0) {
    rtnVal = 1;
  }
  
  return rtnVal;
}

EXPORT int dacl_AreObjectUuidsRequiredOnAccessCheck(mgrUuidP)
     epi_uuid_t *	mgrUuidP;
{
  int		rtnVal = 0;

  if (bcmp((char *)mgrUuidP, (char *)(&episodeAclMgmtUuid),
	   sizeof(epi_uuid_t)) == 0) {
    rtnVal = 1;
  }
  
  return rtnVal;
}

EXPORT int dacl_ArePermBitsRequiredOnAccessCheck(mgrUuidP)
     epi_uuid_t *	mgrUuidP;
{
  int		rtnVal = 0;

  if (bcmp((char *)mgrUuidP, (char *)(&episodeAclMgmtUuid),
	   sizeof(epi_uuid_t)) == 0) {
    rtnVal = 1;
  }
  
  return rtnVal;
}

EXPORT char * dacl_AclMgrName(mgrUuidP)
     epi_uuid_t *	mgrUuidP;
{
  char *	rtnVal;

  if (bcmp((char *)mgrUuidP, (char *)(&episodeAclMgmtUuid),
	   sizeof(epi_uuid_t)) == 0) {
    rtnVal = "episode";
  }
  else if (bcmp((char*)mgrUuidP, (char *)(&bosserverAclMgmtUuid),
		sizeof(epi_uuid_t)) == 0) {
    rtnVal = "bosserver";
  }
  else {
    rtnVal = "(unknown)";
  }
  
  return rtnVal;
}

     
