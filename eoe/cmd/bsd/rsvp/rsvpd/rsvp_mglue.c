/*
 * glue functions for mib functions.
 *
 * rsvp_xxx -> rsvp_mglue.c -> rsvp_mib.c
 *
 * $Id: rsvp_mglue.c,v 1.1 1998/11/25 08:43:14 eddiem Exp $
 */

#ifdef _MIB

#include <sys/types.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <strings.h>
#include <malloc.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <alloca.h>
#include <errno.h>
#include <net/tc_isps.h>
#include <net/rsvp.h>

#include "rsvp_daemon.h"
#include "rsvp_intserv.h"
#include "rsvp_mib.h"

/* 
 * if_vec a vector of all the interfaces rsvpd knows about.  
 * if_num is the number of physical interfaces.
 * vif_num+1 is how many elements are in the vector.
 * if_vec[vif_num] is the API interface.
 */
extern if_rec *if_vec;
extern int if_num;
extern int vif_num;

/*
 *********************************************************************************
 *
 * Glue code for the rsvpSessionTable (1)
 *
 *********************************************************************************
 */
void *
mglue_new_session(Session *sessp)
{
     rsvpSessionEntry_t *entryp;
     int alen, plen;

     /*
      * Fill out a session entry and insert it into the MIB session table.
      */
     entryp = calloc(1, sizeof(rsvpSessionEntry_t));
     if (entryp == NULL) {
	  log(LOG_ERR, 0, "malloc failed for new rsvpSenderEntry\n");
	  return NULL;
     }

     entryp->rsvpSessionType = sessp->d_session->sess_header.obj_ctype;
     if ((entryp->rsvpSessionType == ctype_SESSION_ipv4) ||
	 (entryp->rsvpSessionType == ctype_SESSION_ipv4GPI)) {
	  alen = 4;
	  plen = 2;
     } else {
	  alen = 16;
	  plen = 4;
     }

     octetstring_fill(&(entryp->rsvpSessionDestAddr),
		      (void *) &(sessp->d_addr.s_addr), alen);
     entryp->rsvpSessionDestAddrLength = alen;
     entryp->rsvpSessionProtocol = sessp->d_protid;
     octetstring_fill(&(entryp->rsvpSessionPort), (void *) &(sessp->d_port), plen);
     rsvpd_insert_rsvpSessionEntry(entryp);

     return ((void *) entryp);
}


/*
 *********************************************************************************
 *
 * Glue code for the rsvpSenderTable (2)
 *
 *********************************************************************************
 */
void *
mglue_new_sender(Session *destp, PSB *psbp, struct packet *pkt)
{
     rsvpSenderEntry_t *entryp;
     int alen, plen;

     entryp = calloc(1, sizeof(rsvpSenderEntry_t));
     if (entryp == NULL) {
	  log(LOG_ERR, 0, "malloc failed for new rsvpSenderEntry\n");
	  return NULL;
     }

     entryp->rsvpSenderType = destp->d_session->sess_header.obj_ctype;
     if ((entryp->rsvpSenderType == ctype_SESSION_ipv4) ||
	 (entryp->rsvpSenderType == ctype_SESSION_ipv4GPI)) {
	  alen = 4;
	  plen = 2;
     } else {
	  alen = 16;
	  plen = 4;
     }

     /*
      * session or destination address, address length, port, protocol.
      * This info comes from the RSVP SESSION object.
      */
     octetstring_fill(&(entryp->rsvpSenderDestAddr),
		       (void *) &(destp->d_addr.s_addr), alen);
     entryp->rsvpSenderDestAddrLength = alen;
     octetstring_fill(&(entryp->rsvpSenderDestPort), (void *) &(destp->d_port),
		      plen);
     entryp->rsvpSenderProtocol = destp->d_protid;

     /*
      * The address and port of this sender to this session.  This info
      * comes from the SENDER_TEMPLATE object.
      */
     octetstring_fill(&(entryp->rsvpSenderAddr),
		      (void *) &(psbp->ps_templ->filt_srcaddr.s_addr), alen);
     entryp->rsvpSenderAddrLength = alen;
     octetstring_fill(&(entryp->rsvpSenderPort),
		      (void *) &(psbp->ps_templ->filt_srcport), plen);

     entryp->rsvpSenderFlowId = 0;	/* for IPv6 */

     /*
      * previous hop info.  This just comes from the RSVP_HOP object.
      */
     rsvpd_update_rsvpSenderPhop((void *) entryp, &(psbp->ps_rsvp_phop));

     if ((psbp->ps_in_if >= 0) && (psbp->ps_in_if < if_num))
	  entryp->rsvpSenderInterface = if_vec[psbp->ps_in_if].if_index;
     else
	  entryp->rsvpSenderInterface = if_num;

     rsvpd_update_rsvpSenderTspec((void *) entryp, psbp->ps_tspec);

     /* type refreshinterval, defined by RSVP MIB as ms */
     entryp->rsvpSenderInterval = destp->d_timevalp.timev_R;
     if (psbp->ps_flags & PSBF_NonRSVP)
	  entryp->rsvpSenderRSVPHop = TRUTHVALUE_FALSE;
     else
	  entryp->rsvpSenderRSVPHop = TRUTHVALUE_TRUE;

     /*
      * Leave this blank for now.  But should copy whatever policy objects
      * are present in the incoming packet.
      */
     entryp->rsvpSenderPolicy.val = NULL;
     entryp->rsvpSenderPolicy.len = 0;

     /* skip the Adspec stuff for now. */
     entryp->rsvpSenderAdspecBreak = TRUTHVALUE_FALSE;
     entryp->rsvpSenderAdspecGuaranteedSvc = TRUTHVALUE_FALSE;
     entryp->rsvpSenderAdspecGuaranteedBreak = TRUTHVALUE_FALSE;
     entryp->rsvpSenderAdspecCtrlLoadSvc = TRUTHVALUE_FALSE;
     entryp->rsvpSenderAdspecCtrlLoadBreak = TRUTHVALUE_FALSE;

     entryp->rsvpSenderTTL = pkt->pkt_ttl;

     rsvpd_insert_rsvpSenderEntry(destp->mib_sessionhandle, entryp);

     return( (void *) entryp);
}


void *
mglue_new_resv(Session *destp, RSB *rp, struct packet *pkt)
{
     rsvpResvEntry_t *entryp;
     int alen, plen;
     char zerobuf[16];
     int zeroport=0;

     entryp = calloc(1, sizeof(rsvpResvEntry_t));
     if (entryp == NULL) {
	  log(LOG_ERR, 0, "malloc failed for new rsvpResvEntry\n");
	  return NULL;
     }

     bzero(zerobuf, 16);

     entryp->rsvpResvType = destp->d_session->sess_header.obj_ctype;
     if ((entryp->rsvpResvType == ctype_SESSION_ipv4) ||
	 (entryp->rsvpResvType == ctype_SESSION_ipv4GPI)) {
	  alen = 4;
	  plen = 2;
     } else {
	  alen = 16;
	  plen = 4;
     }

     octetstring_fill(&(entryp->rsvpResvDestAddr),
		      (void *) &(destp->d_addr.s_addr), alen);
     if (rp->rs_style == STYLE_WF) {
	  octetstring_fill(&(entryp->rsvpResvSenderAddr),
			   (void *) zerobuf, alen);
	  octetstring_fill(&(entryp->rsvpResvPort), (void *) &zeroport, plen);
     } else {
	  /* For SE resv, there could be a list of senders assocated with
	   * this resv.  Just copy the first one? */
	  octetstring_fill(&(entryp->rsvpResvSenderAddr),
			   (void *) &(rp->rs_Filtp(0)->filt_srcaddr), alen);
	  octetstring_fill(&(entryp->rsvpResvPort),
			   (void *) &(rp->rs_Filtp(0)->filt_srcport), plen);
     }
     entryp->rsvpResvDestAddrLength = alen;
     entryp->rsvpResvSenderAddrLength = alen;
     entryp->rsvpResvProtocol = destp->d_protid;
     octetstring_fill(&(entryp->rsvpResvDestPort), (void *) &(destp->d_port), plen);
     octetstring_fill(&(entryp->rsvpResvHopAddr),
		      (void *) &(rp->rs_rsvp_nhop.hop4_addr), alen);
     entryp->rsvpResvHopLih = rp->rs_rsvp_nhop.hop4_LIH;

     if ((pkt->pkt_in_if >= 0) && (pkt->pkt_in_if < if_num))
	  entryp->rsvpResvInterface = if_vec[pkt->pkt_in_if].if_index;
     else
	  entryp->rsvpResvInterface = if_num;

     entryp->rsvpResvService = rp->rs_spec->flow_body.spec_u.CL_spec.CL_spec_serv_hdr.issh_service;
     rsvpd_update_rsvpResvTspec((void *) entryp, rp->rs_spec);

     /* leave the guarenteed stuff blank for now */
     entryp->rsvpResvRSpecRate = 0;
     entryp->rsvpResvRSpecSlack = 0;
     entryp->rsvpResvInterval = rp->rs_Filt_TTD(0);
     entryp->rsvpResvScope.val = NULL; /* xxx don't handle for now */

     if (rp->rs_style & Opt_Shared)
	  entryp->rsvpResvShared = TRUTHVALUE_TRUE;
     else
	  entryp->rsvpResvShared = TRUTHVALUE_FALSE;

     if (rp->rs_style & Opt_Explicit)
	  entryp->rsvpResvExplicit = TRUTHVALUE_TRUE;
     else
	  entryp->rsvpResvExplicit = TRUTHVALUE_FALSE;

     if (pkt->pkt_ttl != pkt->pkt_data->rsvp_snd_TTL)
	  entryp->rsvpResvRSVPHop = TRUTHVALUE_FALSE;
     else
	  entryp->rsvpResvRSVPHop = TRUTHVALUE_TRUE;

     entryp->rsvpResvPolicy.val = NULL; /* xxx leave blank for now */
     entryp->rsvpResvTTL = pkt->pkt_data->rsvp_snd_TTL;
     entryp->rsvpResvFlowId = 0;

     rsvpd_insert_rsvpResvEntry(destp->mib_sessionhandle, entryp);

     return ((void *) entryp);
}


void *
mglue_new_resvfwd(Session *destp, RSB *rp, struct packet *pkt)
{
     rsvpResvFwdEntry_t *entryp;
     PSB *psbp;
     int alen, plen;
     char zerobuf[16];

     entryp = calloc(1, sizeof(rsvpResvFwdEntry_t));
     if (entryp == NULL) {
	  log(LOG_ERR, 0, "malloc failed for new rsvpResvEntry\n");
	  return NULL;
     }

     bzero(zerobuf, 16);

     entryp->rsvpResvFwdType = destp->d_session->sess_header.obj_ctype;
     if ((entryp->rsvpResvFwdType == ctype_SESSION_ipv4) ||
	 (entryp->rsvpResvFwdType == ctype_SESSION_ipv4GPI)) {
	  alen = 4;
	  plen = 2;
     } else {
	  alen = 16;
	  plen = 4;
     }

     octetstring_fill(&(entryp->rsvpResvFwdDestAddr),
		      (void *) &(destp->d_addr.s_addr), alen);
     if (rp->rs_style == STYLE_WF) {
	  octetstring_fill(&(entryp->rsvpResvFwdSenderAddr),
			   (void *) zerobuf, alen);
	  octetstring_fill(&(entryp->rsvpResvFwdPort), (void *) &zerobuf, plen);
     } else {
	  /* For SE resv, there could be a list of senders assocated with
	   * this resv.  Just copy the first one? */
	  octetstring_fill(&(entryp->rsvpResvFwdSenderAddr),
			   (void *) &(rp->rs_Filtp(0)->filt_srcaddr), alen);
	  octetstring_fill(&(entryp->rsvpResvFwdPort),
			   (void *) &(rp->rs_Filtp(0)->filt_srcport), plen);
     }
     entryp->rsvpResvFwdDestAddrLength = alen;
     entryp->rsvpResvFwdSenderAddrLength = alen;
     entryp->rsvpResvFwdProtocol = destp->d_protid;
     octetstring_fill(&(entryp->rsvpResvFwdDestPort), (void *) &(destp->d_port), plen);
     octetstring_fill(&(entryp->rsvpResvFwdHopAddr),
		      (void *) &(rp->rs_rsvp_nhop.hop4_addr), alen);
     entryp->rsvpResvFwdHopLih = rp->rs_rsvp_nhop.hop4_LIH;

     psbp = destp->d_PSB_list;
     if ((psbp->ps_in_if >= 0) && (psbp->ps_in_if < if_num))
	  entryp->rsvpResvFwdInterface = if_vec[psbp->ps_in_if].if_index;
     else
	  entryp->rsvpResvFwdInterface = if_num;

     entryp->rsvpResvFwdService = rp->rs_spec->flow_body.spec_u.CL_spec.CL_spec_serv_hdr.issh_service;
     rsvpd_update_rsvpResvFwdTspec((void *) entryp, rp->rs_spec);

     /* leave the guarenteed stuff blank for now */
     entryp->rsvpResvFwdRSpecRate = 0;
     entryp->rsvpResvFwdRSpecSlack = 0;
     entryp->rsvpResvFwdInterval = rp->rs_Filt_TTD(0);
     entryp->rsvpResvFwdScope.val = NULL; /* xxx don't handle for now */

     if (rp->rs_style & Opt_Shared)
	  entryp->rsvpResvFwdShared = TRUTHVALUE_TRUE;
     else
	  entryp->rsvpResvFwdShared = TRUTHVALUE_FALSE;

     if (rp->rs_style & Opt_Explicit)
	  entryp->rsvpResvFwdExplicit = TRUTHVALUE_TRUE;
     else
	  entryp->rsvpResvFwdExplicit = TRUTHVALUE_FALSE;

     if (pkt->pkt_ttl != pkt->pkt_data->rsvp_snd_TTL)
	  entryp->rsvpResvFwdRSVPHop = TRUTHVALUE_FALSE;
     else
	  entryp->rsvpResvFwdRSVPHop = TRUTHVALUE_TRUE;

     entryp->rsvpResvFwdPolicy.val = NULL; /* xxx leave blank for now */
     entryp->rsvpResvFwdTTL = pkt->pkt_data->rsvp_snd_TTL;
     entryp->rsvpResvFwdFlowId = 0;

     rsvpd_insert_rsvpResvFwdEntry(destp->mib_sessionhandle, entryp);

     return ((void *) entryp);
}

#endif /* _MIB */
