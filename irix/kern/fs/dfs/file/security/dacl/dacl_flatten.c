/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_flatten.c,v 65.5 1998/04/01 14:16:13 gwehrman Exp $";
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
 * $Log: dacl_flatten.c,v $
 * Revision 65.5  1998/04/01 14:16:13  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Changed function definitons to ansi style.
 *
 * Revision 65.4  1998/03/23 16:36:32  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/11/06 20:00:34  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:15  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:20:15  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.539.1  1996/10/02  18:15:37  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:31  damon]
 *
 * Revision 1.1.534.4  1994/08/23  18:58:56  rsarbo
 * 	delegation
 * 	[1994/08/23  16:32:06  rsarbo]
 * 
 * Revision 1.1.534.3  1994/07/13  22:25:48  devsrc
 * 	merged with bl-10
 * 	[1994/06/28  21:07:43  devsrc]
 * 
 * 	Merged with changes from 1.1.537.3
 * 	[1994/06/07  19:25:25  delgado]
 * 
 * 	delegation ACL support
 * 	[1994/06/07  14:04:56  delgado]
 * 
 * 	Fixed the problem with the merge of the RCSID string.
 * 	[1994/04/29  19:05:08  mbs]
 * 
 * 	Merged with changes from 1.1.537.1
 * 	[1994/04/28  16:27:33  mbs]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  16:01:11  mbs]
 * 
 * 	remove redundant dprintfs and dmprintfs
 * 	[1994/04/21  14:03:43  delgado]
 * 
 * Revision 1.1.534.2  1994/06/09  14:18:36  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:30:37  annie]
 * 
 * Revision 1.1.534.1  1994/02/04  20:28:28  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:17:22  devsrc]
 * 
 * Revision 1.1.532.1  1993/12/07  17:32:38  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:45:20  jaffe]
 * 
 * Revision 1.1.4.7  1993/01/21  15:17:02  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  15:53:23  cjd]
 * 
 * Revision 1.1.4.6  1993/01/13  19:14:48  shl
 * 	Transarc delta: jaffe-cleanup-2.3-compiler-warnings 1.8
 * 	  Selected comments:
 * 	    Fix compiler warnings found when moving to OSF/1 1.1.1.
 * 	    apply fixes as per beth's instruction.
 * 	    Remove the function dacl_SetOwnerControl.  It is not used.
 * 	[1993/01/12  21:55:55  shl]
 * 
 * Revision 1.1.4.5  1992/12/09  20:37:12  jaffe
 * 	Transarc delta: bwl-ot5793-dummy-acl-i-and-d 1.3
 * 	  Selected comments:
 * 	    Creating a dummy ACL for a given mode, anyone that gets w access to a
 * 	    directory should also get i and d access.
 * 	    dacl_FlattenAclWithModeBits calls dacl_ChmodAcl, which has new specs.
 * 	    Missed a caller of dacl_ChmodAcl.
 * 	    Missed a caller of dacl_InitEpiAcl.
 * 	[1992/12/04  18:21:05  jaffe]
 * 
 * Revision 1.1.4.4  1992/11/24  18:28:05  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:20:29  bolinger]
 * 
 * Revision 1.1.4.3  1992/09/25  19:00:43  jaffe
 * 	Transarc delta: bab-ot4873-dacl-flatten-correct-mode-incorporation 1.1
 * 	  Selected comments:
 * 	    The acl flatten code was confused about when and where to incorporate passed
 * 	    mode bits into the flattened acl buffer.
 * 	    ot 4873
 * 	    See above.
 * 	    Also fixed the incorporation of the mode bits into the flattened acl buffer.
 * 	[1992/09/23  19:33:12  jaffe]
 * 
 * Revision 1.1.4.2  1992/08/31  21:01:53  jaffe
 * 	Replace missing RCS ids
 * 	[1992/08/31  15:45:00  jaffe]
 * 
 * 	Transarc delta: bab-ot4447-security-dacl-remove-ifdef-notdef 1.1
 * 	  Selected comments:
 * 	    Removed #ifdef notdefs from code.
 * 	    ot 4447
 * 	    Removed code covered by #ifdef notdef.
 * 	[1992/08/30  12:18:13  jaffe]
 * 
 * Revision 1.1.2.6  1992/07/07  21:57:04  mason
 * 	Transarc delta: bab-ot4457-security-dacl2acl-more-files 1.1
 * 	  Selected comments:
 * 	    More routines need to be moved into the dacl2acl library to
 * 	    support the new acl_edit work.
 * 	    ot 4457
 * 	    See above.
 * 	[1992/07/07  21:33:06  mason]
 * 
 * Revision 1.1.2.5  1992/04/21  14:30:02  mason
 * 	Transarc delta: cburnett-ot2713-security_changes_for_aix32 1.1
 * 	  Files modified:
 * 	    security/dacl: dacl_flatten.c, dacl_parse.c, epi_id.c
 * 	  Selected comments:
 * 	    changes to build on AIX 3.2
 * 	    include osi_net.h instead of netinet.h
 * 	[1992/04/20  22:57:58  mason]
 * 
 * Revision 1.1.2.4  1992/04/14  03:33:53  mason
 * 	Transarc delta: bab-flatten-side-effect-local2559 1.2
 * 	  Files modified:
 * 	    security/dacl: dacl_flatten.c
 * 	  Selected comments:
 * 	    local bug #2559: dacl_FlattenAclWithModeBits should not have side-effects
 * 	    on passed ACL pointer parameter.  Insert the passed mode bits (if desired)
 * 	    into the flattened ACL buffer.
 * 	    ot 2272
 * 	[1992/04/13  19:22:34  mason]
 * 
 * Revision 1.1.2.3  1992/01/28  03:54:30  treff
 * 	1/26/92 DCT @ OSF
 * 	Get rid of extra arg to free()
 * 	[1992/01/28  03:53:48  treff]
 * 
 * Revision 1.1.2.2  1992/01/22  19:57:34  zeliff
 * 	dce.77c file overlay
 * 	[1992/01/22  19:15:50  zeliff]
 * 
 * $EndLog$
 */
/*
 *	dacl_flatten.c -- linearize an ACL
 *
 * Copyright (C) 1991 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_net.h>
#include <dcedfs/icl.h>
#include <sys/types.h>

#include <dcedfs/fshs_deleg.h>
#include <dacl.h>
#include <dacl_debug.h>
#include <epi_id.h>
#include <dacl_trace.h>

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */

extern struct icl_set *dacl_iclSetp;

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_flatten.c,v 65.5 1998/04/01 14:16:13 gwehrman Exp $")

#define DACL_NO_MODE_BITS	((u_int32)0)

/* the osi alignment macro will also round up a regular size appropriately */
#define dacl_AlignedBytes(subject_type)  (osi_Align(int, sizeof(subject_type)))

#ifdef SGIMIPS
PRIVATE unsigned int dacl_SizeOfFlatList(
     unsigned int	numEntries,
     dacl_entry_t *	entriesP,
     int		flattenForDisk)
#else
PRIVATE unsigned int dacl_SizeOfFlatList(numEntries, entriesP, flattenForDisk)
     unsigned int	numEntries;
     dacl_entry_t *	entriesP;
     int		flattenForDisk;
#endif
{
  unsigned int		rtnVal = 0;
  int			i;
  dacl_entry_t *	thisEntryP;
  long			alignedNumberBytes;
  int			destPrinIdSize = ((flattenForDisk != 0) ?
					  sizeof(epi_principal_id_t) : sizeof(epi_uuid_t));
  static char		routineName[] = "dacl_SizeOfFlatList";

  if (entriesP != (dacl_entry_t *)NULL) {
    for (i = 0; i < numEntries; i++) {
      thisEntryP = &(entriesP[i]);

      /* compute size for fixed portion */
      rtnVal = osi_Align(int, rtnVal);	/* we try to copy permset aligned */
      rtnVal += sizeof(/* perms */ dacl_permset_t);
      rtnVal = osi_Align(int, rtnVal);	/* we try to copy entry type aligned */
      rtnVal += sizeof(/* entry_type */ dacl_entry_type_t);

      icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_0 , ICL_TYPE_LONG, rtnVal, ICL_TYPE_LONG, i);
      
      if ((thisEntryP->entry_type == dacl_entry_type_user) ||
	  (thisEntryP->entry_type == dacl_entry_type_group)||
          (thisEntryP->entry_type == dacl_entry_type_user_delegate) ||
          (thisEntryP->entry_type == dacl_entry_type_group_delegate)) {
	rtnVal += destPrinIdSize;
      }
      else if ((thisEntryP->entry_type == dacl_entry_type_foreign_other) ||
               (thisEntryP->entry_type == dacl_entry_type_foreign_other_delegate)) {
	rtnVal += sizeof(epi_uuid_t);
      }
      else if ((thisEntryP->entry_type == dacl_entry_type_foreign_user) ||
	       (thisEntryP->entry_type == dacl_entry_type_foreign_group) ||
               (thisEntryP->entry_type == dacl_entry_type_foreign_user_delegate)||
               (thisEntryP->entry_type == dacl_entry_type_foreign_group_delegate)) {
	rtnVal += (destPrinIdSize + sizeof(epi_uuid_t));
      }
      else if (thisEntryP->entry_type == dacl_entry_type_extended) {
	/*
	 * The number of bytes taken by the data will be rounded up to realign the
	 * pointer to a four-byte boundary.
	 */
	alignedNumberBytes = osi_Align(int,
				       thisEntryP->entry_info.extended_info.numberBytes);
	rtnVal += (sizeof(epi_uuid_t) + sizeof(dacl_format_label_t));
	rtnVal = osi_Align(int, rtnVal); /* we try to copy number of bytes aligned */
	rtnVal += (sizeof(u_int32) + alignedNumberBytes);
      }
      icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_1 , ICL_TYPE_LONG, rtnVal, ICL_TYPE_LONG, i);
    }
  }
  
  return rtnVal;
}

#ifdef SGIMIPS
PRIVATE unsigned int dacl_SizeOfFlatAcl(
     dacl_t *	aclP,
     int	flattenForDisk)
#else
PRIVATE unsigned int dacl_SizeOfFlatAcl(aclP, flattenForDisk)
     dacl_t *	aclP;
     int	flattenForDisk;
#endif
{
  unsigned int		rtnVal = 0;
  int			i;
  int			numbSimpleEntries = 0;
  static char		routineName[] = "dacl_SizeOfFlatAcl";
  
  icl_Trace0(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_2 );
  
  if (aclP != (dacl_t *)NULL) {
    rtnVal = (2 * sizeof(/* mgr_type_tag & default_realm */ epi_uuid_t));
    rtnVal = osi_Align(int, rtnVal);	/* we try to copy the number of entries aligned */
    rtnVal += sizeof(/* num_entries */ u_int32);
    
    icl_Trace1(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_3 , ICL_TYPE_LONG, rtnVal);

    /* first, count the space we need for the simple entries */
    for (i = 0; i <= (int)dacl_simple_entry_type_unauthmask; i++) {
      numbSimpleEntries += aclP->simple_entries[i].is_entry_good;
    }
    rtnVal += (numbSimpleEntries * osi_Align(int, (sizeof(dacl_permset_t) +
						   sizeof(dacl_entry_type_t))));
    
    icl_Trace1(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_4 , ICL_TYPE_LONG, rtnVal);

    /* now, count the space for the complex entries */
    for (i = 0; i <= (int)dacl_complex_entry_type_other; i++) {
      rtnVal += dacl_SizeOfFlatList(aclP->complex_entries[i].num_entries,
				    aclP->complex_entries[i].complex_entries,
				    flattenForDisk);
    }
    
    icl_Trace1(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_5 , ICL_TYPE_LONG, rtnVal);
  }
  else {
    dacl_LogParamError(routineName, "aclP", dacl_debug_flag, __FILE__, __LINE__);
    /* rtnVal comes back as zero, so it's pretty obvious there's some problem */
  }
  
  icl_Trace1(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_6 , ICL_TYPE_LONG, rtnVal);

  return rtnVal;
}

#ifdef SGIMIPS
PRIVATE long dacl_FlattenExtendedInfo(
     char **			bufferPlacePP,
     int *			bytesLeftP,
     dacl_extended_info_t *	xinfoP)
#else
PRIVATE long dacl_FlattenExtendedInfo(bufferPlacePP, bytesLeftP, xinfoP)
     char **			bufferPlacePP;
     int *			bytesLeftP;
     dacl_extended_info_t *	xinfoP;
#endif
{
  int		rtnVal = DACL_SUCCESS;	/* assume success */
  u_int32 *	longP;
  short *	shortP;
  u_int32 *	alignedP;
  static char	routineName[] = "dacl_FlattenExtendedInfo";
  
  icl_Trace1(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_7 , ICL_TYPE_LONG, *bytesLeftP);

  if (*bytesLeftP >= (sizeof(epi_uuid_t) + sizeof(dacl_format_label_t) +
		      sizeof(u_int32))) {
    bcopy((char *)(&(xinfoP->extensionType)), *bufferPlacePP, sizeof(epi_uuid_t));
    hton_epi_uuid((epi_uuid_t *)*bufferPlacePP);
    *bufferPlacePP += sizeof(epi_uuid_t); *bytesLeftP -= sizeof(epi_uuid_t);

    shortP = (short *)*bufferPlacePP;
    *shortP++ = htons(xinfoP->formatLabel.miscShorts[0]);
    *shortP++ = htons(xinfoP->formatLabel.miscShorts[1]);
    *shortP++ = htons(xinfoP->formatLabel.miscShorts[2]);
    *((char *)shortP) = xinfoP->formatLabel.charField; 
    *bufferPlacePP += sizeof(dacl_format_label_t);
    *bytesLeftP -= sizeof(dacl_format_label_t);

    longP = osi_Align(u_int32 *, *bufferPlacePP);
    if (longP == (u_int32 *)*bufferPlacePP) {
      *longP++ = htonl(xinfoP->numberBytes);
      *bytesLeftP -= ((char *)longP - *bufferPlacePP);
      *bufferPlacePP = (char *)longP;
    }
    else {
      icl_Trace0(dacl_iclSetp, DACL_ICL_FLATTEN_NONE_0 );

      bcopy((char *)&(xinfoP->numberBytes), *bufferPlacePP, sizeof(u_int32));
      **((long **)bufferPlacePP) = htonl(**((long **)bufferPlacePP));
      *bufferPlacePP += sizeof(u_int32); *bytesLeftP -= sizeof(u_int32);
    }

    if (*bytesLeftP >= xinfoP->numberBytes) {
      bcopy(xinfoP->infoP, *bufferPlacePP, xinfoP->numberBytes);
      *bufferPlacePP += xinfoP->numberBytes; *bytesLeftP -= (xinfoP->numberBytes);

      /* realign the pointer */
      alignedP = osi_Align(u_int32 *, *bufferPlacePP);
      *bytesLeftP -= ((char *)alignedP - *bufferPlacePP);
      *bufferPlacePP = (char *)alignedP;
    }
    else {
      icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_8 , ICL_TYPE_LONG, xinfoP->numberBytes, ICL_TYPE_LONG, *bytesLeftP);
      rtnVal = DACL_ERROR_TOO_FEW_BYTES;
    }
  }
  else {
    icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_9 , ICL_TYPE_LONG, sizeof(epi_uuid_t)+sizeof(dacl_format_label_t)+sizeof(u_int32), ICL_TYPE_LONG, *bytesLeftP);
    rtnVal = DACL_ERROR_TOO_FEW_BYTES;
  }
  icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_10 , ICL_TYPE_LONG, rtnVal, ICL_TYPE_LONG, *bytesLeftP);
  return rtnVal;
}

#ifdef SGIMIPS
PRIVATE long dacl_FlattenAclEntry(
     char **		bufferPlacePP,
     int *		bytesLeftP,
     dacl_entry_t *	aclEntryP,
     int		flattenForDisk)
#else
PRIVATE long dacl_FlattenAclEntry(bufferPlacePP, bytesLeftP, aclEntryP, flattenForDisk)
     char **		bufferPlacePP;
     int *		bytesLeftP;
     dacl_entry_t *	aclEntryP;
     int		flattenForDisk;
#endif
{
  long		rtnVal = DACL_SUCCESS;	/* assume success */
  u_int32 *	longP;
  int		destPrinIdSize = ((flattenForDisk != 0) ?
				  sizeof(epi_principal_id_t) : sizeof(epi_uuid_t));
  static char	routineName[] = "dacl_FlattenAclEntry";
  
  icl_Trace1(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_11 , ICL_TYPE_LONG, *bytesLeftP);
  
  if (*bytesLeftP >= (2 * sizeof(u_int32))) {
    longP = osi_Align(u_int32 *, *bufferPlacePP);
    if (longP == (u_int32 *)*bufferPlacePP) {
      /* copy the permset and the entry type */
      /*
       * Note: assumes that buffers are properly aligned and that the above sizes are
       * identical with the sizes for longs (u_int32's)
       */
      *longP++ = htonl(aclEntryP->perms);
      *longP++ = htonl(aclEntryP->entry_type);
      *bytesLeftP -= ((char *)longP - *bufferPlacePP);
      *bufferPlacePP = (char *)longP;
    }
    else {
      icl_Trace0(dacl_iclSetp, DACL_ICL_FLATTEN_NONE_1 );

      /* now, copy in the permset */
      bcopy((char *)(&(aclEntryP->perms)), *bufferPlacePP, sizeof(dacl_permset_t));
      **((long **)bufferPlacePP) = htonl(**((long **)bufferPlacePP));
      *bufferPlacePP += sizeof(dacl_permset_t);
      *bytesLeftP -= sizeof(dacl_permset_t);
      
      /* now, copy the entry type */
      bcopy((char *)(&(aclEntryP->entry_type)), *bufferPlacePP, sizeof(dacl_entry_type_t));
      **((long **)bufferPlacePP) = htonl(**((long **)bufferPlacePP));
      *bufferPlacePP += sizeof(dacl_entry_type_t);
      *bytesLeftP -= sizeof(dacl_entry_type_t);
    }

    /*
     * now, decide how much more we flatten into the buffer, dependent upon
     * what type of entry this is
     */
    if ((aclEntryP->entry_type == dacl_entry_type_user) ||
	(aclEntryP->entry_type == dacl_entry_type_group) ||
        (aclEntryP->entry_type == dacl_entry_type_user_delegate) ||
        (aclEntryP->entry_type == dacl_entry_type_group_delegate)) {
      if (*bytesLeftP >= destPrinIdSize) {
	if (flattenForDisk) {
	  Epi_PrinId_FromUuid((epi_principal_id_t *)(*bufferPlacePP),
			      &(aclEntryP->entry_info.id));
	}
	else {
	  bcopy((char *)(&(aclEntryP->entry_info.id)), *bufferPlacePP, sizeof(epi_uuid_t));
	}
	
	hton_epi_principal_id((epi_principal_id_t *)*bufferPlacePP);
	*bufferPlacePP += destPrinIdSize;
	*bytesLeftP -= destPrinIdSize;
      }
      else {
	icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_12 , ICL_TYPE_LONG, destPrinIdSize, ICL_TYPE_LONG, *bytesLeftP);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
      }
    }
    else if ((aclEntryP->entry_type == dacl_entry_type_foreign_other) ||
             (aclEntryP->entry_type == dacl_entry_type_foreign_other_delegate)) {
      if (*bytesLeftP >= sizeof(epi_uuid_t)) {
	bcopy((char *)(&(aclEntryP->entry_info.id)), *bufferPlacePP, sizeof(epi_uuid_t));
	
	hton_epi_uuid((epi_uuid_t *)*bufferPlacePP);
	*bufferPlacePP += sizeof(epi_uuid_t);
	*bytesLeftP -= sizeof(epi_uuid_t);
      }
      else {
	icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_13 , ICL_TYPE_LONG, sizeof(epi_uuid_t)+sizeof(dacl_format_label_t)+sizeof(u_int32), ICL_TYPE_LONG, *bytesLeftP);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
      }
    }
    else if ((aclEntryP->entry_type == dacl_entry_type_foreign_user) ||
	     (aclEntryP->entry_type == dacl_entry_type_foreign_group) ||
             (aclEntryP->entry_type == dacl_entry_type_foreign_user_delegate)||
             (aclEntryP->entry_type == dacl_entry_type_foreign_group_delegate)) {
      if (*bytesLeftP >= (destPrinIdSize + sizeof(epi_uuid_t))) {
	/* first, copy the principal id */
	if (flattenForDisk) {
	  Epi_PrinId_FromUuid((epi_principal_id_t *)(*bufferPlacePP),
			      &(aclEntryP->entry_info.foreign_id.id));
	}
	else {
	  bcopy((char *)(&(aclEntryP->entry_info.foreign_id.id)), *bufferPlacePP,
		sizeof(epi_uuid_t));
	}
	hton_epi_principal_id((epi_principal_id_t *)*bufferPlacePP);
	*bufferPlacePP += destPrinIdSize;
	
	/* now, copy the realm uuid */
	bcopy((char *)(&(aclEntryP->entry_info.foreign_id.realm)), *bufferPlacePP,
	      sizeof(epi_uuid_t));
	hton_epi_uuid((epi_uuid_t *)*bufferPlacePP);
	*bufferPlacePP += sizeof(epi_uuid_t);

	*bytesLeftP -= (destPrinIdSize + sizeof(epi_uuid_t));
      }
      else {
	icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_14 , 
		   ICL_TYPE_LONG, destPrinIdSize + sizeof(epi_uuid_t), ICL_TYPE_LONG, *bytesLeftP);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
      }
    }
    else if (aclEntryP->entry_type == dacl_entry_type_extended) {
      rtnVal = dacl_FlattenExtendedInfo(bufferPlacePP, bytesLeftP,
					&(aclEntryP->entry_info.extended_info));
    }
  }    
  else {
    icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_15 , 
	       ICL_TYPE_LONG, sizeof(dacl_permset_t)+sizeof(dacl_entry_type_t),
	       ICL_TYPE_LONG, *bytesLeftP);
    rtnVal = DACL_ERROR_TOO_FEW_BYTES;
  }
  icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_16 , ICL_TYPE_LONG, rtnVal, ICL_TYPE_LONG, *bytesLeftP);
  return rtnVal;
}

#ifdef SGIMIPS
EXPORT long dacl_FlattenAclWithModeBits(
     dacl_t *		aclP,
     char **		fileBufferPP,
     unsigned int *	bytesInBufferP,
     u_int32		modeBits,
     int		useModeBits,
     int		flattenForDisk)
#else
EXPORT long dacl_FlattenAclWithModeBits(aclP, fileBufferPP, bytesInBufferP, modeBits,
					useModeBits, flattenForDisk)
     dacl_t *		aclP;
     char **		fileBufferPP;
     unsigned int *	bytesInBufferP;
     u_int32		modeBits;
     int		useModeBits;
     int		flattenForDisk;
#endif
{
  long			rtnVal = DACL_SUCCESS;
  int			userPassedBuffer = 0;	/* not normal case */
  char *		bufferPlaceP;
  u_int32 *		longP;
  int			bytesLeft = 0;
  int			entryCounter;
  int			listCounter;
  dacl_entry_t		tempSimpleEntry;
  int			numberInThisList;
  dacl_entry_t *	thisListP;
  static char		routineName[] = "dacl_FlattenAclWithModeBits";
  
  icl_Trace0(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_17 );

  if ((aclP != (dacl_t *)NULL) && (fileBufferPP != (char **)NULL) &&
      (bytesInBufferP != (unsigned int *)NULL)) {
    bytesLeft = dacl_SizeOfFlatAcl(aclP, flattenForDisk);

    icl_Trace1(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_18 , ICL_TYPE_LONG, bytesLeft);

    if (*fileBufferPP == (char *)NULL) {
      *fileBufferPP = bufferPlaceP = (char *)dacl_Alloc(bytesLeft);
    }
    else {
      /* the caller has passed in the buffer to be used */
      userPassedBuffer = 1;

      if (*bytesInBufferP >= bytesLeft) {
	/* there is enough space in the buffer in this case */
	bufferPlaceP = *fileBufferPP;
      }
      else {
	/* error condition */
	icl_Trace0(dacl_iclSetp, DACL_ICL_FLATTEN_WARNINGS_19 );

	*bytesInBufferP = bytesLeft;	/* tell the caller how much we really need */
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
      }
    }
    
    if (rtnVal == DACL_SUCCESS) {
      *bytesInBufferP = bytesLeft;
      
      if ((*fileBufferPP != (char *)NULL) &&
	  (bytesLeft >= ((2 * sizeof(epi_uuid_t)) + sizeof(u_int32)))) {
	bcopy((char *)(&(aclP->mgr_type_tag)), bufferPlaceP, sizeof(epi_uuid_t));
	hton_epi_uuid((epi_uuid_t *)bufferPlaceP);
	bufferPlaceP += sizeof(epi_uuid_t); bytesLeft -= sizeof(epi_uuid_t);
	
	bcopy((char *)(&(aclP->default_realm)), bufferPlaceP, sizeof(epi_uuid_t));
	hton_epi_uuid((epi_uuid_t *)bufferPlaceP);
	bufferPlaceP += sizeof(epi_uuid_t); bytesLeft -= sizeof(epi_uuid_t);
	
	longP = osi_Align(u_int32 *, bufferPlaceP);
	if (longP == (u_int32 *)bufferPlaceP) {
	  *longP++ = htonl(aclP->num_entries);
	  bytesLeft -= ((char *)longP - bufferPlaceP);
	  bufferPlaceP = (char *)longP;
	}
	else {
	  icl_Trace0(dacl_iclSetp, DACL_ICL_FLATTEN_NONE_2 );
	  
	  bcopy((char *)(&(aclP->num_entries)), bufferPlaceP, sizeof(u_int32));
	  *((long *)bufferPlaceP) = htonl(*((long *)bufferPlaceP));
	  bufferPlaceP += sizeof(u_int32); bytesLeft -= sizeof(u_int32);
	}
	
	/* first, flatten the simple entries */
	for (entryCounter = 0;
	     ((rtnVal == DACL_SUCCESS) &&
	      (entryCounter <= (int)dacl_simple_entry_type_unauthmask));
	     entryCounter++) {
	  if (aclP->simple_entries[entryCounter].is_entry_good == 1) {
	    /* map the simple entry type as approp */
	    switch ((int)entryCounter) {
	    case (int)dacl_simple_entry_type_userobj:
	      tempSimpleEntry.entry_type = dacl_entry_type_user_obj;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
	      if (useModeBits) {
		dacl_ChmodOnePermset(modeBits, &(tempSimpleEntry.perms), 0);
	      }
	      break;
	    case (int)dacl_simple_entry_type_groupobj:
	      tempSimpleEntry.entry_type = dacl_entry_type_group_obj;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
	      if (useModeBits &&
		  ! aclP->simple_entries[(int)dacl_simple_entry_type_maskobj].is_entry_good) {
		/* if there isn't a mask obj, put the group mode bits here */
		dacl_ChmodOnePermset(modeBits << 3, &(tempSimpleEntry.perms), 0);
	      }
	      break;
	    case (int)dacl_simple_entry_type_otherobj:
	      tempSimpleEntry.entry_type = dacl_entry_type_other_obj;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
	      if (useModeBits) {
		dacl_ChmodOnePermset(modeBits << 6, &(tempSimpleEntry.perms), 0);
	      }
	      break;
	    case (int)dacl_simple_entry_type_maskobj:
	      tempSimpleEntry.entry_type = dacl_entry_type_mask_obj;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
	      if (useModeBits) {
		/* put the group mode bits in the mask obj entry that we have found */
		dacl_ChmodOnePermset(modeBits << 3, &(tempSimpleEntry.perms), 0);
	      }
	      break;
              /* 
               * I believe that we don't have to perform the ChmodOnePermset 
               * business for the simple delegate entries
               */
            case (int)dacl_simple_entry_type_userobj_delegate:
	      tempSimpleEntry.entry_type = dacl_entry_type_user_obj_delegate;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
              break;
            case (int)dacl_simple_entry_type_groupobj_delegate:
	      tempSimpleEntry.entry_type = dacl_entry_type_group_obj_delegate;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
              break;
            case (int)dacl_simple_entry_type_otherobj_delegate:
	      tempSimpleEntry.entry_type = dacl_entry_type_other_obj_delegate;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
              break;
	    case (int)dacl_simple_entry_type_anyother:
	      tempSimpleEntry.entry_type = dacl_entry_type_anyother;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
	      break;
	    case (int)dacl_simple_entry_type_anyother_delegate:
	      tempSimpleEntry.entry_type = dacl_entry_type_any_other_delegate;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
	      break;
	    case (int)dacl_simple_entry_type_unauthmask:
	      tempSimpleEntry.entry_type = dacl_entry_type_unauth;
	      tempSimpleEntry.perms = aclP->simple_entries[entryCounter].perms;
	      break;
	      /* no default since we know all the values of the outer iteration */
	    }
	    
	    rtnVal = dacl_FlattenAclEntry(&bufferPlaceP, &bytesLeft, &tempSimpleEntry,
					  flattenForDisk);
	  }
	}
	
	/* now, flatten the entries in the lists, in turn */
	for (listCounter = 0;
	     (rtnVal == DACL_SUCCESS) &&
	     (listCounter <= (int)dacl_complex_entry_type_other);
	     listCounter++) {
	  numberInThisList = aclP->complex_entries[listCounter].num_entries;
	  thisListP = aclP->complex_entries[listCounter].complex_entries;
	  
	  for (entryCounter = 0;
	       (rtnVal == DACL_SUCCESS) && (entryCounter < numberInThisList);
	       entryCounter++) {
	    rtnVal = dacl_FlattenAclEntry(&bufferPlaceP, &bytesLeft,
					  &(thisListP[entryCounter]), flattenForDisk);
	  }
	}
      }
      else {
	icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_PARSE_20, 
		   ICL_TYPE_LONG, (2*sizeof(epi_uuid_t))+sizeof(u_int32),
		   ICL_TYPE_LONG, bytesLeft);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
      }
    }
  
    if ((rtnVal != DACL_SUCCESS) && (userPassedBuffer == 0)) {
      /* only do the de-allocation if the allocation was done locally */
      if (*fileBufferPP != (char *)NULL) {
	(void)dacl_Free((opaque)(*fileBufferPP), (long)(*bytesInBufferP));
      }
      
      *fileBufferPP = (char *)NULL;
      *bytesInBufferP = 0;
    }
    
  }    
  else {
    if (aclP == (dacl_t *)NULL) {
      dacl_LogParamError(routineName, "aclP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (fileBufferPP == (char **)NULL) {
      dacl_LogParamError(routineName, "fileBufferPP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (bytesInBufferP == (unsigned int *)NULL) {
      dacl_LogParamError(routineName, "bytesInBufferP", dacl_debug_flag, __FILE__, __LINE__);
    }
    
    rtnVal = DACL_ERROR_PARAMETER_ERROR;
  }

  icl_Trace2(dacl_iclSetp, DACL_ICL_FLATTEN_FLATTEN_21, 
	     ICL_TYPE_LONG, rtnVal, ICL_TYPE_LONG, 
	     ((bytesInBufferP != (unsigned int *)NULL) ? *bytesInBufferP : -1));
  return rtnVal;
}

#ifdef SGIMIPS
EXPORT long dacl_epi_FlattenAcl(
     dacl_t *		aclP,
     char **		fileBufferPP,
     unsigned int *	bytesInBufferP,
     u_int32		modeBits)
#else
EXPORT long dacl_epi_FlattenAcl(aclP, fileBufferPP, bytesInBufferP, modeBits)
     dacl_t *		aclP;
     char **		fileBufferPP;
     unsigned int *	bytesInBufferP;
     u_int32		modeBits;
#endif
{
  return dacl_FlattenAclWithModeBits(aclP, fileBufferPP, bytesInBufferP,
				     modeBits, 1 /* => use mode bits */,
				     0 /* => not for disk */);
}

#ifdef SGIMIPS
EXPORT long dacl_FlattenAcl(
     dacl_t *		aclP,
     char **		fileBufferPP,
     unsigned int *	bytesInBufferP)
#else
EXPORT long dacl_FlattenAcl(aclP, fileBufferPP, bytesInBufferP)
     dacl_t *		aclP;
     char **		fileBufferPP;
     unsigned int *	bytesInBufferP;
#endif
{
  return dacl_FlattenAclWithModeBits(aclP, fileBufferPP, bytesInBufferP,
				     DACL_NO_MODE_BITS, 0 /* => don't use mode bits */,
				     1 /* => flatten for disk */);
}







