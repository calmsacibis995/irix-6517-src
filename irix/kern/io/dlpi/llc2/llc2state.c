/******************************************************************
 *
 *  SpiderX25 - LLC2 Multiplexer
 *
 *  Copyright 1991 Spider Systems Limited
 *
 *  LLC2STATE.C
 *
 *    LLC2 State Machine.
 *
 ******************************************************************/

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/sys/llc2/86/s.llc2state.c
 *	@(#)llc2state.c	1.50
 *
 *	Last delta created	17:09:03 5/12/93
 *	This file extracted	16:55:22 5/28/93
 *
 */

#ident "@(#)llc2state.c	1.50 (Spider) 5/28/93"

#ifdef ENP
#define FAR
#endif

#ifdef DEVMT
#define FAR far
#endif


#define FAR
#define STATIC static
typedef unsigned char *bufp_t;
typedef char *BUFP_T;

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/sysmacros.h>
#include <sys/log.h>
#include <sys/snet/uint.h>
#include <sys/snet/system.h>
#include <sys/snet/ll_proto.h>
#include <sys/snet/ll_control.h>
#include <sys/snet/dl_proto.h>
#include <sys/snet/timer.h>
#include <sys/dlpi.h>
#include "sys/snet/llc2.h"
/* #include "sys/snet/llc2match_if.h" */


extern mblk_t *llc2_upmp;
extern mblk_t *llc2_dnmp;
extern mblk_t *llc2_rxmp;

void llc2_brk() {
    printf("llc2_brk\n");
}

/* Frame type codes (passed to state machine functions) */
#define CMD_P0  0
#define CMD_P1  1
#define RSP_F0  2
#define RSP_F1  3

/* Decode table for incoming U format frames */
#define U_DUD  -1           /* Illegal value */

#define U_SABME 0           /* Command values */
#define U_DISC  1
#define U_UI	2
#define U_XID   3           /* Command/response values */
#define U_TEST  4
#define U_DM	5           /* Response values */
#define U_UA	6
#define U_FRMR  7


#define U_CMD   U_TEST      /* Last command code */
#define U_RSP   U_XID       /* First response code */

static uswitch [64] =
{
    U_UI,   U_DUD,  U_DUD, U_DM,    U_UI,   U_DUD,  U_DUD, U_DM,
    U_DUD,  U_DUD,  U_DUD, U_DUD,   U_DUD,  U_DUD,  U_DUD, U_DUD,
    U_DISC, U_DUD,  U_DUD, U_DUD,   U_DISC, U_DUD,  U_DUD, U_DUD,
    U_UA,   U_DUD,  U_DUD, U_SABME, U_UA,   U_DUD,  U_DUD, U_SABME,
    U_DUD,  U_FRMR, U_DUD, U_DUD,   U_DUD,  U_FRMR, U_DUD, U_DUD,
    U_DUD,  U_DUD,  U_DUD, U_XID,   U_DUD,  U_DUD,  U_DUD, U_XID,
    U_DUD,  U_DUD,  U_DUD, U_DUD,   U_DUD,  U_DUD,  U_DUD, U_DUD,
    U_TEST, U_DUD,  U_DUD, U_DUD,   U_TEST, U_DUD,  U_DUD, U_DUD
};

#ifdef SGI
extern void llc2_tonet_xid_test(mblk_t *, llc2up_t *, llc2cn_t *);
extern int llc2_xidend(llc2cn_t*, int);
extern int llc2_tonet(llc2cn_t*, int, int);
extern int llc2_tomac(llc2cn_t*, int);

static reset_vars(register llc2cn_t*, uint8);
static llc2cn_t* find_xid_test_sap(int, llc2cn_t**, llc2up_t**);
static int send_ack_rsp(register llc2cn_t*);
static goto_normal(register llc2cn_t*);
static disconnect(register llc2cn_t*, uint8);
static upbycause(register llc2cn_t*, int, int);
static start_reset(register llc2cn_t*);
static uint8 update_nr(register llc2cn_t*, int);
static clr_rem_busy(register llc2cn_t *);
static pfend(llc2cn_t*);
static set_rem_busy(register llc2cn_t *);
static resend_i_rsp(register llc2cn_t*);
static send_SPcmd(register llc2cn_t *, int);
static send_Srsp(register llc2cn_t *, int);
static indinfo(register llc2cn_t *);
static resend_i_rsp(register llc2cn_t *);
static stop_all_timers(register llc2cn_t *);

static ADM_CONNECT_req(register llc2cn_t *);
static ADM_CONOK_req(register llc2cn_t *);
static ADM_CONNAK_req(register llc2cn_t *);

static int IGN_exp(register llc2cn_t *);
static int IGN_U_in(register llc2cn_t *, int);
static int IGN_S_in(register llc2cn_t *, int, int);
static int IGN_I_in(register llc2cn_t *, int, int, int);

static IGN_req(register llc2cn_t *);
static RUN_RESET_req(register llc2cn_t *);
static ANY_DISC_req(register llc2cn_t *);
static RSTCK_RSTCONF_req(register llc2cn_t *);
static RSTCK_DISC_req(register llc2cn_t *);

static RST_ACK_exp(register llc2cn_t *);
static DCN_ACK_exp(register llc2cn_t *);
static ERR_ACK_exp(register llc2cn_t *);
static RUN_ACK_exp(register llc2cn_t *);
static ANY_DLX_exp(register llc2cn_t *);
static RUN_REJ_exp(register llc2cn_t *);

static int ADM_SABME_in(register llc2cn_t *, int);
static int RST_SABME_in(register llc2cn_t *, int);
static int DCN_SABME_in(register llc2cn_t *, int);
static int ERR_SABME_in(register llc2cn_t *, int);
static int RUN_SABME_in(register llc2cn_t *, int);
static int DCN_DISC_in(register llc2cn_t *, int);
static int ANY_XID_in(register llc2cn_t *, int);
static int ANY_TEST_in(register llc2cn_t *, int);
static int ADM_DDM_in(register llc2cn_t *, int);
static int RST_DDM_in(register llc2cn_t *, int);
static int RSTCK_DDM_in(register llc2cn_t *, int);
static int ERR_DDM_in(register llc2cn_t *, int);
static int RUN_DDM_in(register llc2cn_t *, int);
static int DCN_UDM_in(register llc2cn_t *, int);
static int RST_UA_in(register llc2cn_t *, int);
static int RUN_UA_in(register llc2cn_t *, int);
static int ERR_FRMR_in(register llc2cn_t *, int);
static int RUN_FRMR_in(register llc2cn_t *, int);
static int ADM_S_in(register llc2cn_t *, int, int);
static int ERR_S_in(register llc2cn_t *, int, int);
static int RUN_RR_in(register llc2cn_t *, int, int);
static int RUN_RNR_in(register llc2cn_t *, int, int);
static int RUN_REJ_in(register llc2cn_t *, int, int);
static int ADM_I_in(register llc2cn_t *, int, int, int);
static int ERR_I_in(register llc2cn_t *, int, int, int);
static int RUN_I_in(register llc2cn_t *, int, int, int);

static IGN_SABME_in(register llc2cn_t *);
static IGN_DLX_exp(register llc2cn_t *);
static IGN_DDM_in(register llc2cn_t *);
static IGN_XID_in(register llc2cn_t *);
static IGN_TEST_in(register llc2cn_t *);

/* function switch for RESET req */
static int (*RESET_req_fns[])(register llc2cn_t *)= { IGN_req,        IGN_req,
			    IGN_req,        IGN_req,        IGN_req,
			    RUN_RESET_req,  RUN_RESET_req,  IGN_req };

/* function switch for DISC req */
int (*DISC_req_fns[])(register llc2cn_t *) = { IGN_req,        IGN_req,
			    IGN_req,        ANY_DISC_req,   ANY_DISC_req,
			    ANY_DISC_req,   ANY_DISC_req,   RSTCK_DISC_req };

/* function switch for timer expired */
int (*ACK_exp_fns[])()  = { IGN_exp,        IGN_exp,
			    IGN_exp,        RST_ACK_exp,    DCN_ACK_exp,
			    ERR_ACK_exp,    RUN_ACK_exp,    IGN_exp };

int (*DLX_exp_fns[])()  = { IGN_DLX_exp,    IGN_DLX_exp,
			    ANY_DLX_exp,    ANY_DLX_exp,    ANY_DLX_exp,
			    ANY_DLX_exp,    ANY_DLX_exp,    IGN_DLX_exp };

int (*REJ_exp_fns[])()  = { IGN_exp,        IGN_exp,
			    IGN_exp,        IGN_exp,        IGN_exp,
			    IGN_exp,        RUN_REJ_exp,    IGN_exp };

int (**exp_fns[])(register llc2cn_t *)	
			= { ACK_exp_fns,    DLX_exp_fns,    REJ_exp_fns };

/* function switch for U format in */
int (*SABME_in_fns[])() = { IGN_SABME_in,   IGN_SABME_in,
			    ADM_SABME_in,   RST_SABME_in,   DCN_SABME_in,
			    ERR_SABME_in,   RUN_SABME_in,   IGN_SABME_in };

int (*DISC_in_fns[])()  = { IGN_DDM_in,     IGN_DDM_in,
			    ADM_DDM_in,     RST_DDM_in,     DCN_DISC_in,
			    ERR_DDM_in,     RUN_DDM_in,     RSTCK_DDM_in };

int (*UI_in_fns[])()	= { IGN_U_in,       IGN_U_in,
			    IGN_U_in,       IGN_U_in,       IGN_U_in,
			    IGN_U_in,       IGN_U_in,	    IGN_U_in };

int (*XID_in_fns[])()   = { IGN_XID_in,     IGN_XID_in,
			    ANY_XID_in,     ANY_XID_in,     ANY_XID_in,
			    ANY_XID_in,     ANY_XID_in,     ANY_XID_in };

int (*TEST_in_fns[])()  = { IGN_TEST_in,    IGN_TEST_in,
			    ANY_TEST_in,    ANY_TEST_in,    ANY_TEST_in,
			    ANY_TEST_in,    ANY_TEST_in,    ANY_TEST_in };

int (*DM_in_fns[])()	= { IGN_DDM_in,     IGN_DDM_in,
			    ADM_DDM_in,     RST_DDM_in,     DCN_UDM_in,
			    ERR_DDM_in,     RUN_DDM_in,     RSTCK_DDM_in };

int (*UA_in_fns[])()	= { IGN_U_in,       IGN_U_in,
			    IGN_U_in,       RST_UA_in,      DCN_UDM_in,
			    RUN_UA_in,      RUN_UA_in,      IGN_U_in };

int (*FRMR_in_fns[])()  = { IGN_U_in,       IGN_U_in,
			    IGN_U_in,       IGN_U_in,       IGN_U_in,
			    ERR_FRMR_in,    RUN_FRMR_in,    IGN_U_in };

int (**U_in_fns[])(register llc2cn_t *, int)	
			= { SABME_in_fns,   DISC_in_fns,    UI_in_fns,
			    XID_in_fns,     TEST_in_fns,
			    DM_in_fns,      UA_in_fns,      FRMR_in_fns};

/* function switch for S format in */
int (*RR_in_fns[])()	= { IGN_S_in,       IGN_S_in,
			    ADM_S_in,       IGN_S_in,       IGN_S_in,
			    ERR_S_in,       RUN_RR_in,      IGN_S_in };

int (*RNR_in_fns[])()   = { IGN_S_in,       IGN_S_in,
			    ADM_S_in,       IGN_S_in,       IGN_S_in,
			    ERR_S_in,       RUN_RNR_in,     IGN_S_in };

int (*REJ_in_fns[])()   = { IGN_S_in,       IGN_S_in,
			    ADM_S_in,       IGN_S_in,       IGN_S_in,
			    ERR_S_in,       RUN_REJ_in,     IGN_S_in };

int (**S_in_fns[])(register llc2cn_t *, int, int)	
			= { RR_in_fns,      RNR_in_fns,     REJ_in_fns };

/* function switch for I format in */
int (*I_in_fns[])(register llc2cn_t *, int, int, int)	
			= { IGN_I_in,       IGN_I_in,
			    ADM_I_in,       IGN_I_in,       IGN_I_in,
			    ERR_I_in,       RUN_I_in,       IGN_I_in };

#else
extern llc2up_t *llc2_findup();
static uint8 update_nr();
static void find_xid_test_sap();

static ADM_CONNECT_req();
static ADM_CONOK_req();
static ADM_CONNAK_req();

static int IGN_exp();
static int IGN_U_in();
static int IGN_S_in();
static int IGN_I_in();

static IGN_req();
static RUN_RESET_req();
static ANY_DISC_req();
static RSTCK_RSTCONF_req();
static RSTCK_DISC_req();

static RST_ACK_exp();
static DCN_ACK_exp();
static ERR_ACK_exp();
static RUN_ACK_exp();
static ANY_DLX_exp();
static RUN_REJ_exp();

static int ADM_SABME_in();
static int RST_SABME_in();
static int DCN_SABME_in();
static int ERR_SABME_in();
static int RUN_SABME_in();
static int DCN_DISC_in();
static int ANY_XID_in();
static int ANY_TEST_in();
static int ADM_DDM_in();
static int RST_DDM_in();
static int RSTCK_DDM_in();
static int ERR_DDM_in();
static int RUN_DDM_in();
static int DCN_UDM_in();
static int RST_UA_in();
static int RUN_UA_in();
static int ERR_FRMR_in();
static int RUN_FRMR_in();
static int ADM_S_in();
static int ERR_S_in();
static int RUN_RR_in();
static int RUN_RNR_in();
static int RUN_REJ_in();
static int ADM_I_in();
static int ERR_I_in();
static int RUN_I_in();

static IGN_SABME_in();
static IGN_DLX_exp();
static IGN_DDM_in();
static IGN_XID_in();
static IGN_TEST_in();

/* function switch for RESET req */
int (*RESET_req_fns[])()= { IGN_req,        IGN_req,
			    IGN_req,        IGN_req,        IGN_req,
			    RUN_RESET_req,  RUN_RESET_req,  IGN_req };

/* function switch for DISC req */
int (*DISC_req_fns[])() = { IGN_req,        IGN_req,
			    IGN_req,        ANY_DISC_req,   ANY_DISC_req,
			    ANY_DISC_req,   ANY_DISC_req,   RSTCK_DISC_req };

/* function switch for timer expired */
int (*ACK_exp_fns[])()  = { IGN_exp,        IGN_exp,
			    IGN_exp,        RST_ACK_exp,    DCN_ACK_exp,
			    ERR_ACK_exp,    RUN_ACK_exp,    IGN_exp };

int (*DLX_exp_fns[])()  = { IGN_DLX_exp,    IGN_DLX_exp,
			    ANY_DLX_exp,    ANY_DLX_exp,    ANY_DLX_exp,
			    ANY_DLX_exp,    ANY_DLX_exp,    IGN_DLX_exp };

int (*REJ_exp_fns[])()  = { IGN_exp,        IGN_exp,
			    IGN_exp,        IGN_exp,        IGN_exp,
			    IGN_exp,        RUN_REJ_exp,    IGN_exp };

int (**exp_fns[])()	= { ACK_exp_fns,    DLX_exp_fns,    REJ_exp_fns };

/* function switch for U format in */
int (*SABME_in_fns[])() = { IGN_SABME_in,   IGN_SABME_in,
			    ADM_SABME_in,   RST_SABME_in,   DCN_SABME_in,
			    ERR_SABME_in,   RUN_SABME_in,   IGN_SABME_in };

int (*DISC_in_fns[])()  = { IGN_DDM_in,     IGN_DDM_in,
			    ADM_DDM_in,     RST_DDM_in,     DCN_DISC_in,
			    ERR_DDM_in,     RUN_DDM_in,     RSTCK_DDM_in };

int (*UI_in_fns[])()	= { IGN_U_in,       IGN_U_in,
			    IGN_U_in,       IGN_U_in,       IGN_U_in,
			    IGN_U_in,       IGN_U_in,	    IGN_U_in };

int (*XID_in_fns[])()   = { IGN_XID_in,     IGN_XID_in,
			    ANY_XID_in,     ANY_XID_in,     ANY_XID_in,
			    ANY_XID_in,     ANY_XID_in,     ANY_XID_in };

int (*TEST_in_fns[])()  = { IGN_TEST_in,    IGN_TEST_in,
			    ANY_TEST_in,    ANY_TEST_in,    ANY_TEST_in,
			    ANY_TEST_in,    ANY_TEST_in,    ANY_TEST_in };

int (*DM_in_fns[])()	= { IGN_DDM_in,     IGN_DDM_in,
			    ADM_DDM_in,     RST_DDM_in,     DCN_UDM_in,
			    ERR_DDM_in,     RUN_DDM_in,     RSTCK_DDM_in };

int (*UA_in_fns[])()	= { IGN_U_in,       IGN_U_in,
			    IGN_U_in,       RST_UA_in,      DCN_UDM_in,
			    RUN_UA_in,      RUN_UA_in,      IGN_U_in };

int (*FRMR_in_fns[])()  = { IGN_U_in,       IGN_U_in,
			    IGN_U_in,       IGN_U_in,       IGN_U_in,
			    ERR_FRMR_in,    RUN_FRMR_in,    IGN_U_in };

int (**U_in_fns[])()	= { SABME_in_fns,   DISC_in_fns,    UI_in_fns,
			    XID_in_fns,     TEST_in_fns,
			    DM_in_fns,      UA_in_fns,      FRMR_in_fns};

/* function switch for S format in */
int (*RR_in_fns[])()	= { IGN_S_in,       IGN_S_in,
			    ADM_S_in,       IGN_S_in,       IGN_S_in,
			    ERR_S_in,       RUN_RR_in,      IGN_S_in };

int (*RNR_in_fns[])()   = { IGN_S_in,       IGN_S_in,
			    ADM_S_in,       IGN_S_in,       IGN_S_in,
			    ERR_S_in,       RUN_RNR_in,     IGN_S_in };

int (*REJ_in_fns[])()   = { IGN_S_in,       IGN_S_in,
			    ADM_S_in,       IGN_S_in,       IGN_S_in,
			    ERR_S_in,       RUN_REJ_in,     IGN_S_in };

int (**S_in_fns[])()	= { RR_in_fns,      RNR_in_fns,     REJ_in_fns };

/* function switch for I format in */
int (*I_in_fns[])()	= { IGN_I_in,       IGN_I_in,
			    ADM_I_in,       IGN_I_in,       IGN_I_in,
			    ERR_I_in,       RUN_I_in,       IGN_I_in };

#endif



/***************************  I G N  ****************************/
/* IGN implies ignore in ANY state.				*/
/***************************  I G N  ****************************/

/*  *************************************************** IGN_U_in  */
/* ARGSUSED */
static int IGN_U_in(lp, type)
register llc2cn_t *lp;
{
    return IGNORE;
}

/*  *************************************************** IGN_S_in  */
/* ARGSUSED */
static int IGN_S_in(lp, nr, type)
register llc2cn_t *lp;
{
    return IGNORE;
}

/*  *************************************************** IGN_I_in  */
/* ARGSUSED */
static int IGN_I_in(lp, ns, nr, type)
register llc2cn_t *lp;
{
    return IGNORE;
}

/* ARGSUSED */
static int IGN_exp(lp)
register llc2cn_t *lp;
{
    return IGNORE;
}

/* ARGSUSED */
static IGN_req(lp)
register llc2cn_t *lp;
{
    return IGNORE;
}

/* ARGSUSED */
static IGN_DLX_exp(lp)
register llc2cn_t *lp;
{
    return IGNORE;
}


/* ARGSUSED */
static IGN_SABME_in(lp)
register llc2cn_t *lp;
{
return IGNORE;
}

/* ARGSUSED */
static IGN_DDM_in(lp)
register llc2cn_t *lp;
{
return IGNORE;
}

/* ARGSUSED */
static IGN_XID_in(lp)
register llc2cn_t *lp;
{
return IGNORE;
}

/* ARGSUSED */
static IGN_TEST_in(lp)
register llc2cn_t *lp;
{
return IGNORE;
}


/***************************  A N Y  ****************************/
/* ANY implies ANY state.					*/
/***************************  A N Y  ****************************/

/*  *************************************************** ANY_DISC_req  */
static ANY_DISC_req(lp)
register llc2cn_t *lp;
{
    send_Ucmd(lp, DISC);
    reset_vars(lp, DCN);
    set_timer(&lp->ack_timer, (lp->dnp->dn_tunetab.T1 << 1));
    lp->causeflag = 3;  /* See description of DCN */

    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
          "LINK  Dwn : '%x' Id %03x Local",lp->dnp->dn_snid,lp->connID);
    return FRAMEOK;
}

/*  *************************************************** ANY_TEST_in  */
static int ANY_TEST_in(lp, type)
llc2cn_t *lp;
{   /* Reply to TEST command */
    llc2up_t *upp = NULL;
#ifdef SGI
    llc2cn_t *tlp;
#endif

    strlog(LLC_STID,lp->dnp->dn_snid,9,SL_TRACE,
	"TEST  In : '%x' Id %03x",lp->dnp->dn_snid,lp->connID);
#ifdef SGI
    tlp = find_xid_test_sap(TEST, &lp, &upp);
#else
    find_xid_test_sap(TEST, &lp, &upp);
#endif
    if (upp)
    {
	/* Client is a DLPI client who wants to receive incoming TEST
	 * commands/responses. */
	strlog(LLC_STID,lp->dnp->dn_snid,9,SL_TRACE,"send TEST_in up");
/*	llc2_brk(); */
	llc2_tonet_xid_test(llc2_rxmp, upp, lp);
	llc2_rxmp = (mblk_t *)0;
	lp->need_fbit = 0;
	return FRAMEOK;
    }
#ifdef SGI
    if (!tlp) {
#else
    if (!lp) {
#endif
	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
		"ANY_TEST_in: llc2cn_t pointer has been reset; drop it");
	DLP(("ANY_TEST_in: llc2cn_t pointer has been reset; drop it\n"));
	return IGNORE;
#ifdef SGI
    }
#else
    }
#endif

    /* Automatic handling: reply to TEST command, ignore TEST responses */
    strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,"ANY_TEST_in: Auto TEST");
    DLP(("ANY_TEST_in: Auto TEST\n"));

    if (type < RSP_F0)
	send_Ursp(lp, TEST);
    return FRAMEOK;
}

/**************************************************** ANY_XID_in  */
static int ANY_XID_in(lp, type)
llc2cn_t *lp;
{   /* reply to XID command or react to XID response */
    llc2up_t *upp = NULL;
#ifdef SGI
    llc2cn_t *tlp;
#endif

#ifdef SGI
    tlp = find_xid_test_sap(XID, &lp, &upp);
#else
    find_xid_test_sap(XID, &lp, &upp);
#endif
    if (upp && lp->xidflag != 1)
    {
	/* Client is a DLPI client who wants to receive incoming XID
	 * commands/responses, and this is not a response to an
	 * automatic XID. */
	llc2_tonet_xid_test(llc2_rxmp, upp, lp);
	llc2_rxmp = (mblk_t *)0;
	lp->need_fbit = 0;
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
           "XID   In  : '%x' Id %03x",lp->dnp->dn_snid, lp->connID);
	return FRAMEOK;
    }
#ifdef SGI
    if (!tlp)
#else
    if (!lp)
#endif
	return IGNORE;

    /* Automatic handling: reply to XID command or react to XID response */

    if (type < RSP_F0)
	send_Ursp(lp, XID);
    else
    {   /* XID response: end of XID sequence (if one is in progress) */
	if (lp->xidflag)
	{
	    set_timer(&lp->dlx_timer, 0);
	    lp->xidflag = 0;
	    llc2_xidend(lp, 1);
	}
    }


    strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
           "XID   In  : '%x' Id %03x",lp->dnp->dn_snid, lp->connID);

    return FRAMEOK;
}

/*
 * An XID or TEST frame has been received.  Try to find a DLPI stream
 * listening on the given SAP, which needs to be notified of the
 * incoming frame.
 * Preserves the semantics of the Spider interface:
 *	XID or TEST to connected LLC2 SAP -- responded to automatically
 *		    to SAP 0 -- responded to automatically
 *
 * Preconditions:	*upp_p = NULL, *lp_p pts to station component
 * Postconditions:	If there is an up stream to send a DLPI indication to:
 *			  *upp_p = up stream, *lp_p = llc2cn to use
 *			If the frame should be handled automatically:
 *			  *upp_p = NULL, *lp_p = llc2cn to use
 *			If the frame should be dropped
 *			  *upp_p = NULL, *lp_p = NULL
 */
#ifdef SGI
static llc2cn_t*
#else
static void
#endif
find_xid_test_sap(ucmd, lp_p, upp_p)
int ucmd;
llc2cn_t **lp_p;
llc2up_t **upp_p;
{
    llc2cn_t *lp;
    llc2up_t *upp = NULL;
    llc2dn_t *dnp;
    llc2cn_t **hashp;
    uint8 FAR *dte;
#ifdef SGI
    uint8    rcv_dte[MAXHWLEN+2];
#endif

    DLP(("Entering find_xid_test_sap ...\n"));
    if ((*lp_p)->dte[0] == 0)
	/* Automatic handling of frames with DSAP = 0 */
#ifdef SGI
	return(*lp_p);
#else
	return;
#endif

#ifdef SGI
    bcopy((*lp_p)->dte,rcv_dte,MAXHWLEN+2);
    rcv_dte[2] &= 0x7f;	/* turn off RII bit for comparison */
    dte = (uint8 FAR *) &rcv_dte[0];
#else
    dte = (uint8 FAR *) &((*lp_p)->dte[0]);
#endif
    dnp = (*lp_p)->dnp;
    HEXDUMP(dte,(MAXHWLEN+2));

    /* Try to find a connected LLC2 stream -- look in hash table. */
    DLP(("\tLook in hash table for connected stream, index=%d\n",(HASH(dte))));
    hashp = dnp->dn_hashtab + (HASH(dte));
    for (lp = *hashp; lp; lp = lp->hnext)
    {
	DLP(("\tThe received dte:"));
	HEXDUMP(lp->dte,dnp->dn_maclen + 2);
	if (lp->state >= ADM &&
		(bcmp((char *) dte, (char *) lp->dte, dnp->dn_maclen + 2) == 0))
	{
	    DLP(("\tfound lp\n"));
	    /* Found */
	    /* copy over source route */
	    if (lp->routelen = (*lp_p)->routelen)
		bcopy((*lp_p)->route, lp->route, lp->routelen);
#ifndef SGI
	    *lp_p = lp;
#endif
	    upp = lp->upp;
	    if (upp->up_interface != LLC_IF_DLPI)
	    {
		/* Found a connected stream which is running the Spider
		 * interface -- automatic handling applies. */
		DLP(("\tOoop, Not a DLPI stream, return\n"));
#ifdef SGI
		return(lp);
#else
		return;
#endif
	    }
	    break;
	}
    }

    if (!upp)
    {
	DLP(("\tNo upp found in hash table lookup\n"));
	for (upp = dnp->dn_uplist; upp; upp = upp->up_dnext)
	{
		HEXDUMP(upp->up_dsap,dnp->dn_maclen+1);
		if (upp->up_class != LC_LLC2)
			continue;

		if (upp->up_nmlsap != dte[0])
			continue;

#ifdef SGI
		if (upp->up_dsap[dnp->dn_maclen] == 0)
			continue;

		/* It is OK to accept TEST_RSP with remote SAP 0 */
		if (upp->up_dsap[dnp->dn_maclen] != dte[1])
		    	if (!(ucmd==TEST && dte[1]==0))
				continue;
#else
		if (upp->up_dsap[dnp->dn_maclen] == 0 ||
			upp->up_dsap[dnp->dn_maclen] != dte[1])
				continue;
#endif

		if (bcmp((char *)dte + 2, (char *)upp->up_dsap,
			dnp->dn_maclen) == 0)
				break;
	}
    }

    if (!upp)
    {
#ifdef SGI
	queue_t *urq;
	DLP(("\tNo upp found in dnp uplist lookup\n"));
        urq = lookup_sap(dnp->llc1_match, (uint16 *)0,
	   	 (uint8 *) dte, 1, (uint8 *)0, 0);
#else
            queue_t *urq = lookup_sap(dnp->llc1_match, (uint16 *)0,
	   	 (uint8 *) dte, 1, (uint8 *)0, 0);
#endif

	    upp = (llc2up_t *) (urq ? WR(urq)->q_ptr : 0);
    }

    if (!upp)
    {
	/* Try to find an unconnected LLC2 stream */
	DLP(("\tNo upp found from lookup_sap\n"));
	upp = llc2_findup(dnp->dn_uplist, dte[0], LC_LLC2, 1);
	if (upp)
	    if (upp->up_interface != LLC_IF_DLPI)
		upp = NULL;
	if (upp && upp->up_dsap[dnp->dn_maclen] != 0) /* did subs-bind */
		upp = NULL;
    }

    if (upp)
    {
	DLP(("\tFound a DLPI stream\n"));
	/* Found a DLPI stream.  Return a pointer to the up stream only
	 * if automatic TEST/XID handling isn't enabled. */
	if ((ucmd == XID && upp->up_xidtest_flg & DL_AUTO_XID) ||
	    (ucmd == TEST && upp->up_xidtest_flg & DL_AUTO_TEST))
	{
	    DLP(("\tAUTO XID/TEST\n"));
#ifdef SGI
	    return(lp);
#else
	    return;
#endif
	}
	DLP(("\tAssign upp\n"));
	*upp_p = upp;
#ifdef SGI
	return(lp);
#endif
    }

    DLP(("\tNo up stream found\n"));
    if (!upp)
	/* No up stream found ==> XID or TEST should be dropped. */
#ifdef SGI
	return 0;
#else
	*lp_p = NULL;
#endif
#ifdef SGI
    return 0;
#endif
}

/*  *************************************************** ANY_DLX_exp  */
static ANY_DLX_exp(lp)
register llc2cn_t *lp;
{   /*  Ack delay/XID timer has expired */
    if (lp->xidflag)
    {   /* XID timer */
	if (--lp->retry_count == 0)
	{   /* End of last try */
	    lp->xidflag = 0;
	    llc2_xidend(lp, 0);
	}
	else
	{   /* Request another XID */
	    lp->xidflag = 2;
	    llc2_schedule(lp);
	}
    }
    else
	send_ack_rsp(lp);
    return 0;
}


/***************************  A D M  ****************************/
/* LLC2 state within ADM state is always ADM			*/
/* (sflag 0) => no incoming SABME, or DISC/DM after SABME	*/
/* (sflag 1) => SABME received: new connection			*/
/* (sflag 2) => SABME received: awaiting CONOK or CONNAK	*/
/* In this implementation, pflag is always zero in ADM state.   */
/***************************  A D M  ****************************/

/*  *************************************************** ADM_CONNECT_req  */
static ADM_CONNECT_req(lp)
register llc2cn_t *lp;
{
    llc2_tonet(lp, LC_CONOK, 0);

    DLP(("ADM_CONNECT_req: send SABME, timer=%d\n",lp->dnp->dn_tunetab.T1));
    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
	"ADM_CONNECT_req: send SABME, timer=%d\n",lp->dnp->dn_tunetab.T1);
    send_Ucmd(lp, SABME);
    set_timer(&lp->ack_timer, (lp->dnp->dn_tunetab.T1 << 1));
/*  set_timer(&lp->ack_timer, 500); */
    lp->retry_count = lp->dnp->dn_tunetab.N2;

    /* ->LLC2 SETUP, encoded as RST with causeflag = 2 */
    lp->causeflag = 2;
    lp->state = RST;
    return 0;
}

/*  *************************************************** ADM_CONOK_req  */
static ADM_CONOK_req(lp)
register llc2cn_t *lp;
{
    strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
          "LINK  Up  : '%x' Id %03x",lp->dnp->dn_snid, lp->connID);
    DLP(("ADM_CONOK_req: lp=%p, lp->sflag=%d\n",lp,lp->sflag));

    if (lp->sflag)
    {   /* SABME not cancelled by DISC/DM */
	send_Ursp(lp, UA);
	goto_normal(lp);
	return 0;
    }

    /* SABME cancelled, so indicate DISC */
    llc2_tonet(lp, LC_DISC, LS_DISCONNECT);

    /* Stop timers, clear variables and disconnect */
    disconnect(lp, OFF);
    return 0;
}

/*  *************************************************** ADM_CONNAK_req  */
static ADM_CONNAK_req(lp)
register llc2cn_t *lp;
{
    DLP(("ADM_CONNAK_req\n"));
    if (lp->sflag)
    {   /* Reject outstanding SABME */
	DLP(("ADM_CONNAK: Reject outstanding SABME\n"));
	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
	    "ADM_CONNAK: Reject outstanding SABME");
	send_Ursp(lp, DM);
	lp->sflag = 0;
    }

    if (lp->upp->up_interface == LLC_IF_DLPI)
    {
	llc2_tonet(lp, LC_DISCNF, 0);
	disconnect(lp, STOP);
    }
    else
    {
	/* No connection possible */
	disconnect(lp, OFF);
    }
    return 0;
}

/*  *************************************************** ADM_SABME_in  */
/* ARGSUSED */
static int ADM_SABME_in(lp, type)
register llc2cn_t *lp;
{
    switch (lp->sflag)
    {
    case 1:	/* Set by llc2_macinsrv to signal good inward connect */
	llc2_tonet(lp, LC_CONNECT, 0);
	lp->sflag = 2; /* => waiting for reply from net */
	break;

    case 2:	/* Ignore re-transmission before net replies */
    	break;

    default:	/* Unacceptable SABME (e.g. non-existent SAP) */
	DLP(("ADM_SABME_in: Unacceptable SABME, sflag=%d\n",lp->sflag));
	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
	    "ADM_SABME_in: Unacceptable SABME, sflag=%d",lp->sflag);
	send_Ursp(lp, DM);
	break;
    }

    return FRAMEOK;
}

/*  *************************************************** ADM_DDM_in  */
static int ADM_DDM_in(lp, type)
register llc2cn_t *lp;
{   /* Deals with DISC and DM */
    if (lp->sflag == 2 && lp->upp->up_interface == LLC_IF_DLPI)
	llc2_tonet(lp, LC_DISC, 0);
    lp->sflag = 0;
    if (type < RSP_F0)	/* Command => DISC */
	send_Ursp(lp, DM);
    return FRAMEOK;
}

/*  *************************************************** ADM_I_in  */
/* ARGSUSED */
static int ADM_I_in(lp, ns, nr, type)
register llc2cn_t *lp;
{
    if (lp->need_fbit)
	send_Ursp(lp, DM);
    return FRAMEOK;
}

/*  *************************************************** ADM_S_in  */
/* ARGSUSED */
static int ADM_S_in(lp, nr, type)
register llc2cn_t *lp;
{   /* Deals with RR, RNR and REJ */
    DLP(("Entering ADM_S_in\n"));
    if (lp->need_fbit)
	send_Ursp(lp, DM);
    return FRAMEOK;
}


/***************************  R S T  ****************************/
/* LLC2 states are encoded within RST state as follows:		*/
/*	(causeflag 0) => RESET, causeflag=0			*/
/*	(causeflag 1) => RESET, causeflag=1			*/
/*	(causeflag 2) => SETUP					*/
/* In this implementation, pflag is always zero in RST state	*/
/***************************  R S T  ****************************/

/*  *************************************************** RST_SABME_in  */
/* ARGSUSED */
static int RST_SABME_in(lp, type)
register llc2cn_t *lp;
{
    send_Ursp(lp, UA);
    lp->sflag = 1;
    return FRAMEOK;
}

/*  *************************************************** RST_UA_in  */
/* ARGSUSED */
static int RST_UA_in(lp, type)
register llc2cn_t *lp;
{
    upbycause(lp, LS_RESETDONE, LS_SUCCESS);
    goto_normal(lp);
    return FRAMEOK;
}

/*  *************************************************** RST_ACK_exp  */
/* ARGSUSED */
static RST_ACK_exp(lp)
register llc2cn_t *lp;
{   /* ACK timer has expired */
    if (lp->sflag)
    {
	upbycause(lp, LS_RESETDONE, LS_SUCCESS);
	goto_normal(lp);
	return 0;
    }

    if (--lp->retry_count)
    {
	DLP(("RST_ACK_exp: resend SABME\n"));
	send_Ucmd(lp, SABME);
	set_timer(&lp->ack_timer, (lp->dnp->dn_tunetab.T1 << 1));
	return 0;
    }

    upbycause(lp, LS_RST_FAILED, LS_FAILED);

    /* Stop timers, clear variables and disconnect */
    disconnect(lp, STOP);
    return 0;
}

/*  *************************************************** RST_DDM_in  */
static int RST_DDM_in(lp, type)
register llc2cn_t *lp;
{   /* Deals with DISC and DM */
    if (type < RSP_F0)	/* Command => DISC */
	send_Ursp(lp, DM);

    upbycause(lp, LS_RST_REFUSED, LS_DISCONNECT);

    /* Stop timers, clear variables and disconnect */
    disconnect(lp, OFF);
    return FRAMEOK;
}

/* All I- and S-frames ignored */

/************************** R S T C K ***************************/
/* This state corresponds to the LLC2 state RESET_CHECK.  It    */
/* will only be entered if the upper interface is DLPI.  It     */
/* corresponds to the DLPI state DL_PROV_RESET_PENDING          */
/* (a DL_RESET_IND has been received, and waiting for reset     */
/* to be acknowledged with DL_RESET_RES).                       */
/************************** R S T C K ***************************/

/*  *************************************************** RSTCK_RSTCONF_req  */
static RSTCK_RSTCONF_req(lp)
register llc2cn_t *lp;
{
    send_Ursp(lp, UA);
    reset_vars(lp, RUN);
    goto_normal(lp);
    return 0;
}

/*  *************************************************** RSTCK_DISC_req  */
static RSTCK_DISC_req(lp)
register llc2cn_t *lp;
{
    send_Ursp(lp, DM);
    llc2_tonet(lp, LC_DISCNF, LS_DISCONNECT);
    disconnect(lp, STOP);
    return 0;
}

/*  *************************************************** RSTCK_DDM_in  */
static RSTCK_DDM_in(lp, type)
register llc2cn_t *lp;
{   /* Deals with DISC and DM */
    if (type < RSP_F0)    /* => DISC command */
	send_Ursp(lp, DM);

    llc2_tonet(lp, LC_DISC, LS_DISCONNECT);
    disconnect(lp, OFF);
    return FRAMEOK;
}


/***************************  D C N  ****************************/
/* LLC2 states are encoded within DCN state as follows:		*/
/*	(causeflag 0) => D_CONN, causeflag=0			*/
/*	(causeflag 3) => D_CONN, causeflag=1			*/
/*	Using '3' rather than '1' for non-zero causeflag allows */
/*	'upbycause' to distinguish DCN from RST easily.		*/
/* In this implementation, pflag is always zero in DCN state	*/
/***************************  D C N  ****************************/

/*  *************************************************** DCN_SABME_in  */
/* ARGSUSED */
static int DCN_SABME_in(lp, type)
register llc2cn_t *lp;
{
    DLP(("DCN_SABME_in\n"));
    send_Ursp(lp, DM);
    upbycause(lp, LS_CONFLICT, LS_CONFLICT);

    /* Stop timers, clear variables and disconnect */
    disconnect(lp, OFF);
    return FRAMEOK;
}

/*  *************************************************** DCN_UDM_in  */
/* ARGSUSED */
static int DCN_UDM_in(lp, type)
register llc2cn_t *lp;
{   /* Deals with UA and DM */
    upbycause(lp, LS_DISCONNECT, LS_DISCONNECT);

    /* Stop timers, clear variables and disconnect */
    disconnect(lp, OFF);
    return FRAMEOK;
}

/*  *************************************************** DCN_DISC_in  */
/* ARGSUSED */
static int DCN_DISC_in(lp, type)
register llc2cn_t *lp;
{
    send_Ursp(lp, UA);
    return FRAMEOK;
}

/* All I- and S-frames ignored */

/*  *************************************************** DCN_ACK_exp  */
static DCN_ACK_exp(lp)
register llc2cn_t *lp;
{   /* ACK timer has expired */
    if (--lp->retry_count)
    {
	send_Ucmd(lp, DISC);
	set_timer(&lp->ack_timer, (lp->dnp->dn_tunetab.T1 << 1));
	return 0;
    }
    upbycause(lp, LS_FAILED, LS_FAILED);

    /* Stop timers, clear variables and disconnect */
    disconnect(lp, STOP);
    return 0;
}

/***************************  E R R  ****************************/
/* LLC2 state within ERR state is always ERROR			*/
/* In this implementation, pflag is always zero in ERR state	*/
/***************************  E R R  ****************************/

/*  *************************************************** ERR_SABME_in  */
/* ARGSUSED */
static int ERR_SABME_in(lp, type)
register llc2cn_t *lp;
{
    if (lp->upp->up_interface == LLC_IF_DLPI)
    {
	llc2_tonet(lp, LC_RESET, LS_SUCCESS);
	reset_vars(lp, RSTCK);
	return FRAMEOK;
    }
    send_Ursp(lp, UA);
    llc2_tonet(lp, LC_RESET, LS_SUCCESS);
    reset_vars(lp, RUN);
    goto_normal(lp);
    return FRAMEOK;
}

/*  *************************************************** ERR_DDM_in  */
/* ARGSUSED */
static int ERR_DDM_in(lp, type)
register llc2cn_t *lp;
{   /* Deals with DISC and DM */
    if (type < RSP_F0) /* Command => DISC */
	send_Ursp(lp, UA);

    llc2_tonet(lp, LC_DISC, LS_DISCONNECT);

    /* Stop timers, clear variables and disconnect */
    disconnect(lp, OFF);
    return FRAMEOK;
}

/*  *************************************************** ERR_FRMR_in  */
/* ARGSUSED */
static int ERR_FRMR_in(lp, type)
register llc2cn_t *lp;
{
    strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
          "FRMR  In  : '%x' Id %03x ",lp->dnp->dn_snid, lp->connID);

    start_reset(lp);
    /* IND_toNS indtype=LC_REPORT  status=FRMR_RECEIVED is not implemented */

    return FRAMEOK;
}

/*  *************************************************** ERR_I_in  */
/* ARGSUSED */
static int ERR_I_in(lp, ns, nr, type)
register llc2cn_t *lp;
{
    if (type < RSP_F0)
	send_Ursp(lp, FRMR);
    return FRAMEOK;
}

/*  *************************************************** ERR_S_in  */
/* ARGSUSED */
static int ERR_S_in(lp, nr, type)
register llc2cn_t *lp;
{
    if (type < RSP_F0)
	send_Ursp(lp, FRMR);
    return FRAMEOK;
}


/*  *************************************************** ERR_ACK_exp  */
static ERR_ACK_exp(lp)
register llc2cn_t *lp;
{   /* ACK timer has expired */
    if (--lp->retry_count)
    {
	send_Ursp(lp, FRMR);
	set_timer(&lp->ack_timer, (lp->dnp->dn_tunetab.T1 << 1));
	return FRAMEOK;
    }
    llc2_tonet(lp, LC_REPORT, LS_RESETTING);
    start_reset(lp);
    return 0;
}


/***************************  R U N  ****************************/
/* RUN state covers all "running" states, i.e. LLC2 states	*/
/* NORMAL, BUSY, REJECT, AWAIT, AWAIT_BUSY and AWAIT_REJECT	*/
/*								*/
/* LLC2 states are encoded within RUN state as follows:		*/
/*	(loc_busy 0, awaitflag 0):				*/
/*		(dataflag NORMAL)   => NORMAL			*/
/*		(dataflag REJECT)   => REJECT			*/
/*	(loc_busy 0, awaitflag 1):				*/
/*		(dataflag NORMAL)   => AWAIT			*/
/*		(dataflag REJECT)   => AWAIT_REJECT		*/
/*	(loc_busy 1, awaitflag 0):				*/
/*		(dataflag NORMAL)   => BUSY, dataflag=0		*/
/*		(dataflag DATALOST) => BUSY, dataflag=1		*/
/*		(dataflag REJECT)   => BUSY, dataflag=2		*/
/*	(loc_busy 1, awaitflag 1):				*/
/*		(dataflag NORMAL)   => AWAIT_BUSY, dataflag=0	*/
/*		(dataflag DATALOST) => AWAIT_BUSY, dataflag=1   */
/*		(dataflag REJECT)   => AWAIT_BUSY, dataflag=2	*/
/***************************  R U N  ****************************/


/*  *************************************************** RUN_RESET_req  */
static RUN_RESET_req(lp)
register llc2cn_t *lp;
{
    strlog(LLC_STID,lp->dnp->dn_snid,2,SL_TRACE,
          "LINK  Rst : '%x' Id %03x Local",lp->dnp->dn_snid,lp->connID);
    DLP(("LINK  Rst : '%x' Id %03x Local",lp->dnp->dn_snid,lp->connID));

    start_reset(lp);
    lp->causeflag = 1;
    return FRAMEOK;
}

/*  *************************************************** RUN_SABME_in  */
/* ARGSUSED */
static int RUN_SABME_in(lp, type)
register llc2cn_t *lp;
{
    DLP(("RUN_SABME_in(%x)\n",lp->dnp->dn_snid));
    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
	"RUN_SABME_in(%x)",lp->dnp->dn_snid);
/*
    if (lp->upp->up_interface == LLC_IF_DLPI)
    {
	llc2_tonet(lp, LC_RESET, LS_SUCCESS);
	reset_vars(lp, RSTCK);
	return FRAMEOK;
    }
 */
    send_Ursp(lp, UA);
    llc2_tonet(lp, LC_RESET, LS_SUCCESS);
    reset_vars(lp, RUN);
    goto_normal(lp);
    return FRAMEOK;
}

/*  *************************************************** RUN_DDM_in  */
/* ARGSUSED */
static int RUN_DDM_in(lp, type)
register llc2cn_t *lp;
{   /* Deals with DISC and DM */
    if (type < RSP_F0)    /* => DISC command */
	send_Ursp(lp, UA);

    llc2_tonet(lp, LC_DISC, LS_DISCONNECT);

    /* Stop timers, clear variables and disconnect */
    disconnect(lp, OFF);
    return FRAMEOK;
}

/*  *************************************************** RUN_UA_in   */
/* ARGSUSED */
static int RUN_UA_in(lp, type)
register llc2cn_t *lp;
{
    DLP(("RUN_UA_in: return FRMR_WBIT\n"));
    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
	"RUN_UA_in: return FRMR_WBIT");
    return FRMR_WBIT;
}

/*  *************************************************** RUN_FRMR_in  */
/* ARGSUSED */
static int RUN_FRMR_in(lp, type)
register llc2cn_t *lp;
{
    strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
          "FRMR  In  : '%x' Id %03x ",lp->dnp->dn_snid, lp->connID);
	  
#ifdef SGI
    llc2_tonet(lp, LC_REPORT, LS_FMR_RECEIVED);
#else
    llc2_tonet(lp, LC_REPORT, LS_RESETTING);
#endif
    start_reset(lp);

    return FRAMEOK;
}

/*  *************************************************** RUN_RR_in  */
/* ARGSUSED */
static int RUN_RR_in(lp, nr, type)
register llc2cn_t *lp;
{
    int reason;

    if (reason = update_nr(lp, nr))
	return reason;

    clr_rem_busy(lp);

    if (type == RSP_F1)
    {   /* RSP(F=1): end of PF cycle */
	pfend(lp);
	return FRAMEOK;
    }

    if (lp->need_fbit)
	send_ack_rsp(lp);

    return FRAMEOK;
}


/*  *************************************************** RUN_RNR_in  */
/* ARGSUSED */
static int RUN_RNR_in(lp, nr, type)
register llc2cn_t *lp;
{
    int reason;

    if (reason = update_nr(lp, nr))
	return reason;

    set_rem_busy(lp);

    if (type == RSP_F1)
    {   /* RSP(F=1): end of PF cycle */
	pfend(lp);
    }

    if (lp->need_fbit)
	send_ack_rsp(lp);

    return FRAMEOK;
}

/*  *************************************************** RUN_REJ_in  */
static int RUN_REJ_in(lp, nr, type)
register llc2cn_t *lp;
{
    int reason;

    DLP(("REJ In: '%x' Id %03x N(%03d)",lp->dnp->dn_snid,lp->connID,nr));
    strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
          "REJ   In  : '%x' Id %03x N(%03d)",lp->dnp->dn_snid,lp->connID,nr);

    if (reason = update_nr(lp, nr))
	return reason;

    clr_rem_busy(lp);

    if (type == RSP_F1)
    {   /* RSP(F=1): end of PF cycle */
	pfend(lp);
    }

    resend_i_rsp(lp);

    if (lp->need_fbit)
	send_ack_rsp(lp);

    return FRAMEOK;
}

/*  *************************************************** RUN_ACK_exp  */
static RUN_ACK_exp(lp)
register llc2cn_t *lp;
{   /* ACK[P/busy] timer has expired */
    if (--lp->retry_count)
    {
	send_SPcmd(lp, lp->loc_busy ? RNR : RR);

	if (!lp->awaitflag)
	{
	    lp->awaitflag = 1;  /* LLC2 state -> AWAIT[_XXX] */
	    lp->txstopped++;
	}
    }
    else
	start_reset(lp);
    return 0;
}

/*  *************************************************** RUN_REJ_exp  */
static RUN_REJ_exp(lp)
register llc2cn_t *lp;
{   /* Rej/idle timer has expired */
    if (!lp->pflag)
    {
	if (lp->dataflag == NORMAL || --lp->retry_count)
	    send_SPcmd(lp, lp->loc_busy ? RNR : RR);
	else
	    start_reset(lp);
    }
    return 0;
}

/*  *************************************************** RUN_I_in  */
static int RUN_I_in(lp, ns, nr, type)
register llc2cn_t *lp;
{
    int reason;

    if (reason = update_nr(lp, nr))
	return reason;

    if ((uint16)(ns - lp->vr + lp->ur & 127) > lp->dnp->dn_tunetab.xid_window)
    {
	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
	    "RUN_I_in: FRMR_VBIT, ns=%d, nr=%d, vr=%d, ur=%d",
	    ns,nr,lp->vr,lp->ur);
	return(FRMR_VBIT);
    }

    if (type == RSP_F1)
    {   /* End of PF cycle */
	clr_rem_busy(lp);
	pfend(lp);
    }

    if (lp->loc_busy)
    {   /* Local busy */
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
	    "RUN_I_in: send RNR - local busy");
	send_Srsp(lp, RNR);

	if (ns - lp->vr & 127)
	{
	    if (lp->dataflag == NORMAL)
		lp->dataflag = DATALOST;
	}
	else
	{
	    if (lp->dataflag == REJECT)
		set_timer(&lp->rej_timer, 0);
	    lp->dataflag = DATALOST;
	}

	return FRAMEOK;
    }

    if (ns - lp->vr & 127)
    {   /* Unexpected N(S) */
	if (lp->dataflag == NORMAL)
	{
	    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
		"RUN_I_in: Send REJ - unexpected N(S), ns=%d, vr=%d",
		ns, lp->vr);
	    send_Srsp(lp, REJ);
	    lp->dataflag = REJECT;
	    set_timer(&lp->rej_timer, (lp->dnp->dn_tunetab.Trej << 1));
	}

	if (lp->need_fbit)
	    send_ack_rsp(lp);

	return FRAMEOK;
    }

    /* Expected N(S) */
    if (lp->dataflag == REJECT)
    {   /* LLC2 REJECT */
	send_Srsp(lp, RR);
	lp->dataflag = NORMAL;
	set_timer(&lp->rej_timer, (lp->dnp->dn_tunetab.Tidle << 1));
    }

    indinfo(lp);

    return FRAMEOK;
}

#ifdef LOCAL_BUSY /* { LOCAL_BUSY */
/*********************  L O C A L   B U S Y  *********************/

/*  *************************************************** set_loc_busy  */
static set_loc_busy(lp)
register llc2cn_t *lp;
{
    if (lp->loc_busy++ == 0)
	send_Srsp(lp, RNR);

    strlog(LLC_STID,lp->dnp->dn_snid,4,SL_TRACE,
          "LINK  Bsy : '%x' Id %03x Local",lp->dnp->dn_snid, lp->connID);
    return 0;
}

/*  *************************************************** clr_loc_busy  */
static clr_loc_busy(lp)
register llc2cn_t *lp;
{
    if (--lp->loc_busy == 0)
    {
	if (lp->dataflag == DATALOST)
	{
	    send_Srsp(lp, REJ);
	    lp->dataflag = REJECT;
	    set_timer(&lp->rej_timer, (lp->dnp->dn_tunetab.Trej << 1));
	}
	else
	{   /* dataflag=NORMAL or dataflag=REJECT */
	    send_Srsp(lp, RR);
	    if (lp->dataflag == REJECT)
	    {
		lp->dataflag = NORMAL;
		set_timer(&lp->rej_timer, (lp->dnp->dn_tunetab.Tidle << 1));
	    }
	}
    }
    return 0;
}
#endif /* } LOCAL_BUSY */

/*********************  R E M O T E   B U S Y  *********************/

/*  *************************************************** set_rem_busy  */
static set_rem_busy(lp)
register llc2cn_t *lp;
{
    if (!lp->rem_busy)
    {
	lp->rem_busy = 1;
	lp->txstopped++;

	if (lp->tack == 0)
	{
	    lp->tack = lp->dnp->dn_tunetab.Tbusy;
	    if (!lp->pflag)
		set_timer(&lp->ack_timer, (lp->tack << 1));
	}
	strlog(LLC_STID,lp->dnp->dn_snid,4,SL_TRACE,
	      "LINK  Bsy : '%x' Id %03x Remote",lp->dnp->dn_snid, lp->connID);
    }
    return 0;
}

/*  *************************************************** clr_rem_busy  */
static clr_rem_busy(lp)
register llc2cn_t *lp;
{
    if (lp->rem_busy)
    {
	lp->retry_count = lp->dnp->dn_tunetab.N2;
	/* llc2_tonet(LC_REPORT, REMOTE_NOT_BUSY) not implemented */

	lp->rem_busy = 0;

	if (lp->us == 0)
	{
	    lp->tack = 0;

	    if (!lp->pflag)
		set_timer(&lp->ack_timer, 0);
	}

	if (--lp->txstopped == 0)
	    llc2_schedule(lp);
    }
    return 0;
}


/***************  U N N U M B E R E D   F R A M E S  *************/

/*  *************************************************** send_Ucmd  */
send_Ucmd(lp, pdu)
register llc2cn_t *lp;
{   /* Send SABME, DISC, XID, UI or TEST command */

    register bufp_t fp = llc2_dnmp->b_cont->b_wptr;
    int n = 3;

    fp[0] = lp->dte[1];
    fp[1] = lp->dte[0];
    fp[2] = pdu;

    DLP(("send_Ucmd: %x\n",pdu));

    switch (pdu)
    {
    case XID:
	fp[2] |= (lp->need_fbit << 4);
	lp->need_fbit = 0;
 	if (llc2_dnmp->b_cont->b_cont == NULL)
	{
	    /* This command was not generated by a DL_XID_REQ primitive, so
	     * fill in the XID data. */
	    fp[3] = XID_NORMAL;
	    fp[4] = XID_BOTH;
	    fp[5] = lp->dnp->dn_tunetab.xid_window<<1;
	    n = 6;
	}
        LLC2MONITOR(XID_tx_cmd)
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
              "XID   Out : '%x' Id %03x",lp->dnp->dn_snid, lp->connID);
    	break;

    case DISC:
	LLC2MONITOR(DISC_tx_cmd)
	strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
	    "DISC  Out: '%x' Id %03x",lp->dnp->dn_snid, lp->connID);
	break;

    case SABME:
	LLC2MONITOR(SABME_tx_cmd)
	strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
	    "SABME  Out: '%x' Id %03x",lp->dnp->dn_snid, lp->connID);
	break;

    case UI:
	LLC2MONITOR(UI_tx_cmd)
	strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
	    "UI  Out: '%x' Id %03x",lp->dnp->dn_snid, lp->connID);
	break;

    case TEST:
	fp[2] |= (lp->need_fbit << 4);
	lp->need_fbit = 0;
	LLC2MONITOR(TEST_tx_cmd)
	strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
	    "TEST  Out: '%x' Id %03x",lp->dnp->dn_snid, lp->connID);
	break;

    default:
	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
	    "LLC: try to send unknown U cmd %x, '%x' Id %03x",
		pdu, lp->dnp->dn_snid, lp->connID);
	break;
    }

    /* Transmit frame */
    llc2_tomac(lp, n);
    return 0;
}


/*  *************************************************** send_Ursp  */
send_Ursp(lp, pdu)
register llc2cn_t *lp;
{   /* Send UA, DM, FRMR, XID or TEST response */

    register bufp_t fp = llc2_dnmp->b_cont->b_wptr;
    int n = 3;  /* Normal length */

    fp[0] = lp->dte[1];
    fp[1] = lp->dte[0] | 1;  /* Response */
    fp[2] = pdu | lp->need_fbit<<4;
    lp->need_fbit = 0;

    switch (pdu)
    {
    case FRMR:
	{
	    int i;

	    for (i = 0; i<5; i++)
	    {   /* Add information part */
		fp[n++] = lp->frmr[i];
	    }
	}

	/* Statistics */
        LLC2MONITOR(FRMR_tx_rsp)

	strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
	      "FRMR  Out : '%x' Id %03x Rsn %02d",lp->dnp->dn_snid,lp->connID,lp->frmr[4]);
	break;

    case XID:
	if (llc2_dnmp->b_cont->b_cont == NULL)
	{
	    /* This command was not generated by a DL_XID_RES primitive, so
	     * fill in the XID data. */
	    fp[3] = XID_NORMAL;
	    fp[4] = XID_BOTH;
	    fp[5] = lp->dnp->dn_tunetab.xid_window<<1;
	    n = 6;
	}
        LLC2MONITOR(XID_tx_rsp)
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
              "XID   Out : '%x' Id %03x",lp->dnp->dn_snid, lp->connID);
	break;

    case TEST:
	if (llc2_dnmp->b_cont->b_cont == NULL && llc2_rxmp)
	{
	    /* If 'llc2_rxmp' is non-NULL, then this is an automatic response
	     * to a TEST command.  Move the data that was received in the
	     * TEST command over to the down message and strip off the
	     * LLC2 header. */
	    adjmsg(llc2_dnmp->b_cont->b_cont = llc2_rxmp, 3);
	    llc2_rxmp = (mblk_t *)0;
	}
        LLC2MONITOR(TEST_tx_rsp)
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
              "TEST  Out : '%x' Id %03x",lp->dnp->dn_snid, lp->connID);
	break;

    case UA:
        LLC2MONITOR(UA_tx_rsp)
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
	    "UA  Out: '%x' ID %03x",lp->dnp->dn_snid, lp->connID);
        break;

    case DM:
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
	    "DM  Out: '%x' ID %03x",lp->dnp->dn_snid, lp->connID);
        LLC2MONITOR(DM_tx_rsp)
        break;

    default:
	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_ERROR,
	    "Unknown Ursp %x  Out: '%x' ID %03x",
	    pdu, lp->dnp->dn_snid, lp->connID);
	break;
    }

    /* Transmit frame */
    llc2_tomac(lp, n);
    return 0;
}

/***************  S U P E R V I S O R Y   F R A M E S  *************/

/*  *************************************************** send_SPcmd  */
static send_SPcmd(lp, pdu)
register llc2cn_t *lp;
{   /* Send RR, RNR or REJ command with P bit set */

    register bufp_t fp = llc2_dnmp->b_cont->b_wptr;

    fp[0] = lp->dte[1];
    fp[1] = lp->dte[0];
    fp[2] = pdu;
    fp[3] = lp->vr<<1 | 1;  /* P-bit */

    /* Set pflag and start P timer */
    lp->pflag = 1;
    set_timer(&lp->ack_timer, (lp->dnp->dn_tunetab.Tpf << 1));

    /* RR, REJ or RNR all acknowledge current V(R) */
    lp->ur = 0;
    lp->need_ack = 0;
    set_timer(&lp->dlx_timer, 0);

    switch (pdu)
    {
    case RR:
	LLC2MONITOR(RR_tx_cmd)
	break;

    case RNR:
	LLC2MONITOR(RNR_tx_cmd)
	break;

    case REJ:
	LLC2MONITOR(REJ_tx_cmd)
	break;

    default:
	break;
    }

    /* Transmit frame */
    llc2_tomac(lp, 4);
    return 0;
}


/*  *************************************************** send_Srsp  */
static send_Srsp(lp, pdu)
register llc2cn_t *lp;
{   /* Send RR, RNR or REJ response */

    register bufp_t fp = llc2_dnmp->b_cont->b_wptr;

    fp[0] = lp->dte[1];
    fp[1] = lp->dte[0] | 1;  /* Response */
    fp[2] = pdu;
    fp[3] = lp->vr<<1 | lp->need_fbit;
    lp->need_fbit = 0;

    /* RR, REJ or RNR all acknowledge current V(R) */
    lp->ur = 0;
    lp->need_ack = 0;
    set_timer(&lp->dlx_timer, 0);

    switch (pdu)
    {
    case RR:
	LLC2MONITOR(RR_tx_rsp)
	break;

    case RNR:
	LLC2MONITOR(RNR_tx_rsp)
	break;

    case REJ:
	LLC2MONITOR(REJ_tx_rsp)
	strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
		"REJ    Out : '%x' Id %03x N(%03d)",
		lp->dnp->dn_snid, lp->connID,lp->vr);
	break;

    default:
	break;
    }

    /* Transmit frame */
    llc2_tomac(lp, 4);
    return 0;
}




/***************  I N F O R M A T I O N   F R A M E S  *************/

/*  *************************************************** indinfo  */
static indinfo(lp)
register llc2cn_t *lp;
{
    /* increment V(R) */
    lp->vr++;

    /* Move data over to upgoing message and strip off 4-byte LLC2 header */
    adjmsg(llc2_upmp->b_cont = llc2_rxmp, 4);
    llc2_rxmp = (mblk_t *)0;

    /* L_DATA.indication */
    llc2_tonet(lp, LC_DATA, LS_SUCCESS);

    if (++lp->ur >= lp->dnp->dn_tunetab.notack_max || lp->need_fbit)
	send_ack_rsp(lp);
    else
	set_timer(&lp->dlx_timer, (lp->dnp->dn_tunetab.ack_delay << 1));
    return 0;
}


/*  *************************************************** llc2_sendinfo  */
llc2_sendudata(lp)
llc2cn_t *lp;
{   /* Send UI frame: the data is already attached to llc2_dnmp */
    send_Ucmd(lp, UI);
    return 0;
}


/*  *************************************************** llc2_sendinfo  */
llc2_sendinfo(lp)
register llc2cn_t *lp;
{   /* Send info frame: the data is already attached to llc2_dnmp */
    register bufp_t fp = llc2_dnmp->b_cont->b_wptr;

    fp[0] = lp->dte[1];
    fp[1] = lp->dte[0];
    fp[2] = lp->vs<<1;
    fp[3] = lp->vr<<1;

    if (lp->need_fbit)
    {   /* Send response with F-bit set */
	fp[1] |= 1;
	fp[3] |= 1;
	lp->need_fbit = 0;
        LLC2MONITOR(I_tx_rsp)
    }
    else
    {   /* Send command (with P-bit if probe point reached) */
	if (!lp->pflag && lp->us == lp->txprbpt)
	{   /* Add P-bit and start P timer */
	    fp[3] |= 1;
	    lp->pflag = 1;
	    set_timer(&lp->ack_timer, (lp->dnp->dn_tunetab.Tpf << 1));
	}
        LLC2MONITOR(I_tx_cmd)
    }

    /* I-frame acknowledges data */
    lp->ur = 0;
    lp->need_ack = 0;
    set_timer(&lp->dlx_timer, 0);

    /* Transmit frame */
#ifdef DEBUG
    if (msgdsize(llc2_dnmp->b_cont)==0) {
	strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,
	    "Send I-Frame with 0 length");
    }
#endif
    llc2_tomac(lp, 4);

    /* Update sequence number */
    lp->vs++;

    /* Start ACK timer if necessary */
    if (lp->us == 0)
    {   /* First of new batch of frames: start ACK timer */
	lp->tack = lp->dnp->dn_tunetab.T1;
	if (!lp->pflag)
	    set_timer(&lp->ack_timer, (lp->tack << 1));
    }

    /* Don't send more data if end of window */
    if (++lp->us >= lp->txwindow)
    {   /* Next data would be outside window */
	lp->wclosed = 1;
	lp->txstopped++;
    }
    return 0;
}

/*  *************************************************** pfend(lp)  */
static pfend(lp)
llc2cn_t *lp;
{   /* Deal with end of P/F cycle */

    set_timer(&lp->ack_timer, (lp->tack << 1));

    if (lp->awaitflag)
    {
	resend_i_rsp(lp);
	lp->awaitflag = 0;
	if (--lp->txstopped == 0)
	    llc2_schedule(lp);
    }
    return 0;
}


/*  *************************************************** resend_i_rsp  */
static resend_i_rsp(lp)
register llc2cn_t *lp;
{
    if (lp->us)
    {   /* There are unacknowledged I-frames to retransmit */
	lp->datq = lp->ackq;
	lp->vs = lp->vs - lp->us & 127;
	lp->us = 0;
	if (lp->wclosed)
	{
	    lp->wclosed = 0;
	    lp->txstopped--;
	}

	/* Restart transmission unless stopped */
	if (!lp->txstopped)
	    llc2_schedule(lp);
    }
    return 0;
}


/*  *************************************************** update_nr  */
static uint8 update_nr(lp, nr)
register llc2cn_t *lp;
{
    int num_acked;

    /* Check for out-of-range number */
    if ((int)(num_acked = nr - lp->vs + lp->us & 127) > (int)lp->us)
    {
	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
	    "LLC update_nr: send FRMR_ZBIT - nr=%d, vs=%d, us=%d, acked=%d",
	    nr, lp->vs, lp->us, num_acked);
	return FRMR_ZBIT;
    }

    if (num_acked > 0)
    {   /* There are waiting frames acknowledged by this incoming nr */

	/* Release the newly-acknowleged frames from ack queue */
	do
	{   /* Release one message from ack queue on each cycle */
	    register mblk_t *bp = lp->ackq;
	    lp->ackq = bp->b_next;
	    bp->b_next = (mblk_t *)0;
	    freemsg(bp);

	    /* Decrement unacknowledged count */
	    lp->us--;
	}
	while (--num_acked);

	/* Choose new value for 'lp->tack' and start/stop ack/busy timer */
	if (lp->us == 0)
	    lp->tack = lp->rem_busy ? lp->dnp->dn_tunetab.Tbusy : 0;

	if (!lp->pflag)
	    set_timer(&lp->ack_timer, (lp->tack << 1));

	/* Start acknowledgement retries again */
	lp->retry_count = lp->dnp->dn_tunetab.N2;

	/* Restart Tx if window was closed, since it is now open again */
	if (lp->wclosed)
	{
	    lp->wclosed = 0;
	    if (--lp->txstopped == 0)
		llc2_schedule(lp);
	}
    }

    /* Valid sequence number */
    return FRAMEOK;
}



/*    *************************************************** upbycause  */
static upbycause(lp, report, status)
register llc2cn_t *lp;
{
    static indications[] = {LC_REPORT, LC_RSTCNF, LC_CONCNF, LC_DISCNF};

    llc2_tonet(lp, indications[lp->causeflag],
			    lp->causeflag ? status : report);
    return 0;
}



/*    *************************************************** stop_all_timers  */
static stop_all_timers(lp)
register llc2cn_t *lp;
{
    register i;

    for (i = 0; i<NTIMERS; i++)
	set_timer(&lp->tmtab[i], 0);
    lp->pflag = 0;
    return 0;
}


/*    *************************************************** send_ack_rsp  */
static send_ack_rsp(lp)
register llc2cn_t *lp;
{
    lp->need_ack = 1;
    llc2_schedule(lp);
    return 0;
}


/*    *************************************************** goto_normal  */
static goto_normal(lp)
register llc2cn_t *lp;
{
    llc2tune_t *tunep = &lp->dnp->dn_tunetab;

    DLP(("Entering goto_normal, lp=%p\n",lp));
    lp->state = RUN;
    lp->sflag = 0;
    lp->causeflag = 0;
    stop_all_timers(lp);

    /* Set NORMAL and start idle timer */
    lp->dataflag = NORMAL;
    set_timer(&lp->rej_timer, (lp->dnp->dn_tunetab.Tidle << 1));

    if (tunep->xid_Ndup && tunep->xid_Tdup)
    {   /* Send off XID command */
	DLP(("goto_normal: Request LLC XID\n"));
	lp->txstopped++;
	lp->retry_count = tunep->xid_Ndup;

	/* Request XID command */
	lp->xidflag = 2;
	llc2_schedule(lp);
    }
    return 0;
}


/*    *************************************************** disconnect  */
static disconnect(register llc2cn_t *lp, uint8 state)
{
    reset_vars(lp, state);
    llc2_schedule(lp);
    return 0;
}


/*    *************************************************** reset_vars  */
static reset_vars(register llc2cn_t *lp, uint8 state)
{
    register mblk_t *bp;

    stop_all_timers(lp);

    /* Discard ACK/DAT queue */
    while (bp = lp->ackq)
    {
	lp->ackq = bp->b_next;
	bp->b_next = (mblk_t *)0;
	freemsg(bp);
    }
    lp->datq = (mblk_t *)0;

    lp->vs = lp->vr = lp->us = lp->ur = 0;
    lp->pflag = lp->tack = 0;
    lp->retry_count = lp->dnp->dn_tunetab.N2;
    lp->rem_busy = 0;
    lp->sflag = lp->awaitflag = lp->dataflag = lp->wclosed = 0;
    lp->causeflag = 0;

    lp->state = state;
    return 0;
}


/*  *************************************************** start_reset  */
static start_reset(lp)
register llc2cn_t *lp;
{
    reset_vars(lp, RST);
    DLP(("start_reset: send SABME\n"));
    strlog(LLC_STID,lp->dnp->dn_snid,3,SL_TRACE,"start_reset: send SABME");
    send_Ucmd(lp, SABME);
    set_timer(&lp->ack_timer, (lp->dnp->dn_tunetab.T1 << 1));
    return 0;
}

/*  *************************************************** llc2_cninit  */
llc2_cninit(lp)
llc2cn_t *lp;
{   /* Initialises blank connection structure */

    /* Start timer */
    init_timers(&lp->thead, lp->tmtab, NTIMERS, llc2_schedule, (caddr_t)lp);

    lp->state = ADM;
    return 0;
}

/*  *************************************************** llc2_stop  */
llc2_stop(lp)
llc2cn_t *lp;
{   /* Closes connection down */
    register mblk_t *mp;

    reset_vars(lp, OFF);

    /* Throw away control queue */
    while (mp = lp->ctlq)
    {
	lp->ctlq = mp->b_next;
	mp->b_next = (mblk_t *)0;
	freemsg(mp);
    }
    return 0;
}

/*  ******************************************** llc2_control_req  */
llc2_control_req(lp, command)
register llc2cn_t *lp;
uint8 command;
{   /* Control request from network */
    switch (command)
    {
    case LC_CONNECT:
	if (lp->state == ADM)
	    ADM_CONNECT_req(lp);
	break;

    case LC_CONOK:
	if (lp->state == ADM)
	    ADM_CONOK_req(lp);
	break;

    case LC_CONNAK:
	if (lp->state == ADM)
	    ADM_CONNAK_req(lp);
	break;

    case LC_RESET:
	 (*RESET_req_fns[lp->state])(lp);
	 break;

    case LC_RSTCNF:
	/* Will only be received if upper interface is DLPI. */
	if (lp->state == RSTCK)
	    RSTCK_RSTCONF_req(lp);
	break;

    case LC_DISC:
	(*DISC_req_fns[lp->state])(lp);
	break;

    default:
	break;
    }
    return 0;
}

/*  ******************************************** llc2_txack  */
llc2_txack(lp)
register llc2cn_t *lp;
{
    send_Srsp(lp, lp->loc_busy ? RNR : RR);
    return 0;
}


/*  ******************************************** llc2_expired  */
llc2_expired(lp, id)
register llc2cn_t *lp;
unchar id;
{   /* Timer 'id' expired */
    (*exp_fns[id][lp->state])(lp);
    return 0;
}


/*  *************************************************** llc2_rxframe  */
llc2_rxframe(lp)
register llc2cn_t *lp;
{   /* Process message from MAC (held in llc2_rxmp) */

    register bufp_t fp = llc2_rxmp->b_rptr;
    unsigned clen = (unsigned)(llc2_rxmp->b_wptr - fp);
    unsigned int tlen = llc2_rxmp->b_cont ? msgdsize(llc2_rxmp) : clen;
    unsigned int pf_bit;	/* Value of P/F bit */
    unsigned int type;		/* =CMD_P0 => Command; =RSP_F0 => Response */
    unsigned reason = FRAMEOK;  /* =0 => No error; >0 => FRMR error bits */
    register unsigned char c;   /* First control octet */
    register unsigned char ftype;

    STRLOG((LLC_STID,lp->dnp->dn_snid,9,SL_TRACE,
	"LLC - llc2_rxframe called"));
    DLP(("Entering llc2_rxframe ..., lp->state=%d\n",lp->state));
    if (clen < 3)
    {   /* Incomplete */
	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
	    "LLC - rxframe Incomplate");
	DLP(("\tLLC - rxframe Incomplate\n"));
	return 0 ;
    }

    /*  Ignore frames if received while not in a valid state  */
    if (lp->state < ADM)
    {
	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
			"rxframe BADSTATE (%d) : '%x' Id %03x",
			lp->state,lp->dnp->dn_snid, lp->connID);
	DLP(("\tLLC - rxframe BADSTATE (%d) : '%x' Id %03x",lp->state,
		lp->dnp->dn_snid, lp->connID));
	return 0 ;
    }

    /* Reset idle timer */
    if (lp->state == RUN && lp->dataflag == NORMAL)
	set_timer(&lp->rej_timer, (lp->dnp->dn_tunetab.Tidle << 1));

    type = (fp[1] & 1)<<1;  /* CMD_P0 == 0, RSP_F0 == 2 */

    strlog(LLC_STID,lp->dnp->dn_snid,9,SL_TRACE,
	"LLC - header %x %x %x",fp[0], fp[1], fp[2]);
/*  DLP(("\tLLC - header %x %x %x\n",fp[0], fp[1], fp[2])); */
    if (((c = fp[2]) & 3) == 3)
    {   /* U format */
	int u = uswitch[c>>2];

	DLP(("llc2_rxframe: received U frame %x\n",c));
        strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
	    "LLC2 - received U frames %x",c);
	pf_bit = c>>4 & 1;

	if ((c & 0xEF) == XID && clen >= 6 && fp[3] == XID_NORMAL)
	    llc2_settxwind(lp, fp[5]>>1);

	if (type == CMD_P0)
	{   /* U format command */

	    /* Unsigned "u > U_CMD" catches response-only or dud */
	    if ((unsigned)u > U_CMD)
	    {
		strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
		    "llc2_rxframe: send FRMR_WBIT - bad U frame %x", u);
		reason = FRMR_WBIT;
	    }
	    else
		lp->need_fbit = pf_bit;
	}
	else
	{   /* U format response */

	    /* Signed "u < U_RSP" catches command-only or dud */
	    if ((int)u < U_RSP)
	    {
		strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
		    "llc2_rxframe: send FRMR_WBIT - bad U frame %x", u);
		reason = FRMR_WBIT;
	    }
	    else
	    {
		if (pf_bit)
		{
		    if (!lp->pflag && (c & 0xEF) != XID && (c & 0xEF) != TEST)
		    {
			strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
			    "llc2_rxframe: send FRMR_WBIT - #3, c=%x",c);
			reason = FRMR_WBIT;
		    }
		    lp->pflag = 0;
		}
	    }
	}

	if (reason == FRAMEOK)
	{
	    if ((reason = (*U_in_fns[u][lp->state])
					(lp, type | pf_bit)) == FRAMEOK)
	    {
		ftype = c & 0xEF;
		switch (ftype)
		{
		    case SABME:
			LLC2MONITOR(SABME_rx_cmd)
			break;

		    case DISC:
			LLC2MONITOR(DISC_rx_cmd)
			break;

		    case UI:
			LLC2MONITOR(UI_rx_cmd)
			break;

		    case UA:
			LLC2MONITOR(UA_rx_rsp)
			break;

		    case DM:
			LLC2MONITOR(DM_rx_rsp)
			break;

		    case FRMR:
			LLC2MONITOR(FRMR_rx_rsp)
			break;

		    case XID:
			if (type < RSP_F0)
			    LLC2MONITOR(XID_rx_cmd)
			else
			    LLC2MONITOR(XID_rx_rsp)
			break;

		    case TEST:
			if (type < RSP_F0)
			    LLC2MONITOR(TEST_rx_cmd)
			else
			    LLC2MONITOR(TEST_rx_rsp)
			break;

		    default:
			DLP(("\tLLC - Unknown U-frames type 0x%x\n",ftype));
			strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
			    "LLC - Unknown U-frames type 0x%x",ftype);
			break;
		}
	    } else {
        	strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
		    "LLC - failed in state machine, reason=%d", reason);
		DLP(("\tLLC - failed in state machine, reason=%d\n",reason));
	    }
	} else {
	    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
		"LLC - bad LLC header, reason=%d\n",reason);
	    DLP(("\tLLC - bad LLC header, reason=%d\n",reason));
	}
    }
    else
    {   /* I or S format */
	if (clen < 4)
	{   /* Incomplete */
	    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
		"LLC - Incomplete S or I frame");
	    DLP(("\tIncomplate S or I frame\n"));
	    return 0 ;
	}

	pf_bit = fp[3] & 1;

	if (type == CMD_P0)
	{   /* I or S command */
	    lp->need_fbit = pf_bit;
	}
	else
	{   /* I or S response */
	    if (pf_bit)
	    {
		if (!lp->pflag)
		{
	    	    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
			"LLC: send FRMR_WBIT, pf_bit on but not pflag");
		    reason = FRMR_WBIT;
		}
		lp->pflag = 0;
	    }
	}

	if (reason == FRAMEOK)
	{   /* OK so far */
	    if (c & 1)
	    {   /* S format: check PDU and length; if OK call state M/C */
		if (c > 0x09)
		{
	    	    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
			"LLC: send FRMR_WBIT, c 0x%x is greater than 0x09",c);
		    reason = FRMR_WBIT;
		}
		else
		{
		    if (clen != 4)
		    {
			strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
			    "LLC: send FRMR_WBIT & FRMR_XBIT, clen=%d",clen);
			reason = (FRMR_WBIT | FRMR_XBIT);
		    }
		    else
		    {   /* Crank State M/C */
			if ((reason = (*S_in_fns[c>>2 & 3][lp->state])
				      (lp, fp[3]>>1, type | pf_bit)) == FRAMEOK)
			{
			    DLP(("\tS-frame OK, c=%x, type=%x\n",c, type));
			    ftype = c & 0x0F;
			    switch (ftype)
			    {
				case RR:
				    if (type < RSP_F0)
        				LLC2MONITOR(RR_rx_cmd)
				    else
        				LLC2MONITOR(RR_rx_rsp)
				    break;

				case RNR:
				    if (type < RSP_F0)
        				LLC2MONITOR(RNR_rx_cmd)
				    else
        				LLC2MONITOR(RNR_rx_rsp)
				    break;

				case REJ:
				    if (type < RSP_F0)
        				LLC2MONITOR(REJ_rx_cmd)
				    else
        				LLC2MONITOR(REJ_rx_rsp)
				    break;

				default:
				    break;
			    }
			}
#ifdef SGI
			else {
			    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
				"LLC - bad LLC header, reason=%d\n",reason);
			    DLP(("\tState Machine failed on S frame, %d\n",
				reason));
			}
#endif

		    }
		}
	    }
	    else
	    {   /* I format: check length; if OK call state M/C */
		if (tlen > lp->dnp->dn_tunetab.max_I_len)
		{
		    strlog(LLC_STID,lp->dnp->dn_snid,1,SL_TRACE,
			"LLC: send FRMR_YBIT, packet too big %d", tlen);
		    reason = FRMR_YBIT;
		}
		else
		{   /* Crank State M/C */
		    if ((reason = (*I_in_fns[lp->state])
			(lp, fp[2]>>1, fp[3]>>1, type | pf_bit)) == FRAMEOK)
		    {   /* Statistics */
			if (type < RSP_F0)
			    LLC2MONITOR(I_rx_cmd)
			else
			    LLC2MONITOR(I_rx_rsp)
		    }
		}
	    }
	}
    }

    switch (reason)
    {
    case FRAMEOK:
	return 0 ;

    case IGNORE:
	DLP(("\tllc2_rxframe: IGNORE\n"));
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
		"llc2_rxframe: IGNORE\n");
	return 0 ;

    case INVALID:
	DLP(("\tllc2_rxframe: INVALID\n"));
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
		"llc2_rxframe: INVALID\n");
	return 0 ;

    default:
	/* Bad PDU: (re-)send FRMR */
	DLP(("\tllc2_rxframe: Bad DPU\n"));
	strlog(LLC_STID,lp->dnp->dn_snid,5,SL_TRACE,
		"llc2_rxframe: Bad DPU\n");

	if (lp->state >= ERR)
	{
	    unsigned int state = lp->state;

	    /* RUN state - set up FRMR response fields */
	    if (state == RUN)
	    {
		lp->frmr[0] = fp[2];
		lp->frmr[1] = ((fp[2] & 3) == 3 ? 0 : fp[3]);
		lp->frmr[2] = lp->vs<<1;
		lp->frmr[3] = lp->vr<<1 | fp[1] & 1;
		lp->frmr[4] = reason;
	    }

	    /* RUN state OR (ERR state with a command frame received) */
	    if ( (state == RUN) || (state == ERR && (type == CMD_P0)) )
	    {
		send_Ursp(lp, FRMR);
		stop_all_timers(lp);
		set_timer(&lp->ack_timer, (lp->dnp->dn_tunetab.T1 << 1));
		lp->retry_count = lp->dnp->dn_tunetab.N2;
		lp->state = ERR;
	    }
	}
    }
    return 0;
}

/* ***********************  X I D  A C T I O N  ********************* */

/*  ******************************************** llc2_settxwind  */
llc2_settxwind(llc2cn_t *lp, uint8 window)
{
    uint8 probe = lp->dnp->dn_tunetab.tx_probe;

    lp->txprbpt = probe < window ? window - probe : 0;

    if (lp->us < (lp->txwindow = window))
    {   /* Window is now opened */
	if (lp->wclosed)
	{   /* Was closed, so open it */
	    lp->wclosed = 0;
	    if (--lp->txstopped == 0)
		llc2_schedule(lp);
	}
    }
    else
    {   /* Window is now closed */
	if (!lp->wclosed)
	{   /* Was open, so close it */
	    lp->wclosed = 1;
	    lp->txstopped++;
	}
    }
    return 0;
}

/*  ******************************************** llc2_sendxid  */
llc2_sendxid(lp)
llc2cn_t *lp;
{   /* Send XID command and start timer */
    send_Ucmd(lp, XID);
    set_timer(&lp->dlx_timer, (lp->dnp->dn_tunetab.xid_Tdup << 1));

    /* xidflag == 1 => dlx_timer timing XID */
    lp->xidflag = 1;
    return 0;
}

