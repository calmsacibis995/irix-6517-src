/*
 *   tr_lli.h  STREAMS pseudo-driver definitions for the Token Ring Logical
 *	       Link Interface.
 *   Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 */
#ident "$Revision: 1.4 $"


/*
 *   Clearer data type names.
 */

#ifndef u_short
#define u_short unsigned short int
#endif

#ifndef u_int
#define u_int   unsigned int
#endif

#ifndef u_char
#define u_char  unsigned char
#endif

#ifndef u_long
#define u_long  unsigned long
#endif

/*****************************************************************************
 * Interface State Constants
 *****************************************************************************/

/* Interface states - as per DLPI interface states (dlpi.h) */
#define S_UNBND		DL_UNBOUND
#define S_BNDPND	DL_BIND_PENDING
#define S_UNBPND	DL_UNBIND_PENDING
#define S_IDLE		DL_IDLE
#define S_UNATT		DL_UNATTACHED
#define S_ATPND		DL_ATTACH_PENDING
#define S_DTPND		DL_DETACH_PENDING
#define S_SCPND		DL_UDQOS_PENDING   /**** NOTE *****/
#define S_OUTCON	DL_OUTCON_PENDING
#define S_INCPND	DL_INCON_PENDING
#define S_CNRPND	DL_CONN_RES_PENDING
#define S_DTXFR		DL_DATAXFER
#define S_URPND		DL_USER_RESET_PENDING
#define S_VOID13	DL_PROV_RESET_PENDING
#define S_VOID14	DL_RESET_RES_PENDING
#define S_D8PND		DL_DISCON8_PENDING
#define S_D9PND		DL_DISCON9_PENDING
#define S_D11PND	DL_DISCON11_PENDING
#define S_VOID18	DL_DISCON12_PENDING
#define S_VOID19	DL_DISCON13_PENDING
#define S_SBPND		DL_SUBS_BIND_PND
#define S_SUBPND	DL_SUBS_UNBIND_PND

#define S_NOS		DL_SUBS_UNBIND_PND + 1	/* number of states */

/* Interface events */
#define E_NULL		0		/* Null event */
#define E_ATTRQV	1		/* Attach request - valid interface # */
#define E_ATTRQI	2		/* Attach request - invalid i/face # */
#define E_DTTRQV	3		/* Detach request - valid interface # */
#define E_BINDRQ	4		/* Bind request (for SSAP) */
#define E_OSAPOK	5		/* Open SAP good */
#define E_SBINDRQ	6		/* Subs Bind request (for DSAP) */
#define E_OSTNOK	7		/* Open Station good */
#define E_UBINDRQ	8		/* Unbind request */
#define E_CSAPOK	9		/* Close SAP good */
#define E_SUBNDRQ	10		/* Subs Unbind request */
#define E_CSTNOK	11		/* Close Station good */
#define E_CONNRQ	12		/* Connect request */
#define E_BNDOLD	13		/* Bind request - SAP already exists */
#define E_BADCMD	14		/* Bad response from TRD to a request */
#define E_SBINDMG	15		/* Subs Bind request on MGT stream */
#define E_SABRCV	16		/* SABME_RCVD from TRD */
#define E_SABOL		17		/* SABME_OPEN_LINK from TRD */
#define E_CONNRES	18		/* Connect response from UI */
#define E_CONSOK	19		/* CN.STATION ok, outcnt=1, token=0 */
#define E_CONSOK1	20		/* CN.STATION ok, outcnt=1, token!=0 */
#define E_CONSOK2	21		/* CN.STATION ok, outcnt>1, token!=0 */
#define E_PASSCN	22		/* Pass-connection received */
#define E_DISCNRQ	23		/* Disconnect request received */
#define E_DSCIND	24		/* DISC from TRD, outcnt=0 */
#define E_DSCIND1	25		/* DISC from TRD, outcnt=1 */
#define E_DSCIND2	26		/* DISC from TRD, outcnt>1 */
#define E_DATARQ	27		/* Data request from UI */
#define E_RCVD		28		/* RECEIVE from TRD */
#define E_TESTRQ	29		/* TEST request */
#define E_TESTCON	30		/* TEST command reply from TRD */
#define E_XIDRQ		31		/* XID request */
#define E_XIDCON	32		/* XID command reply from TRD */
#define E_XIDIND	33		/* XID command indication from TRD */
#define E_XIDRES	34		/* XID indication response from UI */
#define E_RSTREQ	35		/* Reset request */
#define E_RSTRS		36		/* Reset response from TRD */
#define E_LSTLNK	37		/* LOST_LINK status from TRD */
#define E_SCLOS		38		/* STREAMS CLOSE recvd */
#define E_SAPRS	    	39  	    	/* SAP reset pending */

#define E_NOE		40		/* number of events */

/* Interface actions */
#define A_NOOP		0		/* Null action */
#define A_ATTACH	1		/* perform DL_ATTACH_REQ */
#define A_ATTBAD	2		/* bad DL_ATTACH_REQ */
#define A_DTTCH		3		/* perform DL_DETACH_REQ */
#define A_OSAP		4		/* OPEN.SAP processing */
#define A_OOSAP		5		/* OPEN.SAP for existing SAP */
#define A_OSAPRS	6		/* process OPEN.SAP response */
#define A_OSTN		7		/* perform OPEN.STATION processing */
#define A_CSAP		8		/* perform CLOSE.SAP processing */
#define A_CSAPRS	9		/* process CLOSE.SAP response */
#define A_CSTN		10		/* perform CLOSE.STATION processing */
#define A_OSTNRS	11		/* process OPEN.STATION response */
#define A_CSTNRS	12		/* process CLOSE.STN rsp, DISCON pend */
#define A_CSTRS1	13		/* process CLOSE.STN rsp, DSCON9 pend */
#define A_CNSTN		14		/* perform CONNECT.STATION processing */
#define A_BADRSP	15		/* process bad response from TRD */
#define A_CONCON 	16		/* process SABME_RCVD from TRD */
#define A_CONIND	17		/* process unsolicited CN rq from TRD */
#define A_CONSTN	18		/* perform CONNECT.STATION */
#define A_CNSRSP	19		/* prcs CN.STN rsp,outcnt=0,token=0*/
#define A_CNSRS2	20		/* prcs CN.STN rsp,outcnt=1,token!=0 */
#define A_PASSCN	21		/* process a passed connection */
#define A_CSTN1		22		/* process DISC indic from TRD */
#define A_CSTN2		23		/* process DISC ind from TRD,outcnt-- */
#define A_DREQ		24		/* transmit-I-frame request */
#define A_DIND		25		/* process Data-received from TRD */
#define A_TESTRQ	26		/* perform DL_TEST_REQ */
#define A_TESTCN	27		/* process TEST cmd reply from TRD */
#define A_XIDRQ		28		/* perform DL_XID_REQ */
#define A_XIDCN		29		/* process XID cmd reply from TRD */
#define A_XIDCMD	30		/* process XID cmd from TRD */
#define A_XIDRES	31		/* perform DL_XID_RES */
#define A_RSTRQ		32		/* perform DL_RESET_REQ */
#define A_RSTRS		33		/* process LLC.RESET resp from TRD */
#define A_ABRT0		34		/* process STREAMS Close - 0 */
#define A_ABRT1		35		/* process STREAMS Close - 1 */
#define A_ABRT2		36		/* process STREAMS Close - 2 */
#define A_ABRT3		37		/* process STREAMS Close - 3 */
#define A_DROPIT    	38  	    	/* drop xmit frames */
#define A_RSTND	    	39  	    	/* issue DISC ind for LOST.LINK */

#define X		0xff		/* Invalid state or action */


#ifdef TRDBG
/*
 * Circular trace buffer format (one for all LCB's)
 */

typedef struct lli_ctb
  {
    u_short 	    	interface;  	    /* interface # */
    u_short 	    	event;	    	    /* event code */
    u_short 	    	cstate;	    	    /* current state */
    u_short 	    	action;	    	    /* action code */
    u_short		station;	    /* station ID */
    u_short		pad1;
    u_int		pad2;
  } LLI_CTB;

#define LLI_CTB_SIZE	100
extern u_int  	    	lli_ctbx;
extern LLI_CTB	    	lli_ctb[];

#endif  /* TRDBG */

extern int drv_usecwait();
extern int tenmicrosec();
