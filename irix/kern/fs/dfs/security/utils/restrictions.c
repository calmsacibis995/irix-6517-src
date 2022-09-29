/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: restrictions.c,v 65.6 1998/04/01 14:17:09 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE). See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: restrictions.c,v $
 * Revision 65.6  1998/04/01 14:17:09  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Removed extern declarations for mkmalloc() and mkfree().  Removed
 * 	type casts for argument to FREE() macro.
 *
 * Revision 65.5  1998/03/23  17:31:33  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 18:14:16  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  20:19:01  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:48:32  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.4.2  1996/03/11  01:42:52  marty
 * 	Update OSF copyright years
 * 	[1996/03/10  19:14:03  marty]
 *
 * Revision 1.1.4.1  1995/12/08  18:02:27  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  17:22:58  root]
 * 
 * Revision 1.1.2.4  1994/08/15  20:22:58  cuti
 * 	Fix sec_id_any_other_rstr_in()
 * 	[1994/08/15  20:20:40  cuti]
 * 
 * Revision 1.1.2.2  1994/08/09  17:32:45  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/08  21:58:58  burati]
 * 
 * Revision 1.1.2.1  1994/08/04  16:14:58  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/28  17:16:55  mdf]
 * 
 * 	HP revision /main/cuti_dlg/6  1994/07/28  15:20 UTC  cuti
 * 	Fix memcpy's problem in 486.
 * 
 * 	HP revision /main/ODESSA_2/2  1994/07/27  21:19 UTC  cuti
 * 	 Merge sec_cred_get_authz_session_info()
 * 
 * 	HP revision /main/cuti_dlg/4  1994/07/27  21:15 UTC  cuti
 * 	Modify get_anonymous_epac().
 * 
 * 	HP revision /main/cuti_dlg/3  1994/07/22  03:38 UTC  cuti
 * 	Change architectual anonymous uuid from #define to static uuid_t.
 * 
 * 	HP revision /main/cuti_dlg/2  1994/07/14  20:10 UTC  cuti
 * 	Created for delegation restrictions work.
 * 
 * 	Initial version
 * 
 * $EndLog$
 */

/* restrictions.c
 * 
 * Copyright (c) Hewlett-Packard Company 1993
 * Unpublished work. All Rights Reserved.
 */

#if !defined(KERNEL) || !defined(SGIMIPS)
#include <stdio.h>
#endif
#include <macros.h>
#include <dce/id_epac.h>
#include <restrictions.h>
#include <sec_cred_internal.h>

/*
 * if function pointer is NULL use rpc_ss_free
 */
#define DEALLOCATE(deallocator,ptr) \
         { if (deallocator) (*(deallocator))((ptr)); else rpc_ss_free((ptr)); }

/* Architectural anonymous principal uuid */
static uuid_t anonymous_princ_id = { /*fad18d52-ac83-11cc-b72d-0800092784e9 */
    0xfad18d52,
    0xac83,
    0x11cc,
    0xb7,
    0x2d,
    {0x08, 0x00, 0x09, 0x27, 0x84, 0xe9 }
};

/* Architectual anonymous group uuid */
static uuid_t anonymous_group_id = { /* fc6ed07a-ac83-11cc-97af-0800092784e9 */
    0xfc6ed07a,
    0xac83,
    0x11cc,
    0x97,
    0xaf,
    { 0x08, 0x00, 0x09, 0x27, 0x84, 0xe9 }
};

static sec_id_restriction_set_t dlg = {0, 0}, tgt = {0, 0};
static sec_id_opt_req_t         opt = {0, 0}, req = {0, 0};

extern boolean32 sec_id_any_other_rstr_in (
    sec_id_restriction_set_t *rstrs
)
{
    unsigned32 i;

    for (i = 0; i < rstrs->num_restrictions; i ++) {
	if (rstrs->restrictions[i].entry_info.entry_type == sec_rstr_e_type_any_other) {
	    return true;
	}
    }

    return false;

}


/*
 * Caller needs to supply deallocate function.
 *
 */


extern void sec_id_get_anonymous_epac (
    void                        (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_data_t          *epac,         /* [in/out] */
    error_status_t              *stp           /* [out] */
)
{

    CLEAR_STATUS(stp);

    /* Keep the original deleg_type */
    /* Keep the original opt_restriction */
    /* Keep the original req_restrictions */
    /* Keep the original compat mode */
    epac->deleg_restrictions = dlg; /* NULLify deleg_restrictions */
    epac->target_restrictions = tgt;  /* NULLify target_restrictions */
    epac->num_attrs = 0;
    epac->attrs = NULL;

    /* pa data */
    /* epac->pa.realm.uuid keep the same cell */
    epac->pa.realm.name = NULL;
    epac->pa.principal.uuid = anonymous_princ_id;
    epac->pa.principal.name = NULL;
    epac->pa.group.uuid = anonymous_group_id;
    epac->pa.group.name = NULL;
    epac->pa.num_groups = 0;
    if (epac->pa.groups) {
	DEALLOCATE(dealloc,  epac->pa.groups);
    }
    epac->pa.groups = NULL;
    epac->pa.num_foreign_groupsets = 0;
    if (epac->pa.foreign_groupsets) {
	DEALLOCATE(dealloc,  epac->pa.foreign_groupsets);
    }
    epac->pa.foreign_groupsets = NULL;

}

extern boolean32 sec_id_is_anonymous_epac (
    sec_id_epac_data_t    *epac,         /* [in] */
    error_status_t        *stp           /* [out] */
)
{
    error_status_t dummy;

    if (uuid_equal(&epac->pa.principal.uuid, &anonymous_princ_id,
		   stp) && (uuid_equal(&epac->pa.group.uuid, 
				       &anonymous_group_id,
				       stp))) {
	return(true);
    }
    else {
	return(false);
    }


}


extern boolean32 sec_id_is_anonymous_pa (
    sec_id_pa_t           *pa,         /* [in] */
    error_status_t        *stp           /* [out] */
)
{
    error_status_t dummy;

    if (uuid_equal(&pa->principal.uuid, &anonymous_princ_id,
		   stp) && (uuid_equal(&pa->group.uuid, 
				       &anonymous_group_id,
				       stp))) {
	return(true);
    }
    else {
	return(false);
    }


}




static void match_action(
    boolean32 *match,
    boolean32 *count
)
{

    *match = true;
    (*count) ++;

}

/*
 * Restriction_intersection is for delegate_restriction in impersonator's epac.
 * We compare the original restriction set with the current restriction set, if any restriction
 * appear on both restriction set, it is taken into restriction intersection.
 * To simplify calculating the restriction_intersection, membership is not considered.
 * So for restriction type of user, it is compared with restriction type of user, foreign_user,
 * and foreign_other.  No effort is made to see if restriction type of user is a member of
 * restriction type of group, or even of foreign group.  The reason is that we maybe able
 * to get the membership info if they are in the same cell, but unable to get it if they
 * are on different cell yet.
 * To conclude it, 
 *    type user will compare with type user, foreign_user, foreign_other;
 *    type group with type group, foreign_group, foreign_other; 
 *    type foreign_other with type user, foreign_user, group, foreign_group, foreign_other;
 *    type foreign_user with type user, foreign_user, foreign_other;
 *    type foreign_group with type group, foreign_group, foreign_other;
 *    type no_other won't match anything,'
 *    type any_other will match eveything.
 * If nothing matched, type no_other will return.
 *    
 * Memory management's operator (alloc and dealloc) are for rs3 (the output).  Caller need
 * to handle rs1 and rs2's memory management itself.
 */


extern void sec_id_restriction_intersection (
    idl_void_p_t                (*alloc)(idl_size_t size),     /* [in] */
    void                        (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_restriction_set_t    *rs1,    /* [in] epac's restriction */
    uuid_t                      *cell1_uuid,
    sec_id_restriction_set_t    *rs2,    /* [in] epac's restriction */
    uuid_t                      *cell2_uuid,
    sec_id_restriction_set_t    **rs3,     /* [out] epac's restriction */
    error_status_t              *st      /* [out] */
)
{
    error_status_t dummy;
    unsigned32  i, j, num_restr=0;
    boolean32   *match;
    sec_id_restriction_t  *r1, *r2, r3;
    sec_id_restriction_set_t  *rs;


    CLEAR_STATUS(st);

    /* allocate rs3  for return */
    *rs3 = (sec_id_restriction_set_t *)alloc(sizeof(sec_id_restriction_set_t));
    if (!*rs3) {
	SET_STATUS(st, sec_s_no_memory);
	return;
    }

    /* Empty set.
     * if either one is a NULL set, take the other non-NULL set. 
     * if both are NULL sets, take rs1.
     */
    if (rs1->num_restrictions == 0 || rs2->num_restrictions == 0) {
	if (rs1->num_restrictions == 0)  { /* take rs2 , else take rs1 */
	    rs = rs2;
	}
	else {
	    rs = rs1;
	}
	(*rs3)->num_restrictions = rs->num_restrictions;
	(*rs3)->restrictions = (sec_id_restriction_t *)alloc((*rs3)->num_restrictions * sizeof(sec_id_restriction_t));
	if (!(*rs3)->restrictions) {
	    SET_STATUS(st, sec_s_no_memory);
	    DEALLOCATE(dealloc, (*rs3));
	    return;
	}
	if (rs->restrictions == NULL) {
	    (*rs3)->restrictions = NULL;
	}
	else {
	    memcpy((*rs3)->restrictions, rs->restrictions, sizeof(rs->restrictions));
	}
	return;
    }
	  

    match = (boolean32 *)MALLOC(sizeof(boolean32) * rs1->num_restrictions);
    if (!match) {
	SET_STATUS(st, sec_s_no_memory);
	return;
    }
    memset(match, 0, sizeof(boolean32) * rs1->num_restrictions);

    for (i = 0, r1 = rs1->restrictions; i < rs1->num_restrictions; i++, r1++) {
	/* if entry_type = no_other which has highest precedence, so return it 
	 */
	if (r1->entry_info.entry_type == sec_rstr_e_type_no_other) {
	    (*rs3)->num_restrictions = 1;
	    (*rs3)->restrictions = (sec_id_restriction_t *)alloc((*rs3)->num_restrictions * sizeof(sec_id_restriction_t));
	    if (!(*rs3)->restrictions) {
		SET_STATUS(st, sec_s_no_memory);
		DEALLOCATE(dealloc, (*rs3));
                FREE(match);
		return;
	    }
	    (*rs3)->restrictions->entry_info.entry_type = sec_rstr_e_type_no_other;
            FREE(match);
	    return;
	} 

	if (r1->entry_info.entry_type == sec_rstr_e_type_any_other) {
	    /* r1 is a biggest set it could be, take rs2 */
	    (*rs3)->num_restrictions = rs2->num_restrictions;
	    (*rs3)->restrictions = (sec_id_restriction_t *)alloc((*rs3)->num_restrictions * sizeof(sec_id_restriction_t));
	    if (!(*rs3)->restrictions) {
		SET_STATUS(st, sec_s_no_memory);
		DEALLOCATE(dealloc, (*rs3));
                FREE(match);
		return;
	    }
	    if (rs2->num_restrictions == 0) {
		(*rs3)->restrictions = NULL;
	    }
	    else {
		memcpy((*rs3)->restrictions, rs2->restrictions, sizeof(rs2->restrictions));
	    }
            FREE(match);
	    return;
	}

	for (j = 0, r2 = rs2->restrictions; !match[i] && j < rs2->num_restrictions; j++, r2++) {
	    if (r2->entry_info.entry_type == sec_rstr_e_type_no_other) {
		/* no_other has highest precedence, will override other restrictions. */
		/* Form return restriction */
		(*rs3)->num_restrictions = 1;
		(*rs3)->restrictions = (sec_id_restriction_t *)alloc((*rs3)->num_restrictions * sizeof(sec_id_restriction_t));
		if (!(*rs3)->restrictions) {
		    SET_STATUS(st, sec_s_no_memory);
		    DEALLOCATE(dealloc, (*rs3));
                    FREE(match);
		    return;
		}
		(*rs3)->restrictions->entry_info.entry_type =  sec_rstr_e_type_no_other;
                FREE(match);
		return;
	    }

	    if (r2->entry_info.entry_type == sec_rstr_e_type_any_other) {
		/* r2 is a biggest set it could be, take rs1 */
		(*rs3)->num_restrictions = rs1->num_restrictions;
		(*rs3)->restrictions = (sec_id_restriction_t *)alloc((*rs3)->num_restrictions * sizeof(sec_id_restriction_t));
		if (!(*rs3)->restrictions) {
		    SET_STATUS(st, sec_s_no_memory);
		    DEALLOCATE(dealloc, (*rs3));
                    FREE(match);
		    return;
		}
		if (rs1->num_restrictions == 0) {
		    (*rs3)->restrictions = NULL;
		}
		else {
		    memcpy((*rs3)->restrictions, rs1->restrictions, sizeof(rs1->restrictions));
		}
                FREE(match);
		return;
	    }

	    switch (r1->entry_info.entry_type) {
	    case sec_rstr_e_type_user:
		switch (r2->entry_info.entry_type) {
		case sec_rstr_e_type_user:
		    if (uuid_equal(cell1_uuid, cell2_uuid, &dummy) &&
			uuid_equal (&r1->entry_info.tagged_union.id.uuid,
				    &r2->entry_info.tagged_union.id.uuid,
				    &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		case sec_rstr_e_type_foreign_user:
		    if (uuid_equal(cell1_uuid, &r2->entry_info.tagged_union.foreign_id.realm.uuid, &dummy) &&
			uuid_equal (&r1->entry_info.tagged_union.id.uuid,
				    &r2->entry_info.tagged_union.foreign_id.id.uuid,
				    &dummy)) {
			match_action(&match[i], &num_restr);
    			continue;
		    }
		    break;
		case sec_rstr_e_type_foreign_other:
		    if (uuid_equal(cell1_uuid, &r2->entry_info.tagged_union.id.uuid, &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		default:
		    break;
		}
		break;
	    case sec_rstr_e_type_group:
		switch (r2->entry_info.entry_type) {
		case sec_rstr_e_type_group:
		    if (uuid_equal(cell1_uuid, cell2_uuid, &dummy) &&
			uuid_equal (&r1->entry_info.tagged_union.id.uuid,
				    &r2->entry_info.tagged_union.id.uuid,
				    &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		case sec_rstr_e_type_foreign_group:
		    if (uuid_equal(cell1_uuid, &r2->entry_info.tagged_union.foreign_id.realm.uuid, &dummy) &&
			uuid_equal (&r1->entry_info.tagged_union.id.uuid,
				    &r2->entry_info.tagged_union.foreign_id.id.uuid,
				    &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;	
		case sec_rstr_e_type_foreign_other:
		    if (uuid_equal(cell1_uuid, &r2->entry_info.tagged_union.id.uuid, &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		default:
		    break;
		}
		break;
	    case sec_rstr_e_type_foreign_other:
		switch (r2->entry_info.entry_type) {
		case sec_rstr_e_type_user:
		case sec_rstr_e_type_group:
		    if (uuid_equal (&r1->entry_info.tagged_union.id.uuid,
				    cell2_uuid, &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;	
		case sec_rstr_e_type_foreign_user:
	        case sec_rstr_e_type_foreign_group:
		    if (uuid_equal (&r1->entry_info.tagged_union.id.uuid,
				    &r2->entry_info.tagged_union.foreign_id.realm.uuid, &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		case sec_rstr_e_type_foreign_other:
		    if (uuid_equal (&r1->entry_info.tagged_union.id.uuid,
				    &r2->entry_info.tagged_union.id.uuid,
				    &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		default:
		    break;
		}
		break;
	    case sec_rstr_e_type_foreign_user:
		switch (r2->entry_info.entry_type) {
		case sec_rstr_e_type_user:
		    if (uuid_equal(&r1->entry_info.tagged_union.foreign_id.realm.uuid,
				    cell2_uuid, &dummy) &&
			uuid_equal(&r1->entry_info.tagged_union.foreign_id.id.uuid,
				   &r2->entry_info.tagged_union.id.uuid, &dummy))
			{
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		case sec_rstr_e_type_foreign_user:
		    if (uuid_equal(&r1->entry_info.tagged_union.foreign_id.realm.uuid,
				   &r2->entry_info.tagged_union.foreign_id.realm.uuid, 
				   &dummy) &&
			uuid_equal(&r1->entry_info.tagged_union.foreign_id.id.uuid,
				   &r2->entry_info.tagged_union.foreign_id.id.uuid, 
				   &dummy))
			{
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		case sec_rstr_e_type_foreign_other:
		    if (uuid_equal(&r1->entry_info.tagged_union.foreign_id.realm.uuid,
				    &r2->entry_info.tagged_union.id.uuid,
				    &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		default:
		    break;
		}
		break;
	    case sec_rstr_e_type_foreign_group:
		switch (r2->entry_info.entry_type) {
		case sec_rstr_e_type_group:
		    if (uuid_equal(&r1->entry_info.tagged_union.foreign_id.realm.uuid,
				    cell2_uuid, &dummy) &&
			uuid_equal(&r1->entry_info.tagged_union.foreign_id.id.uuid,
				   &r2->entry_info.tagged_union.id.uuid, &dummy))
			{
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		case sec_rstr_e_type_foreign_group:
		    if (uuid_equal(&r1->entry_info.tagged_union.foreign_id.realm.uuid,
				   &r2->entry_info.tagged_union.foreign_id.realm.uuid, 
				   &dummy) &&
			uuid_equal(&r1->entry_info.tagged_union.foreign_id.id.uuid,
				   &r2->entry_info.tagged_union.foreign_id.id.uuid, 
				   &dummy))
			{
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		case sec_rstr_e_type_foreign_other:
		    if (uuid_equal(&r1->entry_info.tagged_union.foreign_id.realm.uuid,
				    &r2->entry_info.tagged_union.id.uuid,
				    &dummy)) {
			match_action(&match[i], &num_restr);
			continue;
		    }
		    break;
		default:
		    break;
		}
		break;
	    default:
		break;
	    }
	}
    }

    /* Form returne restriction */
    /* If nothing matched, should return no_other restriction */

    if (num_restr == 0) {
	(*rs3)->num_restrictions = 1;
	(*rs3)->restrictions = (sec_id_restriction_t *)alloc((*rs3)->num_restrictions * sizeof(sec_id_restriction_t));
	if (!(*rs3)->restrictions) {
	    SET_STATUS(st, sec_s_no_memory);
	    DEALLOCATE(dealloc, (*rs3));
            FREE(match);
	    return;
	}
 	(*rs3)->restrictions->entry_info.entry_type =  sec_rstr_e_type_no_other;
    }
    else {
	(*rs3)->num_restrictions = num_restr;
	(*rs3)->restrictions = (sec_id_restriction_t *)alloc((*rs3)->num_restrictions * sizeof(sec_id_restriction_t));
	if (!(*rs3)->restrictions) {
	    SET_STATUS(st, sec_s_no_memory);
	    DEALLOCATE(dealloc, (*rs3));
            FREE(match);
	    return;
	}
	for (i = 0, j = 0; i < rs1->num_restrictions; i++) {
	    if (match[i]) {
		memcpy (&(*rs3)->restrictions[j], &rs1->restrictions[i], sizeof(rs1->restrictions[i]));
		j++;
	    }
	}
    }

    FREE(match);
}


#ifndef KERNEL
extern void sec_id_opt_req_restriction_union (
    idl_void_p_t                (*alloc)(idl_size_t size),     /* [in] */
    void                        (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_opt_req_t            *rs1,    /* [in] epac's opt_req restriction */
    sec_id_opt_req_t            *rs2,    /* [in] epac's opt_req restriction */
    sec_id_opt_req_t            **rs3,   /* [out] epac's opt_req restriction */    
    error_status_t              *st      /* [out] */
)
{

    idl_byte                    *r;
    sec_id_opt_req_t            *rs;

    CLEAR_STATUS(st);

    (*rs3) = (sec_id_opt_req_t *)alloc(sizeof(sec_id_opt_req_t));
    if (!*rs3) {
	SET_STATUS(st, sec_s_no_memory);
	return;
    }

    /* if either one is a NULL set, take the other non-NULL set. 
     * if both are NULL sets, take rs1.
     */
    if (rs1->restriction_len == 0 || rs2->restriction_len == 0) {
	if (rs1->restriction_len == 0) {
	    rs = rs2;
	}
	else {
	    rs = rs1;
	}
	(*rs3)->restriction_len = rs->restriction_len;
	(*rs3)->restrictions = (idl_byte *)alloc((*rs3)->restriction_len);
	if (!(*rs3)->restrictions) {
	    SET_STATUS(st, sec_s_no_memory);
	    DEALLOCATE(dealloc, (*rs3));
	    return;
	}
	if (rs->restriction_len == 0) {
	    (*rs3)->restrictions = NULL;
	}
	else {
	    memcpy((*rs3)->restrictions, rs->restrictions, sizeof(rs->restrictions));
	}
	return;
    }
	    

    /* combine rs1 and rs2 
     */
    (*rs3)->restriction_len = rs1->restriction_len + rs2->restriction_len;
    (*rs3)->restrictions = (idl_byte *)alloc((*rs3)->restriction_len * sizeof(idl_byte));
    if (!(*rs3)->restrictions) {
	SET_STATUS(st, sec_s_no_memory);
	DEALLOCATE(dealloc, (*rs3));
	return;
    }
    
    
    r = (*rs3)->restrictions;
    if (rs1->restriction_len == 0) {
	r = NULL;
    }
    else {
	memcpy(r, rs1->restrictions, sizeof(rs1->restrictions));
    }
    r += rs1->restriction_len;
    if (rs2->restriction_len != 0) {
	memcpy(r, rs2->restrictions, sizeof(rs2->restrictions));
    }


}
#endif /* KERNEL */


/*  
 * If restriction set is NULL, return true.
 * else for each restriction , 
 *    if entry_type = no_other
 *       return false;
 *    if entry_type = any_other
 *       return true;
 *    else check the following.
 *       if epacs realm which restriction coming from = server's realm
 *          local is set
 *       if local, check restriction's entry_types of user, group and
 *               foreign_user, foreign_group (foreign means global name).
 *       else check foreign user, foreign_group only
 *       switch (entry_type) {
 *         case user: if local && uuid = spa.principal.uuid, 
 *                       return true
 *         case group: if local
 *                        if uuid = spa.group.uuid, 
 *                           return true
 *                        else if uuid = one of spa.groups.uuid, 
 *                                return true
 *         case foreign_user: if realm.uuid = spa.realm.uuid &&
 *                            id.uuid = spa.principal.uuid
 *                               return true
 *         case foreign_group: if realm.uuid = spa.realm.uuid
 *                                if id.uuid = spa.group.uuid
 *                                   return true
 *                                else if id.uuid = one of spa.groups.uuid
 *                                        return true
 *                             else if realm.uuid = spa.foreign_groupsets.realm.uuid
 *                                     if id.uuid = one of spa.foreign_groupsets.groups.uuid
 *                                        then return  
 *        case foreign_other: if uuid = spa.realm.uuid,
 *                                return true
 *       }
 *    if no return point hit yet, continue next restriction  
 *     
 *  if all of retriction doesn't have any match, return false.
 *
 *  if check_restrictions return false, change this epac to anonymous epac
 *  else do nothing.  
 *
 */

/*
 * An empty set of target restriction (or restriction type = any_other)
 *     All principals are vaild.
 * Otherwise according to restrictin type, the valid user:
 *    user : specified user
 *    group: any member of the specified group.
 *    foreign_other:  any member in specified global group.
 *    foreign_user: specified foreign cell user (cell uuid,  uuid, name)
 *    foreign_group: any member in specified global group.
 */
                              

extern boolean32 sec_id_check_restrictions
(
    sec_id_pa_t                 *epa,    /* [in] epac's pa */
    sec_id_restriction_set_t    *ers,    /* [in] epac's restriction */
    sec_id_pa_t                 *spa,     /* [in] server's pa */
    error_status_t              *st     /* [out] */
)
{

    boolean32    match=false, local=false, no_other= false;
    unsigned32   i, j, k;
    sec_id_restriction_t    *cr;

    CLEAR_STATUS(st);

    if (ers->num_restrictions == 0) {/* no restriction = any_other */
	return(true);
    }

    cr = &ers->restrictions[0];

    for (i = 0; i < ers->num_restrictions; i++, cr++){
	if (cr->entry_info.entry_type == sec_rstr_e_type_no_other) {
	    /* no other is allowed.  This overrides anything */
	    return(false);
	}

        /*
	* Since no_other overrides everything, we need to loop through every
         * restriction to check no_other.
	 */
        if (cr->entry_info.entry_type == sec_rstr_e_type_any_other) {
	    match = true;
	    continue;
	}

        if (uuid_equal(&epa->realm.uuid, &spa->realm.uuid, st)) {
	    local = true;
	}
            
	switch(cr->entry_info.entry_type) {
	    case sec_rstr_e_type_user: 
		if (local && uuid_equal(&cr->entry_info.tagged_union.id.uuid,
                               &spa->principal.uuid, st)) {
		    match = true;
	        }
		break;
	    case sec_rstr_e_type_group:
		if (local) {
		    if (uuid_equal(&cr->entry_info.tagged_union.id.uuid,
		               &spa->group.uuid, st)) {
		        match = true;
			continue;
		    }

		    for (j = 0; j < spa->num_groups && !match; j++) {
			if (uuid_equal(&cr->entry_info.tagged_union.id.uuid,
		                       &spa->groups[j].uuid, st)) {
			    match = true;
			}
		    }
		}
		break;
	    case sec_rstr_e_type_foreign_other:
		if (uuid_equal(&cr->entry_info.tagged_union.id.uuid,
				   &spa->realm.uuid, st)) {
		    match = true;
		}
		break;
	    case sec_rstr_e_type_foreign_user:
		if (uuid_equal(&cr->entry_info.tagged_union.foreign_id.realm.uuid,
                               &spa->realm.uuid, st) &&
                    uuid_equal(&cr->entry_info.tagged_union.foreign_id.id.uuid,
				&spa->principal.uuid, st)) {
		    match = true;
		}
		break;
	    case sec_rstr_e_type_foreign_group:
		if (uuid_equal(&cr->entry_info.tagged_union.foreign_id.realm.uuid,
                               &spa->realm.uuid, st)) { /* same foreign cell */
		    if (uuid_equal(&cr->entry_info.tagged_union.foreign_id.id.uuid,
                                   &spa->group.uuid, st)) {
			match = true;
			continue;
		    }
		    
		    for (j = 0; j < spa->num_groups && !match; j++) {
			if (uuid_equal(&cr->entry_info.tagged_union.foreign_id.id.uuid,
		                       &spa->groups[j].uuid, st)) {
			    match = true;
			}
		    }
		}
		else {
		    for (j = 0; j < spa->num_foreign_groupsets && !match; j++) {
		        if (uuid_equal(&cr->entry_info.tagged_union.foreign_id.realm.uuid,
		                       &spa->foreign_groupsets[j].realm.uuid, st)) {
			     for (k = 0; k < spa->foreign_groupsets[j].num_groups && !match; k++) {
			         if (uuid_equal(&cr->entry_info.tagged_union.foreign_id.id.uuid,
			                        &spa->foreign_groupsets[j].groups[k].uuid, st)){
					     match = true;
			         }
			     }
			 }
		     }
		 }

		break;

	    default:
		break;
		
	   } 

    }

    return(match); 
}


