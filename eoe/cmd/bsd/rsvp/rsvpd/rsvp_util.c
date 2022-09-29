/*
 * @(#) $Id: rsvp_util.c,v 1.7 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_util.c  *******************************
 *                                                                   *
 *     Common routines for managing state and parsing protocol       *
 *     data structure (flowspecs, filterspecs, flow descriptors...)  *
 *     Used by rsvp_path.c and rsvp_resv.c                           *
 *                                                                   *
 *********************************************************************/
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

            Original Version: Shai Herzog, Nov. 1993.
            Current Version:  Steven Berson & Bob Braden, May 1996.

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

#include "rsvp_daemon.h"

#ifdef _MIB
#include "rsvp_mib.h"
#endif

/*	External declarations
 */
extern void	del_from_timer();
int		IsRoutePSB2nhop(Session *, PSB *, RSVP_HOP *);

/*	Forward declarations
 */
Session		*locate_session(SESSION *);
Session		*locate_session_p(SESSION *);
int		match_filter(FILTER_SPEC *, FILTER_SPEC *);
void		scope_cat(SCOPE **, struct in_addr *);
void		scope_catf(SCOPE **, FILTER_SPEC *);
void		merge_SCOPEs(Session *, SCOPE *);
int		form_scope_union(Session *);
void		clear_scope_union(Session *);
Object_header * copy_object(Object_header *);
Fobject	      *	copy_obj2Fobj(Object_header *);
void		FQins(Fobject *, Fobject **);
Fobject       *	FQrmv(Fobject **);
void		FQconcat(Fobject *, Fobject **);
void		FQkill(Fobject **);
int		match_policy(POLICY_DATA *, POLICY_DATA *);


/*
 *   match_filter(): Compares two FILTER_SPEC objects for equality,
 *	returns Boolean value.
 */
int
match_filter(FILTER_SPEC *f1, FILTER_SPEC *f2)
	{
	if (!f1 && !f2 )
		return(1);
	if (!f1 || !f2)
		return(0);

	if (Obj_CType(f2) != Obj_CType(f1))
		return(0);

	switch (Obj_CType(f1)) {

	    case ctype_FILTER_SPEC_ipv4:
		{
		Filter_Spec_IPv4 *f1a = &f1->filt_u.filt_ipv4;
		Filter_Spec_IPv4 *f2a = &f2->filt_u.filt_ipv4;

		return (f1a->filt_ipaddr.s_addr == INADDR_ANY
		     ||  f2a->filt_ipaddr.s_addr == INADDR_ANY
		     || (f1a->filt_ipaddr.s_addr == f2a->filt_ipaddr.s_addr
		        && f1a->filt_port == f2a->filt_port) );
		}

	    case ctype_FILTER_SPEC_ipv4GPI:
		{
		Filter_Spec_IPv4GPI *f1a = &f1->filt_u.filt_ipv4gpi;
		Filter_Spec_IPv4GPI *f2a = &f2->filt_u.filt_ipv4gpi;

		return (f1a->filt_ipaddr.s_addr == INADDR_ANY
		     || f2a->filt_ipaddr.s_addr == INADDR_ANY
		     || (f1a->filt_ipaddr.s_addr == f2a->filt_ipaddr.s_addr
		        	 && f1a->filt_gpi == f2a->filt_gpi) );
		}
	
	    default:
		/* Treat unknown type as not matching.
		 *	XXX This is actually a bug... each refresh with an
		 *	unknown sender template makes new path state...!
		 */
		return(0);
	}
}


/*	Same, but second argument is SENDER_TEMPLATE.  Really just
 *	type matching.
 */
int
match_sender_filter(FILTER_SPEC *f1, SENDER_TEMPLATE *f2)
	{
	return( match_filter(f1, (FILTER_SPEC *)f2) );
}


/*
 *  Locate Session block for given SESSION object
 *	Return 0 if no match, -1 if there is confusion about zero
 *	ports, and the Session struct address otherwise.
 */
Session    *
locate_session_p(SESSION *sessp)
	{
	Session	*destp;
	u_int16_t	port = sessp->sess_u.sess_ipv4.sess_destport;
	struct in_addr  addr = sessp->sess_u.sess_ipv4.sess_destaddr;
	int		hash = Sess_hashf(sessp);

	for (destp = session_hash[hash]; destp ; destp = destp->d_next) {
		if (destp->d_addr.s_addr != addr.s_addr)
			continue;
		if (destp->d_protid != sessp->sess4_prot)
			continue;
		if (destp->d_port == port)
			return (destp);
		if (port == 0 || destp->d_port == 0)
			return ((Session *) -1);
	}
	return (NULL);
}

/*	locate_session(): Same as locate_session_p, except do not check
 *		for confusion about zero ports; require exact match.
 */
Session    *
locate_session(SESSION *sessp)
	{
	Session	*destp;
	u_int16_t	port = sessp->sess4_port;
	struct in_addr  addr = sessp->sess4_addr;
	int		hash = Sess_hashf(sessp);

	for (destp = session_hash[hash]; destp ; destp = destp->d_next) {
		if (destp->d_addr.s_addr != addr.s_addr)
			continue;
		if (destp->d_protid != sessp->sess4_prot)
			continue;
		if (destp->d_port == port)
			return (destp);
	}
	return (NULL);
}


/*
 * 	Create Session block for new session
 */
Session *
make_session(SESSION *sessp)
	{
	Session		*destp;
	int		hash = Sess_hashf(sessp);
	int		i;

	destp = (Session *) calloc(1, sizeof(Session));
	if (!destp) {
		Log_Mem_Full("New session");
		return(NULL);
	}

	/*
	 *   Initialize fields of Session structure.
	 */
	destp->d_PSB_list = NULL;	/* no senders yet  */

	destp->d_session = (SESSION *)copy_object((Object_header *)sessp);

	Init_Object(&destp->d_timevalp, TIME_VALUES, ctype_TIME_VALUES)
	destp->d_timevalp.timev_R = 0;
	Init_Object(&destp->d_timevalr, TIME_VALUES, ctype_TIME_VALUES)
	destp->d_timevalr.timev_R = 0;
	destp->d_flags = 0;

	destp->d_TCSB_listv = (TCSB **) calloc(vif_num + 1, sizeof(TCSB *));
	if (!destp->d_TCSB_listv) {
		free((char *) destp);
		return(NULL);
	}
	for (i = 0; i <= if_num; i++)
		destp->d_TCSB_listv[i] = NULL;

	/*
	 *  Insert new destination first in hast list
	 */
	destp->d_next = session_hash[hash];
	session_hash[hash] = destp;

#ifdef _MIB
	if (mib_enabled)
	        destp->mib_sessionhandle = mglue_new_session(destp);
#endif

	return(destp);
}

/*
 * kill_session():  All senders and reservations for this Session have
 *	been deleted; complete cleanup of the session and delete the
 *	Session (session) block itself.
 */
int
kill_session(Session *sessp)
	{
	Session	**d;
	int	hash = Sess_hashf(sessp->d_session);
#if DEBUG
	int	i;

	assert(!sessp->d_PSB_list && !sessp->d_RSB_list);
	for (i = 0; i <= if_num; i++)
		assert(sessp->d_TCSB_listv[i] == NULL);
#endif /* DEBUG */

#ifdef _MIB
	if (mib_enabled)
	        rsvpd_deactivate_rsvpSessionEntry(sessp->mib_sessionhandle);
#endif

	free(sessp->d_TCSB_listv);
	free(sessp->d_session);

	/*  Take dest off hash list, and then delete all its timer events.
	 *  Finally, free the control block.
	 */
	for (d = &session_hash[hash]; (*d) != NULL && (*d) != sessp;
						 d = &((*d)->d_next));
	assert(*d);
	*d = sessp->d_next;
	del_from_timer((char *) sessp, TIMEV_RESV);
	del_from_timer((char *) sessp, TIMEV_PATH);
	free((char *) sessp);
	return (1);
}

void
move_object(Object_header *oldp, Object_header *newp)
	{
	int size = Object_Size(oldp);

	assert((oldp) && (newp) && (size) && (size&3) == 0 );
	memcpy((char *) newp, (char *) oldp, size);
} 

Object_header *
copy_object(Object_header *oldp)
	{
	int size = Object_Size(oldp);
	Object_header *newp = malloc(size);

	if (!(oldp))
		return(NULL);
	assert((newp));
	move_object(oldp, newp);
	return(newp);
}

/*  Manipulate framed objects (Fobject's)
 *
 */

/*	copy_obj2Fobj: Copy object into framed object
 */
Fobject *
copy_obj2Fobj(Object_header *oldp)
	{
	int size = Object_Size(oldp) ;
	Fobject *newp;

	if (!(oldp))
		return(NULL);
	newp = malloc(size + sizeof(Fobject *));
	if (!newp)
		return(NULL);
	newp->Fobj_next = NULL;
	memcpy((char *) &newp->Fobj_objhdr, (char *) oldp, size);
	return(newp);
}

/*	FQkill: delete list of framed objects.
 */
void
FQkill(Fobject **head)
	{
	Fobject	*fop;

	while ((fop = FQrmv(head)))
		free(fop);
}

void
FQins(Fobject *in, Fobject **head)
	{
	in->Fobj_next = *head;
	*head = in;
}

Fobject *FQrmv(Fobject **head)
	{
	Fobject *topfop = *head;

	if (topfop)
		*head = topfop->Fobj_next;
	return(topfop);
}

void
FQconcat(Fobject *newp, Fobject **head)
	{
	Fobject *fop, *fcp;               

	for (fop = newp; fop; fop = fop->Fobj_next) {
		fcp = copy_obj2Fobj(&fop->Fobj_objhdr);
		if (!fcp)
			return;
		FQins(fcp, head);
	}
}

/*	merge_UnkObjL2: merge new unknown-object list into target list,
 *	retaining only one copy of each distinct object.
 *	If IsCpy is TRUE, do malloc for Fobjects added to target list and
 *	leave new list unchanged. If ISCpy is FALSE, destroy the new object
 *	list, either moving new objects into the target list or freeing them.
 */
void
merge_UnkObjL2(Fobject **newLpp, Fobject **targLpp, int IsCpy)
	{
	Fobject 	**prevpp, *newp, *fop;
	Object_header	*newobjp;

	for (prevpp = newLpp; *prevpp; prevpp = &(*prevpp)->Fobj_next) {
	    newp = (*prevpp)->Fobj_next;
	    newobjp = &newp->Fobj_objhdr;

	    for (fop = *targLpp; fop; fop = fop->Fobj_next) {
		Object_header	*tobjp = &fop->Fobj_objhdr;
		
		if (Obj_Length(tobjp) == Obj_Length(newobjp) &&
		    memcmp((char *)tobjp, (char *)&newobjp, Obj_Length(tobjp))
								== 0)
			break;
	    }
	    if (!fop) {
		if (IsCpy)
			FQins(copy_obj2Fobj(newobjp), targLpp);
		else
			FQins(FQrmv(prevpp), targLpp);
	    }
	    else if (!IsCpy)
			free(FQrmv(prevpp));
	}	
}

int
match_policy(POLICY_DATA *pdo1p, POLICY_DATA *pdo2p)
	{
	if (Obj_Length(pdo1p) != Obj_Length(pdo2p))
		return(0);
	if (memcmp((char *)pdo1p, (char *)pdo2p, Obj_Length(pdo1p)))
		return 0;
	return(1);
}



/*	Given a consecutive area with room for a packet structure,
 *	a map vector, and a packet buffer, initialize the packet
 *	structure.
 */
struct packet *
new_packet_area(packet_area *pap)
	{
	struct packet	*pkt = (struct packet *) pap;
	packet_map	*mapp = (packet_map *) (pkt+1);
	char		*buffp = (char *)(mapp+1);
	common_header	*hdrp;

	pkt->pkt_offset = 0;
	hdrp = (common_header *)(buffp /* + pkt->pkt_offset */);
	pkt->pkt_len = 0;
	pkt->pkt_data =	hdrp;		/* Addr of msg in buffer */
	pkt->pkt_map = mapp;		/* Addr of map vector */
	pkt->pkt_flags = pkt->pkt_ttl = 0;
	pkt->pkt_order = BO_HOST;
		/* Clear packet map */
	memset((char *) mapp, 0, sizeof(packet_map));
	return(pkt);
}


/*
 *	Create new SCOPE object with specified number of slots.
 */
SCOPE *
new_scope_obj(int count)
	{
	int size = count*sizeof(Scope_list_ipv4) + sizeof(Object_header);
	/*  XXX check for IPv4 vs IP6 address */
	SCOPE *scp;

	scp = (SCOPE *)malloc(size);
	assert(scp);
	Obj_Length(scp) = size;
	Obj_Class(scp) = class_SCOPE;
	Obj_CType(scp) = ctype_SCOPE_list_ipv4;
	return(scp);
}

/*
 *	Search SCOPE list for given IP address; return ordinal # of
 *	entry, or -1 if not found.
 */
int
search_scope(SCOPE *scp, struct in_addr *ipaddrp)
	{
	int i, n = Scope_Cnt(scp);
	u_int32_t	*adp = (u_int32_t *) &scp->scope4_addr->s_addr;

	for (i = 0; i < n; i++, adp++) {
		if (*adp == ipaddrp->s_addr)
			return(i);
	}
	return(-1);
}



#define INIT_SCOPE_LEN 64
#define MAX_SCOPE_LEN 65536

/*
 *	scope_catf():  Catenate IP address from given FILTER_SPEC onto
 *			existing SCOPE list, which is assumed to be ordered,
 *			and return pointer to new SCOPE list.  Create SCOPE
 *			list if necessary.  Ignore a duplicate address.
 *			*Assumes IPv4*
 */
void
scope_catf(SCOPE ** scppp, FILTER_SPEC *filtp)
	{
	scope_cat(scppp, &filtp->filt_srcaddr);
}


void
scope_cat(SCOPE ** scppp, struct in_addr *ipaddrp)
	{
	int N, L;
	struct in_addr *newp, *oldp;
	SCOPE	*scpp, *new_scpp;

	scpp = *scppp;
	if (!scpp) {
		/*	First time... set up object
		 *	We use the object length field to keep track of
		 *	the number of entries at present.  The size of
		 *	malloc'd area is nearest power of 2 that is >=
		 *	this len, but at least INIT_SCOPE_LEN.
		 */
		scpp = (SCOPE *) malloc(INIT_SCOPE_LEN);
		if (!scpp)
			return;
		Init_Var_Obj(scpp, SCOPE, ctype_SCOPE_list_ipv4,INIT_SCOPE_LEN);
		Obj_Length(scpp) = sizeof(Object_header);
	}
	else {
		/*	Ignore a duplicate of the last ip addr in list.
		 */
		newp = (struct in_addr *)((char *)scpp + Obj_Length(scpp));
		oldp = newp - 1;
		if (oldp->s_addr == ipaddrp->s_addr)
			return;
		assert(oldp->s_addr < ipaddrp->s_addr);  /* Must be increasing */
	}
	/*	Compute size of existing area
	 */
	L = Obj_Length(scpp);
	for (N = INIT_SCOPE_LEN; N < MAX_SCOPE_LEN; N <<= 1)
		if (N >= L)
			break;
	if (L + sizeof(struct in_addr) > N) {
		/* Overflow.  Malloc a new object area of double size,
		 *	copy into it, and free original one.
		 */
		new_scpp = (SCOPE *) malloc(N+N);
		if (!new_scpp) {
			/* XXX ?? */
			return;
		}
		memset(new_scpp, 0, N+N);
		memcpy(new_scpp, scpp, L);
		free(scpp);
		scpp = new_scpp;
	}
	newp = (struct in_addr *)((char *)scpp + Obj_Length(scpp));
	*newp = *ipaddrp;
	Obj_Length(scpp) += sizeof(struct in_addr);
	*scppp = scpp;
}

/*
 *  form_scope_union(): Form global union of all SCOPE lists for given session,
 *		if it does not already exist, with local senders removed.
 *
 *		Turn on scope bit in each matching PSB.
 */
int
form_scope_union(Session *destp)
	{
	RSB		*rp;
	PSB		*sp;

	if (destp->d_flags & SESSF_HaveScope)
		return(0);

	for (rp = destp->d_RSB_list; rp != NULL; rp = rp->rs_next) {
	    if (rp->rs_scope)
		merge_SCOPEs(destp, rp->rs_scope);
	    else {
		/*
		 *	No scope list.  Add to union all senders that route
		 *	to this RSB.
		 */
		for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
		    if (IsAPI(sp) || 
				!IsRoutePSB2nhop(destp, sp, &rp->rs_rsvp_nhop)) 
			continue;
		    sp->ps_flags |= PSBF_InScope;
		}
	    }
	}
	destp->d_flags |= SESSF_HaveScope;
	return(0);
}

/*
 *  clear_scope_union(): Delete existing scope union (ie turn off scope
 *	flag bits in all PSBs.  Union will be recomputed when needed.
 */
void
clear_scope_union(Session *destp)
	{
	PSB	*sp;

	if ((destp->d_flags & SESSF_HaveScope) == 0)
		return;
	for (sp = destp->d_PSB_list ; sp != NULL; sp = sp->ps_next)
		sp->ps_flags &= ~PSBF_InScope;
	destp->d_flags &= ~SESSF_HaveScope;
}
	

/*
 *	Marge scope list into scope union, by turning on scope bit in each
 *	matching PSB that is not local API.  We are doing a linear search
 *	of PSBs, but could use a hash table.

 */
void
merge_SCOPEs(Session *destp, SCOPE *scp)
	{
	PSB		*sp;
	struct in_addr	*adrp, *ENDadrp = (struct in_addr *) Next_Object(scp);

	assert(scp);

	for (adrp = (struct in_addr *) Obj_data(scp); adrp < ENDadrp; adrp++) {
		for (sp = destp->d_PSB_list ; sp != NULL; sp = sp->ps_next) {
			if (IsAPI(sp))
				continue;
			if (adrp->s_addr == sp->ps_phop.s_addr) {
				sp->ps_flags |= PSBF_InScope;
				break;
			}
		}
		adrp++;
	}
}

/*
 *  Check to see if two SCOPE's are equal
 *
 *	return 1 if true, and 0 otherwise
 */
int
match_scope(SCOPE *s1, SCOPE *s2) {
	int i, j;
	int count;
	struct in_addr *sin1, *sin2;

	if (!s1) {
		return(!s2);
	}
	else if (!s2)
		return(0);
	if (Obj_Length(s1) != Obj_Length(s2))
		return(0);
	count = Scope_Cnt(s1);
	sin1 = s1->scope4_addr;
	sin2 = s2->scope4_addr;
	for (i = 0; i < count; i++) {
		for (j = 0; j < count; j++)
			if (sin1[i].s_addr == sin2[j].s_addr)
				break;
		if (j == count)
			return(0);
	}
	return(1);
}
