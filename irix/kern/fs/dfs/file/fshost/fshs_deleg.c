/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: fshs_deleg.c,v 65.7 1998/04/02 19:44:12 bdr Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * 
 */
/*
 * HISTORY
 * $Log: fshs_deleg.c,v $
 * Revision 65.7  1998/04/02 19:44:12  bdr
 * compiler warning/remark cleanup.
 *
 * Revision 65.6  1998/04/01  14:15:52  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Provided prototype for sec_id_is_anonymous_pa().  Removed unreachable
 * 	return statement.
 *
 * Revision 65.5  1998/03/23 16:33:32  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/01/22 17:53:35  lmc
 * Removed signed32 changes for some functions.  They weren't needed and just
 * caused more mismatches between longs and ints.  Added ansi prototypes and
 * explicit casting.  I checked all the casting to make sure it is proper.
 *
 * Revision 65.2  1997/11/06  19:59:57  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:19:37  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.11.1  1996/10/02  17:46:32  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:37:21  damon]
 *
 * Revision 1.1.4.1  1994/10/20  14:26:59  rsarbo
 * 	map sec to DFS anonymous id
 * 	[1994/10/19  20:32:21  rsarbo]
 * 
 * Revision 1.1.2.3  1994/10/06  20:26:00  agd
 * 	expand copyright
 * 	[1994/10/06  14:11:56  agd]
 * 
 * 	expand copyright
 * 
 * Revision 1.1.2.2  1994/09/06  20:50:48  rsarbo
 * 	allocate and copy pac and associated data; don't rely
 * 	on pointers into the rpc_authz_cred_handle_t
 * 	[1994/09/06  20:02:50  rsarbo]
 * 
 * Revision 1.1.2.1  1994/08/23  18:58:35  rsarbo
 * 	moved from file/security/dacl/dfsdlg_utils.c to allow
 * 	cleaner RIOS kernel extension builds
 * 	[1994/08/23  16:49:15  rsarbo]
 * 
 * $EndLog$
 */

/*
 *      Copyright (C) 1995 Transarc Corporation
 *      All rights reserved.
 */

#include <stddef.h>
#include <sys/errno.h>
#include <dce/rpcbase.h>
#include <dce/id_base.h>
#include <dce/id_epac.h>
#include <dce/sec_cred.h>
#include <dcedfs/param.h>
#include <dcedfs/common_data.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <fshs_trace.h>
#include <dcedfs/icl.h>
#include <fshs_deleg.h>

extern struct icl_set *fshs_iclSetp;

#ifdef SGIMIPS
/* defined in security/utils/restrictions.c */
extern boolean32 sec_id_is_anonymous_pa (
    sec_id_pa_t           *pa,         /* [in] */
    error_status_t        *stp           /* [out] */
);
#endif

/*
 * Fetch a delegation chain from the rpc 
 */
void
fshs_SetupDelegationChain(handle, isAuthn, unauthPAp, head, st)
    rpc_authz_cred_handle_t handle;
    unsigned isAuthn;
    sec_id_pa_t *unauthPAp;
    pac_list_t **head;
    error_status_t *st;
{
   sec_cred_pa_handle_t pahandle;
   sec_id_pa_t *pa;
   pac_list_t *next;
   sec_cred_cursor_t cursor;
#ifndef SGIMIPS
   boolean32 test;
#endif /* SGIMIPS */

   icl_Trace0(fshs_iclSetp, FSHS_ICL_DFSDLG_1);

   *st = 0;
   *head = (pac_list_t *)osi_Alloc(sizeof(pac_list_t));   
   bzero((char *) *head, sizeof(pac_list_t));

   if (isAuthn == 0) {
	(*head)->pacp = unauthPAp;
	(*head)->lastDelegate = *head;
	return;
   }

   icl_Trace0(fshs_iclSetp, FSHS_ICL_DFSDLG_2);
   pahandle = sec_cred_get_initiator(handle, st);
   next = *head;

   icl_Trace0(fshs_iclSetp, FSHS_ICL_DFSDLG_3);
   sec_cred_initialize_cursor(&cursor, st);
   if (*st){
       printf("sec_cred_initialize_cursor, %x\n", *st);
       fshs_FreeDelegationChain(head);
       return;
   }

   while (1){
       pa = sec_cred_get_pa_data(pahandle, st);
       if (*st){
           printf("fx: sec_cred_get_pa_data, error %x\n", *st);
           fshs_FreeDelegationChain(head);
           return;
       }
#ifdef SGIMIPS
       icl_Trace3(fshs_iclSetp, FSHS_ICL_DFSDLG_4, ICL_TYPE_LONG, 
			*((signed32 *)(&pa->principal.uuid)), ICL_TYPE_LONG, 
			*((signed32 *)(&pa->group.uuid)), ICL_TYPE_LONG, 
			pa->num_groups);
#else
       icl_Trace3(fshs_iclSetp, FSHS_ICL_DFSDLG_4, ICL_TYPE_LONG, 
			*((long *)(&pa->principal.uuid)), ICL_TYPE_LONG, 
			*((long *)(&pa->group.uuid)), ICL_TYPE_LONG, 
			pa->num_groups);
#endif
       if (sec_id_is_anonymous_pa(pa, st)) {
	   /*
	    * Map the security anonymous uuid to the DFS anonymous uuid.
	    * Mapping is required because DFS expects the first 32 bits
	    * to be a uid.
	    */
	   next->pacp = unauthPAp;
           next->authenticated = 0;
       } else {
           next->pacp = osi_Alloc(sizeof(sec_id_pa_t));
           bcopy((char*)pa, (char*)next->pacp, sizeof(sec_id_pa_t));
           next->authenticated = 1;
           if (pa->num_groups) {
               next->pacp->groups = osi_Alloc(pa->num_groups * sizeof(sec_id_t));
               bcopy((char*)pa->groups, (char*)next->pacp->groups, 
    				pa->num_groups * sizeof(sec_id_t));
           }
           if (pa->num_foreign_groupsets) {
    	    int i;
    
    	    next->pacp->foreign_groupsets = osi_Alloc(pa->num_foreign_groupsets
    				* sizeof(sec_id_foreign_groupset_t));
                bcopy((char*)pa->foreign_groupsets, (char*)next->pacp->foreign_groupsets, 
                                    pa->num_foreign_groupsets * 
    				sizeof(sec_id_foreign_groupset_t));
    	    for (i = 0; i < ((int)pa->num_foreign_groupsets); i++) {
    		if (pa->foreign_groupsets[i].num_groups) {
    			next->pacp->foreign_groupsets[i].groups = 
    			osi_Alloc(pa->foreign_groupsets[i].num_groups * 
    					sizeof(sec_id_t));
    			bcopy((char*)pa->foreign_groupsets[i].groups, 
			          (char*)next->pacp->foreign_groupsets[i].groups,
    				pa->foreign_groupsets[i].num_groups * 
                                            sizeof(sec_id_t));
    		}
             }
           }
       } 

       icl_Trace0(fshs_iclSetp, FSHS_ICL_DFSDLG_5);
       pahandle = sec_cred_get_delegate(handle, &cursor, st);
       if (*st == sec_cred_s_no_more_entries){
           *st = 0;
           sec_cred_free_cursor(&cursor, st);
	   (*head)->lastDelegate = next;
           return;
       }
       if (*st){
           printf("sec_cred_get_delegate, error %x\n", *st);
           fshs_FreeDelegationChain(head);
           return;
       }
    
       next->next = (pac_list_t *)osi_Alloc(sizeof(pac_list_t));
       next = next->next;
       next->next = (pac_list_t *)NULL;
       next->lastDelegate = NULL;
   }
}

#ifdef SGIMIPS
int
fshs_FreeDelegationChain(pac_list_t **head)
#else
fshs_FreeDelegationChain(head)
  pac_list_t **head;
#endif
{
  pac_list_t *next,*current;
  
  current=*head;

  while(current != (pac_list_t *)NULL){
    int i;

    next = current->next;
    if (current->pacp->num_foreign_groupsets) {
      for (i = 0; i < ((int)current->pacp->num_foreign_groupsets); i++)
        if (current->pacp->foreign_groupsets[i].num_groups)
          osi_Free(current->pacp->foreign_groupsets[i].groups, 
            current->pacp->foreign_groupsets[i].num_groups * sizeof(sec_id_t));
        osi_Free(current->pacp->foreign_groupsets, 
		current->pacp->num_foreign_groupsets * 
		sizeof(sec_id_foreign_groupset_t));
    }
    if (current->pacp->num_groups)
      osi_Free(current->pacp->groups, current->pacp->num_groups * 
				sizeof(sec_id_t));
    if (current->authenticated)
      osi_Free(current->pacp, sizeof(sec_id_pa_t));
    osi_Free(current, sizeof (pac_list_t));
    current = next;
  }
  *head=(pac_list_t *)NULL;
#ifdef SGIMIPS
  return 0;
#endif
}
