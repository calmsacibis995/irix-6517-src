/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_validate.c,v 65.4 1998/03/23 16:36:29 gwehrman Exp $";
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
 * $Log: dacl_validate.c,v $
 * Revision 65.4  1998/03/23 16:36:29  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/11/06 20:00:34  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:17  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:20:18  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.855.1  1996/10/02  20:56:20  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:57  damon]
 *
 * $EndLog$
 */

/*
 * Copyright (C) 1995, 1991 Transarc Corporation
 * All rights reserved.
 */

/*
 * dacl_validate.c -- reads an acl from a buffer containing the acl's bytes
 */


#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/stds.h>
#include <dcedfs/icl.h>

#if !defined(KERNEL)
#include <dcedfs/compat.h>		/* dfs_dceErrTxt */
#endif /* !defined(KERNEL) */

#include <dcedfs/fshs_deleg.h>
#include <dacl.h>
#include <dacl_debug.h>
#include <dacl_trace.h>

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_validate.c,v 65.4 1998/03/23 16:36:29 gwehrman Exp $")

extern struct icl_set *dacl_iclSetp;
  
/* generic failure: DACL_ERROR_VALIDATION_FAILURE */
/*
 * NB: this is the routine in which we would do duplicate detection, if we 
 * were doing it, yet.
 *
 * OT 8005: Under this OT, since other_obj in an initial acl became a template
 * only (like user_obj), the storing cell could no longer specify any explicit 
 * rights for all the members of the storing cell for files/dirs created by 
 * foreign users in a local directory ( since there is no local_other ACL 
 * entry).
 * Hence, a foreign_other:storingCell:<perms for members of storing cell>
 * can be added to an initial ACL to simulate local_other. Hence, the rule 
 * that the cellname in a foreign_other entry should never be the default cell
 * of the acl is NO LONGER VALID. Remove the code that implements this rule.
 */


static long dacl_ComplexList_Validate(
    dacl_entry_t *   complexEntriesP,
    int		     numEntries,
    epi_uuid_t *     defaultRealmP,
    int		     makeMinorRepairs)
{
    long	     rtnVal = 0;
    dacl_entry_t *   thisEntryP;
    epi_uuid_t	     tempUuid;
    int		     i;
    static char	     routineName[] = "dacl_ComplexList_Validate";

    for (i = 0; i < numEntries; i++) {
	thisEntryP = &(complexEntriesP[i]);

	/* make sure that an entry labelled foreign isn't really in the 
	 * default cell */
	if (((thisEntryP->entry_type == dacl_entry_type_foreign_user) ||
	     (thisEntryP->entry_type == dacl_entry_type_foreign_user_delegate)) &&
	    (bcmp((char *)&(thisEntryP->entry_info.foreign_id.realm),
		  (char *)defaultRealmP, sizeof(epi_uuid_t)) == 0)) {
	    if (makeMinorRepairs != 0) {
		icl_Trace0(dacl_iclSetp, DACL_ICL_VALIDATE_VALIDATE_0 );

		thisEntryP->entry_type =
		    ((thisEntryP->entry_type == dacl_entry_type_foreign_user)?
		   dacl_entry_type_user: dacl_entry_type_user_delegate);

		/* avoid trying to copy directly within the union */
		tempUuid = thisEntryP->entry_info.foreign_id.id;
		thisEntryP->entry_info.id = tempUuid;
	    } else {
		icl_Trace0(dacl_iclSetp, DACL_ICL_VALIDATE_VALIDATE_1 );
		rtnVal = DACL_ERROR_VALIDATION_FAILURE;
	    }
	} else if (((thisEntryP->entry_type == dacl_entry_type_foreign_group) ||
		    (thisEntryP->entry_type == dacl_entry_type_foreign_group_delegate)) &&
		   (bcmp((char *)&(thisEntryP->entry_info.foreign_id.realm),
			 (char *)defaultRealmP, sizeof(epi_uuid_t)) == 0)) {
	    if (makeMinorRepairs != 0) {
		icl_Trace0(dacl_iclSetp, DACL_ICL_VALIDATE_VALIDATE_2 );

		thisEntryP->entry_type =
		    ((thisEntryP->entry_type == dacl_entry_type_foreign_group)?
		   dacl_entry_type_group: dacl_entry_type_group_delegate);
	      
		/* avoid trying to copy directly within the union */
		tempUuid = thisEntryP->entry_info.foreign_id.id;
		thisEntryP->entry_info.id = tempUuid;
	    } else {
		icl_Trace0(dacl_iclSetp, DACL_ICL_VALIDATE_VALIDATE_3 );
		rtnVal = DACL_ERROR_VALIDATION_FAILURE;
	    }
	}
    }	/* end for loop looping over entries in list */
    
    return rtnVal;
}

/* EXPORT */
long dacl_ValidateBuffer(
     char *	        byteBufferP,
     unsigned int *	bytesInBufferP,
     epi_uuid_t *	mgrUuidP,
     int		flags,
     dacl_t **		daclPP)
{
    long	             rtnVal = 0;      /* assume that buffer is OK */
    dacl_t *	             theDaclP;
    int		             i;
    char *	             shortBufferP;
    unsigned int             shortBufferLength;
    static char	             routineName[] = "dacl_ValidateBuffer";
    dacl_complex_entry_t *   complex;
    dacl_simple_entry_t *    simple;

    if ((byteBufferP != (char *)NULL) && (mgrUuidP != (epi_uuid_t *)NULL) &&
	(bytesInBufferP != (unsigned int *)NULL)) {
	if (daclPP != (dacl_t **)NULL) {
	    theDaclP = *daclPP;
	} else {
	    theDaclP = (dacl_t *)osi_Alloc(sizeof(dacl_t));
	}
    
	rtnVal = dacl_ParseSyscallAcl(byteBufferP, *bytesInBufferP,
				      theDaclP, mgrUuidP);
	if (rtnVal == DACL_SUCCESS) {
	    complex = theDaclP->complex_entries;
	    simple = theDaclP->simple_entries;

	    /* this loop will never even start if we have gotten an error up
	     * to this point */
	    /* now, validate each of the complex entry lists */
	    for (i = (int)dacl_complex_entry_type_user;
		 (rtnVal == DACL_SUCCESS) &&
		 (i <= (int)dacl_complex_entry_type_other);
		 i++) {
		rtnVal =
		    dacl_ComplexList_Validate(complex[i].complex_entries,
					      complex[i].num_entries,
					      &(theDaclP->default_realm),
					      flags & 
					      DACL_VB_MAKE_MINOR_REPAIRS);
	    }	/* end for loop */

	    /* check that user obj has c permission, if required */
	    if (dacl_AreObjectEntriesRequired(&(theDaclP->mgr_type_tag)) &&
		!(simple[(int)dacl_simple_entry_type_userobj].perms &
		  dacl_perm_control)) {
		/* fail the validation if user_obj does not have c permission 
		 */
		rtnVal = DACL_ERROR_VALIDATION_FAILURE;
	    }

	    /* check that a mask obj is present, if it is required */
	    if ((dacl_AreObjectEntriesRequired(&(theDaclP->mgr_type_tag))) &&
		((simple[(int)dacl_simple_entry_type_anyother].is_entry_good)
		 ||
		 (complex[(int)dacl_complex_entry_type_user].num_entries > 0)
		 ||
		 (complex[(int)dacl_complex_entry_type_group].num_entries > 0) 
		 ||
		 (complex[(int)dacl_complex_entry_type_other].num_entries > 0)
		 )) {
		/* if a mask obj is required */
		if (simple[(int)dacl_simple_entry_type_maskobj].is_entry_good 
		    == 0) {
		    /* fail the validation if the mask_obj is missing */
		    rtnVal = DACL_ERROR_VALIDATION_FAILURE;
		}
	    }

	    /* DB5716: Check for non-existence of an unauth_mask entry */

	    if (simple[(int)dacl_simple_entry_type_unauthmask].is_entry_good) {
		if (flags & DACL_VB_REMOVE_UNAUTH_MASK) {
		    simple[(int)dacl_simple_entry_type_unauthmask].is_entry_good = 0;
		    simple[(int)dacl_simple_entry_type_unauthmask].perms = 0;
		    theDaclP->num_entries--;
		    rtnVal = DACL_REMOVED_UNAUTH_MASK;
		} else if ((flags & DACL_VB_FIND_UNAUTH_MASK) &&
			   (rtnVal == DACL_SUCCESS)) {
		    /* return this warning only if no other errors */
		    rtnVal = DACL_FOUND_UNAUTH_MASK;
		} else {
		    rtnVal = DACL_ERROR_VALIDATION_FAILURE;
		}
	    }
	} else {
#if defined(KERNEL)
	    icl_Trace1(dacl_iclSetp, DACL_ICL_VALIDATE_VALIDATE_5,
		       ICL_TYPE_LONG, rtnVal);
#else /* defined(KERNEL) */
	    icl_Trace1(dacl_iclSetp, DACL_ICL_VALIDATE_VALIDATE_6,
		       ICL_TYPE_STRING, dfs_dceErrTxt(rtnVal));
#endif /* defined(KERNEL) */
      
	    /* leave rtnVal as determined by the parsing routine */
	}

	if (daclPP == (dacl_t **)NULL) {
	    /* in this case, we had to allocate our own, and the caller didn't 
	     * want to it */
	    dacl_FreeAclEntries(theDaclP);
	    osi_Free((opaque)theDaclP, (long)sizeof(dacl_t));
	}
    } else {
	if (byteBufferP == (char *)NULL) {
	    dacl_LogParamError(routineName, "byteBufferP", dacl_debug_flag,
			       __FILE__, __LINE__);
	}
	if (mgrUuidP == (epi_uuid_t *)NULL) {
	    dacl_LogParamError(routineName, "mgrUuidP", dacl_debug_flag,
			       __FILE__, __LINE__);
	}
	if (bytesInBufferP == (unsigned int *)NULL) {
	    dacl_LogParamError(routineName, "bytesInBufferP", dacl_debug_flag,
			       __FILE__, __LINE__);
	}
	rtnVal = DACL_ERROR_PARAMETER_ERROR;
    }
    
    return rtnVal;
}

/* IMPORT */
long dacl_epi_ValidateBuffer(
    char *           byteBufferP,
    unsigned int *   bytesInBufferP,
    int              flags,
    dacl_t **        daclPP)
{
    return dacl_ValidateBuffer(byteBufferP, bytesInBufferP, 
			       &episodeAclMgmtUuid, flags, daclPP);
}
