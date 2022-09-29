/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_parse.c,v 65.5 1998/04/01 14:16:15 gwehrman Exp $";
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
 * $Log: dacl_parse.c,v $
 * Revision 65.5  1998/04/01 14:16:15  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Changed function definitons to ansi style.
 *
 * Revision 65.4  1998/03/23 16:36:38  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/11/06 20:00:37  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:16  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:20:17  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.746.1  1996/10/02  20:53:52  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:44  damon]
 *
 * $EndLog$
 */

/*
 * dacl_parse.c -- reads an acl from a buffer containing the acl's bytes
 *
 * Copyright (C) 1996, 1991 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_net.h>
#include <dcedfs/stds.h>
#include <dcedfs/icl.h>

#include <sys/types.h>

#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
#include <dce/uuid.h>
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
#include <dcedfs/fshs_deleg.h>
#include <dacl.h>
#include <dacl_debug.h>
#include <dacl_trace.h>
#include <epi_id.h>

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */

extern struct icl_set *dacl_iclSetp;
  
RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_parse.c,v 65.5 1998/04/01 14:16:15 gwehrman Exp $")

#ifdef SGIMIPS
static long dacl_ParseExtendedInfo(
  char **fileBufferPP,
  int *bytesLeftP,
  dacl_extended_info_t *xinfoP)
#else
static long dacl_ParseExtendedInfo(fileBufferPP, bytesLeftP, xinfoP)
  char **fileBufferPP;
  int *bytesLeftP;
  dacl_extended_info_t *xinfoP;
#endif
{
    long rtnVal = DACL_SUCCESS;	/* assume success */
    u_int32 *longP;
    u_int32 *alignedP;
    static char	routineName[] = "dacl_ParseExtendedInfo";
    
    icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_PARSE_0, ICL_TYPE_LONG,
	       *bytesLeftP);
    
    if (*bytesLeftP >= (sizeof(epi_uuid_t) + sizeof(dacl_format_label_t) +
			sizeof(u_int32))) {
	bcopy(*fileBufferPP, (char *)(&(xinfoP->extensionType)),
	      sizeof(epi_uuid_t));
	*fileBufferPP += sizeof(epi_uuid_t); *bytesLeftP -= sizeof(epi_uuid_t);
	ntoh_epi_uuid(&(xinfoP->extensionType));
	
	bcopy(*fileBufferPP, (char *)&(xinfoP->formatLabel),
	      sizeof(dacl_format_label_t));
	*fileBufferPP += sizeof(dacl_format_label_t);
	*bytesLeftP -= sizeof(dacl_format_label_t);
	xinfoP->formatLabel.miscShorts[0] =
	    ntohs(xinfoP->formatLabel.miscShorts[0]);
	xinfoP->formatLabel.miscShorts[1] =
	    ntohs(xinfoP->formatLabel.miscShorts[1]);
	xinfoP->formatLabel.miscShorts[2] =
	    ntohs(xinfoP->formatLabel.miscShorts[2]);
	
	longP = osi_Align(u_int32 *, *fileBufferPP);
	if (longP == (u_int32 *)*fileBufferPP) {
	    xinfoP->numberBytes = *longP++;
	    *bytesLeftP -= ((char *)longP - *fileBufferPP);
	    *fileBufferPP = (char *)longP;
	} else {
	    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_NONE_0);
	    bcopy(*fileBufferPP, (char *)&(xinfoP->numberBytes),
		  sizeof(u_int32));
	    *fileBufferPP += sizeof(u_int32); *bytesLeftP -= sizeof(u_int32);
	}
	xinfoP->numberBytes = ntohl(xinfoP->numberBytes);
	
	if (*bytesLeftP >= xinfoP->numberBytes) {
	    xinfoP->infoP = (char *)dacl_Alloc(xinfoP->numberBytes);
	    if (xinfoP->infoP != (char *)NULL) {
		bcopy(*fileBufferPP, xinfoP->infoP, xinfoP->numberBytes);
		*fileBufferPP += xinfoP->numberBytes;
		*bytesLeftP -= (xinfoP->numberBytes);
		
		/* realign the pointer */
		alignedP = osi_Align(u_int32 *, *fileBufferPP);
		*bytesLeftP -= ((char *)alignedP - *fileBufferPP);
		*fileBufferPP = (char *)alignedP;
	    } else {
		icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_NONE_1 , ICL_TYPE_LONG,
			   xinfoP->numberBytes);
		xinfoP->numberBytes = 0;
		rtnVal = DACL_ERROR_BUFFER_ALLOCATION;
	    }
	} else {
	    icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_1 , ICL_TYPE_LONG,
		       xinfoP->numberBytes, ICL_TYPE_LONG, *bytesLeftP);
	    xinfoP->numberBytes = 0;
	    xinfoP->infoP = (char *)NULL;
	    rtnVal = DACL_ERROR_TOO_FEW_BYTES;
	}
    } else {
	icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_2 , ICL_TYPE_LONG,
		   sizeof(epi_uuid_t) + sizeof(dacl_format_label_t) +
		   sizeof(u_int32), 
		   ICL_TYPE_LONG, *bytesLeftP);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
    }
    
    icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_3 , ICL_TYPE_LONG, rtnVal,
	       ICL_TYPE_LONG, *bytesLeftP);
    return rtnVal;
}

#ifdef SGIMIPS
static long dacl_ParseAclEntry(
  char **fileBufferPP,
  int *bytesLeftP,
  dacl_t *aclBufferP,
  int parseFromDisk)
#else
static long dacl_ParseAclEntry(fileBufferPP, bytesLeftP, aclBufferP,
			       parseFromDisk)
  char **fileBufferPP;
  int *bytesLeftP;
  dacl_t *aclBufferP;
  int parseFromDisk;
#endif
{
    long  rtnVal = DACL_SUCCESS;	/* assume success */
    u_int32 *longP;
    int	intEntryType;
    dacl_entry_t aclEntry;		/* the scratch area */
    dacl_entry_t *aclEntryP = (dacl_entry_t *)NULL;
    dacl_simple_entry_t *aclSimpleEntryP = (dacl_simple_entry_t *)NULL;
    u_int32 *numEntriesP = (u_int32 *)NULL;
    int	srcPrinIdSize = ((parseFromDisk != 0) ?
			 sizeof(epi_principal_id_t) : sizeof(epi_uuid_t));
    static char routineName[] = "dacl_ParseAclEntry";
    
#define DACL_SET_DELEGATE_ENTRY_FLAG(daclP) \
    (daclP)->flags |= DACL_DELEGATE_ENTRY

    icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_PARSE_4 , ICL_TYPE_LONG,
	       *bytesLeftP);
    
    /*
     * First, we parse the header of the next entry into a local entry buffer,
     * then we decide where to leave the entry data on exit.
     */
    
    if (*bytesLeftP >= (2 * sizeof(u_int32))) {
	longP = osi_Align(u_int32 *, *fileBufferPP);
	if (longP == (u_int32 *)*fileBufferPP) {
	    /* Copy the permset and the entry type 
	     * Note: assumes that buffers are properly aligned and that the 
	     * above sizes are identical with the sizes for longs (u_int32's)
	     */
	    aclEntry.perms = *longP++;
	    aclEntry.entry_type = *longP++;
	    *bytesLeftP -= ((char *)longP - *fileBufferPP);
	    *fileBufferPP = (char *)longP;
	} else {
	    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_NONE_2);
	    
	    /* now, copy in the permset */
	    bcopy(*fileBufferPP, (char *)&(aclEntry.perms), 
		  sizeof(dacl_permset_t));
	    *fileBufferPP += sizeof(dacl_permset_t);
	    *bytesLeftP -= sizeof(dacl_permset_t);

	    /* now, parse the entry type */
	    bcopy(*fileBufferPP, (char *)&(aclEntry.entry_type), 
		  sizeof(dacl_entry_type_t));
	    *fileBufferPP += sizeof(dacl_entry_type_t);
	    *bytesLeftP -= sizeof(dacl_entry_type_t);
	}
	aclEntry.perms = ntohl(aclEntry.perms);
	aclEntry.entry_type = ntohl(aclEntry.entry_type);
	
	intEntryType = aclEntry.entry_type;
	if ((long)intEntryType == aclEntry.entry_type) {
	    switch (intEntryType) {
		/* for the simple entries, we already have all the info we
		 * need */
	      case (int)dacl_entry_type_user_obj:
		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_5);
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_userobj]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_group_obj:
		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_6);
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_groupobj]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_other_obj:
		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_7);
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_otherobj]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_mask_obj:
		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_8);
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_maskobj]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_anyother:
		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_9);
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_anyother]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_unauth:
		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_10);
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_unauthmask]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_user_obj_delegate:
    		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_userobj_delegate]);
		    DACL_SET_DELEGATE_ENTRY_FLAG(aclBufferP);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_group_obj_delegate:
		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_groupobj_delegate]);
		    DACL_SET_DELEGATE_ENTRY_FLAG(aclBufferP);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_other_obj_delegate:
		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_otherobj_delegate]);
		    DACL_SET_DELEGATE_ENTRY_FLAG(aclBufferP);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_any_other_delegate:
		if (aclSimpleEntryP == (dacl_simple_entry_t *)NULL) {
		    aclSimpleEntryP =
			&(aclBufferP->simple_entries[(int)dacl_simple_entry_type_anyother_delegate]);
		    DACL_SET_DELEGATE_ENTRY_FLAG(aclBufferP);
		}
		
		if (aclSimpleEntryP->is_entry_good == 0) {
		    aclSimpleEntryP->is_entry_good = 1;
		    aclSimpleEntryP->perms = aclEntry.perms;
		} else {
		    icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_PARSE_11, 
			       ICL_TYPE_LONG, aclEntry.entry_type);
		    rtnVal = DACL_ERROR_DUPLICATE_ENTRY_FOUND;
		}
		
		break;
		
		/* we need more info for the complex entries */
	      case (int)dacl_entry_type_user:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_12);
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_user].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_user].complex_entries[*numEntriesP]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_group:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_13);
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_group].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_group].complex_entries[*numEntriesP]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_user_delegate:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_user_delegate].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_user_delegate].complex_entries[*numEntriesP]);
		    DACL_SET_DELEGATE_ENTRY_FLAG(aclBufferP);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_group_delegate:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_group_delegate].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_group_delegate].complex_entries[*numEntriesP]);
		    DACL_SET_DELEGATE_ENTRY_FLAG(aclBufferP);
		}
		
		/* now, we pull the info out of the character buffer */
		if (*bytesLeftP >= srcPrinIdSize) {
		    *aclEntryP = aclEntry; /*copy the previously parsed info */
		    
		    /* now, get the id out of the buffer */
		    if (parseFromDisk) {
			Epi_PrinId_ToUuid((epi_principal_id_t *)
					  (*fileBufferPP),
					  &(aclEntryP->entry_info.id));
		    } else {
			bcopy((char *)*fileBufferPP, 
			      (char *)(&(aclEntryP->entry_info.id)),
			      sizeof(epi_uuid_t));
		    }
		    
		    *fileBufferPP += srcPrinIdSize;
		    *bytesLeftP -= srcPrinIdSize;
		    
		    ntoh_epi_uuid(&(aclEntryP->entry_info.id));
		} else {
		    icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_14,
			       ICL_TYPE_LONG, srcPrinIdSize, ICL_TYPE_LONG,
			       *bytesLeftP);
		    rtnVal = DACL_ERROR_TOO_FEW_BYTES;
		}
		
		if (rtnVal == DACL_SUCCESS) {
		    (*numEntriesP)++;
		}
		
		break;
		
	      case (int)dacl_entry_type_foreign_other:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_15);
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_other].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_other].complex_entries[*numEntriesP]);
		}
		/* intentional fall through */
	      case (int)dacl_entry_type_foreign_other_delegate:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_for_other_delegate].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_for_other_delegate].complex_entries[*numEntriesP]);
		    DACL_SET_DELEGATE_ENTRY_FLAG(aclBufferP);
		}
		
		/* now, we pull the info out of the character buffer */
		if (*bytesLeftP >= sizeof(epi_uuid_t)) {
		    *aclEntryP = aclEntry; /*copy the previously parsed info */
		    
		    /* now, get the id out of the buffer */
		    bcopy(*fileBufferPP, (char *)(&(aclEntryP->entry_info.id)),
			  sizeof(epi_uuid_t));
		    *fileBufferPP += sizeof(epi_uuid_t);
		    *bytesLeftP -= sizeof(epi_uuid_t);
		    
		    ntoh_epi_uuid(&(aclEntryP->entry_info.id));
		} else {
		    icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_16, 
			       ICL_TYPE_LONG, sizeof(epi_uuid_t),
			       ICL_TYPE_LONG, *bytesLeftP);
		    rtnVal = DACL_ERROR_TOO_FEW_BYTES;
		}
		
		if (rtnVal == DACL_SUCCESS) {
		    (*numEntriesP)++;
		}
		
		break;
		
	      case (int)dacl_entry_type_foreign_user:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_17);
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_user].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_user].complex_entries[*numEntriesP]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_foreign_group:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_18);
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_group].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_group].complex_entries[*numEntriesP]);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_foreign_user_delegate:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_user_delegate].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_user_delegate].complex_entries[*numEntriesP]);
		    DACL_SET_DELEGATE_ENTRY_FLAG(aclBufferP);
		}
		/* fall through to next case is intentional */
	      case (int)dacl_entry_type_foreign_group_delegate:
		if (aclEntryP == (dacl_entry_t *)NULL) {
		    numEntriesP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_group_delegate].num_entries);
		    aclEntryP =
			&(aclBufferP->complex_entries[(int)dacl_complex_entry_type_group_delegate].complex_entries[*numEntriesP]);
		    DACL_SET_DELEGATE_ENTRY_FLAG(aclBufferP);
		}

		/* now, we pull the info out of the character buffer */
		if (*bytesLeftP >= (srcPrinIdSize + sizeof(epi_uuid_t))) {
		    *aclEntryP = aclEntry; /*copy the previously parsed info */
		    
		    if (parseFromDisk) {
			Epi_PrinId_ToUuid((epi_principal_id_t *)
					  (*fileBufferPP),
					  &(aclEntryP->entry_info.foreign_id.id));
		    } else {
			bcopy(*fileBufferPP,
			      (char *)(&(aclEntryP->entry_info.foreign_id.id)),
			      sizeof(epi_uuid_t));
		    }
		    
		    *fileBufferPP += srcPrinIdSize;
		    
		    /* now, parse out the realm id */
		    bcopy(*fileBufferPP,
			  (char *)(&(aclEntryP->entry_info.foreign_id.realm)),
			  sizeof(epi_uuid_t));
		    *fileBufferPP += sizeof(epi_uuid_t);
		    
		    *bytesLeftP -= (srcPrinIdSize + sizeof(epi_uuid_t));
		    
		    ntoh_epi_uuid(&(aclEntryP->entry_info.foreign_id.realm));
		    ntoh_epi_uuid(&(aclEntryP->entry_info.foreign_id.id));
		} else {
		    icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_19 , 
			       ICL_TYPE_LONG,
			       srcPrinIdSize + sizeof(epi_uuid_t),
			       ICL_TYPE_LONG, *bytesLeftP);
		    rtnVal = DACL_ERROR_TOO_FEW_BYTES;
		}
		
		if (rtnVal == DACL_SUCCESS) {
		    (*numEntriesP)++;
		}
		
		break;
		
	      case (int)dacl_entry_type_extended:
		/* in this case, we can tell the parsing routine directly 
		 * where to put it */
		icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_20);
		numEntriesP =
		    &(aclBufferP->complex_entries[(int)dacl_complex_entry_type_other].num_entries);
		aclEntryP =
		    &(aclBufferP->complex_entries[(int)dacl_complex_entry_type_other].complex_entries[*numEntriesP]);
		*aclEntryP = aclEntry; /* copy the previously parsed info */
		
		rtnVal = dacl_ParseExtendedInfo(fileBufferPP, bytesLeftP,
						&(aclEntryP->entry_info.extended_info));
		if (rtnVal == DACL_SUCCESS) {
		    (*numEntriesP)++;
		}
		
		break;
		
	      default:
		/* some sort of unrecognized type logging */
		rtnVal = DACL_ERROR_UNRECOGNIZED_ENTRY_TYPE;
		break;
	    }	/* end switch */
	} else {
	    icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_NONE_3, ICL_TYPE_LONG,
		       aclEntry.entry_type);
	    rtnVal = DACL_ERROR_ENTRY_TYPE_TOO_LARGE;
	}
    } else {
	icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_21, 
		   ICL_TYPE_LONG,
		   sizeof(dacl_permset_t) + sizeof(dacl_entry_type_t),
		   ICL_TYPE_LONG, *bytesLeftP);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
    }
    
    icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_22,
	       ICL_TYPE_LONG, rtnVal, ICL_TYPE_LONG, *bytesLeftP);
    
    return rtnVal;
}

#ifdef SGIMIPS
static long dacl_CheckObjEntries(
  dacl_t *aclP)
#else
static long dacl_CheckObjEntries(aclP)
  dacl_t *aclP;
#endif
{
    long rtnVal = 0;
    static char	routineName[] = "dacl_CheckObjPtrsFromAcl";
    
    if (aclP != (dacl_t *)NULL) {
	if (dacl_AreObjectEntriesRequired(&(aclP->mgr_type_tag)) == 1) {
	    if (aclP->simple_entries[(int)dacl_simple_entry_type_userobj].is_entry_good == 0) {
		icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_NONE_4);
		rtnVal = DACL_ERROR_REQUIRED_ENTRY_MISSING;
	    }
	    if (aclP->simple_entries[(int)dacl_simple_entry_type_groupobj].is_entry_good == 0) {
		icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_NONE_5);
		rtnVal = DACL_ERROR_REQUIRED_ENTRY_MISSING;
	    }
	    if (aclP->simple_entries[(int)dacl_simple_entry_type_otherobj].is_entry_good == 0) {
		icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_NONE_6);
		rtnVal = DACL_ERROR_REQUIRED_ENTRY_MISSING;
	    }
	}
    } else {
	rtnVal = DACL_ERROR_PARAMETER_ERROR;
    }
    
    return rtnVal;
}

#ifdef SGIMIPS
EXPORT long dacl_ParseAclDiskOption(
  char *fileBufferP,
  int bytesInBuffer,
  dacl_t *aclBufferP,
  epi_uuid_t *mgrUuidP,
  int parseFromDisk)
#else
EXPORT long dacl_ParseAclDiskOption(fileBufferP, bytesInBuffer, aclBufferP,
				    mgrUuidP, parseFromDisk)
  char *fileBufferP;
  int bytesInBuffer;
  dacl_t *aclBufferP;
  epi_uuid_t *mgrUuidP;
  int parseFromDisk;
#endif
{
    long rtnVal = DACL_SUCCESS;	/* assume success */
    int	entryCounter;
    u_int32 *longP;
    int i;
    int	bytesLeft = bytesInBuffer;
#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
    unsigned_char_p_t uuidString;
    unsigned32 uuidStatus;
#endif  /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
    dacl_entry_t *daclEntries;
    static char	routineName[] = "dacl_ParseAclDiskOption";
    
    icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_PARSE_23, 
	       ICL_TYPE_LONG, bytesInBuffer);
    
    /* read the first three parts of the acl */
    if (bytesLeft >= ((2 * sizeof(epi_uuid_t)) + sizeof(u_int32))) {
	bcopy(fileBufferP, (char *)(&(aclBufferP->mgr_type_tag)),
	      sizeof(epi_uuid_t));
	fileBufferP += sizeof(epi_uuid_t);
	bytesLeft -= sizeof(epi_uuid_t);
	ntoh_epi_uuid(&(aclBufferP->mgr_type_tag));
	
	if (bcmp((char *)mgrUuidP, (char *)&(aclBufferP->mgr_type_tag),
		 sizeof(epi_uuid_t)) == 0) {
	    bcopy(fileBufferP, (char *)(&(aclBufferP->default_realm)), 
		  sizeof(epi_uuid_t));
	    fileBufferP += sizeof(epi_uuid_t); 
	    bytesLeft -= sizeof(epi_uuid_t);
	    ntoh_epi_uuid(&(aclBufferP->default_realm));
	    
	    longP = osi_Align(u_int32 *, fileBufferP);
	    if (longP == (u_int32 *)fileBufferP) {
		aclBufferP->num_entries = *longP++;
		bytesLeft -= ((char *)longP - fileBufferP);
		fileBufferP = (char *)longP;
	    } else {
		icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_NONE_7);
		
		bcopy(fileBufferP, (char *)(&(aclBufferP->num_entries)),
		      sizeof(u_int32));
		fileBufferP += sizeof(u_int32); 
		bytesLeft -= sizeof(u_int32);
	    }
	    aclBufferP->num_entries = ntohl(aclBufferP->num_entries);
	    
	    for (i = 0; i <= (int)dacl_simple_entry_type_unauthmask; i++) {
		aclBufferP->simple_entries[i].is_entry_good = 0;
	    }
	    
	    for (i = 0;
		 (rtnVal == DACL_SUCCESS) &&
		 (i <= (int)dacl_complex_entry_type_other);
		 i++) {
		/* XXX a bit wasteful of space, but safe */
		aclBufferP->complex_entries[i].num_entries = 0;
		aclBufferP->complex_entries[i].entries_allocated =
		    aclBufferP->num_entries;
		aclBufferP->complex_entries[i].complex_entries =
		    (dacl_entry_t *)dacl_Alloc(aclBufferP->num_entries *
					       sizeof(dacl_entry_t));
		if (aclBufferP->complex_entries[i].complex_entries ==
		    (dacl_entry_t *)NULL) {
		    icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_NONE_8,
			       ICL_TYPE_LONG,
			       aclBufferP->num_entries*sizeof(dacl_entry_t),
			       ICL_TYPE_LONG, i);
		    rtnVal = DACL_ERROR_BUFFER_ALLOCATION;
		}
	    }
	    
	    if (rtnVal == DACL_SUCCESS) {
		/* now, parse each entry in turn */
		for (entryCounter = 0;
		     (rtnVal == DACL_SUCCESS) &&
		     (entryCounter < aclBufferP->num_entries);
		     entryCounter++) {
		    rtnVal = dacl_ParseAclEntry(&fileBufferP, &bytesLeft,
						aclBufferP, parseFromDisk);
		}
		
		/* 
		 * Now that we know how much memory we need in each 
		 * array give back unused memory 
		 */	
		for (i = 0; i <= (int)dacl_complex_entry_type_other; i++) {
		    if (aclBufferP->complex_entries[i].num_entries) {
			daclEntries = (dacl_entry_t *)dacl_Alloc(
			    aclBufferP->complex_entries[i].num_entries *
			    sizeof(dacl_entry_t));
			bcopy((char *)
			      aclBufferP->complex_entries[i].complex_entries,
			      (char *)daclEntries,  
			      aclBufferP->complex_entries[i].num_entries * 
			      sizeof(dacl_entry_t));
		    } else {
			daclEntries = 0;
		    }
		    dacl_Free(aclBufferP->complex_entries[i].complex_entries,
		              aclBufferP->complex_entries[i].entries_allocated
			      * sizeof(dacl_entry_t));
		    aclBufferP->complex_entries[i].complex_entries = 
			daclEntries;
		    daclEntries = 0; /* hygiene */
		    aclBufferP->complex_entries[i].entries_allocated =
			aclBufferP->complex_entries[i].num_entries;
		}
	    }
	    
	    if (rtnVal == DACL_SUCCESS) {
		rtnVal = dacl_CheckObjEntries(aclBufferP);
	    }
	} else {
	    icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_24);
	    
#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
	    uuid_to_string((uuid_t *)mgrUuidP, &uuidString, &uuidStatus);
	    if (uuidStatus == error_status_ok) {
		icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_PARSE_25,
			   ICL_TYPE_STRING, uuidString);
		rpc_string_free(&uuidString, &uuidStatus);
	    } else {
		icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_26);
	    }
	    
	    uuid_to_string((uuid_t *)&(aclBufferP->mgr_type_tag), &uuidString,
			   &uuidStatus);
	    if (uuidStatus == error_status_ok) {
		icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_PARSE_27,
			   ICL_TYPE_STRING, uuidString);
		rpc_string_free(&uuidString, &uuidStatus);
	    } else {
		icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_28);
	    }
	    
	    uuid_to_string((uuid_t *)&(aclBufferP->default_realm), &uuidString,
			   &uuidStatus);
	    if (uuidStatus == error_status_ok) {
		icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_PARSE_29,
			   ICL_TYPE_STRING, uuidString);
		rpc_string_free(&uuidString, &uuidStatus);
	    } else {
		icl_Trace0(dacl_iclSetp, DACL_ICL_PARSE_PARSE_30);
	    }
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
	    
	    rtnVal = DACL_ERROR_INCORRECT_MGR_UUID;
	}
    } else {
	icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_31, 
		   ICL_TYPE_LONG, (2*sizeof(epi_uuid_t))+sizeof(u_int32),
		   ICL_TYPE_LONG, bytesLeft);
	rtnVal = DACL_ERROR_TOO_FEW_BYTES;
    }
    
    if ((rtnVal == DACL_SUCCESS) && (bytesLeft != 0)) {
	icl_Trace1(dacl_iclSetp, DACL_ICL_PARSE_PARSE_32, ICL_TYPE_LONG,
		   bytesLeft);
	rtnVal = DACL_ERROR_TOO_MANY_BYTES;
	/*
	 * We should be at the end of the file here; zero should be 
	 * returned if everything is as expect it to be.
	 */
    }
    
    icl_Trace2(dacl_iclSetp, DACL_ICL_PARSE_PARSE_33, ICL_TYPE_LONG,
	       rtnVal, ICL_TYPE_LONG, bytesLeft);
    
    return rtnVal;
}

#ifdef SGIMIPS
EXPORT long dacl_ParseSyscallAcl(
  char *fileBufferP,
  int bytesInBuffer,
  dacl_t *aclBufferP,
  epi_uuid_t *mgrUuidP)
#else
EXPORT long dacl_ParseSyscallAcl(fileBufferP, bytesInBuffer, aclBufferP,
				 mgrUuidP)
  char *fileBufferP;
  int bytesInBuffer;
  dacl_t *aclBufferP;
  epi_uuid_t *mgrUuidP;
#endif
{
    return dacl_ParseAclDiskOption(fileBufferP, bytesInBuffer, aclBufferP,
				   mgrUuidP,
				   0 /* => parse from syscall buffer */);
}

#ifdef SGIMIPS
EXPORT long dacl_ParseAcl(
  char *fileBufferP,
  int bytesInBuffer,
  dacl_t *aclBufferP,
  epi_uuid_t *mgrUuidP)
#else
EXPORT long dacl_ParseAcl(fileBufferP, bytesInBuffer, aclBufferP, mgrUuidP)
  char *fileBufferP;
  int bytesInBuffer;
  dacl_t *aclBufferP;
  epi_uuid_t *mgrUuidP;
#endif
{
    return dacl_ParseAclDiskOption(fileBufferP, bytesInBuffer, aclBufferP,
				   mgrUuidP,
				   1 /* => parse from disk buffer */);
}

#ifdef SGIMIPS
EXPORT long dacl_FreeAclEntries(
  dacl_t *	theAclP)
#else
EXPORT long dacl_FreeAclEntries(theAclP)
  dacl_t *	theAclP;
#endif
{
    long rtnVal = 0;
    dacl_entry_t *thisListP;
    unsigned int numbEntriesThisList;
    int i;
    static char routineName[] = "dacl_FreeAclEntries";
    
    if (theAclP != (dacl_t *)NULL) {
	/* first free any out of line data for extended entries */
	thisListP =
	    theAclP->complex_entries[(int)dacl_complex_entry_type_other].complex_entries;
	numbEntriesThisList =
	    theAclP->complex_entries[(int)dacl_complex_entry_type_other].num_entries;
	for (i = 0; i < numbEntriesThisList; i++) {
	    if (thisListP[i].entry_type == dacl_entry_type_extended) {
		dacl_Free(thisListP[i].entry_info.extended_info.infoP,
			  thisListP[i].entry_info.extended_info.numberBytes);
	    }
	}
	
	/* now, free all the entry lists */
	for (i = 0; i <= (int)dacl_complex_entry_type_other; i++) {
	    if (theAclP->complex_entries[i].entries_allocated > 0) {
		dacl_Free((opaque)
			  (theAclP->complex_entries[i].complex_entries),
			  (long)(theAclP->complex_entries[i].entries_allocated * sizeof(dacl_entry_t)));
	    }
	    
	    theAclP->complex_entries[i].num_entries = 0;
	}
    } else {
	dacl_LogParamError(routineName, "aclP", dacl_debug_flag,
			   __FILE__, __LINE__);
	rtnVal = DACL_ERROR_PARAMETER_ERROR;
    }
    
    return rtnVal;
}
