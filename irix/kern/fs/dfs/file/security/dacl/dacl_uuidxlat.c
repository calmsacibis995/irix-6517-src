/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_uuidxlat.c,v 65.5 1998/04/01 14:16:16 gwehrman Exp $";
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
 * $Log: dacl_uuidxlat.c,v $
 * Revision 65.5  1998/04/01 14:16:16  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Changed function definitons to ansi style.
 *
 * Revision 65.4  1998/03/23 16:36:20  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/11/06 20:00:32  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:17  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:20:18  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.654.1  1996/10/02  20:56:18  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:56  damon]
 *
 * Revision 1.1.649.3  1994/07/13  22:26:00  devsrc
 * 	merged with bl-10
 * 	[1994/06/28  21:07:54  devsrc]
 * 
 * 	Merged with changes from 1.1.652.2
 * 	[1994/06/07  19:33:21  delgado]
 * 
 * 	delegation ACL support
 * 	[1994/06/07  14:05:14  delgado]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  16:01:29  mbs]
 * 
 * Revision 1.1.649.2  1994/06/09  14:19:00  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:31:01  annie]
 * 
 * Revision 1.1.649.1  1994/02/04  20:29:04  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:17:36  devsrc]
 * 
 * Revision 1.1.647.1  1993/12/07  17:32:57  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:49:45  jaffe]
 * 
 * Revision 1.1.5.4  1993/01/21  15:17:59  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  15:55:08  cjd]
 * 
 * Revision 1.1.5.3  1992/11/24  18:30:00  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:22:13  bolinger]
 * 
 * Revision 1.1.5.2  1992/08/31  21:03:27  jaffe
 * 	Replace missing RCS ids
 * 	[1992/08/31  15:45:48  jaffe]
 * 
 * 	Transarc delta: bab-ot4447-security-dacl-remove-ifdef-notdef 1.1
 * 	  Selected comments:
 * 	    Removed #ifdef notdefs from code.
 * 	    ot 4447
 * 	    Removed code covered by #ifdef notdef.
 * 	[1992/08/30  12:20:59  jaffe]
 * 
 * Revision 1.1.2.4  1992/07/07  21:57:36  mason
 * 	Transarc delta: bab-ot4457-security-dacl2acl-more-files 1.1
 * 	  Selected comments:
 * 	    More routines need to be moved into the dacl2acl library to
 * 	    support the new acl_edit work.
 * 	    ot 4457
 * 	    See above.
 * 	[1992/07/07  21:35:44  mason]
 * 
 * Revision 1.1.2.3  1992/01/28  03:51:39  treff
 * 	1/26/92 DCT @ OSF
 * 	Get rid of extra arg to free()
 * 	[1992/01/28  03:51:10  treff]
 * 
 * Revision 1.1.2.2  1992/01/22  19:58:07  zeliff
 * 	dce.77c file overlay
 * 	[1992/01/22  19:16:19  zeliff]
 * 
 * $EndLog$
 */
/*
 *	dacl_uuidxlat.c -- routines to translate incoming uuid into whatever is used
 * by Episode
 *
 * Copyright (C) 1991 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <dcedfs/icl.h>

#include <epi_id.h>
#include <dacl.h>
#include <dacl_debug.h>
#include <dacl_trace.h>

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */


extern struct icl_set *dacl_iclSetp;
  
RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_uuidxlat.c,v 65.5 1998/04/01 14:16:16 gwehrman Exp $")

#ifdef SGIMIPS
PRIVATE long dacl_ExtendedInfoUuidTranslate(
     char **	srcPP,
     int *	bytesLeftP,
     char **	dstPP)
#else
PRIVATE long dacl_ExtendedInfoUuidTranslate(srcPP, bytesLeftP, dstPP)
     char **	srcPP;
     int *	bytesLeftP;
     char **	dstPP;
#endif
{
  long		rtnVal = DACL_SUCCESS;
  u_int32	numberBytes;
  u_int32 *	srcLongP;
  u_int32 *	dstLongP;
  u_int32 *	alignedP;
  static char	routineName[] = "dacl_ExtendedInfoUuidTranslate";
  
  dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	   ("%s: entered, bytes in buffer: %d",
	    routineName, *bytesLeftP));
  icl_Trace1(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_0 , ICL_TYPE_LONG, *bytesLeftP);

  if (*bytesLeftP >= (sizeof(epi_uuid_t) + sizeof(dacl_format_label_t) +
		      sizeof(u_int32))) {
    bcopy(*srcPP, *dstPP, sizeof(epi_uuid_t));
    *srcPP += sizeof(epi_uuid_t);
    *dstPP += sizeof(epi_uuid_t);
    *bytesLeftP -= sizeof(epi_uuid_t);

    bcopy(*srcPP, *dstPP, sizeof(dacl_format_label_t));
    *srcPP += sizeof(dacl_format_label_t);
    *dstPP += sizeof(dacl_format_label_t);
    *bytesLeftP -= sizeof(dacl_format_label_t);

    /* copy the number of bytes in the extended entry */
    srcLongP = osi_Align(u_int32 *, *srcPP);
    dstLongP = osi_Align(u_int32 *, *dstPP);
    if ((srcLongP == (u_int32 *)*srcPP) && (dstLongP == (u_int32 *)*dstPP)) {
      numberBytes = *dstLongP++ = *srcLongP++;
      *bytesLeftP -= ((char *)srcLongP - *srcPP);
      *srcPP = (char *)srcLongP;
      *dstPP = (char *)dstLongP;
    }
    else {
      dprintf(1, ("%s: Warning: unexpected pointer misalignment", routineName));
      icl_Trace0(dacl_iclSetp, DACL_ICL_UUIDXLAT_NONE_0 );
      
      bcopy(*srcPP, *dstPP, sizeof(u_int32));
      bcopy(*srcPP, (char *)(&numberBytes), sizeof(u_int32));
      *srcPP += sizeof(u_int32);
      *dstPP += sizeof(u_int32);
      *bytesLeftP -= sizeof(u_int32);
    }

    if (*bytesLeftP >= numberBytes) {
      bcopy(*srcPP, *dstPP, numberBytes);
      *srcPP += numberBytes;
      *dstPP += numberBytes;
      *bytesLeftP -= numberBytes;

      /* realign the pointers */
      alignedP = osi_Align(u_int32 *, *srcPP);
      *bytesLeftP -= ((char *)alignedP - *srcPP);
      *srcPP = (char *)alignedP;
      
      alignedP = osi_Align(u_int32 *, *dstPP);
      *dstPP = (char *)alignedP;
    }
    else {
      dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	       ("%s: Error: not enough bytes for extended entry data (%d needed, %d left)",
		routineName, numberBytes, *bytesLeftP));
      icl_Trace2(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_1 , ICL_TYPE_LONG, numberBytes, ICL_TYPE_LONG, *bytesLeftP);
      rtnVal = DACL_ERROR_TOO_FEW_BYTES;
    }
  }
  else {
    dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	     ("%s: Error: not enough bytes for extended entry header (%d needed, %d left)",
	      routineName,
	      sizeof(epi_uuid_t) + sizeof(dacl_format_label_t) + sizeof(u_int32),
	      *bytesLeftP));
    icl_Trace2(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_2 , ICL_TYPE_LONG, sizeof(epi_uuid_t)+sizeof(dacl_format_label_t)+sizeof(u_int32), ICL_TYPE_LONG, *bytesLeftP);
    rtnVal = DACL_ERROR_TOO_FEW_BYTES;
  }
  
  dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	   ("%s: exiting, returning %#x, bytes left: %d",
	    routineName, rtnVal, *bytesLeftP));
  icl_Trace2(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_3 , ICL_TYPE_LONG, rtnVal, ICL_TYPE_LONG, *bytesLeftP);
  return rtnVal;
}

#ifdef SGIMIPS
PRIVATE long dacl_AclEntryUuidTranslate(
     char **	srcPP,
     int *	bytesLeftP,
     char **	dstPP)
#else
PRIVATE long dacl_AclEntryUuidTranslate(srcPP, bytesLeftP, dstPP)
     char **	srcPP;
     int *	bytesLeftP;
     char **	dstPP;
#endif
{
  long			rtnVal = DACL_SUCCESS;
  dacl_entry_type_t	entryType;
  u_int32 *		srcLongP;
  u_int32 *		dstLongP;
  static char		routineName[] = "dacl_AclEntryUuidTranslate";
  
  dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	   ("%s: entered, bytes left: %d", routineName, *bytesLeftP));
  icl_Trace1(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_4 , ICL_TYPE_LONG, *bytesLeftP);
  
  if (*bytesLeftP >= (2 * sizeof(u_int32))) {
    /* copy the permset and the entry type */
    srcLongP = osi_Align(u_int32 *, *srcPP);
    dstLongP = osi_Align(u_int32 *, *dstPP);
    if ((srcLongP == (u_int32 *)*srcPP) && (dstLongP == (u_int32 *)*dstPP)) {
      /*
       * Note: assumes that buffers are properly aligned and that the above sizes are
       * identical with the sizes for longs (u_int32's)
       */
      *dstLongP++ = *srcLongP++;	/* copy entry permset, point to entry type */
      entryType = *srcLongP;	/* hang onto a copy of the entry type */
      *dstLongP++ = *srcLongP++;	/* copy entry type, point to ?? */
      *bytesLeftP -= ((char *)srcLongP - *srcPP);
      *srcPP = (char *)srcLongP;
      *dstPP = (char *)dstLongP;
    }
    else {
      dprintf(1, ("%s: Warning: unexpected pointer misalignment", routineName));
      icl_Trace0(dacl_iclSetp, DACL_ICL_UUIDXLAT_NONE_1 );
      
      bcopy(*srcPP, *dstPP, (sizeof(dacl_permset_t) + sizeof(dacl_entry_type_t)));
      *srcPP += sizeof(dacl_permset_t);
      bcopy(*srcPP, (char *)&entryType, sizeof(u_int32));	/* save the entry type */
      *srcPP += sizeof(u_int32);
      *dstPP += (sizeof(dacl_permset_t) + sizeof(dacl_entry_type_t));
      *bytesLeftP -= (sizeof(dacl_permset_t) + sizeof(dacl_entry_type_t));
    }
    
    if ((entryType == dacl_entry_type_user) ||
	(entryType == dacl_entry_type_group) ||
        (entryType == dacl_entry_type_user_delegate)||
        (entryType == dacl_entry_type_group_delegate)||
        (entryType == dacl_entry_type_foreign_other_delegate)||
	(entryType == dacl_entry_type_foreign_other)) {
      if (*bytesLeftP >= sizeof(epi_uuid_t)) {
	Epi_PrinId_FromUuid((epi_principal_id_t *)(*dstPP), (epi_uuid_t *)(*srcPP));
	*srcPP += sizeof(epi_uuid_t);
	*dstPP += sizeof(epi_principal_id_t);
	*bytesLeftP -= sizeof(epi_uuid_t);
      }
      else {
	dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	 ("%s: Error: not enough bytes for single epi_uuid_t (%d needed, %d left)",
		  routineName, sizeof(epi_uuid_t), *bytesLeftP));
	icl_Trace2(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_5, ICL_TYPE_LONG, 
		   sizeof(epi_uuid_t), ICL_TYPE_LONG, *bytesLeftP);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
      }
    }
    else if ((entryType == dacl_entry_type_foreign_user) ||
             (entryType == dacl_entry_type_foreign_user_delegate) ||
             (entryType == dacl_entry_type_foreign_group_delegate) ||
	     (entryType == dacl_entry_type_foreign_group)) {
      if (*bytesLeftP >= (2 * sizeof(epi_uuid_t))) {
	Epi_PrinId_FromUuid((epi_principal_id_t *)(*dstPP), (epi_uuid_t *)(*srcPP));
	*srcPP += sizeof(epi_uuid_t);
	*dstPP += sizeof(epi_uuid_t);
	*bytesLeftP -= sizeof(epi_uuid_t);
	
	Epi_PrinId_FromUuid((epi_principal_id_t *)(*dstPP), (epi_uuid_t *)(*srcPP));
	*srcPP += sizeof(epi_uuid_t);
	*dstPP += sizeof(epi_uuid_t);
	*bytesLeftP -= sizeof(epi_uuid_t);
      }
      else {
	dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	 ("%s: Error: not enough bytes for two epi_uuid_ts (%d needed, %d left)",
		  routineName, 2 * sizeof(epi_uuid_t), *bytesLeftP));
	icl_Trace2(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_6, ICL_TYPE_LONG, 2*sizeof(epi_uuid_t),
		   ICL_TYPE_LONG, *bytesLeftP);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
      }
    }
    else if (entryType == dacl_entry_type_extended) {
      rtnVal = dacl_ExtendedInfoUuidTranslate(srcPP, bytesLeftP, dstPP);
    }
  }
  else {
    dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	     ("%s: Error: not enough bytes for entry header (%d needed, %d left)",
	      routineName,
	      sizeof(dacl_permset_t) + sizeof(dacl_entry_type_t),
	      *bytesLeftP));
    icl_Trace2(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_7, ICL_TYPE_LONG,
	       sizeof(dacl_permset_t)+sizeof(dacl_entry_type_t),
	       ICL_TYPE_LONG, *bytesLeftP);
    rtnVal = DACL_ERROR_TOO_FEW_BYTES;
  }
  
  dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	   ("%s: exiting, returning %#x, bytes left: %d",
	    routineName, rtnVal, *bytesLeftP));
  icl_Trace2(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_8, ICL_TYPE_LONG, rtnVal, ICL_TYPE_LONG, *bytesLeftP);

  return rtnVal;
}

#ifdef SGIMIPS
EXPORT long dacl_AclUuidTranslate(
     char *		incomingBufferP,
     unsigned int	bytesInBuffer,
     char **		translatedBufferPP)
#else
EXPORT long dacl_AclUuidTranslate(incomingBufferP, bytesInBuffer, translatedBufferPP)
     char *		incomingBufferP;
     unsigned int	bytesInBuffer;
     char **		translatedBufferPP;
#endif
{
  long		rtnVal = DACL_SUCCESS;
  char *	srcP = incomingBufferP;
  char *	dstP;
  int		bytesLeft = bytesInBuffer;
  u_int32	numberEntries;
  int		i;
  char *	tempTranslationP;
  static char	routineName[] = "dacl_BufferUuidTranslate";
  
  dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_TRANSLATE,
	   ("%s entered, bytes in buffer: %d", routineName, bytesInBuffer));
  icl_Trace1(dacl_iclSetp, DACL_ICL_UUIDXLAT_TRANSLATE_9 , ICL_TYPE_LONG, bytesInBuffer);

  if (sizeof(epi_principal_id_t) == sizeof(epi_uuid_t)) {
    /* there is nothing to do, everything is already the right size */
    *translatedBufferPP = incomingBufferP;
  }
  else {
    /* the amount of real data will be smaller in the translated buffer */
    tempTranslationP = dacl_Alloc(bytesInBuffer);
    if (tempTranslationP != (char *)NULL) {
      dstP = tempTranslationP;
      /*
       * first, copy the manager type tag and default realm, which do not vary in size,
       * along with the number of entries
       */
      if (bytesLeft >= ((2 * sizeof(epi_uuid_t)) + sizeof(u_int32))) {
	bcopy(srcP, dstP, (2 * sizeof(epi_uuid_t)) + sizeof(u_int32));
	srcP += (2 * sizeof(epi_uuid_t));
	numberEntries = *((u_int32 *)srcP);	/* save the number of entries */
	srcP += sizeof(u_int32);
	dstP += (2 * sizeof(epi_uuid_t)) + sizeof(u_int32);
	bytesLeft -= (2 * sizeof(epi_uuid_t)) + sizeof(u_int32);
	
	/* now, translate each entry */
	for (i = 0; (rtnVal == DACL_SUCCESS) && (i < numberEntries); i++) {
	  rtnVal = dacl_AclEntryUuidTranslate(&srcP, &bytesLeft, &dstP);
	}
      }
      else {
	dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_TRANSLATE,
		("%s: Error: not enough bytes for acl header (%d needed, %d left)",
		 routineName, (2 * sizeof(epi_uuid_t) + sizeof(u_int32)), bytesLeft));
	icl_Trace2(dacl_iclSetp, DACL_ICL_UUIDXLAT_TRANSLATE_10, ICL_TYPE_LONG, (2*sizeof(epi_uuid_t)+sizeof(u_int32)), ICL_TYPE_LONG, bytesLeft);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
      }
    }
    else {
      dprintf(dacl_debug_flag,
	      ("%s: Error: unable to allocate %d bytes of buffer space",
	       routineName, bytesInBuffer));
      icl_Trace1(dacl_iclSetp, DACL_ICL_UUIDXLAT_NONE_2 , ICL_TYPE_LONG, bytesInBuffer);
      rtnVal = DACL_ERROR_BUFFER_ALLOCATION;
    }

    if ((rtnVal == DACL_SUCCESS) && (bytesLeft != 0)) {
      dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_TRANSLATE,
	       ("%s: Error: %d bytes left-over after acl parse", routineName, bytesLeft));
      icl_Trace1(dacl_iclSetp, DACL_ICL_UUIDXLAT_TRANSLATE_11 , ICL_TYPE_LONG, bytesLeft);
      rtnVal = DACL_ERROR_TOO_MANY_BYTES;
      /*
       * We should be at the end of the file here; zero should be returned if everything
       * is as expect it to be.
       */
    }
    
    if (rtnVal == DACL_SUCCESS) {
      *translatedBufferPP = tempTranslationP;
    }
    else {
      dacl_Free(tempTranslationP, bytesInBuffer);
      *translatedBufferPP = (char *)NULL;
    }
  }
  
  dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_PARSE,
	   ("%s: exiting, returning %#x, bytes left: %d",
	    routineName, rtnVal, bytesLeft));
  icl_Trace2(dacl_iclSetp, DACL_ICL_UUIDXLAT_PARSE_12 , ICL_TYPE_LONG, rtnVal, ICL_TYPE_LONG, bytesLeft);

  return rtnVal;
}

