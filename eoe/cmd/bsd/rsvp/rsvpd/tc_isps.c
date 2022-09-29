
/*
 * @(#) $Id: tc_isps.c,v 1.10 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_isps.c  *******************************
 *                                                                   *
 *  Adaptation Module: Converts RSVP's TC interface into kernel      *
 *	interface for MIT ISPS kernel.                               *
 *                                                                   *
 *********************************************************************/

/*
 * Copyright 1994, 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stddef.h>

#include "rsvp_daemon.h"
#include "rapi_lib.h"		/* Define flowspec formats */
#include "rsvp_specs.h"		/* Flowspec descriptor format */
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <cap_net.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net/tc_isps.h>

#ifdef _MIB
#include "rsvp_mib.h"
extern void bcopy(void *, void *, int);
#endif

/* external declarations */
static int	udp_socket = -1;

static int	kern_translate_error __P((int kernel_error));
static int	kern_ispsclear __P((int));
static int	rsvp2isps_spec __P((FLOWSPEC *, SENDER_TSPEC *,
				    isps_fs_t *, int));
static isps_filt_t *
		rsvp2isps_filter __P((FILTER_SPEC *, Session *, int));
static void	log_isps_filter __P((isps_filt_t *, int));

/*** static char	*fmt_isps_fs __P((isps_fs_t *)); ***/

#ifndef hton16
#define hton16(x)	((u_int16_t)htons((u_int16_t)(x)))
#define HTON16(x)	HTONS(x)
#define hton32(x)	((u_int32_t)htonl((u_int32_t)(x)))
#define HTON32(x)	HTONL(x)
#endif

#define TC_flags (TCF_E_POLICE | TCF_M_POLICE | TCF_B_POLICE)
	
/************************************************************************
 *
 *	Interface routines to call the ISPS kernel functions
 * 
 ************************************************************************/

/* Note: these calls use the address of the kern_resv block as a parameter
 *	to carry the interface number.  This block actually contains the
 *	the other parameters too, but we leave the parameters explicit to
 *	make the code exactly parallel the Functional Specification.
 */
void
TC_init(int kernel_socket)
	{
	int i;

	/*  Initialize UDP socket used to communicate with the
	 *	kernel traffic control code.
	 */
	udp_socket = kernel_socket;

        /* clear old reservations from the kernel and then make a
	 *	reservation for RSVP packets, on each interface.
	 */
        for (i = 0; i < if_num; i++) {
                kern_ispsclear(i);
        }
}

/*
 * TC_AddFlowspec(): Call the kernel to make reservation for a flow.
 * 	It checks admission control, and returns a handle for the
 *	reservation, or -1 if an error.  It may also set *fwd_specpp
 * 	to point to a modified flowspec to be forwarded.
 */
u_long
TC_AddFlowspec(int OIf, FLOWSPEC *spec, SENDER_TSPEC *stspec, int flags,
							FLOWSPEC **fwd_specpp)
	{
	char		*if_name;
	struct ispsreq	 ir;
	struct ispsreq	*irp = &ir;
	int rc;
#ifdef _MIB
	if_rec		*ifvecp;
	int		bkt;
	intSrvFlowEntry_t *flowp;
#endif

	rsvp_errno = 0;
	*fwd_specpp = NULL;
	if (OIf == vif_num)
		return (TC_OK);  /* don't call kernel for API */
	if_name = if_vec[OIf].if_name;
	if (!if_vec[OIf].if_up) {
		rsvp_errno = Set_Errno(RSVP_Err_TC_ERROR, 0);
		return(TC_ERROR);
	}

	if (rsvp2isps_spec(spec, stspec, &irp->iq_fs, (flags&TC_flags)) < 0)
		return (TC_ERROR);

	strncpy(irp->iq_name, if_name, IFNAMSIZ);
	irp->iq_function = IF_ADDFLW;

	rc = cap_network_ioctl(udp_socket, SIOCIFISPSCTL, irp);
	if (rc < 0) {
		printf("TC_AddFlowspec kernel errno %d\n", errno);
		rsvp_errno = kern_translate_error(errno);
	/****	log(LOG_ERR, errno, "SIOCIFADDFLW");	****/
		return (TC_ERROR);
	}
#ifdef _MIB
	if (mib_enabled) {
	     /*
	      * Now that the reservation has been accepted, update the
	      * IfAttribEntry table.
	      */
	     ifvecp = &(if_vec[OIf]);
	     bkt = irp->iq_fs.fs_tspec.ts_c.c_bkt;
	     ifvecp->if_numflows++;
	     ifvecp->if_numbufs += bkt;
	     rc = rsvpd_update_intSrvIfAttribEntry(ifvecp->if_index,
						   ifvecp->if_numbufs,
						   ifvecp->if_numflows);
	     /* log the error, but its not fatal */
	     if (rc < 0)
		  log(LOG_ERR, 0, "Error during add update of MIB IntSrvIfAttribTable\n");

	     /*
	      * Insert a new entry into the intSrvFlowTable.
	      */
	     flowp = malloc(sizeof(intSrvFlowEntry_t));
	     if (flowp == NULL) {
		  log(LOG_ERR, 0, "Malloc for new intSrvFlowEntry failed\n");
	     } else {
		  int addrlen=4;

		  bzero(flowp, sizeof(intSrvFlowEntry_t));
		  flowp->intSrvFlowType = ctype_SESSION_ipv4;
		  if (flowp->intSrvFlowType == ctype_SESSION_ipv6)
		       addrlen = 16;
		  flowp->intSrvFlowOwner = FLOWOWNER_RSVP;
		  flowp->intSrvFlowDestAddrLength = addrlen;
		  flowp->intSrvFlowSenderAddrLength = addrlen;
		  flowp->intSrvFlowInterface = ifvecp->if_index;
		  flowp->intSrvFlowIfAddr.val = (u_char *) malloc(addrlen);
		  if (flowp->intSrvFlowIfAddr.val == NULL) {
		       log(LOG_ERR, 0, "Malloc for FlowIfAddr OCTETSTRING failed\n");
		  } else {
		       bcopy((void *) &(ifvecp->if_orig),
			     (void *) flowp->intSrvFlowIfAddr.val, addrlen);
		       flowp->intSrvFlowIfAddr.len = addrlen;
		  }
		  flowp->intSrvFlowRate = irp->iq_fs.fs_tspec.ts_c.c_rate * 8;
		  flowp->intSrvFlowBurst = bkt;
		  flowp->intSrvFlowWeight = 0;
		  flowp->intSrvFlowQueue = 0;
		  flowp->intSrvFlowMinTU = irp->iq_fs.fs_tspec.ts_c.c_m;
		  flowp->intSrvFlowMaxTU = irp->iq_fs.fs_tspec.ts_c.c_M;
		  flowp->intSrvFlowBestEffort = 0;
		  flowp->intSrvFlowPoliced = 0;
		  flowp->intSrvFlowDiscard = TRUTHVALUE_FALSE;
		  flowp->intSrvFlowService = QOS_CONTROLLEDLOAD;
		  flowp->intSrvFlowOrder = 0;

		  rsvpd_insert_intSrvFlowEntry(irp->iq_handle, flowp);
	     }
	}
#endif /* _MIB */

	return (irp->iq_handle);
}

/*
 * TC_DelFlowspec(): This routine deletes flow for specified handle
 */
int
TC_DelFlowspec(int OIf, u_long rhandle) {
	char           *if_name;
	struct ispsreq  ir;
#ifdef _MIB
	if_rec		*ifvecp;
	int		bkt, rc;
#endif

	rsvp_errno = 0;
	if (OIf == vif_num)
		return (TC_OK);  /* don't call kernel for API */
	if_name = if_vec[OIf].if_name;
	if (!if_vec[OIf].if_up) {
		rsvp_errno = Set_Errno(RSVP_Err_TC_ERROR, 0);
		return(TC_ERROR);
	}
	strncpy(ir.iq_name, if_name, IFNAMSIZ);
	ir.iq_handle = rhandle;
	ir.iq_function = IF_DELFLW;
	if (cap_network_ioctl(udp_socket, SIOCIFISPSCTL, &ir) < 0) {
		rsvp_errno = kern_translate_error(errno);
	/***	log(LOG_ERR, errno, "SIOCIFDELFLW");  ***/
		return (TC_ERROR);
	}

#ifdef _MIB
	if (mib_enabled) {
	     /*
	      * Now that a flow has been torn down, notify the MIB module.
	      */
	     ifvecp = &(if_vec[OIf]);
	     bkt = rsvpd_get_intSrvFlowEntry_bkt((int) rhandle);
	     rsvpd_deactivate_intSrvFlowEntry((int) rhandle);

	     /*
	      * Update the intSrvIfAttribTable
	      */
	     ifvecp->if_numflows--;
	     ifvecp->if_numbufs -= bkt;
	     rc = rsvpd_update_intSrvIfAttribEntry(ifvecp->if_index,
						   ifvecp->if_numbufs,
						   ifvecp->if_numflows);
	     /* log the error, but its not fatal */
	     if (rc < 0)
		  log(LOG_ERR, 0, "Error during del update of MIB IntSrvIfAttribTable\n");
	}
#endif /* _MIB */

	return (TC_OK);
}

/*
 * TC_AddFilter(): Adds a filter for an existing flow.
 *
 *	Returns fhandle or TC_ERROR.
 */
u_long
TC_AddFilter(int OIf, u_long rhandle, Session *dest, FILTER_SPEC *filtp) {
	char		*if_name;
	struct ispsreq   ir;
	isps_filt_t	*ifp = NULL;
	int rc;

	rsvp_errno = 0;
	if (OIf == vif_num)
		return (TC_OK);  /* don't call kernel for API */
	if_name = if_vec[OIf].if_name;
	if (!if_vec[OIf].if_up) {
		rsvp_errno = Set_Errno(RSVP_Err_TC_ERROR, 0);
		return(TC_ERROR);
	}

	strncpy(ir.iq_name, if_name, IFNAMSIZ);
	ir.iq_handle = rhandle;
	ir.iq_function = IF_SETFILT;
	ifp = rsvp2isps_filter(filtp, dest, 0);
	if (ifp == NULL) {
		return (TC_ERROR);
	}
	ir.iq_function = IF_SETFILT;
	ir.iq_filt = *ifp;

	rc = cap_network_ioctl(udp_socket, SIOCIFISPSCTL, &ir);
	if (rc < 0) {
		printf("TC_AddFilter: kernel errno %d\n", errno);

		/* Since we constructed kernel filter here, assume for now
		 *	that any error must be internal, between us and
		 *	the kernel.
		 * 
		 * Note that this will happen, because the isps kernel
		 * has stricter rules for filters than RSVP currently does
		 * (since the RSVP rules are currently somewhat broken).
		 */
		rsvp_errno = kern_translate_error(errno);
	/****	log(LOG_ERR, 0, "SIOCIFSETFILT\n");	****/
		return (TC_ERROR);
	}
	if (IsDebug(DEBUG_EVENTS)) {
		log(LOG_DEBUG, 0, "       kernel handle=0x%x\n", rhandle);
		log(LOG_DEBUG, 0, "  ISPS_FILT:\n");
		log_isps_filter(ifp, 5);
	}

#ifdef _MIB
	/*
	 * This completes the second and final step of activating a flow.  We
	 * now have the address and port information needed for the flow entry.
	 * If this is the second, third, etc.. filter for this flow, then
	 * the MIB code simply increases the ref. count for the flow.  It does
	 * not have space to store these additional filters.
	 */
	if (mib_enabled) {
	     rc = rsvpd_activate_intSrvFlowEntry((int) rhandle, ifp->f_ipv4_protocol,
						 &(ifp->f_ipv4_srcaddr), ifp->f_ipv4_srcport,
						 &(ifp->f_ipv4_destaddr), ifp->f_ipv4_destport);
	     if (rc < 0)
		  log(LOG_ERR, 0, "Error during activate of intSrvFlowEntry %d\n",
		      rhandle);
	}
#endif /* _MIB */

	/* For now, until kernel implements fhandles, use rhandle as
	 * the fhandle.
	 */
	return (rhandle);
}


/*
 * TC_DelFilter(): Deletes existing filter.
 */
int
TC_DelFilter(int OIf, u_long rhandle)
	{
	char           *if_name;
	struct ispsreq  ir;
	isps_filt_t	*ifp = NULL;

	rsvp_errno = 0;
	if (OIf == vif_num)
		return (TC_OK);  /* don't call kernel for API */
	if_name = if_vec[OIf].if_name;
	if (!if_vec[OIf].if_up) {
		rsvp_errno = Set_Errno(RSVP_Err_TC_ERROR, 0);
		return(TC_ERROR);
	}

	strncpy(ir.iq_name, if_name, IFNAMSIZ);
	ir.iq_handle = rhandle;
	ir.iq_function = IF_SETFILT;
	ir.iq_filt.f_pf = PF_UNSPEC; /* a NULL filter: deletes it */

	if (cap_network_ioctl(udp_socket, SIOCIFISPSCTL, &ir) < 0) {
		/* Since we constructed kernel filter here, assume for now
		 *	that any error must be internal, between us and
		 *	the kernel.
		 * 
		 * Note that this will happen, because the isps kernel
		 * has stricter rules for filters than RSVP currently does
		 * (since the RSVP rules are currently somewhat broken).
		 */
		rsvp_errno = kern_translate_error(errno);
	/****	log(LOG_ERR, 0, "SIOCIFSETFILT\n");	****/
		return (TC_ERROR);
	}
	if (IsDebug(DEBUG_EVENTS)) {
		log(LOG_DEBUG, 0, "       kernel handle=0x%x\n", rhandle);
		log(LOG_DEBUG, 0, "  ISPS_FILT:\n");
		log_isps_filter(ifp, 5);
	}

#ifdef _MIB
	if (mib_enabled) {
	     if (rsvpd_dec_intSrvFlowEntry((int) rhandle))
		  log(LOG_ERR, 0, "error while decrementing intSrvFlowEntry %d\n",
		      rhandle);
	}
#endif
	return (TC_OK);
}

/*
 * TC_ModFlowspec(): Modifies a flowspec of a given flow.
 *
 *	It may also set *fwd_specpp to point to a modified flowspec to
 *	be forwarded.
 */
int
TC_ModFlowspec(int OIf, u_long rhandle,
	 		FLOWSPEC *specp, SENDER_TSPEC *stspecp, int flags,
					FLOWSPEC **fwd_specpp)
	{
	char           *if_name;
	struct ispsreq	ir;
	struct ispsreq	*irp = &ir;
#ifdef _MIB
	int delta_bkt, old_bkt, new_bkt, new_rate, rc;
#endif

	rsvp_errno = 0;
	*fwd_specpp = NULL;
	if (OIf == vif_num)
		return (TC_OK);  /* don't call kernel for API */
	if_name = if_vec[OIf].if_name;
	if (!if_vec[OIf].if_up) {
		rsvp_errno = Set_Errno(RSVP_Err_TC_ERROR, 0);
		return(TC_ERROR);
	}

	strncpy(irp->iq_name, if_name, IFNAMSIZ);
	irp->iq_function = IF_MODFLW;
	irp->iq_handle = rhandle;
	if (rsvp2isps_spec(specp, stspecp, &irp->iq_fs, (flags&TC_flags))<0)
		return (TC_ERROR);

	if (cap_network_ioctl(udp_socket, SIOCIFISPSCTL, irp) < 0) {
		rsvp_errno = kern_translate_error(errno);
	/****	log(LOG_ERR, 0, "SIOCIFMODFLW\n");	****/
		return (TC_ERROR);
	}
#ifdef _MIB
	if (mib_enabled) {
	     old_bkt = rsvpd_get_intSrvFlowEntry_bkt((int) rhandle);
	     new_bkt = irp->iq_fs.fs_tspec.ts_c.c_bkt;
	     new_rate = irp->iq_fs.fs_tspec.ts_c.c_rate * 8;

	     /*
	      * Modify the AllocatedBuffer variable in the IfAttribTable.
	      */
	     if (old_bkt < 0) {
		  log(LOG_ERR, 0, "modFlowspec: bad rhandle %d\n", rhandle);
	     } else {
		  delta_bkt = new_bkt - old_bkt;
		  rc = rsvpd_update_intSrvIfAttribEntry(if_vec[OIf].if_index, delta_bkt,
							if_vec[OIf].if_numflows);
		  if (rc < 0) 
		       log(LOG_ERR, 0, "modFlowspec: updating intSrvIfAttrib if %d\n",
			   if_vec[OIf].if_index);
	     }

	     /*
	      * Modify the rate and burst variables in the intSrvFlowTable.
	      */
	     rc = rsvpd_set_intSrvFlowEntry_rate_bkt((int) rhandle, new_rate, new_bkt);
	     if (rc < 0) 
		  log(LOG_ERR, 0, "modFlowspec: updating intSrvFlowEntry %d\n",
		      rhandle);
	}
#endif
	return (TC_OK);
}

/*
 * TC_Advertise(): Update OPWA ADSPEC.
 */
ADSPEC *
TC_Advertise(int OIf, ADSPEC * old_asp, int flags)
	{
	Object_header *copy_object(Object_header *);

	if (!if_vec[OIf].if_up) {
		return(NULL);
	}
	return(copy_adspec(old_asp));
}

static int
kern_translate_error(int kernel_error)
{
	switch (kernel_error) {
	case EBDHDL:
		return (EBDHDL << 8 | RSVP_Err_TC_ERROR);
	case EDELAY:
		return (RSVP_Erv_DelayBnd << 8 | RSVP_Err_ADMISSION);
	case ENOBWD:
		return (RSVP_Erv_Bandwidth << 8 | RSVP_Err_ADMISSION);
	case EBADRSPEC:
		return (RSVP_Erv_Crazy_Flowspec << 8 | RSVP_Err_ADMISSION);
	case EBADTSPEC:
		return (RSVP_Erv_Crazy_Tspec << 8 | RSVP_Err_ADMISSION);
	case EBADFILT:
		return (EBADFILT << 8 | RSVP_Err_TC_ERROR);
	default:
	    	return (kernel_error << 8 | RSVP_Err_TC_ERROR);
	}
}

/*
 * kern_ispsclear(): This routine resets an interface, cleaning up old
 * 	reservations. It is used when RSVP daemon is initialized.
 */
static int
kern_ispsclear(int in_if) {
    	char           *if_name;
	struct ispsreq  ir;
	int 		rc;
    
    	if_name = if_vec[in_if].if_name;
	if_vec[in_if].if_up = 0;	    

#ifdef __sgi
	if (if_vec[in_if].if_flags & IF_FLAG_Disable)
	       return 1;
#endif
    
	log(LOG_INFO, 0, "ISPS: Re-init ISPS TC on %s\n", if_name);

	/*
	 * Delete all state for unlocked flows. Puts the scheduler in a
	 * known starting state when RSVP is reset.
	 */
	strncpy(ir.iq_name, if_name, IFNAMSIZ);
	ir.iq_function = IF_RESET;
	rc = cap_network_ioctl(udp_socket, SIOCIFISPSCTL, &ir);
	if (rc< 0) {
	    	if (errno == EINVAL||errno == ENXIO) {
		    	log(LOG_INFO, 0, 
				"No ISPS traffic control on interface %s\n",
			    	if_name);
			return(0);
		} else {
		    	log(LOG_ERR, errno,
				"Error resetting ISPS interface", 0);
			return (-1);
		}
	}
	/*
	 *	Mark Traffic Control up on interface
	 */
	if_vec[in_if].if_up = 1;
	return (1);
}

/*
 *  rsvp2isps_spec(): Create kernel isps flowspec from RSVP flowspec.
 *	'flags' contains F_POLICE bit if merging data so policing should
 *	 be done.  Set rsvp_errno and return -1 if flowspec is illegal.
 */
static int
rsvp2isps_spec(
	FLOWSPEC	*specp,
	SENDER_TSPEC	*stspecp,
	isps_fs_t	*i_fs,
	int		flags)
	{
	IS_serv_hdr_t	*sp, *stp;

	if (Obj_CType(specp) != ctype_FLOWSPEC_Intserv0) {
		/* Unknown CTYPE */
		rsvp_errno = Set_Errno(RSVP_Err_UNKNOWN_CTYPE,
				class_FLOWSPEC*256+Obj_CType(specp));
		return(-1);
	}
	if (Obj_CType(stspecp) != ctype_SENDER_TSPEC) {
		rsvp_errno = Set_Errno(RSVP_Err_UNKNOWN_CTYPE,
				class_SENDER_TSPEC*256+Obj_CType(stspecp));
		return(-1);
	}

	sp = (IS_serv_hdr_t *) &specp->flow_body.spec_u;
	stp = (IS_serv_hdr_t *) &stspecp->stspec_body.tspec_u;

	switch (sp->issh_service) {

	    case GUARANTEED_SERV:
		{ Guar_flowspec_t *s = (Guar_flowspec_t *) sp;
		  gen_Tspec_t *t = (gen_Tspec_t *) stp;

		i_fs->fs_tos = TOS_GUARANTEED;
		if (flags)
			  /* wrong policer, but the right one doesn't exist*/
			i_fs->fs_plc = PLC_TBF2;
		else
			i_fs->fs_plc = PLC_NONE;

		/* Set RSPEC info. */
		i_fs->fs_rspec.rs_g.g_rate = s->Gspec_R;

		/* Set TSPEC info. 
		 */
		if (stspecp) {
			 /* And why would there not be?? */

			 i_fs->fs_tspec.ts_tb.tbf_rate
				= MIN(s->Gspec_r, t->gtspec_r);

			 i_fs->fs_tspec.ts_tb.tbf_bkt
				= MAX(s->Gspec_b, t->gtspec_b);

			 /* Will take MIN of m, MIN of p, and MAX of M, too */
		} else {
			/* No sender tspec (shouldn't happen, but..)
			 * Best we can do for now is use whatever
			 * receiver gave us.
			 */
			i_fs->fs_tspec.ts_tb.tbf_rate = s->Gspec_r;
			i_fs->fs_tspec.ts_tb.tbf_bkt = s->Gspec_b;
		}
		}
		break;

	    case CONTROLLED_LOAD_SERV:
		{ CL_flowspec_t *s = (CL_flowspec_t *) sp;
		  gen_Tspec_t *t = (gen_Tspec_t *) stp;

		i_fs->fs_tos = TOS_CNTRLD_LOAD;
		if (flags)
			i_fs->fs_plc = PLC_TBF2;
		else
			i_fs->fs_plc = PLC_NONE;


		/* No RSPEC info. */

		if (stspecp) {
			i_fs->fs_tspec.ts_c.c_rate =
			  MIN(s->CLspec_r, t->gtspec_r);
			i_fs->fs_tspec.ts_c.c_bkt =
			  MAX(s->CLspec_b, t->gtspec_b);
			i_fs->fs_tspec.ts_c.c_m =
			  MIN(s->CLspec_m, t->gtspec_m);
			i_fs->fs_tspec.ts_c.c_M =
			  MAX(s->CLspec_M, t->gtspec_M);

			 /* Will take MIN of m and MAX of M, too */
		} else {
			/* No sender tspec (shouldn't happen, but..)
			 * Best we can do for now is use whatever
			 * receiver gave us.
			 */
			i_fs->fs_tspec.ts_c.c_rate = s->CLspec_r;
			i_fs->fs_tspec.ts_c.c_bkt = s->CLspec_b;
			i_fs->fs_tspec.ts_c.c_m = s->CLspec_m;
			i_fs->fs_tspec.ts_c.c_M = s->CLspec_M;
		}
		}
		break;

	    default:
			/* Unknown service */
		rsvp_errno = Set_Errno(RSVP_Err_ADMISSION,
							 RSVP_Erv_No_Serv);
		return(-1);
	}
		
	/*
	 * Future: set share class info from local mapping function,
	 * RSVP policy credentials, etc.
	 */
	i_fs->fs_handle = -1;

	return(0);	/* OK */
}	

/*
 * rsvp2isps_filter():  Builds kernel ISPS filter from RSVP filter
 *	spec in static area.  Sets rsvp_errno and return NULL if
 *	error in filter spec, else returns ptr to static area.
 *
 *	IPv4 addresses and ports in the isps filter spec are given in
 *	network byte order.
 */
static isps_filt_t *
rsvp2isps_filter(FilterSpec *filtp, Session *dest, int flags) {
	static isps_filt_t fl;

	assert(Obj_CType(filtp) == ctype_FILTER_SPEC_ipv4);

	fl.f_pf = PF_INET;
	fl.f_ipv4_protocol = IPPROTO_UDP; /* RSVP spec bug forces this
					   assumption */

	fl.f_ipv4_destaddr.s_addr = dest->d_addr.s_addr;
	fl.f_ipv4_destport = dest->d_port;
	fl.f_ipv4_srcaddr.s_addr = filtp->filt_srcaddr.s_addr;
	fl.f_ipv4_srcport = filtp->filt_srcport;

	/*
	 *  If the data will be coming in on a tunnel, the filter will
	 *  have to be adjusted so that the filter offset points to the
	 *  beginning of the unencapsulated data.  Thus the filter offset
	 *  must be incremented by the size of the encapsulating ip
	 *  header.
	 *
	 *  Note that there is an implicit assumption that the encapsulating
	 *  header size is fixed length.
	 */
	/*  XXX what to do... This can't possibly be right, since
	 *  rsvp doesn't know about all the different sorts of tunnels
	if (flags & F_MTUNNEL) {
	}
	****/
	return (&fl);
}

/*
 *  log_isps_filter(): Log ISPS filter spec
 */
static void
log_isps_filter(isps_filt_t *filt, int pr_ofs)
{
	if (filt == NULL) {
		log(LOG_DEBUG, 0, "Trying to parse NULL filter\n");
		return;
	}
	log(LOG_DEBUG, 0, "---------- Parsing ISPS Filter: -------\n");
	log(LOG_DEBUG, 0, "%*s PF %s Proto %s: \n", pr_ofs, "",
	    filt->f_pf == PF_INET ? "IPv4" : "(Unknown!)",
	    filt->f_ipv4_protocol == IPPROTO_UDP ? "UDP" :
	    filt->f_ipv4_protocol == IPPROTO_TCP ? "TCP" : "Unknown");
	log(LOG_DEBUG, 0, "%*s SrcAddr %s SrcPort %d: \n", pr_ofs, "",
	    inet_ntoa(filt->f_ipv4_srcaddr), ntoh16(filt->f_ipv4_srcport));
	log(LOG_DEBUG, 0, "%*s DestAddr %s DestPort %d: \n", pr_ofs, "",
	    inet_ntoa(filt->f_ipv4_destaddr), ntoh16(filt->f_ipv4_destport));
}

