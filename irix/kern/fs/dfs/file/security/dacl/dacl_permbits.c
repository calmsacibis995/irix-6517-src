/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_permbits.c,v 65.5 1998/04/01 14:16:16 gwehrman Exp $";
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
 * $Log: dacl_permbits.c,v $
 * Revision 65.5  1998/04/01 14:16:16  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Changed function definitons to ansi style.
 *
 * Revision 65.4  1998/03/23 16:36:43  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/11/06 20:00:38  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:16  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:20:17  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.447.1  1996/10/02  20:56:03  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:45  damon]
 *
 * $EndLog$
 */
/*
 *	dacl_permbits.c -- routines for interchanging between regular UNIX
 * permission bits and ACL permsets...Of course, the bit ordering for permission
 * bits and permsets is gratuitously incorrect for just dropping one into the other
 *
 * Copyright (C) 1991, 1994 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/icl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <dcedfs/fshs_deleg.h>
#include <dacl.h>
#include <dacl_debug.h>
#include <dacl_trace.h>

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */


extern struct icl_set *dacl_iclSetp;

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_permbits.c,v 65.5 1998/04/01 14:16:16 gwehrman Exp $")

#ifdef SGIMIPS
EXPORT void dacl_MaskOnePermsetByPermBits(
     u_int32		permBits,
     dacl_permset_t *	thePermsetP)
#else
EXPORT void dacl_MaskOnePermsetByPermBits(permBits, thePermsetP)
     u_int32		permBits;
     dacl_permset_t *	thePermsetP;
#endif
{
  if ((permBits & S_IREAD) == 0) {
    *thePermsetP &= ~dacl_perm_read;
  }
  if ((permBits & S_IWRITE) == 0) {
    *thePermsetP &= ~dacl_perm_write;
  }
  if ((permBits & S_IEXEC) == 0) {
    *thePermsetP &= ~dacl_perm_execute;
  }
}

#ifdef SGIMIPS
EXPORT void dacl_ChmodOnePermset(
     u_int32		permBits,
     dacl_permset_t *	thePermsetP,
     int		forDirectory)
#else
EXPORT void dacl_ChmodOnePermset(permBits, thePermsetP, forDirectory)
     u_int32		permBits;
     dacl_permset_t *	thePermsetP;
     int		forDirectory;
#endif
{
  *thePermsetP &= ~0x7;		/* clear what's there now */

  if (permBits & S_IREAD) *thePermsetP |= dacl_perm_read;
  if (permBits & S_IWRITE)
    if (forDirectory)
      *thePermsetP |= (dacl_perm_write | dacl_perm_insert | dacl_perm_delete);
    else
      *thePermsetP |= dacl_perm_write;
  if (permBits & S_IEXEC) *thePermsetP |= dacl_perm_execute;
}

#ifdef SGIMIPS
EXPORT long dacl_MaskPermsetsByPermBits(
     u_int32		permBits,
     dacl_permset_t *	userObjPermsetP,
     dacl_permset_t *	groupObjPermsetP,
     dacl_permset_t *	otherObjPermsetP)
#else
EXPORT long dacl_MaskPermsetsByPermBits(permBits, userObjPermsetP, groupObjPermsetP,
					otherObjPermsetP)
     u_int32		permBits;
     dacl_permset_t *	userObjPermsetP;
     dacl_permset_t *	groupObjPermsetP;
     dacl_permset_t *	otherObjPermsetP;
#endif
{
  long		rtnVal = 0;

  if (userObjPermsetP != (dacl_permset_t *)NULL) {
    dacl_MaskOnePermsetByPermBits(permBits, userObjPermsetP);
  }
  
  if (groupObjPermsetP != (dacl_permset_t *)NULL) {
    dacl_MaskOnePermsetByPermBits(permBits << 3, groupObjPermsetP);
  }

  if (otherObjPermsetP != (dacl_permset_t *)NULL) {
    dacl_MaskOnePermsetByPermBits(permBits << 6, otherObjPermsetP);
  }
  
  return rtnVal;
}

#ifdef SGIMIPS
EXPORT void dacl_AddPermBitsToOnePermset(
     u_int32		permBits,
     dacl_permset_t *	thePermsetP)
#else
EXPORT void dacl_AddPermBitsToOnePermset(permBits, thePermsetP)
     u_int32		permBits;
     dacl_permset_t *	thePermsetP;
#endif
{
  if (permBits & S_IREAD) {
    *thePermsetP |= dacl_perm_read;
  }
  if (permBits & S_IWRITE) {
    *thePermsetP |= dacl_perm_write;
  }
  if (permBits & S_IEXEC) {
    *thePermsetP |= dacl_perm_execute;
  }
}

#ifdef SGIMIPS
EXPORT long dacl_AddPermBitsToPermsets(
     u_int32		permBits,
     dacl_permset_t *	userObjPermsetP,
     dacl_permset_t *	groupObjPermsetP,
     dacl_permset_t *	otherObjPermsetP)
#else
EXPORT long dacl_AddPermBitsToPermsets(permBits, userObjPermsetP, groupObjPermsetP,
				       otherObjPermsetP)
     u_int32		permBits;
     dacl_permset_t *	userObjPermsetP;
     dacl_permset_t *	groupObjPermsetP;
     dacl_permset_t *	otherObjPermsetP;
#endif
{
  long		rtnVal = 0;

  if (userObjPermsetP != (dacl_permset_t *)NULL) {
    dacl_AddPermBitsToOnePermset(permBits, userObjPermsetP);
  }
  
  if (groupObjPermsetP != (dacl_permset_t *)NULL) {
    dacl_AddPermBitsToOnePermset(permBits << 3, groupObjPermsetP);
  }

  if (otherObjPermsetP != (dacl_permset_t *)NULL) {
    dacl_AddPermBitsToOnePermset(permBits << 6, otherObjPermsetP);
  }
  
  return rtnVal;
}

#ifdef SGIMIPS
EXPORT void dacl_OnePermsetToPermBits(
     dacl_permset_t	thePermset,
     u_int32 *		permBitsP)
#else
EXPORT void dacl_OnePermsetToPermBits(thePermset, permBitsP)
     dacl_permset_t	thePermset;
     u_int32 *		permBitsP;
#endif
{
  *permBitsP = 0;
  if (thePermset & dacl_perm_read) {
    *permBitsP |= S_IREAD;
  }
  if (thePermset & dacl_perm_write) {
    *permBitsP |= S_IWRITE;
  }
  if (thePermset & dacl_perm_execute) {
    *permBitsP |= S_IEXEC;
  }
}

#ifdef SGIMIPS
EXPORT long dacl_PermsetsToPermBits(
     dacl_permset_t	userObjPermset,
     dacl_permset_t	groupObjPermset,
     dacl_permset_t	otherObjPermset,
     u_int32 *		permBitsP)
#else
EXPORT long dacl_PermsetsToPermBits(userObjPermset, groupObjPermset, otherObjPermset,
				    permBitsP)
     dacl_permset_t	userObjPermset;
     dacl_permset_t	groupObjPermset;
     dacl_permset_t	otherObjPermset;
     u_int32 *		permBitsP;
#endif
{
  long		rtnVal = 0;
  u_int32	tempPermset;
  static char	routineName[] = "dacl_PermsetsToPermBits";

  if (permBitsP != (u_int32 *)NULL) {
    dacl_OnePermsetToPermBits(userObjPermset, permBitsP);
    dacl_OnePermsetToPermBits(groupObjPermset, &tempPermset);
    *permBitsP |= (tempPermset >> 3);
    dacl_OnePermsetToPermBits(otherObjPermset, &tempPermset);
    *permBitsP |= (tempPermset >> 6);
  }
  else {
    dprintf(1, ("%s: Error: required parameter, permBitsP, is NULL", routineName));
    icl_Trace0(dacl_iclSetp, DACL_ICL_PERMBITS_NONE_0);
    rtnVal = DACL_ERROR_PARAMETER_ERROR;
  }
  
  return rtnVal;
}
#if !defined(KERNEL)
#ifdef SGIMIPS
EXPORT long dacl_PermbitString(
     u_int32 *	permBitsP,
     char *	permStringP)
#else
EXPORT long dacl_PermbitString(permBitsP, permStringP)
     u_int32 *	permBitsP;
     char *	permStringP;
#endif
{
  long		rtnVal = DACL_SUCCESS;
  static char	routineName[] = "dacl_PermbitString";

  if ((permBitsP != (u_int32 *)NULL) && (permStringP != (char *)NULL)) {
    /*
     * not very efficient, but it's quick and dirty - we don't expect to use it for
     * anything except testing, anyway
     */
    (void)sprintf(permStringP, "%c%c%c%c%c%c%c%c%c",
		  (*permBitsP & S_IREAD) ? 'r' : '-',
		  (*permBitsP & S_IWRITE) ? 'w' : '-',
		  (*permBitsP & S_IEXEC) ? 'x' : '-',
		  (*permBitsP & (S_IREAD >> 3)) ? 'r' : '-',
		  (*permBitsP & (S_IWRITE >> 3)) ? 'w' : '-',
		  (*permBitsP & (S_IEXEC >> 3)) ? 'x' : '-',
		  (*permBitsP & (S_IREAD >> 6)) ? 'r' : '-',
		  (*permBitsP & (S_IWRITE >> 6)) ? 'w' : '-',
		  (*permBitsP & (S_IEXEC >> 6)) ? 'x' : '-'
		  );
  }
  else {
    if (permBitsP == (u_int32 *)NULL) {
      dacl_LogParamError(routineName, "permBitsP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (permStringP == (char *)NULL) {
      dacl_LogParamError(routineName, "permStringP", dacl_debug_flag, __FILE__, __LINE__);
    }
    rtnVal = DACL_ERROR_PARAMETER_ERROR;
  }
  
  return rtnVal;
}

#ifdef SGIMIPS
EXPORT long dacl_PermbitsFromString(
     char *	permStringP,
     u_int32 *	permBitsP)
#else
EXPORT long dacl_PermbitsFromString(permStringP, permBitsP)
     char *	permStringP;
     u_int32 *	permBitsP;
#endif
{
  long		rtnVal = DACL_SUCCESS;
  static char	routineName[] = "dacl_PermbitsFromString";
  
  if ((permBitsP != (u_int32 *)NULL) && (permStringP != (char *)NULL)) {
  }
  else {
    if (permBitsP == (u_int32 *)NULL) {
      dacl_LogParamError(routineName, "permBitsP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (permStringP == (char *)NULL) {
      dacl_LogParamError(routineName, "permStringP", dacl_debug_flag, __FILE__, __LINE__);
    }
    rtnVal = DACL_ERROR_PARAMETER_ERROR;
  }
}
#endif /* !defined(KERNEL) */
