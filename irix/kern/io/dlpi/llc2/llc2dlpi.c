
/******************************************************************
 *
 *  SpiderX25 - LLC2 Multiplexer
 *
 *  Copyright 1991 Spider Systems Limited
 *
 *  LLC2DLPI.C
 *
 *    Top-level STREAMS driver for LLC2.
 *
 ******************************************************************/

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/sys/llc2/86/s.llc2dlpi.c
 *	@(#)llc2dlpi.c	1.17
 *
 *	Last delta created	16:54:04 5/28/93
 *	This file extracted	16:55:24 5/28/93
 *
 */

/*
 * Routines to accept DLPI primitives for LLC2.
 */

#define FAR
#define STATIC static
typedef unsigned char *bufp_t;
#define BUFP_T bufp_t

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stream.h>
#include <sys/strmp.h>
#include <sys/stropts.h>
#include <sys/systm.h>
#include <sys/strlog.h>
#include <sys/dlsap_register.h>
#include <sys/dlpi.h>
#include <sys/snet/uint.h>
#include <sys/snet/ll_proto.h>
#include <sys/snet/ll_control.h>
#include <sys/snet/dl_proto.h>
#include <sys/snet/timer.h>
#include <sys/snet/x25trace.h>
#include <sys/snet/system.h>
/* #include "sys/snet/llc2match_if.h" */
#include "sys/snet/llc2.h"

/* Counts from 'space.c' */
extern unsigned int	llc2_ndns;
extern unsigned int	llc2_nups;

/* Slot tables from 'space.c' */
extern llc2up_t     *llc2_uptab;
extern llc2dn_t     *llc2_dntab;

/* Stock buffer pointers */
extern mblk_t *llc2_upmp;

extern llc2cn_t	*llc2_connectup();

/* #define __STDC__	1 */

#ifdef  __STDC__
extern int	dlokack(queue_t *, mblk_t *, dlu32_t);
extern int	dlerrorack(queue_t *, mblk_t *, dlu32_t, dlu32_t, dlu32_t);
extern int	merror(queue_t *, mblk_t *, int);

extern void	llc2_dlpi_tonet(llc2cn_t *, int, int, mblk_t *);
extern void	llc2_tonet_xid_test(mblk_t *, llc2up_t *, llc2cn_t *);
STATIC int	disconnect_ind(llc2cn_t *, mblk_t *, int);
STATIC void	format_discon_ind(mblk_t *, int, int);
extern int	llc2wput_dlpi(queue_t *, mblk_t *);
STATIC int	llc2_info_req(queue_t *, mblk_t *);
STATIC int	llc2_attach_req(queue_t *, mblk_t *);
STATIC int	llc2_detach_req(queue_t *, mblk_t *);
STATIC int	llc2_bind_req(queue_t *, mblk_t *);
#ifdef SGI
STATIC int	send_down_reg(queue_t *, mblk_t *, queue_t *, uint16, uint16);
extern int	send_down_unreg(queue_t *, mblk_t *, queue_t *, uint16, uint16);
STATIC int 	format_error_ack(mblk_t **mpp,dlu32_t,dlu32_t,dlu32_t);
STATIC int	format_ok_ack(mblk_t **mpp, dlu32_t prim);
STATIC int	format_merror(mblk_t **mpp, int error);
#endif
STATIC int	format_bind_ack(queue_t *, mblk_t **, dl32_t, dl32_t);
STATIC int	llc2_unbind_req(queue_t *, mblk_t *);
STATIC int	llc2_subs_bind_req(queue_t *, mblk_t *);
STATIC int	llc2_subs_unbind_req(queue_t *, mblk_t *);
STATIC int	llc2_conn_req(queue_t *, mblk_t *);
STATIC int	llc2_conn_res(queue_t *, mblk_t *);
STATIC int	llc2_token_req(queue_t *, mblk_t *);
STATIC int	llc2_discon_req(queue_t *, mblk_t *);
STATIC int	llc2_reset_req(queue_t *, mblk_t *);
STATIC int	llc2_reset_res(queue_t *, mblk_t *);
STATIC int	llc2_unitdata_req(queue_t *, mblk_t *);
STATIC int	llc2_enab_disab_multi(queue_t *, mblk_t *);
STATIC int	llc2_xid_test(queue_t *, mblk_t *);
STATIC int	llc2_get_phys(queue_t *, mblk_t *);
STATIC int	llc2_set_phys(queue_t *, mblk_t *);
STATIC int	llc2_stats_req(queue_t *, mblk_t *);
extern void	schedule_udata(llc2up_t *, int);
extern int	llc2_dlpi_dtecpy(llc2cn_t *, llc2up_t *, mblk_t *, uint8 *,int);
extern void	set_dlsap_addr(mblk_t *, int, unchar *, unchar, unchar);
STATIC void	enqueue_ctrl_msg(mblk_t *, llc2cn_t *, int);
extern void	reply_uderror(queue_t *, mblk_t *, int);
#else /* __STDC__ */
int		dlokack();
int		dlerrorack();
int		merror();

void		llc2_dlpi_tonet();
void		llc2_tonet_xid_test();
STATIC int	disconnect_ind();
STATIC void	format_discon_ind();
int		llc2wput_dlpi();
STATIC int	llc2_info_req();
STATIC int	llc2_attach_req();
STATIC int	llc2_detach_req();
STATIC int	llc2_bind_req();
#ifdef SGI
STATIC int	send_down_reg();
int	send_down_unreg();
#endif
STATIC int	format_bind_ack();
STATIC int	llc2_unbind_req();
STATIC int	llc2_subs_bind_req();
STATIC int	llc2_subs_unbind_req();
STATIC int	llc2_conn_req();
STATIC int	llc2_conn_res();
STATIC int	llc2_token_req();
STATIC int	llc2_discon_req();
STATIC int	llc2_reset_req();
STATIC int	llc2_reset_res();
STATIC int	llc2_unitdata_req();
STATIC int	llc2_enab_disab_multi();
STATIC int	llc2_xid_test();
STATIC int	llc2_get_phys();
STATIC int	llc2_set_phys();
STATIC int	llc2_stats_req();
void		schedule_udata();
int		llc2_dlpi_dtecpy();
void		set_dlsap_addr();
STATIC void	enqueue_ctrl_msg();
void		reply_uderror();
#endif /* __STDC__ */


/*
 * Template for response to DL_INFO_REQ.
 */
dl_info_ack_t llc2_dl_info_ack = {
	DL_INFO_ACK,		/* dl_primitive */
	1031,			/* dl_max_sdu */
	1,			/* dl_min_sdu */
	0,			/* dl_addr_length */
	DL_OTHER,		/* dl_mac_type */
	0,			/* dl_reserved */
	DL_UNATTACHED,		/* dl_current_state */
	0,			/* dl_sap_length */
	DL_CODLS | DL_CLDLS,	/* dl_service_mode */
	0,			/* dl_qos_length */
	0,			/* dl_qos_offset */
	0,			/* dl_qos_range_length */
	0,			/* dl_qos_range_offset */
	DL_STYLE2,		/* dl_provider_style */
	0,			/* dl_addr_offset */
	2,			/* dl_version */
	0,			/* dl_brdcast_addr_length */
	0,			/* dl_brdcast_addr_offset */
	0,			/* dl_growth */
};


/*
 * Variable used to generate unique correlation IDs for connections.
 */
static int	dlpi_correlation = 1;


typedef struct dlpi_prim {
	int		pi_minlen;	/* Minimum primitive length */
	dl32_t		pi_state;	/* Acceptable starting state */
	int		(*pi_funcp)(queue_t*, mblk_t*);
					/* function() to call */
} LLC2_prim_t;


/*
 * For llc2_priminfo -- indicate that primitive is valid in several states.
 */
#define DL_SPECIAL		(DL_SUBS_UNBIND_PND+1)


/*
 * llc2_priminfo[] is used to do some initial DLPI primitive checks
 * before calling the routine to interpret each specific primitive.
 */
static LLC2_prim_t llc2_priminfo[] = {
/*INFO_REQ*/	{DL_INFO_REQ_SIZE,	DL_SPECIAL,	llc2_info_req},
/*BIND_REQ*/	{DL_BIND_REQ_SIZE,	DL_UNBOUND,	llc2_bind_req},
/*UNBIND_REQ*/	{DL_UNBIND_REQ_SIZE,	DL_IDLE,	llc2_unbind_req},
/*INFO_ACK*/		{0,		0,		0},
/*BIND_ACK*/		{0,		0,		0},
/*ERROR_ACK*/		{0,		0,		0},
/*OK_ACK*/		{0,		0,		0},
/*UNITDATA_REQ*/{DL_UNITDATA_REQ_SIZE,	DL_SPECIAL,	llc2_unitdata_req},
/*UNITDATA_IND*/	{0,		0,		0},
/*UDERROR_IND*/		{0,		0,		0},
/*UDQOS_REQ*/		{0,		0,		0},
/*ATTACH_REQ*/	{DL_ATTACH_REQ_SIZE,	DL_UNATTACHED,	llc2_attach_req},
/*DETACH_REQ*/	{DL_DETACH_REQ_SIZE,	DL_UNBOUND,	llc2_detach_req},
/*CONNECT_REQ*/	{DL_CONNECT_REQ_SIZE,	DL_IDLE,	llc2_conn_req},
/*CONNECT_IND*/		{0,		0,		0},
/*CONNECT_RES*/	{DL_CONNECT_RES_SIZE,	DL_INCON_PENDING, llc2_conn_res},
/*CONNECT_CON*/		{0,		0,		0},
/*TOKEN_REQ*/	{DL_TOKEN_REQ_SIZE,	DL_SPECIAL,	llc2_token_req},
/*TOKEN_ACK*/		{0,		0,		0},
/*DISCONNECT_REQ*/ {DL_DISCONNECT_REQ_SIZE, DL_SPECIAL,	llc2_discon_req},
/*DISCONNECT_IND*/	{0,		0,		0},
/*SUBS_UNBIND_REQ*/ {DL_SUBS_UNBIND_REQ_SIZE, DL_IDLE,	llc2_subs_unbind_req},
/*not used*/		{0,		0,		0},
/*RESET_REQ*/	{DL_RESET_REQ_SIZE,	DL_DATAXFER,	llc2_reset_req},
/*RESET_IND*/		{0,		0,		0},
/*RESET_RES*/	{DL_RESET_RES_SIZE,	DL_PROV_RESET_PENDING, llc2_reset_res},
/*RESET_CON*/		{0,		0,		0},
/*SUBS_BIND_REQ*/ {DL_SUBS_BIND_REQ_SIZE, DL_IDLE,	llc2_subs_bind_req},
/*SUBS_BIND_ACK*/	{0,		0,		0},
/*ENABMULTI_REQ*/ {DL_ENABMULTI_REQ_SIZE, DL_SPECIAL,	llc2_enab_disab_multi},
/*DISABMULTI_REQ*/{DL_DISABMULTI_REQ_SIZE, DL_SPECIAL,	llc2_enab_disab_multi},
/*PROMISCON_REQ*/	{0,		0,		0},
/*PROMISCOFF_REQ*/	{0,		0,		0},
/*DATA_ACK_REQ*/	{0,		0,		0},
/*DATA_ACK_IND*/	{0,		0,		0},
/*DATA_ACK_STATUS_IND*/ {0,		0,		0},
/*REPLY_REQ*/		{0,		0,		0},
/*REPLY_IND*/		{0,		0,		0},
/*REPLY_STATUS_IND*/	{0,		0,		0},
/*REPLY_UPDATE_REQ*/	{0,		0,		0},
/*REPLY_UPDATE_STATUS_IND*/{0,		0,		0},
/*XID_REQ*/	{DL_XID_REQ_SIZE,	DL_SPECIAL,	llc2_xid_test},
/*XID_IND*/		{0,		0,		0},
/*XID_RES*/	{DL_XID_RES_SIZE,	DL_SPECIAL,	llc2_xid_test},
/*XID_CON*/		{0,		0,		0},
/*TEST_REQ*/	{DL_TEST_REQ_SIZE,	DL_SPECIAL,	llc2_xid_test},
/*TEST_IND*/		{0,		0,		0},
/*TEST_RES*/	{DL_TEST_RES_SIZE,	DL_SPECIAL,	llc2_xid_test},
/*TEST_CON*/		{0,		0,		0},
/*PHYS_ADDR_REQ*/ {DL_PHYS_ADDR_REQ_SIZE, DL_SPECIAL,	llc2_get_phys},
/*PHYS_ADDR_ACK*/ 	{0,		0,		0},
/*SET_PHYS_ADDR_REQ*/ {DL_SET_PHYS_ADDR_REQ_SIZE, DL_SPECIAL,  llc2_set_phys},
/*GET_STATISTICS_REQ*/{DL_GET_STATISTICS_REQ_SIZE, DL_SPECIAL, llc2_stats_req},
/*GET_STATISTICS_ACK*/	{0,		0,		0},
};

#ifndef DL_MAXPRIM
#define DL_MAXPRIM sizeof(llc2_priminfo)/sizeof(llc2_priminfo[0])
#endif

#ifdef SGI
/* 
 * In single threaded Streams, dlpi_token can be updated within
 * Streams monitor.
 */
#define QPTR_TO_TOKEN(q) ((dlu32_t)(0xFFFFFFFF & (ulong)(q)))
#endif /* SGI */
/*
 * Send a DLPI message upstream.
 */
void
llc2_dlpi_tonet(lp, command, status, mp)
llc2cn_t	*lp;
int		command;
int		status;
mblk_t		*mp;
{
	llc2up_t	*upp;

	upp = lp->upp;
LLC2_PRINTF("llc2_dlpi_tonet upp %p", upp);
LLC2_PRINTF(" lp %p\n", lp);
	mp->b_datap->db_type = M_PROTO;
	switch (command) {
	case LC_CONNECT: {	/* Incoming connect */
		dl_connect_ind_t *conn_ind;

		LLC2_PRINTF("llc2_dlpi_tonet: LC_CONNECT\n", 0);
		if (upp->up_dlpi_state != DL_IDLE &&
		    upp->up_dlpi_state != DL_INCON_PENDING) {
			/*
			 * Out of state, so don't send the message upstream.
			 */
			freemsg(mp);
			return;
		}
		/*
		 * Sanity check that up message is large enough.
		 */
		if (mp->b_datap->db_lim - mp->b_datap->db_base <
		     CONNECT_UPSIZE) {
			strlog(LLC_STID, 0, 0, SL_ERROR,
				"LLC2 - UP message too small");
			freemsg(mp);
			return;
		}
		upp->up_num_conind++;
		upp->up_dlpi_state = DL_INCON_PENDING;
		conn_ind = (dl_connect_ind_t *) mp->b_rptr;
		conn_ind->dl_primitive = DL_CONNECT_IND;
		lp->connID = dlpi_correlation++;
		conn_ind->dl_correlation = lp->connID;
		LLC2_PRINTF("llc2_dlpi_tonet: correlation %d\n", lp->connID);

		/*
		 * Set called (or local) addr.
		 */
		conn_ind->dl_called_addr_length = lp->dnp->dn_maclen + 1;
		conn_ind->dl_called_addr_offset = sizeof(*conn_ind);
		set_dlsap_addr(mp, conn_ind->dl_called_addr_offset,
			lp->dnp->dn_macaddr, lp->dnp->dn_maclen, lp->dte[0]);

		/*
		 * Set calling (or remote) addr.
		 */
		conn_ind->dl_calling_addr_length = lp->dnp->dn_maclen + 1;
		conn_ind->dl_calling_addr_offset = sizeof(*conn_ind) +
						    DLADDR_ALIGNED_SIZE;
		set_dlsap_addr(mp, conn_ind->dl_calling_addr_offset,
				lp->dte+2, lp->dnp->dn_maclen, lp->dte[1]);
#ifdef SGI
		*(lp->dte+2) &= 0x7f;
#endif

		conn_ind->dl_qos_length = 0;
		conn_ind->dl_qos_offset = 0;
		mp->b_wptr += conn_ind->dl_calling_addr_offset +
				conn_ind->dl_calling_addr_length;
		break;
	}  /* case LC_CONNECT */

	case LC_CONCNF:
		/*
		 * Confirmation of previous call request.
		 */
		LLC2_PRINTF("llc2_dlpi_tonet: LC_CONCNF\n", 0);
		if (upp->up_dlpi_state != DL_OUTCON_PENDING) {
			freemsg(mp);
			return;
		}
		switch (status) {
		case LS_SUCCESS: {
			dl_connect_con_t *conn_con;

			/*
			 * Remote end accepted the connection.
			 */
			upp->up_dlpi_state = DL_DATAXFER;
			conn_con = (dl_connect_con_t *) mp->b_rptr;
			conn_con->dl_primitive = DL_CONNECT_CON;
			conn_con->dl_resp_addr_length = lp->dnp->dn_maclen + 1;
			conn_con->dl_resp_addr_offset = sizeof(*conn_con);
			set_dlsap_addr(mp, sizeof(*conn_con), lp->dte+2,
				lp->dnp->dn_maclen, lp->dte[1]);
			conn_con->dl_qos_length = 0;
			mp->b_wptr += conn_con->dl_resp_addr_offset +
					conn_con->dl_resp_addr_length;
			strlog(LLC_STID, (short)lp->dnp->dn_snid,1, SL_TRACE,
			       "LINK  Up  : '%x' Id %03x",
			       (short) lp->dnp->dn_snid, lp->connID);
			break;
		}

		case LS_DISCONNECT:
		case LS_FAILED:
			/*
			 * LS_DISCONNECT -- Remote end did not accept the
			 *	connection.
			 * LS_FAILED -- The ACK timer expired N2 times, with
			 *	no response from the remote end.
			 */
			upp->up_dlpi_state = DL_IDLE;
			format_discon_ind(mp, (status == LS_DISCONNECT ?
						DL_USER : DL_PROVIDER), 
					  DL_CONREJ_DEST_UNREACH_TRANSIENT);
			break;

		default:
			freemsg(mp);
			return;
		}
		break;
	/* end case LC_CONCNF */

	case LC_CONOK:
		/*
		 * Layer 2 has accepted the outgoing connection.  There is
		 * no message for this in DLPI, so drop it.
		 */
		LLC2_PRINTF("llc2_dlpi_tonet: LC_CONOK\n", 0);
		freemsg(mp);
		return;

	case LC_DISC:
		/*
		 * Receiving a disconnect indication.
		 */
		LLC2_PRINTF("llc2_dlpi_tonet: LC_DISC\n", 0);
		if (disconnect_ind(lp, mp, DL_USER)) {
			/*
			 * Error occurred, so don't pass the message upstream.
			 */
			return;
		}
		break;

	case LC_DISCNF: {
		dl_ok_ack_t	*ok_ack;

		/*
		 * Confirmation of previous disconnect request.  Ignore the
		 * 'status' field, because even if an error occurred during
		 * the disconnect sequence, the final result is that the
		 * the circuit has been disconnected.
		 */
		LLC2_PRINTF("llc2_dlpi_tonet: LC_DISCNF\n", 0);
		if (upp->up_dlpi_state == DL_DISCON9_PENDING) {
			upp->up_num_conind--;
			if (upp->up_num_conind) {
				upp->up_dlpi_state = DL_INCON_PENDING;
			} else {
				upp->up_dlpi_state = DL_IDLE;
			}
		} else {
			upp->up_dlpi_state = DL_IDLE;
		}
		mp->b_wptr += sizeof(*ok_ack);
		mp->b_datap->db_type = M_PCPROTO;
		ok_ack = (dl_ok_ack_t *) mp->b_rptr;
		ok_ack->dl_primitive = DL_OK_ACK;
		ok_ack->dl_correct_primitive = DL_DISCONNECT_REQ;
		break;
	}  /* case LC_DISCNF */

	case LC_RESET: {
		dl_reset_ind_t	*reset_ind;

		/*
		 * Receiving a reset indication.
		 */
		LLC2_PRINTF("llc2_dlpi_tonet: LC_RESET\n", 0);
		if (upp->up_dlpi_state != DL_DATAXFER) {
			/*
			 * Received while out of state, so drop it.
			 */
			freemsg(mp);
			return;
		}
		mp->b_wptr += sizeof(*reset_ind);
		reset_ind = (dl_reset_ind_t *) mp->b_rptr;
		reset_ind->dl_primitive = DL_RESET_IND;
		reset_ind->dl_originator = DL_USER;
		reset_ind->dl_reason = DL_RESET_RESYNCH; /* XXX? */
		upp->up_dlpi_state = DL_PROV_RESET_PENDING;
		strlog(LLC_STID, (short) lp->dnp->dn_snid, 2, SL_TRACE,
		       "LINK  Rst : '%x' Id %03x Remote",
		       (short) lp->dnp->dn_snid, lp->connID);
		break;
	}  /* case LC_RESET */

	case LC_RSTCNF:
		/*
		 * Confirmation of previous reset request.
		 */
		LLC2_PRINTF("llc2_dlpi_tonet: LC_RSTCNF\n", 0);
		if (upp->up_dlpi_state != DL_USER_RESET_PENDING) {
			freemsg(mp);
			return;
		}
		switch (status) {
		case LS_SUCCESS:
			upp->up_dlpi_state = DL_DATAXFER;
			mp->b_wptr += sizeof(dl_reset_con_t);
			((dl_reset_con_t *) mp->b_rptr)->dl_primitive =
								DL_RESET_CON;
			break;
		case LS_DISCONNECT:
		case LS_FAILED:
			/*
			 * LS_DISCONNECT -- Remote end decided to disconnect
			 * LS_FAILED -- The ACK timer expired N2 times, with
			 *	no response from the remote end.
			 */
			upp->up_dlpi_state = DL_IDLE;
			format_discon_ind(mp, (status == LS_DISCONNECT ?
						DL_USER : DL_PROVIDER), 
					  DL_DISC_TRANSIENT_CONDITION);
			break;

		default:
			freemsg(mp);
			return;
		}
		break;
	/* end case LC_RSTCNF */

	case LC_DATA:
		/*
		 * Incoming data -- pass it up in M_DATA msg blocks.
		 */
		LLC2_PRINTF("llc2_dlpi_tonet: LC_DATA\n", 0);
		if (upp->up_dlpi_state != DL_DATAXFER) {
			/*
			 * Received data while out of state, so drop it.
			 */
			freemsg(mp);
			return;
		}
		/*
		 * The M_DATA msg blocks have already been appended to
		 * the message by indinfo().  Put the unnecessary
		 * M_PROTO header back into 'llc2_upmp' here.
		 * XXX better for performance if didn't get this header in
		 * the first place!
		 */
		llc2_upmp = mp;
		mp = mp->b_cont;
		llc2_upmp->b_cont = NULL;

		/*
		 * In loopback there is a zero length mblk that
		 * should be freed.  Do it now so that a pullup
		 * is not needed.
		 */
		if (!(mp->b_wptr - mp->b_rptr)) {
			mblk_t *omp = mp;
			mp = mp->b_cont;
			omp->b_cont = NULL;
			freeb(omp);
		}
		break;

	case LC_REPORT:
		LLC2_PRINTF("llc2_dlpi_tonet: LC_REPORT\n", 0);
		switch (status) {
		case LS_RST_FAILED:
			/*
			 * A reset attempt has failed.
			 */
			if (disconnect_ind(lp, mp, DL_PROVIDER)) {
				return;
			}
			break;

#ifdef SGI
		case LS_FMR_RECEIVED: {
			dl_reset_ind_t	*reset_ind;

			/*
			 * Receiving a reset indication.
			 */
			LLC2_PRINTF("llc2_dlpi_tonet: LC_RESET\n", 0);
			if (upp->up_dlpi_state != DL_DATAXFER) {
				/*
				 * Received while out of state, so drop it.
				 */
				freemsg(mp);
				return;
			}
			mp->b_wptr += sizeof(*reset_ind);
			reset_ind = (dl_reset_ind_t *) mp->b_rptr;
			reset_ind->dl_primitive = DL_RESET_IND;
			reset_ind->dl_originator = DL_USER;
			reset_ind->dl_reason = DL_RESET_RESYNCH; /* XXX? */
			upp->up_dlpi_state = DL_PROV_RESET_PENDING;
			strlog(LLC_STID, (short) lp->dnp->dn_snid, 2, SL_TRACE,
		       		"LINK  Rst : '%x' Id %03x Remote",
		       		(short) lp->dnp->dn_snid, lp->connID);
			break;
			}
#endif

		default:
			LLC2_PRINTF("  LC_REPORT, unknown status %d\n", status);
			freemsg(mp);
			return;
		}
		break;
	/* end case LC_REPORT */

	default:
		LLC2_PRINTF("llc2_dpli_tonet: unknown command %d\n", command);
		freemsg(mp);
		return;
	}
	putnext(upp->up_readq, mp);
	return;
}


/*
 * An event has occurred which forces a DL_DISCONNECT_IND message to be
 * sent upstream.  (A DISC frame arrived, or a reset attempt failed.)
 * Format the DL_DISCONNECT_IND message here and handle state table
 * transitions.
 */
STATIC int
disconnect_ind(lp, mp, originator)
llc2cn_t	*lp;
mblk_t		*mp;
int		originator;
{
	llc2up_t	*upp;
	int		nextstate;

	upp = lp->upp;
	nextstate = DL_IDLE;
	switch (upp->up_dlpi_state) {
	default:
		/*
		 * Received while out of state, so drop it.
		 */
		freemsg(mp);
		return 1;

	case DL_OUTCON_PENDING:
	case DL_DATAXFER:
	case DL_USER_RESET_PENDING:
	case DL_PROV_RESET_PENDING:
		break;

	case DL_INCON_PENDING:
		switch (upp->up_num_conind) {
		case 0:
			break;
		case 1:
			upp->up_num_conind--;
			break;
		default:
			nextstate = DL_INCON_PENDING;
			upp->up_num_conind--;
			break;
		}
		break;
	}

	format_discon_ind(mp, originator, DL_DISC_TRANSIENT_CONDITION);
	if (upp->up_dlpi_state == DL_INCON_PENDING) {
		dl_disconnect_ind_t *disc_ind;

		disc_ind = (dl_disconnect_ind_t *) mp->b_rptr;
		disc_ind->dl_correlation = lp->connID;
	}
	upp->up_dlpi_state = nextstate;
	strlog(LLC_STID, (short) lp->dnp->dn_snid, 1, SL_TRACE,
	       "LINK  Dwn : '%x' Id %03x Remote",
	       (short) lp->dnp->dn_snid, lp->connID);
	return 0;
}


/*
 * Send DL_TEST_IND/CON, DL_XID_IND/CON upstream.  Have already verified
 * that the client is a DLPI client who wants to receive incoming TEST/XID
 * commands/responses.  'mp' points to the complete LLC frame received from
 * the LAN driver.  'dte' contains the DSAP (dte[0]), SSAP (dte[1]), and
 * MAC address from the incoming packet.
 */
void
llc2_tonet_xid_test(mp, upp, lp)
mblk_t		*mp;
llc2up_t	*upp;
llc2cn_t	*lp;
{
	mblk_t		*ump;
	unchar		ucmd;
	int		pf_bit;		/* Value of P/F bit */
	int		is_response;	/* Value of Command/Response bit */
	int		prim;		/* DLPI primitive to pass up */
	dl_test_ind_t	*test_ind;
	uint8		*dte = lp->dte;

	ump = llc2_upmp;
	if (!ump) {
		DLP(("tonet_xidtest: No UP message\n"));
		strlog(LLC_STID, 0, 0, SL_ERROR, "LLC2 - No UP message");
		return;
	}

	ucmd = ((char *) mp->b_rptr)[2];
	pf_bit = ucmd>>4 & 1;
	ucmd &= ~UPF;
	LLC2_PRINTF("llc2_tonet_xid_test: ucmd 0x%x\n", ucmd);
	is_response = ((char *) mp->b_rptr)[1] & 1;
	if (ucmd == TEST) {
		if (is_response) {
			prim = DL_TEST_CON;
		} else {
			prim = DL_TEST_IND;
		}
	} else {
		if (is_response) {
			prim = DL_XID_CON;
		} else {
			prim = DL_XID_IND;
		}
	}

	switch (prim) {		/* XXX remove later */
	case DL_TEST_IND:
		LLC2_PRINTF("llc2_tonet_xid_test: DL_TEST_IND\n", 0);
		break;
	case DL_TEST_CON:
		LLC2_PRINTF("llc2_tonet_xid_test: DL_TEST_CON\n", 0);
		break;
	case DL_XID_IND:
		LLC2_PRINTF("llc2_tonet_xid_test: DL_XID_IND\n", 0);
		break;
	case DL_XID_CON:
		LLC2_PRINTF("llc2_tonet_xid_test: DL_XID_CON\n", 0);
		break;
	}
	if (upp->up_dlpi_state != DL_IDLE &&
	    upp->up_dlpi_state != DL_DATAXFER) {
		freemsg(mp);
		return;
	}
	llc2_upmp = NULL;
	ump->b_datap->db_type = M_PROTO;
	test_ind = (dl_test_ind_t *) ump->b_rptr;
	test_ind->dl_primitive = prim;
	/*
	 * 'dl_flag' will be set to 1 (DL_POLL_FINAL) if P/F bit
	 * was set in received frame.
	 */
	test_ind->dl_flag = pf_bit;

	/*
	 * Set destination (or local) addr.
	 */
	test_ind->dl_dest_addr_length = upp->up_dnp->dn_maclen + 1;
	test_ind->dl_dest_addr_offset = sizeof(*test_ind);
	set_dlsap_addr(ump, test_ind->dl_dest_addr_offset,
	       upp->up_dnp->dn_macaddr, upp->up_dnp->dn_maclen, dte[0]);

	/*
	 * Set calling (or remote) addr.
	 */
	test_ind->dl_src_addr_length = upp->up_dnp->dn_maclen + 1;
	test_ind->dl_src_addr_offset = sizeof(*test_ind) +
					    DLADDR_ALIGNED_SIZE;
	set_dlsap_addr(ump, test_ind->dl_src_addr_offset,
		       (unchar *) dte+2, upp->up_dnp->dn_maclen, dte[1]);
	if (lp->routelen)
	{
#ifdef SGI
		/* restore RII bit in Source Address */
		*(char *)(ump->b_rptr+test_ind->dl_src_addr_offset) |= 0x80;
#endif
		/* restore D bit to what it was on the way in */
		DLP(("\tlp->routelen=%d\n", lp->routelen));
		HEXDUMP(lp->route,lp->routelen);
		TR_COMPLEMENT_DBIT(lp->route);
		bcopy(lp->route,
			(ump->b_rptr + test_ind->dl_src_addr_offset +
				test_ind->dl_src_addr_length),
			lp->routelen);
		test_ind->dl_src_addr_length += lp->routelen;
	}

	ump->b_wptr = ump->b_rptr + test_ind->dl_src_addr_offset +
			test_ind->dl_src_addr_length;

	/*
	 * Move the data over to the upgoing message and strip off the
	 * 3-byte LLC2 header.
	 */
	if (msgdsize(mp) > 3) {
		adjmsg(mp, 3);
		LLC2_PRINTF("llc2_tonet_xid_test: data 0x%p\n", mp);
		ump->b_cont = mp;
	} else {
		freemsg(mp);
	}
/*	DLP(("\tsend xid_test upstreams\n")); */
	putnext(upp->up_readq, ump);
}


STATIC void
format_discon_ind(mp, originator, reason)
mblk_t		*mp;
int		originator;
int		reason;
{
	dl_disconnect_ind_t *disc_ind;

	mp->b_wptr += sizeof(*disc_ind);
	disc_ind = (dl_disconnect_ind_t *) mp->b_rptr;
	disc_ind->dl_primitive = DL_DISCONNECT_IND;
	disc_ind->dl_originator = originator;
	disc_ind->dl_reason = reason;
	disc_ind->dl_correlation = 0;
}

static void gotbuf(arg)
caddr_t arg;
{
	register llc2up_t *up = (llc2up_t *)arg;
	up->up_bufcall = 0;
	if (up->up_readq)
		qenable(WR(up->up_readq));
}

static void timedout(arg)
caddr_t arg;
{
	register llc2up_t *up = (llc2up_t *)arg;
	up->up_timeout = 0;
	if (up->up_readq)
		qenable(WR(up->up_readq));
}


/*
 * Handle DLPI M_PROTO/M_PCPROTO message received from upstream.
 */
int
llc2wput_dlpi(q, mp)
queue_t *q;
mblk_t  *mp;
{
	LLC2_prim_t	*pip;
	dlu32_t		prim;
	dl32_t		state;
	int		error;
	mblk_t  	*newmp;
        int 		size;

	prim = ((union DL_primitives *) mp->b_rptr)->dl_primitive;
	DLP(("llc2wput_dlpi: prim=%x\n",prim));

	/*
         * Assume the buffer will be large enough
         */

	newmp = mp;

	/*
	 * If given buffer too small for the ack then alloc new message.
	 * Do a bufcall if we can't get the buffer just now.
	 */
	size = DL_ERROR_ACK_SIZE;

	if (!(mp) || size > BPSIZE((mp)))
	{
		if (!(newmp = allocb(size, BPRI_HI))) {
			llc2up_t *up = (llc2up_t *)q->q_ptr;

			if (!up) {
				/* not much we can do here - send merror */
				(void) merror(q, mp, ENOSR);
				return 0;
			}
			if (up->up_waiting_buf)
				freemsg(up->up_waiting_buf);
			up->up_waiting_buf = mp;
			if (!(up->up_bufcall = bufcall(size, BPRI_HI, gotbuf, 
				(long)up)))
			{
				up->up_timeout = STREAMS_TIMEOUT(timedout, up, HZ);
			}
	    		return 1;
		}

    		newmp->b_datap->db_type = mp->b_datap->db_type;
    		newmp->b_wptr += mp->b_wptr - mp->b_rptr;
		bcopy(mp->b_rptr, newmp->b_rptr, mp->b_wptr - mp->b_rptr);
		newmp->b_cont = unlinkb(mp);
		freemsg(mp);
		mp = newmp;
    	}

	/*
	 * Verify that primitive is supported.
	 */

	if (prim > DL_MAXPRIM) {
		(void) dlerrorack(q, mp, prim, DL_BADPRIM, 0);
		DLP(("\tBad primitive\n"));
		return 0;
	}
	pip = &llc2_priminfo[prim];
	if (pip->pi_funcp == NULL) {
		(void) dlerrorack(q, mp, prim, DL_NOTSUPPORTED, 0);
		DLP(("\tUnsupported primitive\n"));
		return 0;
	}

	/*
	 * Check minimum length.
	 */
	if (MBLKL(mp) < pip->pi_minlen) {
		(void) dlerrorack(q, mp, prim, DL_BADPRIM, 0);
		return 0;
	}

	/*
	 * Check valid state.
	 */
	if (q->q_ptr) {
		state = ((llc2up_t *)q->q_ptr)->up_dlpi_state;
	} else {
		state = DL_UNATTACHED;
	}
	if (pip->pi_state != DL_SPECIAL && pip->pi_state != state) {
		(void)dlerrorack(q, mp, prim, DL_OUTSTATE, 0);
		return 0;
	}

/*	DLP(("\tBefore calling DLPI primitive function\n")); */
	error = (*pip->pi_funcp)(q, mp);
	if (error) {
		(void)dlerrorack(q, mp, prim, error, 0);
	}
	return 0;
}


/*
 * Received a DL_INFO_REQ message from upstream.  Reply with a DL_INFO_ACK.
 */
STATIC int
llc2_info_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	dl_info_ack_t	*info_ack;
	uint8		*addr;
	int		maxsize = sizeof(dl_info_ack_t) +
				2 * MAXHWLEN + 2;
	llc2up_t	*upp;
	llc2dn_t	*dnp;
	dl32_t		state;

	LLC2_PRINTF("llc2_info_req: DL_INFO_REQ %p\n", q->q_ptr);
	if (maxsize > BPSIZE(mp))
	{
		mblk_t		*bp;

		bp = allocb(maxsize, BPRI_HI);
		if (!bp)
		{
			(void) dlerrorack(q, mp, DL_INFO_REQ, DL_SYSERR, ENOSR);
			return (0);
		}
		freemsg(mp);
		mp = bp;
	}
	mp->b_datap->db_type = M_PCPROTO;
	mp->b_rptr = mp->b_datap->db_base;
	mp->b_wptr = mp->b_rptr + sizeof(dl_info_ack_t);
	bcopy((char *) &llc2_dl_info_ack, (char *) mp->b_rptr,
		sizeof(dl_info_ack_t));
	info_ack = (dl_info_ack_t *) mp->b_rptr;
	addr = (uint8 *)(mp->b_rptr + sizeof(dl_info_ack_t));
	upp = (llc2up_t *) q->q_ptr;
	if (upp)
	{
		state = upp->up_dlpi_state;
		if (dnp = upp->up_dnp)
		{
			info_ack->dl_max_sdu =
				dnp->dn_tunetab.max_I_len;
		}
		info_ack->dl_mac_type = dnp->dn_mactype;
	}
	else
	{
		state = DL_UNATTACHED;
	}
	info_ack->dl_current_state = state;
	if (state != DL_UNATTACHED && state != DL_UNBOUND &&
		state != DL_BIND_PENDING)
	{
		/*
		 * The stream has been bound.
		 */

		info_ack->dl_mac_type = dnp->dn_mactype;
		if (upp->up_mode == LLC_MODE_NORMAL)
		{
			/*
			 * dl_sap_length = -1 indicates that the SAP is one
			 * byte long, and follows the physical address in the
			 * DLSAP address.
			 */
			info_ack->dl_sap_length = -1;
			addr[dnp->dn_maclen] = upp->up_nmlsap;
		}
		else if (upp->up_mode == LLC_MODE_ETHERNET)
		{
			/*
			 * dl_sap_length = -2 indicates that the SAP is two
			 * bytes long, and follows the physical address in the
			 * DLSAP address.
			 */
			info_ack->dl_sap_length = -2;
			bcopy((char *)&(upp->up_ethsap), (char *) addr +
				dnp->dn_maclen, 2);
		}
		/*
		 * upp->up_mode == LLC_MODE_NETWARE, so there is no SAP
		 * and the so we leave the sap length as zero.
		 */
		if (upp->up_class == LC_LLC2)
			info_ack->dl_service_mode = DL_CODLS;
		else {
			info_ack->dl_service_mode = DL_CLDLS;
			info_ack->dl_max_sdu = dnp->dn_frgsz - LLC1_HDR_SIZE;
		}

		/*
		 * The length is the MAC length plus the SAP, but
		 * as the dl_sap_length value is negative, we subtract it.
		 */
		info_ack->dl_addr_length = dnp->dn_maclen -
			info_ack->dl_sap_length;
		info_ack->dl_addr_offset = sizeof(dl_info_ack_t);
		llc2_maccpy((bufp_t) addr, (bufp_t) dnp->dn_macaddr,
			dnp->dn_maclen);
		mp->b_wptr += info_ack->dl_addr_length;
	}
	qreply(q, mp);
	return (0);
}


/*
 * Handle DL_ATTACH_REQ message received from upstream.
 */
STATIC int
llc2_attach_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	llc2up_t	*upp;
	llc2dn_t	*dnp;
	dlu32_t		ppa;
	int		found = 0;
	int		n;

	ppa = ((dl_attach_req_t *) mp->b_rptr)->dl_ppa;
	LLC2_PRINTF("llc2_attach_req: DL_ATTACH_REQ - ppa:%d\n", ppa);
	DLP(("llc2_attach_req: DL_ATTACH_REQ - ppa:%d\n", ppa));
	for (n = llc2_ndns, dnp = llc2_dntab; n--; dnp++) {
		/*
		 * Scan 'down's to find matching PPA.
		 */
		LLC2_PRINTF("llc2_attach_req: DL_ATTACH_REQ - snid:%d\n",
			dnp->dn_snid);
		LLC2_PRINTF("llc2_attach_req: DL_ATTACH_REQ - dn_q:%p\n",
			dnp->dn_downq);
		if (dnp->dn_downq && dnp->dn_snid == ppa) {
			found = 1;
			break;
		}
	}
	if (!found) {
		LLC2_PRINTF("llc2_attach_req: DL_ATTACH_REQ BADPPA\n", 0);
		return DL_BADPPA;
	}
	/*
	 * Find an unused 'up' struct in llc2_uptab.
	 */
	found = 0;
	for (n = llc2_nups, upp = llc2_uptab; n--; upp++) {
		if (!upp->up_readq) {
			found = 1;
			break;
		}
	}
	if (!found) {
		(void) dlerrorack(q, mp, DL_ATTACH_REQ, DL_SYSERR, ENOBUFS);
		return 0;
	}
	DLP(("\tdn_regstage=%d\n",dnp->dn_regstage));
	if (dnp->dn_regstage == REGNONE)
	{   /* Send off MAC registration */
		mblk_t *mmp;
		struct datal_type FAR *mpp;

#if DL_VERSION >= 2
		if (!(mmp = allocb(sizeof(struct datal_type) + MAXHWLEN - 1,
			BPRI_MED)))
#else /* DL_VERSION >= 2 */
		if (!(mmp = allocb(sizeof(struct datal_type), BPRI_MED)))
#endif /* DL_VERSION >= 2 */
		{
			strlog(LLC_STID, 0, 0, SL_ERROR,
				"LLC2: No buffer for DLPI Registration");
			(void) dlerrorack(q, mp, DL_ATTACH_REQ, DL_SYSERR, ENOSR);
			return 0;
		}
		/* Assemble MAC registration message */
		mpp = (struct datal_type FAR *)mmp->b_wptr;
#ifdef ETH_PTYPE
		mpp->prim_type = ETH_PTYPE;
#else
		mpp->prim_type = DATAL_TYPE;
#endif /* ETH_PTYPE */
		mpp->version = DL_VERSION;
		mpp->lwb = 0;
#ifdef NO_FULL_REG
		mpp->upb = 1500;
#else
		mpp->upb = 0xFFFF;
#endif
		mmp->b_datap->db_type = M_PROTO;
#if DL_VERSION >= 2
                mmp->b_wptr += sizeof(struct datal_type) + MAXHWLEN - 1;
#else /* DL_VERSION >= 2 */
		mmp->b_wptr += sizeof(struct datal_type);
#endif /* DL_VERSION >= 2 */

		/* Registration now in hand */
		dnp->dn_regstage = REGTOMAC;

		/* Send MAC registration off */
		DLP(("\tSend MAC registration off\n"));
		putnext(dnp->dn_downq, mmp);
	}
	/*
	 * Only attach the 'up' struct to the write side.
	 */
	q->q_ptr = (caddr_t)upp;
	upp->up_readq = RD(q);
	upp->up_dnp = dnp;
	upp->up_interface = LLC_IF_DLPI;
	upp->up_dlpi_state = DL_ATTACH_PENDING;
	putq(q, mp);
	llc2_schedule(&dnp->dn_station);
	return 0;
}


/*
 * Handle DL_DETACH_REQ message received from upstream.
 */
STATIC int
llc2_detach_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	LLC2_PRINTF("llc2_detach_req: DL_DETACH_REQ\n", 0);
	((llc2up_t *)q->q_ptr)->up_readq = NULL;
	q->q_ptr = NULL;
	(void) dlokack(q, mp, DL_DETACH_REQ);
	return 0;
}


/*
 * Handle DL_BIND_REQ message received from upstream.  (Is equiv. to
 * Spider LL_REG message.)
 */
STATIC int
llc2_bind_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	dl_bind_req_t	*bind_req;
	llc2up_t	*upp;
	llc2up_t	*upp2;
	llc2dn_t	*dnp;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_bind_req: DL_BIND_REQ %p\n", upp);
	dnp = upp->up_dnp;
	bind_req = (dl_bind_req_t *) mp->b_rptr;

	if (bind_req->dl_service_mode == DL_CLDLS)
	{
		/* connectionless service mode */
		dlu32_t dl_sap = bind_req->dl_sap;

		if ((dl_sap & 0xFFFFFF00) != 0)
		{
			if (dl_sap > 0xFFFF)	/* value too large */
				return (DL_BADADDR);

			/* a two-byte sap, so assume Ethernet */
			if (dnp->eth_match == (caddr_t) 0)
			{
				/*
				 * There is currently no Ethernet match table,
				 * so create one.
				 */
				dnp->eth_match = init_match();
				if (dnp->eth_match == (caddr_t) 0)
				{
					(void) dlerrorack(q, mp, DL_BIND_REQ,
						DL_SYSERR, ENOSR);
					return (0);
				}
			}

			/*
			 * Copy the SAP into up_ethsap, which holds it in
			 * network order.
			 */
			((char *)&upp->up_ethsap)[0] = dl_sap >> 8;
			((char *)&upp->up_ethsap)[1] = dl_sap & 0xFF;

			if (register_sap(dnp->eth_match, RD(q), (uint8 *)
				&(upp->up_ethsap), 2) < 0)
			{
				upp->up_ethsap = 0;
				return (DL_BOUND);
			}

			upp->up_nmlsap = 0;
			upp->up_mode = LLC_MODE_ETHERNET;
		}
		else
		{
			/*
			 * Binding a normal stream.  Verify that the SAP
			 * is valid.
			 */
			upp->up_mode = LLC_MODE_NORMAL;

			if (dl_sap == 0 || dl_sap & 1 || dl_sap > 0xFF)
			{
				if (dl_sap == 0x00FF)
				{
					/*
					 * Special SAP, indicating Netware
					 * "raw" mode.
					 */
					upp->up_mode = LLC_MODE_NETWARE;
				}
				else
					/*
					 * Zero, odd or too big SAP not allowed.
					 */
					return (DL_BADADDR);
			}

			if (dnp->llc1_match == (caddr_t) 0)
			{
				/*
				 * There is currently no LLC match table,
				 * so create one.
				 */
				dnp->llc1_match = init_match();
				if (dnp->llc1_match == (caddr_t) 0)
				{
					upp->up_mode = 0;
					(void) dlerrorack(q, mp, DL_BIND_REQ,
						DL_SYSERR, ENOSR);
					return (0);
				}
			}

			upp->up_nmlsap = dl_sap;
			if (register_sap(dnp->llc1_match, RD(q), (uint8 *)
				&(upp->up_nmlsap), 1) < 0)
			{
				upp->up_mode = 0;
				upp->up_nmlsap = 0;
				return (DL_BOUND);
			}
		}

		upp->up_class = LC_LLC1;
		upp->up_max_conind = 0;
		upp->up_conn_mgmt = 0;
	}
	else if (bind_req->dl_service_mode == DL_CODLS)
	{
		/* connection-oriented service mode */

		if (bind_req->dl_conn_mgmt)
		{
			/*
			 * Binding a "connection management" stream.  In this
			 * case, the SAP is ignored, and only one such stream
			 * is allowed per PPA (== down stream).
			 */
			bind_req->dl_sap = 0;
			for (upp2 = dnp->dn_uplist; upp2;
				upp2 = upp2->up_dnext)
			{
				if (upp2->up_conn_mgmt)
					return DL_BOUND;
			}
		}
		else
		{
			/*
			 * Binding a normal stream.  Verify that the SAP
			 * is valid.
			 */
			if (bind_req->dl_sap == 0 || bind_req->dl_sap & 1
				|| bind_req->dl_sap > 0xFF)
			{
				/*
				 * Zero, odd SAP, or too big not allowed. 
				 */
				return (DL_BADADDR);
			}

			for (upp2 = dnp->dn_uplist; upp2; upp2 = upp2->up_dnext)
			{
				if (bind_req->dl_sap != upp2->up_nmlsap)
					continue;

				/*
				 * (dl_max_conind > 0) ==> User wants this
				 * to be a listen stream (that can accept
				 * incoming DL_CONNECT_IND messages).  Only
				 * one DLPI listen stream can exist per SAP.
				 * Ensure here that no other stream has been
				 * bound to the SAP with dl_max_conind > 0.
				 */
				if (bind_req->dl_max_conind > 0 &&
					(upp2->up_interface != LLC_IF_DLPI ||
					upp2->up_max_conind > 0))
				{
					return DL_BOUND;
				}
			}
		}
		upp->up_class = LC_LLC2;
		upp->up_max_conind = bind_req->dl_max_conind;
		upp->up_conn_mgmt = bind_req->dl_conn_mgmt;
		upp->up_nmlsap = bind_req->dl_sap;
		upp->up_mode = LLC_MODE_NORMAL;
	}
	else
		return (DL_UNSUPPORTED);

	/*
	 * Insert up stream into down stream's list of up streams.
	 */
	upp->up_dnext = dnp->dn_uplist;
	dnp->dn_uplist = upp;

	/*
	 * Don't set 'up_lpbsap', or the "loopback SAP" here.  All this
	 * field is used for in the Spider interface is to set DSAP =
	 * "loopback SAP" in the case that the user has specified a
	 * zero-len DSAP (in llc2_dtecpy).
	 */
	upp->up_lpbsap = 0;

	upp->up_num_conind = 0;
	upp->up_xidtest_flg = bind_req->dl_xidtest_flg;
	if (!format_bind_ack(q, &mp, bind_req->dl_sap, bind_req->dl_max_conind))
		goto fail;

#ifdef SGI
	if (bind_req->dl_sap > 0xFF) {
	    if (send_down_reg(q, mp, dnp->dn_downq, bind_req->dl_sap,
	    	    DL_ETHER_ENCAP) < 0)
		goto fail;
	} else {
	    if (send_down_reg(q, mp, dnp->dn_downq, bind_req->dl_sap,
		    DL_LLC_ENCAP) < 0)
		goto fail;
	}
#endif

	/* all's well ... */
	{
	    dl_bind_ack_t *bind_ack = (dl_bind_ack_t *) mp->b_rptr;

	    llc2_maccpy(mp->b_rptr + bind_ack->dl_addr_offset,
		        (bufp_t) dnp->dn_macaddr, dnp->dn_maclen);
	    upp->up_dlpi_state = DL_IDLE;
	    bind_ack->dl_addr_length = dnp->dn_maclen;
	    if (upp->up_mode == LLC_MODE_NORMAL)
	    {
		bind_ack->dl_addr_length++;
		*(mp->b_rptr + bind_ack->dl_addr_offset +
			dnp->dn_maclen) = upp->up_nmlsap;
	    }
	    else if (upp->up_mode == LLC_MODE_ETHERNET)
	    {
		bind_ack->dl_addr_length += 2;
		bcopy((char *)&upp->up_ethsap,
			mp->b_rptr + bind_ack->dl_addr_offset +
			dnp->dn_maclen, 2);
	    }
	    mp->b_wptr = mp->b_rptr + sizeof(*bind_ack) +
			bind_ack->dl_addr_length;
	    qreply(q, mp);
	}
	return (0);

fail:
	/* bind failed after sap registered - deregister sap again */
	if (upp->up_class == LC_LLC1)
	{
		if (upp->up_mode == LLC_MODE_NORMAL ||
				upp->up_mode == LLC_MODE_NETWARE)
		{
			(void) deregister_sap(dnp->llc1_match, RD(q), (uint8 *)
					&upp->up_nmlsap, 1);
			upp->up_nmlsap = 0;
		}
		else if (upp->up_mode == LLC_MODE_ETHERNET)
		{
			(void) deregister_sap(dnp->eth_match, RD(q), (uint8 *)
					&(upp->up_ethsap), 2);
			upp->up_ethsap = 0;
		}
		upp->up_mode = 0;
	}
	upp->up_dlpi_state = DL_UNBOUND;
	return (0);
}

#ifdef SGI
/*
 * Send downstream a registration message
 */
STATIC int
send_down_reg(queue_t *uq, mblk_t *mp, queue_t *dq, uint16 sap, uint16 encap)
{
	mblk_t		*bp;
	S_DATAL_REG     *bpp;
	
	if (!(bp = allocb(sizeof(S_DATAL_REG), BPRI_MED)))
	{
		if (mp)
			(void) dlerrorack(uq, mp, DL_BIND_REQ, DL_SYSERR, ENOSR);
		return (-1);
	}

	bp->b_datap->db_type = M_PROTO;
	bp->b_wptr += sizeof(S_DATAL_REG);
	bpp = (S_DATAL_REG *)bp->b_rptr;
	bpp->prim_type = DATAL_REG;
	bpp->version = DL_VERSION;
	bpp->sap = sap;
	bpp->encap = encap;
	putnext(dq, bp);
	return (0);
}

/*
 * Send downstream a un-registration message
 */
int
send_down_unreg(queue_t *uq, mblk_t *mp, queue_t *dq, uint16 sap, uint16 encap)
{
	mblk_t		*bp;
	S_DATAL_UNREG     *bpp;
	
	if (!(bp = allocb(sizeof(S_DATAL_UNREG), BPRI_MED)))
	{
		if (mp)
			(void) dlerrorack(uq, mp, DL_UNBIND_REQ, DL_SYSERR, ENOSR);
		return (-1);
	}

	bp->b_datap->db_type = M_PROTO;
	bp->b_wptr += sizeof(S_DATAL_UNREG);
	bpp = (S_DATAL_UNREG *)bp->b_rptr;
	bpp->prim_type = DATAL_UNREG;
	bpp->version = DL_VERSION;
	bpp->sap = sap;
	bpp->encap = encap;
	putnext(dq, bp);
	return (0);
}
#endif /* SGI */

/*
 * Format DL_BIND_ACK message to send upstream.
 */
STATIC int
format_bind_ack(q, mpp, sap, max_conind)
queue_t		*q;
mblk_t		**mpp;
dl32_t		sap;
dl32_t		max_conind;
{
	mblk_t		*mp;
	dl_bind_ack_t	*bind_ack;
	int		size;

	size = sizeof(dl_bind_ack_t) + MAXHWLEN;
	if (size > BPSIZE((*mpp))) {
		mp = allocb(size, BPRI_HI);
		if (!mp) {
			(void) dlerrorack(q, *mpp, DL_BIND_REQ, DL_SYSERR, ENOSR);
			return 0;
		}
		freemsg(*mpp);
		*mpp = mp;
	} else {
		mp = *mpp;
	}
	mp->b_datap->db_type = M_PCPROTO;
	mp->b_rptr = mp->b_datap->db_base;
	mp->b_wptr = mp->b_rptr + size;
	bind_ack = (dl_bind_ack_t *) mp->b_rptr;
	bind_ack->dl_primitive = DL_BIND_ACK;
	bind_ack->dl_sap = sap;
	bind_ack->dl_addr_length = MAXHWLEN;
	bind_ack->dl_addr_offset = sizeof(dl_bind_ack_t);
	bind_ack->dl_max_conind = max_conind;
	bind_ack->dl_xidtest_flg = (DL_AUTO_XID | DL_AUTO_TEST);
	/*
	 * Set the SAP in the DLSAP address and write in the MAC
	 * address later, when we know that registration with the LAN
	 * driver is complete.
	 */
	return 1;
}


/*
 * Handle DL_UNBIND_REQ message received from upstream.
 */
STATIC int
llc2_unbind_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	llc2up_t	*upp;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_unbind_req: DL_UNBIND_REQ %p\n", upp);
	upp->up_nmlsap = 0;
	upp->up_lpbsap = 0;
	bzero(upp->up_dsap, MAXHWLEN + 1);
	upp->up_ethsap = 0;
#ifdef SGI
	closeup(upp,0);	/* do not do DL_DIASBMULTI_REQ */
#else
	closeup(upp);
#endif
	upp->up_dlpi_state = DL_UNBOUND;
	(void) dlokack(q, mp, DL_UNBIND_REQ);
	return 0;
}

/*
 * Handle DL_SUBS_BIND_REQ message received from upstream.
 */
STATIC int
llc2_subs_bind_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	dl_subs_bind_req_t *dl;
	llc2up_t *upp;
	llc2dn_t *dnp;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_subs_bind_req: DL_SUBS_BIND_REQ %p\n", upp);
	dnp = upp->up_dnp;
	dl = (dl_subs_bind_req_t *) mp->b_rptr;

	if (upp->up_class == LC_LLC2)
	{
		uint8 dsap;

		if (dl->dl_subs_bind_class == DL_PEER_BIND)
			return (DL_UNSUPPORTED);

		if (upp->up_dsap[dnp->dn_maclen])
			return (DL_BADADDR);	/* already subs-bound */

		if (dl->dl_subs_sap_length != dnp->dn_maclen + 1)
			return (DL_BADADDR);	/* bad format */
			
		dsap = *((uint8 *)(mp->b_rptr + dnp->dn_maclen +
			dl->dl_subs_sap_offset));

		if (dsap == 0 || dsap & 1)
			return (DL_BADADDR);	/* Zero/odd SAP not allowed. */

		bcopy((char *)mp->b_rptr + dl->dl_subs_sap_offset,
			(char *) upp->up_dsap, dl->dl_subs_sap_length);

		/*
		 * NB: the construction of the ACK makes use of the
		 * similarity with the REQ. The "class" field is still
		 * there, but should be ignored as "offset" points after it.
		 */
		dl->dl_primitive = DL_SUBS_BIND_ACK;
		mp->b_datap->db_type = M_PCPROTO;
		qreply(q, mp);
		return (0);
	}

	/* upp->up_class == LC_LLC1 */

	if (upp->up_mode == LLC_MODE_NORMAL)
	{
		uint8 ssap;

		if (dl->dl_subs_bind_class == DL_PEER_BIND &&
			dl->dl_subs_sap_length != dnp->dn_maclen + 1)
				return (DL_BADADDR);	/* bad format */

		if (dl->dl_subs_sap_length < dnp->dn_maclen + 1)
			return (DL_BADADDR);

		ssap = *((uint8 *)(mp->b_rptr + dnp->dn_maclen +
			dl->dl_subs_sap_offset));

		if (ssap == 0 || ssap & 1)
			return (DL_BADADDR);	/* Zero/odd SAP not allowed. */

		if (register_sap(dnp->llc1_match, RD(q), (uint8 *)
			mp->b_rptr + dl->dl_subs_sap_offset + dnp->dn_maclen,
			dl->dl_subs_sap_length - dnp->dn_maclen) < 0)
				return (DL_BOUND);

#ifdef SGI
		if (ssap == 0xAA && 
		    (dl->dl_subs_sap_length == dnp->dn_maclen + 1 + 5) )
		{
		    uint8 *p=(uint8*)mp->b_rptr+dl->dl_subs_sap_offset+
				dnp->dn_maclen+1;
		    uint16 etype;
		    DLP(("llc2_subs_bind_req: SNAP,"));
		    etype = p[3];
		    etype = etype << 8 | p[4];
		    if (p[0] == '\0' && p[1]=='\0' && p[2]=='\0') {
			DLP((" SNAP0 ENCAP\n"));
			if (send_down_reg(q, mp, dnp->dn_downq, 
				etype, DL_SNAP0_ENCAP) < 0)
			    return (0);
		    } else {
			DLP((" LLC ENCAP\n"));
			if (send_down_reg(q, mp, dnp->dn_downq, 
				etype, DL_LLC_ENCAP) < 0)
			    return (0);
		    }
		}
#endif
		/*
		 * NB: the construction of the ACK makes use of the
		 * similarity with the REQ. The "class" field is still
		 * there, but should be ignored as "offset" points after it.
		 */
		dl->dl_primitive = DL_SUBS_BIND_ACK;
		mp->b_datap->db_type = M_PCPROTO;
		qreply(q, mp);
		return (0);
	}
	if (upp->up_mode == LLC_MODE_ETHERNET)
	{
#ifdef SGI
		uint16 sap;
		uint8 *fsap = (uint8 *) mp->b_rptr + dl->dl_subs_sap_offset +
			dnp->dn_maclen;
#endif
		if (dl->dl_subs_bind_class == DL_PEER_BIND &&
			dl->dl_subs_sap_length != dnp->dn_maclen + 2)
				return (DL_BADADDR);	/* bad format */

		if (dl->dl_subs_sap_length < dnp->dn_maclen + 2)
			return (DL_BADADDR);

		if (register_sap(dnp->eth_match, RD(q), (uint8 *)
			mp->b_rptr + dl->dl_subs_sap_offset + dnp->dn_maclen,
			dl->dl_subs_sap_length - dnp->dn_maclen) < 0)
				return (DL_BOUND);
#ifdef SGI
		sap = fsap[1] | fsap[0] << 8;	/* in machine order */

		if (send_down_reg(q, mp, dnp->dn_downq, sap,DL_ETHER_ENCAP) < 0)
			return (0);
#endif
		/*
		 * NB: the construction of the ACK makes use of the
		 * similarity with the REQ. The "class" field is still
		 * there, but should be ignored as "offset" points after it.
		 */
		mp->b_datap->db_type = M_PCPROTO;
		dl->dl_primitive = DL_SUBS_BIND_ACK;
		qreply(q, mp);
		return (0);
	}
	if (upp->up_mode == LLC_MODE_NETWARE)
		return (DL_NOTSUPPORTED);

	return (DL_NOTSUPPORTED);
}

/*
 * Handle DL_SUBS_UNBIND_REQ message received from upstream.
 */
STATIC int
llc2_subs_unbind_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	dl_subs_unbind_req_t *dl;
	llc2up_t *upp;
	llc2dn_t *dnp;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_subs_unbind_req: DL_SUBS_UNBIND_REQ %p\n", upp);
	dnp = upp->up_dnp;
	dl = (dl_subs_unbind_req_t *) mp->b_rptr;

	if (upp->up_class == LC_LLC2)
	{
		if (upp->up_dsap[dnp->dn_maclen] == 0)
			return (DL_BADADDR);	/* not subs-bound */

		if (dl->dl_subs_sap_length != dnp->dn_maclen + 1)
			return (DL_BADADDR);	/* bad format */
			
		if (bcmp((char *)mp->b_rptr + dl->dl_subs_sap_offset,
			(char *) upp->up_dsap, dl->dl_subs_sap_length) != 0)
			return (DL_BADADDR);	/* not bound */

		bzero((char *) upp->up_dsap, MAXHWLEN + 1);

		(void) dlokack(q, mp, DL_SUBS_UNBIND_REQ);
		return (0);
	}

	/* upp->up_class == LC_LLC1 */

	if (upp->up_mode == LLC_MODE_NORMAL)
	{
		uint8 sap;

		if (dl->dl_subs_sap_length < dnp->dn_maclen + 1)
			return (DL_BADADDR);

		sap = *(uint8 *)((mp->b_rptr + dl->dl_subs_sap_offset +
			dnp->dn_maclen));

		/*
		 * Can't subs-unbind for value supplied in the bind.
		 */
		if (sap == upp->up_nmlsap &&
			dl->dl_subs_sap_length == dnp->dn_maclen + 1)
			return (DL_BADADDR);

		if (deregister_sap(dnp->llc1_match, RD(q), (uint8 *)
			mp->b_rptr + dl->dl_subs_sap_offset + dnp->dn_maclen,
			dl->dl_subs_sap_length - dnp->dn_maclen) < 0)
				return (DL_BADADDR);

#ifdef SGI
		if (sap == 0xAA && 
		    (dl->dl_subs_sap_length == dnp->dn_maclen + 1 + 5) )
		{
		    uint8 *p=(uint8*)mp->b_rptr+dl->dl_subs_sap_offset+
				dnp->dn_maclen+1;
		    uint16 etype;
		    DLP(("llc2_subs_unbind_req: SNAP,"));
		    etype = p[3];
		    etype = etype << 8 | p[4];
		    if (p[0] == '\0' && p[1]=='\0' && p[2]=='\0') {
			DLP((" SNAP0 ENCAP\n"));
			if (send_down_unreg(q, mp, dnp->dn_downq, 
				etype, DL_SNAP0_ENCAP) < 0)
			    return (0);
		    } else {
			DLP((" LLC ENCAP\n"));
			if (send_down_unreg(q, mp, dnp->dn_downq, 
				etype, DL_LLC_ENCAP) < 0)
			    return (0);
		    }
		} else {
		    if (send_down_unreg(q,mp,dnp->dn_downq,
			    (uint16)sap, DL_LLC_ENCAP) < 0)
			return (0);
		}
#endif
		(void) dlokack(q, mp, DL_SUBS_UNBIND_REQ);
		return (0);
	}
	if (upp->up_mode == LLC_MODE_ETHERNET)
	{
		uint16 sap;

		if (dl->dl_subs_sap_length < dnp->dn_maclen + 2)
			return (DL_BADADDR);

		bcopy((char *) mp->b_rptr + dl->dl_subs_sap_offset +
			dnp->dn_maclen, &sap, 2);

		/*
		 * Can't subs-unbind for value supplied in the bind.
		 */
		if (sap == upp->up_ethsap &&
			dl->dl_subs_sap_length == dnp->dn_maclen + 2)
			return (DL_BADADDR);

		if (deregister_sap(dnp->eth_match, RD(q), (uint8 *)
			mp->b_rptr + dl->dl_subs_sap_offset + dnp->dn_maclen,
			dl->dl_subs_sap_length - dnp->dn_maclen) < 0)
				return (DL_BADADDR);

#ifdef SGI
		if (send_down_unreg(q, mp, dnp->dn_downq, 
			    sap, DL_ETHER_ENCAP) < 0)
			return (0);
#endif
		(void) dlokack(q, mp, DL_SUBS_UNBIND_REQ);
		return (0);
	}
	if (upp->up_mode == LLC_MODE_NETWARE)
		return (DL_NOTSUPPORTED);

	return (DL_NOTSUPPORTED);
}

/*
 * Handle DL_CONNECT_REQ message received from upstream.  (Is equiv. to
 * Spider LC_CONNECT message.)
 */
STATIC int
llc2_conn_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	llc2up_t	*upp;
	llc2cn_t	*lp;
	llc2cn_t	*xlp;
	llc2cn_t	**hashp;
	int		error;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_conn_req: DL_CONNECT_REQ %p\n", upp);
	if (upp->up_class != LC_LLC2) {
		return DL_OUTSTATE;
	}
	lp = llc2_connectup(upp);
	if (lp == NULL) {
		(void) dlerrorack(q, mp, DL_CONNECT_REQ, DL_SYSERR, ENOBUFS);
		return 0;
	}
	error = llc2_dlpi_dtecpy(lp, upp, mp, upp->up_dnp->dn_macaddr, 0);
	if (error) {
		llc2_disconnect(lp);
		return error;
	}
	/*
	 * Ensure that (remote MAC addr + DSAP + SSAP) is unique.
	 * If unique, add to down stream's hash table.
	 */
	DLP(("DL_CONNECT_REQ: look into hashtab with index=%d\n",
		(HASH(lp->dte))));
	hashp = upp->up_dnp->dn_hashtab + (HASH(lp->dte));
	for (xlp = *hashp;; xlp = xlp->hnext) {
		if (!xlp) {
			/* Unique, so add to hash */
			lp->hnext = *hashp;
			*hashp = lp;
			break;
		}
		if (bcmp((char *)xlp->dte, (char *)lp->dte,
			upp->up_dnp->dn_maclen + 2) == 0)
		{
			/*
			 * Found, so fail.  It would be appropriate to
			 * return an "address already in use" error here,
			 * but no such error exists in DLPI.
			 */
			llc2_disconnect(lp);
			return DL_ACCESS;
		}
	}

	upp->up_dlpi_state = DL_OUTCON_PENDING;
	enqueue_ctrl_msg(mp, lp, LC_CONNECT);
	return 0;
}


/*
 * Handle DL_CONNECT_RES message received from upstream.  (Is equiv. to
 * Spider LC_CONOK message.)
 */
STATIC int
llc2_conn_res(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	llc2up_t	*upp;
	llc2cn_t	*lp;
	dl_connect_res_t *conn_res;
	mblk_t		*bp;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_conn_res: DL_CONNECT_RES %p\n", upp);
	conn_res = (dl_connect_res_t *) mp->b_rptr;
	lp = upp->up_cnlist;
	while (lp && lp->connID != conn_res->dl_correlation) {
		lp = lp->next;
	}
	if (!lp) {
		return DL_BADCORR;
	}
	DLP(("llc2_conn_res: lp=%p, up_cnlist=%p\n",lp,upp->up_cnlist));
	/*
	 * Allocate a message block to eventually hold the
	 * DL_OK_ACK message sent upstream.
	 */
	bp = allocb(sizeof(dl_ok_ack_t), BPRI_HI);
	if (!bp) {
		(void) dlerrorack(q, mp, DL_CONNECT_RES, DL_SYSERR, ENOSR);
		return 0;
	}
	if (conn_res->dl_resp_token) {
		llc2up_t	*accept_upp;
		int		n;
		int		found;

		/*
		 * The connection will be not be accepted on the
		 * listen stream, but instead on the stream whose
		 * token (== "up" pointer) matches 'dl_resp_token'.
		 * First look for stream with matching token.
		 */
		DLP(("\tdl_resp_token: %x\n",conn_res->dl_resp_token));
		found = 0;
		for (n = llc2_nups, accept_upp = llc2_uptab; n--;
		     accept_upp++) {
#ifdef SGI
			if (accept_upp->up_readq)
			    if (QPTR_TO_TOKEN(accept_upp->up_readq) ==
					conn_res->dl_resp_token) 
					{
				found = 1;
				break;
			}
#else
			if (accept_upp->up_readq &&
			    (ulong)accept_upp->up_readq ==
					conn_res->dl_resp_token) 
					{
				found = 1;
				break;
			}
#endif
		}
		if (!found) {
			freeb(bp);
			return DL_BADTOKEN;
		}
		/*
		 * Ensure that the accepting stream is attached to the
		 * same physical device as the listen stream, and that.
		 * the accepting stream is in the correct DLPI state.
		 */
		if (accept_upp->up_dnp != upp->up_dnp ||
		    accept_upp->up_dlpi_state != DL_IDLE) {
			freeb(bp);
			return DL_OUTSTATE;
		}
		/*
		 * Make sure that the accepting stream is bound to the
		 * correct SAP for the connection!
		 */
		if (accept_upp->up_nmlsap != lp->dte[0]) {
			freeb(bp);
			return DL_ACCESS;
		}
		/*
		 * Move lp from the connection list of 'upp' (the listen
		 * stream) to the connection list of 'accept_upp'
		 * (the accepting stream).
		 */
		if (lp == upp->up_cnlist) {
			upp->up_cnlist = lp->next;
		} else {
			llc2cn_t	*lpprev;

			lpprev = upp->up_cnlist;
			while (lpprev->next != lp) {
				lpprev = lpprev->next;
			}
			lpprev->next = lp->next;
		}
		lp->next = NULL;
		lp->upp = accept_upp;
		accept_upp->up_cnlist = lp;
		accept_upp->up_dlpi_state = DL_DATAXFER;
		/*
		 * If there are no other outstanding connect indications
		 * on the listen stream, its state can go back to DL_IDLE.
		 */
		if (upp->up_cnlist == NULL) {
			upp->up_dlpi_state = DL_IDLE;
		}
	} else {
		/*
		 * Accepting the connection on the listen stream.
		 * Ensure first that 1), the user is not trying to
		 * accept a connection on the "connection management"
		 * stream, and 2), that there are no other outstanding
		 * connect indications for this stream (because
		 * once a connection has been established on a
		 * listen stream, it can't accept any more incoming
		 * connect indications).
		 */
		if (upp->up_conn_mgmt || upp->up_cnlist->next) {
			freeb(bp);
			return DL_PENDING;
		}
		upp->up_dlpi_state = DL_DATAXFER;
	}
	upp->up_num_conind--;
	enqueue_ctrl_msg(mp, lp, LC_CONOK);
	(void) dlokack(q, bp, DL_CONNECT_RES);
	return 0;
}


/*
 * Handle DL_TOKEN_REQ message received from upstream.
 */
STATIC int
llc2_token_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	dl_token_ack_t	*token_ack;

	LLC2_PRINTF("llc2_token_req: DL_TOKEN_REQ, upp=%p\n", q->q_ptr);
	if (sizeof(dl_token_ack_t) > BPSIZE(mp)) {
		mblk_t		*bp;

		bp = allocb(sizeof(dl_token_ack_t), BPRI_HI);
		if (!bp) {
			(void) dlerrorack(q, mp, DL_TOKEN_REQ, DL_SYSERR, ENOSR);
			return 0;
		}
		freemsg(mp);
		mp = bp;
	}
	mp->b_datap->db_type = M_PCPROTO;
	mp->b_rptr = mp->b_datap->db_base;
	mp->b_wptr = mp->b_rptr + sizeof(dl_token_ack_t);
	token_ack = (dl_token_ack_t *) mp->b_rptr;
	token_ack->dl_primitive = DL_TOKEN_ACK;
#ifdef SGI
	token_ack->dl_token = QPTR_TO_TOKEN(RD(q));
#else /* SGI */
	token_ack->dl_token = (ulong) RD(q);
#endif /* SGI */
	qreply(q, mp);
	return 0;
}


/*
 * Handle DL_DISCONNECT_REQ message received from upstream.  (Is equiv. to
 * Spider LC_DISC message.)
 */
STATIC int
llc2_discon_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	dl_disconnect_req_t *discon_req;
	llc2up_t	*upp;
	llc2cn_t	*lp;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_discon_req: DL_DISCONNECT_REQ %p\n", upp);
	if (!upp) {
		return DL_OUTSTATE;
	}
	switch (upp->up_dlpi_state) {
	default:
		return DL_OUTSTATE;

	case DL_INCON_PENDING:
	case DL_OUTCON_PENDING:
	case DL_DATAXFER:
	case DL_USER_RESET_PENDING:
	case DL_PROV_RESET_PENDING:
		break;
	}
	discon_req = (dl_disconnect_req_t *) mp->b_rptr;
	if (upp->up_dlpi_state == DL_INCON_PENDING) {
		/*
		 * Rejecting previous DL_CONNECT_IND.
		 */
		lp = upp->up_cnlist;
		while (lp && lp->connID != discon_req->dl_correlation) {
			lp = lp->next;
		}
		if (!lp) {
			return DL_BADCORR;
		}
		upp->up_dlpi_state = DL_DISCON9_PENDING;
		enqueue_ctrl_msg(mp, lp, LC_CONNAK);
	} else {
		if (discon_req->dl_correlation != 0) {
			return DL_BADCORR;
		}
		upp->up_dlpi_state = DL_DISCON8_PENDING;
		enqueue_ctrl_msg(mp, upp->up_cnlist, LC_DISC);
	}
	return 0;
}


/*
 * Handle DL_RESET_REQ message received from upstream.  (Is equiv. to
 * Spider LC_RESET message.)
 */
STATIC int
llc2_reset_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	llc2up_t	*upp;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_reset_req: DL_RESET_REQ %p\n", upp);
	upp->up_dlpi_state = DL_USER_RESET_PENDING;
	enqueue_ctrl_msg(mp, upp->up_cnlist, LC_RESET);
	return 0;
}


/*
 * Handle DL_RESET_RES message received from upstream.  (No equivalent
 * to a Spider message, so pass LC_RSTCNF message downstream.)
 */
STATIC int
llc2_reset_res(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	llc2up_t	*upp;
	mblk_t		*bp;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_reset_res: DL_RESET_RES %p\n", upp);
	bp = allocb(sizeof(dl_ok_ack_t), BPRI_HI);
	if (!bp) {
		(void) dlerrorack(q, mp, DL_RESET_RES, DL_SYSERR, ENOSR);
		return 0;
	}
	upp->up_dlpi_state = DL_DATAXFER;
	enqueue_ctrl_msg(mp, upp->up_cnlist, LC_RSTCNF);
	(void) dlokack(q, bp, DL_RESET_RES);
	return 0;
}


/*
 * Handle DL_UNITDATA_REQ message received from upstream.  (Is equiv. to
 * Spider LC_UDATA message.)
 */
STATIC int
llc2_unitdata_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	llc2up_t	*upp;

	upp = (llc2up_t *) q->q_ptr;
	LLC2_PRINTF("llc2_unitdata_req: DL_UNITDATA_REQ %p\n", upp);
	if (!upp || upp->up_dlpi_state != DL_IDLE || upp->up_class != LC_LLC1) {
		reply_uderror(q, mp, DL_OUTSTATE);
		return 0;
	} 
	putq(q, mp);
	schedule_udata(upp, 1);
	return 0;
}


/*
 * Handle DL_ENABMULTI_REQ and DL_DISABMULTI_REQ primitives.  Do some
 * sanity checks, then pass the primitive on to the LAN driver.  This
 * is necessary because multicast addresses are enabled on a per-stream
 * basis at the LAN driver level.  So, in order to enable a multicast
 * address on the stream to the LAN driver that is I_LINKed underneath
 * the LLC2 driver, it is necessary to pass the request through the
 * LLC2 driver.
 */
STATIC int
llc2_enab_disab_multi(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	llc2up_t	*upp;
	dl_enabmulti_req_t *req;
	uint8		*addr;
	dlu32_t		prim;
	struct datal_address    *datal_msg;

	DLP(("llc2_enab_disab_multi: %d\n", *((dlu32_t*)mp->b_rptr)));
	upp = (llc2up_t *) q->q_ptr;
	if (!upp || !upp->up_dnp || upp->up_waiting_ack) {
		DLP(("\tDL_OUTSTATE\n"));
		return DL_OUTSTATE;
	}
	req = (dl_enabmulti_req_t *) mp->b_rptr;
	if (req->dl_addr_length != upp->up_dnp->dn_maclen) {
		DLP(("\tBADADDR, req=%d, dn_maclen=%d\n",
			req->dl_addr_length, upp->up_dnp->dn_maclen));
		return DL_BADADDR;
	}
	addr = (uint8 *) (mp->b_rptr + req->dl_addr_offset);

	/*
	 * Add an entry to the multicast table, or delete an entry,
	 * depending on the primitive.
	 */
	if ((prim = req->dl_primitive) == DL_ENABMULTI_REQ) {
		datal_msg = (struct datal_address *) mp->b_rptr;
		datal_msg->prim_type = DATAL_ENAB_MULTI;
		if (upp->up_multi_count == LLC_MAX_MULTI) {
			DLP(("\tDL_TOOMANY\n"));
			return DL_TOOMANY;
		}
		bcopy(addr, (char *)&upp->up_multi_tab[upp->up_multi_count],
			upp->up_dnp->dn_maclen);
		upp->up_multi_count++;

	} else {  /* primitive == DL_DISABMULTI_REQ */
		int		i;
		int		found = 0;

		datal_msg = (struct datal_address *) mp->b_rptr;
		datal_msg->prim_type = DATAL_DISAB_MULTI;
		/*
		 * Find the MAC address in the multicast table.  If found,
		 * delete it by shifting all subsequent table entries down
		 * by one.
		 */
		for (i = 0; i < (int) upp->up_multi_count; i++) {
		    if (bcmp(addr, (char *)&upp->up_multi_tab[i],
				upp->up_dnp->dn_maclen) == 0) {
			if (--upp->up_multi_count > i)
			{
			    bcopy((char *)&upp->up_multi_tab[i+1],
				(char *)&upp->up_multi_tab[i],
				(upp->up_multi_count - i) * MAXHWLEN);
			}
			found = 1;
		    }
		}
		if (!found) {
			DLP(("\tDL_NOTENAB\n"));
			return DL_NOTENAB;
		}
	}
	upp->up_waiting_ack = prim;

	/* Convert message to DATAL type and pass down */
	datal_msg->addr_len = req->dl_addr_length;
	bcopy(addr, datal_msg->addr, req->dl_addr_length);

	putnext (upp->up_dnp->dn_downq, mp);
	DLP(("\tOK\n"));
	return 0;
}

/*
 * Handle DL_PHYS_ADDR_REQ primitive.
 */
STATIC int
llc2_get_phys(q, mp)
queue_t         *q;
mblk_t          *mp;
{
	llc2up_t        	*upp;
	dl_phys_addr_req_t 	*req;
        struct datal_address    *datal_msg;
	mblk_t			*msg;

	upp = (llc2up_t *) q->q_ptr;
	if (!upp || upp->up_dnp == NULL || upp->up_waiting_ack)
	{
		return DL_OUTSTATE;
	}
	req = (dl_phys_addr_req_t *)  mp->b_rptr;
	msg = allocb((int)sizeof(struct datal_address)+upp->up_dnp->dn_maclen, 
			BPRI_HI);
	if (!msg)
	{
                (void) dlerrorack(q, mp, DL_PHYS_ADDR_REQ, DL_SYSERR, ENOSR);
                return 0;
	}
        datal_msg = (struct datal_address *) msg->b_rptr;
	msg->b_wptr += sizeof(struct datal_address) +upp->up_dnp->dn_maclen;
	msg->b_datap->db_type = M_PROTO;
	if (req->dl_addr_type == DL_FACT_PHYS_ADDR)
        	datal_msg->prim_type = DATAL_GET_PHYS_ADDR;
	else
		datal_msg->prim_type = DATAL_GET_CUR_ADDR;
	datal_msg->error_field = 0;
	datal_msg->addr_len = 0;
	upp->up_waiting_ack = req->dl_primitive;
	freemsg(mp);
	putnext (upp->up_dnp->dn_downq, msg);
	return 0;
}

/*
 * Handle DL_SET_PHYS_ADDR_REQ primitive.
 */
STATIC int
llc2_set_phys(q, mp)
queue_t         *q;
mblk_t          *mp;
{
	llc2up_t        	*upp, *up;
	dl_set_phys_addr_req_t 	*req;
        struct datal_address    *datal_msg;
	uint8			*addr;

	upp = (llc2up_t *) q->q_ptr;
	if (!upp || upp->up_dnp == NULL || upp->up_waiting_ack)
	{
		return DL_OUTSTATE;
	}

	/* check address length */
	req = (dl_set_phys_addr_req_t *) mp->b_rptr;
	if (req->dl_addr_length != upp->up_dnp->dn_maclen)
	{
		(void) dlerrorack(q,mp,DL_SET_PHYS_ADDR_REQ,DL_BADADDR,0);
		return 0;
	}

	/* check nothing bound to this PPA */
	for (up = upp->up_dnp->dn_uplist; up; up = up->up_dnext)
	{
		if (up->up_interface != LLC_IF_DLPI ||
			up->up_dlpi_state != DL_UNBOUND)
		{
			(void) dlerrorack(q,mp,DL_SET_PHYS_ADDR_REQ,DL_BUSY,0);
			return 0;
		}
	}
	upp->up_waiting_ack = req->dl_primitive;
	addr = (uint8*) (mp->b_rptr + req->dl_addr_offset);
        datal_msg = (struct datal_address *) mp->b_rptr;
        datal_msg->prim_type = DATAL_SET_CUR_ADDR;
	datal_msg->addr_len = req->dl_addr_length;
	datal_msg->error_field = 0;
	bcopy (addr, datal_msg->addr, req->dl_addr_length);
	putnext (upp->up_dnp->dn_downq, mp);
	return 0;
}


/*
 * Handle DL_TEST_REQ, DL_TEST_RES, DL_XID_REQ, and DL_XID_RES primitives.
 */
STATIC int
llc2_xid_test(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	llc2up_t	*upp;
	int		prim;
	int		error = 0;

	upp = (llc2up_t *) q->q_ptr;
	prim = ((union DL_primitives *) mp->b_rptr)->dl_primitive;
	switch (prim) {		/* XXX remove later */
	case DL_TEST_REQ:
		LLC2_PRINTF("llc2_xid_test: DL_TEST_REQ %p\n", upp);
		strlog(LLC_STID,0,9,SL_TRACE,"llc2: DL_TEST_REQ %p\n",upp);
		break;
	case DL_TEST_RES:
		LLC2_PRINTF("llc2_xid_test: DL_TEST_RES %p\n", upp);
		strlog(LLC_STID,0,9,SL_TRACE,"llc2: DL_TEST_RES %p\n",upp);
		break;
	case DL_XID_REQ:
		LLC2_PRINTF("llc2_xid_test: DL_XID_REQ %p\n", upp);
		strlog(LLC_STID,0,9,SL_TRACE,"llc2: DL_XID_REQ %p\n",upp);
		break;
	case DL_XID_RES:
		LLC2_PRINTF("llc2_xid_test: DL_XID_RES %p\n", upp);
		strlog(LLC_STID,0,9,SL_TRACE,"llc2: DL_XID_RES %p\n",upp);
		break;
	default:
		break;
	}
	if (!upp || (upp->up_dlpi_state != DL_IDLE &&
		     upp->up_dlpi_state != DL_DATAXFER)) {
		error = DL_OUTSTATE;
	}
	if (!error) {
		switch (prim) {
		case DL_TEST_REQ:
		case DL_TEST_RES:
			if (upp->up_xidtest_flg & DL_AUTO_TEST) {
				error = DL_TESTAUTO;
			}
			break;
		case DL_XID_REQ:
		case DL_XID_RES:
			if (upp->up_xidtest_flg & DL_AUTO_XID) {
				error = DL_XIDAUTO;
				break;
			}
			if (mp->b_cont == NULL) {
				mblk_t		*bp;

				/*
				 * No M_DATA block was supplied from
				 * upstream.  Append a zero-length M_DATA
				 * block to prevent send_Ucmd/send_Ursp
				 * from inserting XID data into the
				 * outgoing frame.
				 */
				bp = allocb(0, BPRI_HI);
				if (bp) {
					mp->b_cont = bp;
				} else {
					error = DL_SYSERR;
				}
			}
			break;
		default:
			break;
		}
	}
	LLC2_PRINTF("llc2_xid_test: error 0x%x\n", error);
	if (error) {
		/*
		 * Error condition.  If DL_TEST_REQ and DL_XID_REQ generate
		 * errors, they cause a DL_ERROR_ACK message to be sent
		 * upstream.  If a DL_TEST_RES or a DL_XID_RES command is
		 * invalid, it will be dropped silently, because DLPI
		 * makes no provision for error returns from these
		 * primitives.
		 */
		strlog(LLC_STID,0,1,SL_TRACE,"llc2_xid_test: error 0x%x\n", error);
		if (prim == DL_TEST_REQ || prim == DL_XID_REQ) {
			if (error == DL_SYSERR) {
				(void) dlerrorack(q, mp, prim, DL_SYSERR, ENOSR);
				return 0;
			} else {
				return error;
			}
		} else {
			freemsg(mp);
			return 0;
		}
	}
	putq(q, mp);
	schedule_udata(upp, 1);
	return 0;
}


/*
 * Received a DL_GET_STATISTICS message from upstream.
 * Reply with a DL_GET_STATISTICS_ACK.
 */
STATIC int
llc2_stats_req(q, mp)
queue_t		*q;
mblk_t		*mp;
{
	dl_get_statistics_ack_t	*ack;
	llc2up_t	*upp;
	llc2dn_t	*dnp;
	int		size = sizeof(*ack) + sizeof(dnp->dn_statstab);

	upp = (llc2up_t *)q->q_ptr;
	LLC2_PRINTF("llc2wput: DL_GET_STATISTICS_REQ %p\n", upp);
	if (!upp || !(dnp = upp->up_dnp))
	{
		return DL_OUTSTATE;
	}
	if (size > BPSIZE(mp))
	{
		mblk_t		*bp;

		bp = allocb(size, BPRI_HI);
		if (!bp) {
			(void) dlerrorack(q,mp,DL_GET_STATISTICS_REQ,DL_SYSERR,ENOSR);
			return 0;
		}
		freemsg(mp);
		mp = bp;
	}
	mp->b_datap->db_type = M_PCPROTO;
	mp->b_rptr = mp->b_datap->db_base;
	mp->b_wptr = mp->b_rptr + size;
	ack = (dl_get_statistics_ack_t *) mp->b_rptr;
	ack->dl_primitive = DL_GET_STATISTICS_ACK;
	ack->dl_stat_length = sizeof(dnp->dn_statstab);
	ack->dl_stat_offset = sizeof(*ack);
	bcopy((BUFP_T)&dnp->dn_statstab, (BUFP_T)(mp->b_rptr + sizeof(*ack)),
		sizeof(dnp->dn_statstab));
	qreply(q, mp);
	return 0;
}

/*
 * Schedule the stream with q_ptr "upp" for service on the queue of
 * streams with connectionless data to send (dnp->dn_udatq).
 */
void
schedule_udata(upp, need_enable)
llc2up_t	*upp;
int		need_enable;
{
	llc2dn_t	*dnp;

	dnp = upp->up_dnp;
	if (dnp->dn_udatq) {
		/*
		 * There is a stream already scheduled.
		 */
		if (!(upp->up_udatqnext || dnp->dn_udatqtail == upp)) {
			/* This stream is not on the chain */
			dnp->dn_udatqtail->up_udatqnext = upp;
			dnp->dn_udatqtail = upp;
		}
	} else {
		/*
		 * Chain was empty, so start with this component
		 * and enable queue.
		 */
		dnp->dn_udatq = upp;
		dnp->dn_udatqtail = upp;
		if (need_enable) {
			qenable(dnp->dn_downq);
		}
	}
}


/*
 * Copy destination DLSAP address from the DLPI message held in 'mp' into
 * 'lp'.  Accepts DLPI messages in two different formats.
 * Format 1:
 *   DL_CONNECT_REQ or DL_UNITDATA_REQ
 * Format 2:
 *   DL_TEST_REQ, DL_TEST_RES, DL_XID_REQ, or DL_XID_RES
 * 'test_or_xid' non-zero indicates that the message is in format #2.
 */
int
llc2_dlpi_dtecpy(lp, upp, mp, dnmac, test_or_xid)
llc2cn_t	*lp;
llc2up_t	*upp;
mblk_t		*mp;
uint8		*dnmac;
int		test_or_xid;
{
	uint8 *addr;
	uint8 maclen = upp->up_dnp->dn_maclen;
	uint8 addrlen;
	uint32 routelen;
	if (test_or_xid) {
		/*
		 * NB: This code assumes that the structures "dl_test_req_t",
		 * "dl_test_res_t", "dl_xid_req_t", and "dl_xid_res_t" have
		 * fields "dl_dest_addr_length" and "dl_dest_addr_offset"
		 * in the same position. This is true for DLPI version 2.
		 * If this position changes, then this code needs updating.
		 */

		dl_test_req_t	*test_req;

		strlog(LLC_STID,0,9,SL_TRACE,
			"LLC - dlpi_dtecpy, test_or_xid");
		if (upp->up_mode != LLC_MODE_NORMAL) {
			strlog(LLC_STID,0,1,SL_TRACE,"LLC - Not Normal Mode\n");
			return DL_BADADDR;
		}

		addrlen = maclen + 1;

		test_req = (dl_test_req_t *) mp->b_rptr;
		routelen = test_req->dl_dest_addr_length - addrlen;
		strlog(LLC_STID,0,9,SL_TRACE,"LLC - routelen=%d\n", routelen);
		if (routelen && (routelen < MINRTLEN || routelen > MAXRTLEN)) {
			strlog(LLC_STID,0,1,SL_TRACE,"LLC - Bad routelen\n");
			return DL_BADADDR;
		}
		addr = (uint8 *)(mp->b_rptr + test_req->dl_dest_addr_offset);
#ifdef DEBUG
		if (routelen) {
			DLP(("LLC - xid/test rptr=%p, addr=%p\n",
				mp->b_rptr, addr));
/*			llc2_hexdump(addr, test_req->dl_dest_addr_length); */
		}
#endif

		lp->dte[0] = upp->up_nmlsap;	/* SSAP */
		lp->dte[1] = addr[maclen];	/* DSAP */
	}
	else if (upp->up_class == LC_LLC2)
	{
		dl_connect_req_t *conn_req;

		conn_req = (dl_connect_req_t *) mp->b_rptr;
		addrlen = maclen + 1;
		routelen = conn_req->dl_dest_addr_length - addrlen;
		if (routelen && (routelen < MINRTLEN || routelen > MAXRTLEN)) {
			return DL_BADADDR;
		}
		addr = (uint8 *)(mp->b_rptr + conn_req->dl_dest_addr_offset);

		if (addr[maclen] == 0 || addr[maclen] & 1)
		{
			/*
			 * Zero or odd SAP not allowed.
			 */
			return DL_BADADDR;
		}

		lp->dte[0] = upp->up_nmlsap;	/* SSAP */
		lp->dte[1] = addr[maclen];	/* DSAP */
	} else	/* upp->up_class == LC_LLC1 */
	{
		dl_unitdata_req_t *unit_req;

		unit_req = (dl_unitdata_req_t *) mp->b_rptr;
		addr = (uint8 *)(mp->b_rptr + unit_req->dl_dest_addr_offset);

		if (upp->up_mode == LLC_MODE_NORMAL)
		{
			addrlen = maclen + 1;

			if (addr[maclen] == 0 || addr[maclen] & 1)
			{
				/*
				 * Zero or odd SAP not allowed.
				 */
				return DL_BADADDR;
			}
			lp->dte[0] = upp->up_nmlsap;	/* SSAP */
			lp->dte[1] = addr[maclen];	/* DSAP */
		}
		else if (upp->up_mode == LLC_MODE_ETHERNET)
		{
			addrlen = maclen + 2;
			/* store the Ethernet type in dte[0-1] */
			lp->dte[0] = addr[maclen];
			lp->dte[1] = addr[maclen + 1];
		}
		else /* upp->up_mode == LLC_MODE_NETWARE */
		{
			uint8		*fp;

			addrlen = maclen;


			/*
			 * In Netware "raw mode", the first two bytes of
			 * trailing M_DATA msg block must be 0xff.
			 */
			fp = (uint8 *) mp->b_cont->b_rptr;
			LLC2_PRINTF("llc2_dlpi_dtecpy:NetWare mode, fp[0] 0x%x",
				fp[0]);
			LLC2_PRINTF("    fp[1] 0x%x\n", fp[1]);
			if (fp[0] != 0xff || fp[1] != 0xff) {
				return DL_BADADDR;
			}
		}
		routelen = unit_req->dl_dest_addr_length - addrlen;
		if (routelen && (routelen < MINRTLEN || routelen > MAXRTLEN)) {
			return DL_BADADDR;
		}
	}

	/*
	 * Copy in the destination MAC address.
	 */
	bcopy((char *)addr, (char *)lp->dte + 2, maclen);

	/*
	 * if we have source routing info, put a copy in lp->route
	 */
	if (lp->routelen = (uint8) routelen)
	{
	    bcopy(addr + addrlen, lp->route, routelen);
	    strlog(LLC_STID,0,9,SL_TRACE,"LLC - dtecpy: copy over RIF to lp\n");
	}

	/*
	 * Check for loopback.
	 */
	if (bcmp((char *)addr, (char *)dnmac, maclen) == 0)
	{
		/*
                 * Loopback: SAPs must be different except for LLC1 when
		 * loopback to the same SAP is allowed.
		 */
		if (lp->dte[0] == lp->dte[1] && upp->up_class != LC_LLC1) {
			return DL_BADADDR;
		}
		lp->loopback = 1;
	} else {
		lp->loopback = 0;
	}
	return 0;
}


/*
 * Copy the DLSAP address composed of 'macaddr' and 'sap' into message
 * block 'mp' at 'offset'.
 */
void
set_dlsap_addr(mblk_t *mp,int offset,unchar *macaddr,unchar maclen,unchar sap)
{
	uint8	*addr;

	addr = (uint8 *) (mp->b_rptr + offset);
	llc2_maccpy((bufp_t) addr, (bufp_t) macaddr, maclen);
	if (sap != 0xFF)
	addr[maclen] = sap;
}


/*
 * Transform the DLPI message in 'mp' into a Spider message with
 * command = 'command' (setting the 'll_command' field suffices),
 * and put it on the control queue for the connection 'lp'.
 */
STATIC void
enqueue_ctrl_msg(mp, lp, command)
mblk_t		*mp;
llc2cn_t	*lp;
int		command;
{
	((struct ll_msgc *) mp->b_rptr)->ll_command = command;
	if (lp->ctlq) {
		lp->ctlqtail->b_next = mp;
	} else {
		/*
		 * First message, so schedule connection.
		 */
		lp->ctlq = mp;
		llc2_schedule(lp);
	}
	lp->ctlqtail = mp;
}


/*
 * Send a DL_UDERROR_IND message upstream (error ack of a DL_UNITDATA_REQ
 * primitive).  'mp' points to the DL_UNITDATA_REQ message.  Since
 * sizeof(DL_UDERROR_IND) == sizeof(DL_UNITDATA_REQ), the message can
 * just be reused.
 */
void
reply_uderror(q, mp, error)
queue_t		*q;
mblk_t		*mp;
int		error;
{
	dl_uderror_ind_t *err_ind;

	err_ind = (dl_uderror_ind_t *) mp->b_rptr;
	err_ind->dl_primitive = DL_UDERROR_IND;
	err_ind->dl_errno = error;
	err_ind->dl_unix_errno = 0;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
	}
	mp->b_cont = NULL;
	qreply(q, mp);
}



/*
 *  Formats a DL_ERROR_ACK message using the given parameters and
 *  sends it on the given queue.
 *
 *  Assumes that the given queue is the WR(q).
 */
int
dlerrorack(queue_t *q, mblk_t *mp, dlu32_t prim, dlu32_t err, dlu32_t sys)

{
    int rval;
    
    if (!(rval = format_error_ack(&mp, prim, err, sys)))
	qreply(q,mp);

    return(rval);
}



/*
 *  Formats a DL_OK_ACK message using the given parameter and
 *  sends it on the given queue.
 *
 *  Assumes that the given queue is the WR(q).
 */

int
dlokack(queue_t *q, mblk_t *mp, dlu32_t prim)
{
    int rval;
    
    if (!(rval = format_ok_ack(&mp, prim)))
	qreply(q,mp);

    return(rval);
}


/*
 *  Formats a M_ERROR message using the given parameter and
 *  sends it on the given queue.
 *
 *  Assumes that the given queue is the WR(q).
 */
int
merror(q, mp, error)
queue_t	*q;
mblk_t	*mp;
int      error;
{
    int rval;
    
    if (!(rval = format_merror(&mp, error)))
	qreply(q,mp);

    return(rval);
}



/*
 *  Formats up a DL_OK_ACK. Will use the given buffer if it is large
 *  enough, otherwise it will try and allocate one of suitable size.
 *  Error returned on buffer failure.
 */
STATIC int
format_ok_ack(mblk_t **mpp, dlu32_t prim)
{
    mblk_t *mp;
    dl_ok_ack_t *ok;
    
    int size;
    
    /*
     * Assume the buffer will be large enough
     */
    mp = *mpp;
    
    /*
     * If given buffer too small for the ack then alloc new message.
     * Error if buffers scarce.
     */
    size = DL_OK_ACK_SIZE;
    
    if (!(*mpp) || size > BPSIZE((*mpp)))
    {
	if (!(mp = allocb(size, BPRI_HI)))
	    return(1);

	freemsg(*mpp);
	*mpp = mp;
    }
    mp->b_datap->db_type = M_PCPROTO;
    mp->b_rptr = mp->b_datap->db_base;
    mp->b_wptr = mp->b_rptr + size;

    /*
     * Fill in the appropriate fields
     */
    ok = (dl_ok_ack_t *)mp->b_rptr;
    
    ok->dl_primitive         = DL_OK_ACK;
    ok->dl_correct_primitive = prim;

    return(0);
}

/*
 *  Formats up a DL_ERROR_ACK. Will use the given buffer if it is large
 *  enough, otherwise it will try and allocate one of suitable size.
 *  Error returned on buffer failure.
*/
STATIC int
format_error_ack(mblk_t **mpp, dlu32_t prim, dlu32_t err, dlu32_t sys)

{
    mblk_t *mp;
    dl_error_ack_t *ea;
    
    int size;
    
    /*
     * Assume the buffer will be large enough
     */
    mp = *mpp;
    
    /*
     * If given buffer too small for the ack then alloc new message.
     * Error if buffers scarce.
     */
    size = DL_ERROR_ACK_SIZE;
    
    if (!(*mpp) || size > BPSIZE((*mpp)))
    {
	if (!(mp = allocb(size, BPRI_HI)))
	    return(1);

	freemsg(*mpp);
	*mpp = mp;
    }
    mp->b_datap->db_type = M_PCPROTO;
    mp->b_rptr = mp->b_datap->db_base;
    mp->b_wptr = mp->b_rptr + size;

    /*
     * Fill in the appropriate fields
     */
    ea = (dl_error_ack_t *)mp->b_rptr;
    
    ea->dl_primitive       = DL_ERROR_ACK;
    ea->dl_error_primitive = prim;
    ea->dl_errno           = err;
    ea->dl_unix_errno      = sys;

    return(0);
}


/*
 *  Formats up a M_ERROR. Will use the given buffer if it is large
 *  enough, otherwise it will try and allocate one of suitable size.
 *  Error returned on buffer failure.
 */
STATIC
int format_merror(mblk_t **mpp, int error)

{
    mblk_t *mp;
    
    int size;
    
    /*
     * Assume the buffer will be large enough
     */
    mp = *mpp;
    
    /*
     * If given buffer too small for the ack then alloc new message.
     * Error if buffers scarce.
     */
    size = 1;
    
    if (!(*mpp) || size > BPSIZE((*mpp)))
    {
	if (!(mp = allocb(size, BPRI_HI)))
	    return(1);

	freemsg(*mpp);
	*mpp = mp;
    }
    mp->b_datap->db_type = M_ERROR;
    mp->b_rptr = mp->b_datap->db_base;
    mp->b_wptr = mp->b_rptr + size;

    /*
     * Fill in the error field
     */
    *mp->b_rptr = error;
    
    return(0);
}



#ifdef DEBUG
print_cnlist(cnlist) /* XXX */
llc2cn_t	*cnlist;
{
	llc2cn_t	*lp;
	int		i = 0;

	lp = cnlist;
	while (lp) {
		LLC2_PRINTF(" %p", lp);
		i++;
		if (i >= 5) {
			LLC2_PRINTF("\n", 0);
			return 0;
		}
		lp = lp->next;
	}
	if (i > 0) {
		LLC2_PRINTF("\n", 0);
	} else {
		LLC2_PRINTF("<empty list>\n", 0);
	}
	return 0;
}
#endif /* DEBUG */
