#ident "$Revision: 1.29 $"
/*
 *  
 *  tr_lli.c
 *
 *  STREAMS pseudo-device driver for the the Token Ring Logical Link Interface
 *
 *  Author:     C  Philbrick
 *
 *  Date:       Oct 1991.
 * 
 *  History:
 *
 *  (1) 10/10/91        Initial driver
 *      C Philbrick     Supports /dev/snatr
 *
 */

/****************************************************************************
 *  Driver definitions and include files
 ****************************************************************************/

#define	Error	-1		
#define NoError	0

/*  Console monitor: logs in memory as well as printing to console  */
#include <sys/cmn_err.h>
/*  minor() macro  */
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/signal.h> 	/* for HZ, among others */
#include <sys/param.h> 	
#include <sys/kthread.h>
#include <sys/systm.h>

#include <sys/kmem.h>
#include <bstring.h>
#include <sys/debug.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strids.h>
#include <sys/strmp.h>
#include <net/if.h>
#include <dlpi.h>
#include <tr_lli.h>
#include <sys/tr.h>
#include <sys/llc.h>
#include <sys/tms380.h>
#include <sys/tr_user.h>

#define IFRMDBG 0
#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    (!FALSE)
#endif

/*
#define EMSGSIZE         106
*/
#define EINTRNL          107

int	lli_timeout = 0;

/**************************************************************************
 *  STREAMS driver definitions
 **************************************************************************/

static struct module_info lli_info =
     {
     STRID_TRLINK,              /* module ID */
     "tr_lli",                  /* module name */
     0,                         /* min pkt size */
     INFPSZ,                    /* max pkt size */
     30*4096,                   /* hi-water mark */
     20*4096                    /* lo-water mark */
     };

int lli_rput(queue_t *, mblk_t *);
int lli_rsrv(queue_t *);
static int lli_wsrv(queue_t *);
int lli_open(queue_t *, dev_t *, int, int, struct cred *);
int lli_close(queue_t *, int, struct cred *);
int lli_wput(queue_t *, mblk_t *);
int execute_DLPI_FSM(LCB *);

static struct qinit lli_rinit =
     {
     lli_rput, lli_rsrv, lli_open, lli_close, NULL, &lli_info, NULL
     };

static struct qinit lli_winit = 
     {
     lli_wput, lli_wsrv, NULL, NULL, NULL, &lli_info, NULL
     };

struct streamtab triinfo =
     {
     &lli_rinit,
     &lli_winit,
     NULL,
     NULL
     };
int tridevflag = 0;

/***************************************************************************
 * LLI storage
 ***************************************************************************/

/* Station table - to convert STATION_ID to a pointer to the WR q */
STN_TBL 	stn_tbl[MAX_ADAPTORS];	/* 1 STN TBL per i/f */

/* Storage for LCBs */
static queue_t			*mdev_tbl[MAX_ADAPTORS * MAX_STNS]; /* minor dev tbl */

/* There are (MAX_ADAPTORS) MGT LCBs, each pointing to the same queue_t */
static LCB			*mgt_lcb[MAX_ADAPTORS];	/* ptr to mgt LCBs */

/*
 * Storage for SAP info - 3 parallel tables.
 * sapid_2_sap - given stn_id, get actual sap #
 * sapid_2_q   - used only during OPEN.SAP processing
 * sapid_2_stn - given sap_id, get stn_id of associated sap (should
 *                             always be <sap_id> << 8)
 */

u_int 				sapid_2_sap[MAX_ADAPTORS][MAX_SAPS] = {0};
static queue_t			*sapid_2_q[MAX_ADAPTORS][MAX_SAPS] = {0};
static u_int			sapid_2_stn[MAX_ADAPTORS][MAX_SAPS] = {0};

/* DLPI variables */

u_int				outcnt = 0;

#ifdef TRDBG
LLI_CTB				lli_ctb[LLI_CTB_SIZE];  /* circular trace buf */
u_int				lli_ctbx = 0;		/* next available */
#endif /* TRDBG */

/***************************************************************************
 * Interface State Tables   
 ***************************************************************************/
/***************************************************************************
 *
 * Deviation from DLPI spec:
 *    1/22/92: Jay Lan
 *	S_DTXFR(E_UNBINDRQ) -> S_UNBPND + A_CSAP
 *	S_DTXFR(E_SUBNDRQ)  -> S_SUBPND + A_CSTN
 *    2/5/92:  Jay Lan
 *	S_CNRPND(E_SABRCV)  -> S_CNRPND + A_DROPIT
 *    2/8/92:  Jay Lan
 *      S_DTXFR(E_CONNRQ)   -> S_DTXFR  + A_DROPIT
 *    2/8/92:  Jay Lan
 *      S_OUTCON(E_XIDIND)  -> S_OUTCON + A_DROPIT
 *    2/27/92: Jay Lan
 *	S_OUTCON(E_XIDCON)  -> S_OUTCON + A_DROPIT
 *    4/9/92: Jay Lan
 *	E_SCLOS should put the state to S_UNATT and triger ABORT
 *
 ***************************************************************************/

/* Interface state transition table */
static unsigned int LLI_STT[E_NOE][S_NOS] = {

                        /*  S T A T E S  */

/* UNBND ,  BNDPND,  UNBPND,   IDLE ,   UNATT,   ATPND,   DTPND,   SCPND,
     OUTCON,  INCPND,  CNRPND,   DTXFR,   URPND,  VOID13,  VOID14,   D8PND,
        D9PND,  D11PND,  VOID18,  VOID19,   SBPND,  SUBPND */

/* ctrig = 0 */
{S_UNBND ,S_BNDPND,S_UNBPND, S_IDLE , S_UNATT, S_ATPND, S_DTPND, S_SCPND,
   S_OUTCON,S_INCPND,S_CNRPND, S_DTXFR, S_URPND,S_VOID13,S_VOID14, S_D8PND,
      S_D9PND,S_D11PND,S_VOID18,S_VOID19, S_SBPND,S_SUBPND },
{    X   ,    X   ,    X   ,    X   , S_UNBND,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   , S_UNATT,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{ S_UNATT,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{S_BNDPND,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 5 */
{    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   , S_SBPND,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   , S_IDLE ,    X    },
{    X   ,    X   ,    X   ,S_UNBPND,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,S_UNBPND,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   , S_UNBND,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 10 */
{    X   ,    X   ,    X   ,S_SUBPND,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,S_SUBPND,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   , S_UNATT,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   , S_IDLE ,
      S_IDLE , S_IDLE ,    X   ,    X   ,    X   , S_IDLE  },
{    X   ,    X   ,    X   ,S_OUTCON,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{ S_IDLE ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{ S_UNBND, S_UNBND, S_IDLE ,    X   ,    X   ,    X   ,    X   ,    X   ,
    S_IDLE ,    X   ,S_INCPND,    X   ,    X   ,    X   ,    X   ,S_OUTCON,
     S_INCPND, S_DTXFR,    X   ,    X   , S_IDLE , S_IDLE  },
/* ctrig = 15 */
{    X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,S_INCPND,    X   ,    X   ,    X   ,    X   ,
    S_DTXFR,S_INCPND,S_CNRPND,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,S_INCPND,    X   ,    X   ,    X   ,    X   ,
       X   ,S_INCPND,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,S_CNRPND,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 20 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
    S_D8PND, S_D9PND,    X   ,S_D11PND,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
    S_IDLE ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 25 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,S_INCPND,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 30 */
{    X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
   S_OUTCON,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
   S_OUTCON,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , S_DTXFR,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 35 */
{ S_URPND, S_URPND,S_UNBPND, S_URPND, S_UNATT, S_ATPND, S_DTPND,    X   ,
    S_URPND, S_URPND, S_URPND, S_URPND, S_URPND,    X   ,    X   , S_URPND,
      S_URPND, S_URPND,    X   ,    X   , S_URPND, S_URPND },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   , S_UNATT,
       X   ,    X   ,    X   ,    X   , S_UNBND,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , S_IDLE ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
{ S_UNATT, S_UNATT, S_UNATT, S_UNATT, S_UNATT, S_UNATT, S_UNATT,    X   ,
    S_UNATT, S_UNATT, S_UNATT, S_UNATT, S_UNATT,    X   ,    X   , S_UNATT,
      S_UNATT, S_UNATT,    X   ,    X   , S_UNATT, S_UNATT },
{ S_URPND, S_URPND,S_UNBPND, S_URPND, S_UNATT, S_ATPND, S_DTPND,    X   ,
    S_URPND, S_URPND, S_URPND, S_URPND, S_URPND,    X   ,    X   , S_URPND,
      S_URPND, S_URPND,    X   ,    X   , S_URPND, S_URPND },
};


/* Interface state action table */
static unsigned int LLI_SAT[E_NOE][S_NOS] = {

                        /*  S T A T E S  */
/* UNBND ,  BNDPND,  UNBPND,   IDLE ,   UNATT,   ATPND,   DTPND,   SCPND,
     OUTCON,  INCPND,  CNRPND,   DTXFR,   URPND,  VOID13,  VOID14,   D8PND,
        D9PND,  D11PND,  VOID18,  VOID19,   SBPND,  SUBPND */

/* ctrig = 0 */
{ A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP ,
    A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP ,
       A_NOOP, A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP  },
/* ctrig = 1 */
{    X   ,    X   ,    X   ,    X   ,A_ATTACH,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 2 */
{    X   ,    X   ,    X   ,    X   ,A_ATTBAD,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 3 */
{ A_DTTCH,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 4 */
{ A_OSAP ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 5 */
{    X   ,A_OSAPRS,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 6 */
{    X   ,    X   ,    X   , A_OSTN ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 7 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,A_OSTNRS,    X    },
/* ctrig = 8 */
{    X   ,    X   ,    X   , A_CSAP ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , A_CSAP ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 9 */
{    X   ,    X   ,A_CSAPRS,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 10 */
{    X   ,    X   ,    X   , A_CSTN ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , A_CSTN ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 11 */
{    X   ,    X   ,    X   ,A_DROPIT,    X   ,    X   ,    X   , A_ABRT0,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,A_CSTNRS,
     A_CSTRS1,A_CSTNRS,    X   ,    X   ,    X   ,A_CSTNRS },
/* ctrig = 12 */
{    X   ,    X   ,    X   , A_CNSTN,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,A_DROPIT,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 13 */
{ A_OOSAP,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 14 */
{A_BADRSP,A_BADRSP,A_BADRSP,    X   ,    X   ,    X   ,    X   ,    X   ,
   A_BADRSP,    X   ,A_BADRSP,    X   ,A_BADRSP,    X   ,    X   ,A_BADRSP,
     A_BADRSP,A_BADRSP,    X   ,    X   ,A_BADRSP,A_BADRSP },
/* ctrig = 15 */
{    X   ,    X   ,    X   ,A_OSTNRS,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 16 */
{    X   ,    X   ,    X   ,A_CONIND,    X   ,    X   ,    X   ,    X   ,
   A_CONCON,A_DROPIT,A_DROPIT,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 17 */
{    X   ,    X   ,    X   ,A_CONIND,    X   ,    X   ,    X   ,    X   ,
       X   ,A_CONIND,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 18 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,A_CONSTN,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 19 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,A_CNSRSP,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 20 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,A_CNSRS2,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 21 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,A_CNSRS2,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 22 */
{    X   ,    X   ,    X   ,A_PASSCN,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 23 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
    A_CSTN , A_CSTN ,    X   , A_CSTN ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 24 */
{    X   ,    X   ,    X   , A_CSTN1,    X   ,    X   ,    X   ,    X   ,
    A_CSTN1,    X   ,    X   , A_CSTN1,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 25 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   , A_CSTN2,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 26 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   , A_CSTN2,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 27 */
{    X   ,    X   ,    X   ,A_DROPIT,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , A_DREQ ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 28 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , A_DIND ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 29 */
{    X   ,    X   ,    X   ,A_TESTRQ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,A_TESTRQ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 30 */
{    X   ,    X   ,    X   ,A_TESTCN,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,A_TESTCN,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 31 */
{    X   ,    X   ,    X   , A_XIDRQ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , A_XIDRQ,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 32 */
{    X   ,    X   ,    X   , A_XIDCN,    X   ,    X   ,    X   ,    X   ,
   A_DROPIT,    X   ,    X   , A_XIDCN,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 33` */
{    X   ,    X   ,    X   ,A_XIDCMD,    X   ,    X   ,    X   ,    X   ,
   A_DROPIT,    X   ,    X   ,A_XIDCMD,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 34 */
{    X   ,    X   ,    X   ,A_XIDRES,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   ,A_XIDRES,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 35 */
{    X   ,    X   ,    X   , A_RSTRQ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , A_RSTRQ, A_NOOP ,    X   ,    X   , A_RSTRQ,
      A_RSTRQ, A_RSTRQ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 36 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   , A_ABRT0,
       X   ,    X   ,    X   ,    X   , A_RSTRS,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 37 */
{    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,    X   ,
       X   ,    X   ,    X   , A_RSTND,    X   ,    X   ,    X   ,    X   ,
         X   ,    X   ,    X   ,    X   ,    X   ,    X    },
/* ctrig = 38 */
{  A_ABRT0, A_ABRT2, A_ABRT2, A_ABRT2, A_ABRT0, A_ABRT0, A_ABRT0,    X   ,
    A_ABRT2, A_ABRT2, A_ABRT2, A_ABRT2, A_NOOP ,    X   ,    X   , A_ABRT2,
      A_ABRT2, A_ABRT2,    X   ,    X   , A_ABRT2, A_ABRT2 },
/* ctrig = 39 */
{ A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP ,    X   ,
    A_NOOP , A_NOOP , A_NOOP , A_NOOP , A_NOOP ,    X   ,    X   , A_NOOP ,
       A_NOOP, A_NOOP ,    X   ,    X   , A_NOOP , A_NOOP  },
};

#ifdef TRDBG
struct xxyy {
	ushort		func;
	ushort		where;
	mblk_t		*addr;
};

struct xxyy mblk_buf[100];
int mblk_cnt = 0;

mblk_trace(func, where, addr)
mblk_t *addr;
ushort where,func;
{
	mblk_t *bp, *tp;

	if (func == 3) {
	    bp = addr;
	    while (bp) {
	        tp = bp->b_cont;
	        mblk_buf[mblk_cnt].addr = bp;
	        mblk_buf[mblk_cnt].where = where;
	        mblk_buf[mblk_cnt].func = 1;
	        if (++mblk_cnt == 100)
		    mblk_cnt = 0;
		bp = tp;
	    }
	} else if (func == 2) {
	    bp = addr;
	    while (bp) {
	        tp = bp->b_cont;
	        mblk_buf[mblk_cnt].addr = bp;
	        mblk_buf[mblk_cnt].where = where;
	        mblk_buf[mblk_cnt].func = 2;
	        if (++mblk_cnt == 100)
		    mblk_cnt = 0;
		bp = tp;
	    }
	} else {

	    mblk_buf[mblk_cnt].addr = addr;
	    mblk_buf[mblk_cnt].where = where;
	    mblk_buf[mblk_cnt].func = func;
	    if (++mblk_cnt == 100)
		mblk_cnt = 0;
	}
}

#define LLP(a)		printf a
#define LEP(a)		if (error) printf a
#define HEXDUMP(a,b)	fv_hexdump(a,b);
#define LLI_BRK()	lli_brk()
#else
#define mblk_trace(a,b,c)
#define LLI_BRK()
#define LLP(a)
#define LEP(a)
#define HEXDUMP(a,b)
#endif /* TRDBG */

void
lli_brk(int cnt)
{
	printf("LLI_BRK, %d\n",cnt);
}

#ifdef TIMETRC
#define TIMEDUMP(a,b,c) time_trace(a,b,c)
struct timetrc {
    ushort  func;       /* 1=SEND, 2=RECEIVE */
    ushort  length;     /* length of user data */
    time_t  time;
};

struct timetrc lli_tbuf[320];
int lli_tbuf_cnt = 0;

static time_trace(func, length, time)
time_t time;
ushort length,func;
{
    lli_tbuf[lli_tbuf_cnt].func = func;
    lli_tbuf[lli_tbuf_cnt].length = length;
    lli_tbuf[lli_tbuf_cnt].time = time;
    if (++lli_tbuf_cnt == 100)
        lli_tbuf_cnt = 0;
}

#else
#define TIMEDUMP(a,b,c)
#endif

#if defined(RIFDBG) || defined(TRDBG)
static void
fv_hexdump(char *cp, int len)
{
        register int idx;
        register int qqq;
        char qstr[512];
        static char digits[] = "0123456789abcdef";

        for (idx = 0, qqq = 0; qqq<len; qqq++) {
                if ((qqq%16) == 0)
                        qstr[idx++] = '\n';
                qstr[idx++] = digits[cp[qqq] >> 4];
                qstr[idx++] = digits[cp[qqq] & 0xf];
                qstr[idx++] = ' ';
        }
        qstr[idx] = 0;
        printf("%s\n", qstr);
}
#endif /* TRDBG || RIFDBG */


/* Routine called by bufcall() when buffer available */
static void
prcs_bufc(long 	cbptr)
{

    queue_t		*wq = NULL;
    LCB 		*lcbptr;

    lcbptr = (LCB *)cbptr;
    if ((LCB *) lcbptr->qptr != NULL)
    	wq = lcbptr->qptr;

    /* if control block not valid, may have gone away - ignore call */
    if (wq->q_ptr == (void *) lcbptr)
        {
	/* ctrig was set before bufcall */
        execute_DLPI_FSM(lcbptr);
        }
    return;
}


/***************************************************************************
 *  Driver internal utility routines
 ***************************************************************************/

/* Send DL_BIND_ACK reply to user (use input block if large enough) */
static int 
bind_ack_rply(queue_t		*wq,	/* WR q expected */
	      mblk_t		*mp)
{
    union DL_primitives 	*dl_ptr;
    mblk_t			*em_ptr;
    u_int			i;
    u_int			*j;
    LCB    			*cbptr = (LCB *) wq->q_ptr;

    /* check if input block is big enough */
    i = sizeof(dl_bind_ack_t) + sizeof(int);

    if (i > (mp->b_datap->db_lim - mp->b_datap->db_base))
	{
	if ((em_ptr = (mblk_t *)allocb(i, BPRI_MED)) == (mblk_t *)NULL)
	    { 				/* no buffers - use bufcall() */
	    if (bufcall(i,
		    	BPRI_MED,
			prcs_bufc,
			(long)cbptr) == 0)	/* param is LCB ptr */
		{			/* call failed - just send error_ack */
		cmn_err(CE_NOTE, "bind_ack_rply: bufcall() failure\n");
    		dl_ptr = (void *) mp->b_rptr;
    		dl_ptr->dl_primitive = DL_ERROR_ACK;
	  	mblk_trace(2,603,mp);
    		qreply(wq, mp);
		cbptr->ip_msg = NULL;
		return(TRUE);
		}
	    return(FALSE);		/* so caller will exit FSM */
	    }
	    mblk_trace(0,200,em_ptr);
	}				/* rcvd msg not big enough */
    else
	{				/* recvd msg is big enough */
	em_ptr = mp;
    	ASSERT(mp->b_next == NULL);
    	ASSERT(unlinkb(mp) == NULL); 	/* only 1 block expected */
	}

    em_ptr->b_datap->db_type = M_PCPROTO;
    em_ptr->b_rptr = em_ptr->b_datap->db_base;
    em_ptr->b_wptr = em_ptr->b_rptr + i;
    dl_ptr = (void *) em_ptr->b_rptr;
    dl_ptr->dl_primitive = DL_BIND_ACK;
    ((dl_bind_ack_t *) dl_ptr)->dl_sap = cbptr->ssap_id;
    ((dl_bind_ack_t *) dl_ptr)->dl_addr_length = sizeof(u_int);
    ((dl_bind_ack_t *) dl_ptr)->dl_addr_offset = sizeof(dl_bind_ack_t);
    ((dl_bind_ack_t *) dl_ptr)->dl_max_conind = cbptr->max_conind;
    ((dl_bind_ack_t *) dl_ptr)->dl_xidtest_flg = DL_AUTO_TEST; /* no choice */
    j = (u_int *)((dl_bind_ack_t *) dl_ptr + 1); /* to 1st word after prim. */
    *j = cbptr->sap_stnid;		/* STN_ID of original opened SAP */
    mblk_trace(2,604,em_ptr);
    qreply(wq, em_ptr);
    if (em_ptr != mp) {
	mblk_trace(3,1,mp);
	freemsg(mp);
    }
    cbptr->ip_msg = NULL;
    return(TRUE);
}
	   
/* Send DL_OK_ACK reply to user (use input block) */
static void
send_ok_ack(queue_t		*wq,
	    mblk_t		*mp,
	    u_int		prim)		/* primitive to ACK */
{
    union DL_primitives     	*dl_ptr;
    LCB    			*cbptr;

    /* Use i/p block to format and send DL_OK_ACK */
    /* ensure room for ack */
    ASSERT(sizeof(dl_ok_ack_t) <= (mp->b_datap->db_lim - mp->b_datap->db_base));
    ASSERT(mp->b_next == NULL);
    ASSERT(unlinkb(mp) == NULL); 	/* only 1 block expected */
    cbptr = (LCB *) wq->q_ptr;
    mp->b_datap->db_type = M_PCPROTO;
    mp->b_rptr = mp->b_datap->db_base;
    mp->b_wptr = mp->b_rptr + sizeof(dl_ok_ack_t);
    dl_ptr = (void *) mp->b_rptr;
    ((dl_ok_ack_t *) dl_ptr)->dl_correct_primitive = prim;
    dl_ptr->dl_primitive = DL_OK_ACK;
    mblk_trace(2,605,mp);
    qreply(wq, mp);
    cbptr->ip_msg = NULL;
}
	   
/*
 * Send DL_ERROR_ACK response to user (use input block if possible)
 * 	- returns TRUE if msg sent and frees/uses ip_msg, 
 *	- returns FALSE if unable to (bufcall called), & does not free ip_msg.
 */
static int
send_error_ack(queue_t		*wq,		/* NOTE: WR q expected */
	       mblk_t		*mp,
               u_long           errno,
	       u_long		syserr)
{
    union DL_primitives		*ddl_ptr;	/* build error_ack in here */
    mblk_t			*em_ptr;
    LCB    			*cbptr;

    /* Ensure room for error ack */
    if (sizeof(dl_error_ack_t) > (mp->b_datap->db_lim
				    - mp->b_datap->db_base))
	{
	if ((em_ptr = (mblk_t *)allocb(sizeof(dl_error_ack_t), BPRI_MED)) == (mblk_t *)NULL) {	
	    				/* no buffers - use bufcall() */
	    if (bufcall(sizeof(dl_error_ack_t),
		    	BPRI_MED,
			prcs_bufc,
			(long)wq->q_ptr) == 0)
		{			/* call failed - just send error_ack */
		cmn_err(CE_NOTE, "send_error_ack: bufcall() failure\n");

    		ddl_ptr = (void *) mp->b_rptr;
    		ddl_ptr->dl_primitive = DL_ERROR_ACK;

    		mblk_trace(2,606,mp);
    		qreply(wq, mp);
    		cbptr = (LCB *) wq->q_ptr;
		cbptr->ip_msg = NULL;
		return(TRUE);
		}
	    return(FALSE);		/* so caller will exit FSM */
	    }
	    mblk_trace(0,201,em_ptr);
	}				/* rcvd msg not big enough */
    else
	{
	em_ptr = mp;			/* use rcvd msg */
    	ASSERT(mp->b_next == NULL);
    	ASSERT(unlinkb(mp) == NULL); 	/* only 1 block expected */
	}
    em_ptr->b_datap->db_type = M_PCPROTO;
    em_ptr->b_rptr = em_ptr->b_datap->db_base;
    em_ptr->b_wptr = em_ptr->b_rptr + sizeof(dl_error_ack_t);
    ddl_ptr = (void *) em_ptr->b_rptr;
    cbptr = (LCB *) wq->q_ptr;
    ((dl_error_ack_t *) ddl_ptr)->dl_error_primitive = cbptr->err_prim;
    ddl_ptr->dl_primitive = DL_ERROR_ACK;
    ((dl_error_ack_t *) ddl_ptr)->dl_errno = errno;
    ((dl_error_ack_t *) ddl_ptr)->dl_unix_errno = syserr;
    mblk_trace(2,607,em_ptr);
    qreply(wq, em_ptr);
    if (em_ptr != mp) {
	mblk_trace(3,2, mp);
	freemsg(mp);
    }
    cbptr->ip_msg = NULL;
    return(TRUE);
}

/*
 * tr_cstn(cbptr) - issue CLOSE.STATION on specified LCB.
 * Returns: TRUE if successful, FALSE if not.
 */
int
tr_cstn(LCB 	    *cbptr)

{
  SCB	        sap_scb;	    	    /* build cmd in this */

  /* format CLOSE.STATION to TRD */
  LLP(("CLOSE_STATION: stn_id=%x\n", cbptr->stn_id));
  sap_scb.scb_cmd = TR_CMD_CLOSE_STATION;
  sap_scb.scb_parm1 = cbptr->stn_id;
  sap_scb.scb_parm2 = 0;
  if (!(tr_lli_output(cbptr->ppa, &sap_scb))) /* cmd error */
      {
      cbptr->err_prim = DL_SUBS_UNBIND_REQ;
      ASSERT(cbptr->ip_msg);
      send_error_ack(cbptr->qptr, cbptr->ip_msg, DL_BADPRIM, 0);
      return(FALSE);
      }
  return(TRUE);
}

/* tr_build_hdr(mp) - build protocol header in an mblk_t */
void
tr_build_hdr(mblk_t 	    *mp,
	     u_int  	    primitive,
	     u_int  	    size)
{
  union DL_primitives	    *dl_ptr;

  mp->b_datap->db_type = M_PROTO;
  mp->b_rptr = mp->b_datap->db_base;
  mp->b_wptr = mp->b_rptr + size;
  dl_ptr = (void *) mp->b_rptr;
  dl_ptr->dl_primitive = primitive;
}

/*
 * tr_discind(cbptr) - issue DISCONNECT_IND to user.
 */
void
tr_discind(LCB 	    	*cbptr)
{
  mblk_t    	    	*mp = cbptr->ip_msg;
  union DL_primitives   *dl_ptr;

  ASSERT(mp != 0);

  /* build DL_DISCONNECT_IND block - will fit in ICB passed up */
  ASSERT(sizeof(dl_disconnect_ind_t) <= (mp->b_datap->db_lim - mp->b_datap->db_base));
  tr_build_hdr(mp, DL_DISCONNECT_IND, sizeof(dl_disconnect_ind_t));
  dl_ptr = (void *) mp->b_rptr;
  ((dl_disconnect_ind_t *) dl_ptr)->dl_originator = DL_PROVIDER;
  ((dl_disconnect_ind_t *) dl_ptr)->dl_reason = DL_DISC_PERMANENT_CONDITION;
  ((dl_disconnect_ind_t *) dl_ptr)->dl_correlation = 0;

  mblk_trace(2,608,mp);
  ASSERT(cbptr->qptr != 0);
  qreply(cbptr->qptr, mp);
  cbptr->ip_msg = NULL;
}
    
/*
 * tr_get_blk(cbptr, size) - get a block for XID/TEST primitive
 * Returns: pointer to the block if successful,
 *          NULL if no blocks.
 */
mblk_t *
tr_get_blk(LCB		*cbptr,
	    u_int	size)
{
    union DL_primitives *dl_ptr;
    mblk_t		*em_ptr;

    /* get a block for the appropriate primitive */
    if ((em_ptr = (mblk_t *)allocb(size, BPRI_MED)) == (mblk_t *)NULL)
    	{			/* no buffers - use bufcall() */
    	if (bufcall(size,
		    BPRI_MED,
		    prcs_bufc,
		    (long)cbptr) == 0)	/* param is LCB ptr */
	    {				/* call failed - just send error_ack */
	    cmn_err(CE_NOTE, "tr_get_blk: bufcall() failure\n");
   	    dl_ptr = (void *) cbptr->ip_msg->b_rptr;
	    dl_ptr->dl_primitive = DL_ERROR_ACK;
  	    mblk_trace(2,609,cbptr->ip_msg);
	    qreply(cbptr->qptr, cbptr->ip_msg);
	    cbptr->ip_msg = NULL;
	    }
        }
    mblk_trace(0, 202, em_ptr);
    return(em_ptr);
}

/*
 * tr_update_ri(ri_ptr, mp) - update routing info in the LCB from this block.
 * It would also adjust mp->b_rptr to point pass LLC header.
 */
void
tr_update_ri(LCB		*cbptr,
	     mblk_t    		*mp)
{
    TR_RII			*ri_ptr = (TR_RII *) cbptr->src_ri;
    TR_MAC_HDR			*fr_ptr;

    /* extract RI if exists */
    fr_ptr = (TR_MAC_HDR *)(mp->b_rptr + sizeof(LLI_CB) + sizeof(TR_ICB));
    if (fr_ptr->t_mac_sa[0] & 0x80)
	{
	LLP(("tr_lli: current rii=%x, new rii=%x\n",
		ri_ptr->rii, fr_ptr->t_mac_rii));
	LLP(("\t ri_ptr=%x, fr_ptr=%x\n", ri_ptr, fr_ptr));
	if ((((ri_ptr->rii & TR_RIF_LENGTH)>>8) < 3) ||
	   ((ri_ptr->rii & TR_RIF_LENGTH) 
		> (fr_ptr->t_mac_rii & TR_RIF_LENGTH)))
	    {
#ifdef RIFDBG
	    printf("tr_lli: Update routing information\n");
#endif
	    *ri_ptr = fr_ptr->t_mac_ri;
	    ri_ptr->rii &= (TR_RIF_LENGTH|TR_RIF_LF_MASK);/* retain lng field */
	    if (fr_ptr->t_mac_rii & TR_RIF_DIRECTION)
		ri_ptr->rii &= (~TR_RIF_DIRECTION);
	    else
		ri_ptr->rii |= TR_RIF_DIRECTION;
#ifdef RIFDBG
	    fv_hexdump((char *)ri_ptr, 18);
#endif
	    }
	cbptr->rif_len = ((fr_ptr->t_mac_rii & TR_RIF_LENGTH) >> 8);

	ASSERT(cbptr->rif_len <=18);
	mp->b_rptr += cbptr->rif_len;
	}
    else
	cbptr->rif_len = 0;
}

/*
 * tr_xid_rqrsp(cbptr, *stn_scb, *xmit_parms) - build XID request/response
 * Returns: pointer to built block (may be original block) if successful,
 *	    NULL if no passed down data and unable to get a block.
 */
mblk_t *
tr_xid_rqrsp(LCB		*cbptr,
	     SCB		*sc_ptr,
	     TR_XMIT_PARMS	*xp_ptr,
	     u_int		prim)
{
    mblk_t		*em_ptr;
    TR_MAC_HDR		*fr_ptr;
    u_int		i;
    char		*sa_ptr;
    TR_RII		*ri_ptr;
	    
    sc_ptr->scb_cmd = TR_CMD_TRANSMIT;
    sc_ptr->scb_parm1 = (u_short) ((__psunsigned_t) xp_ptr >> 16);
    sc_ptr->scb_parm2 = (u_short) ((__psunsigned_t) xp_ptr & 0xffff);
    ri_ptr = (TR_RII *) cbptr->src_ri;	/* ptr to possible RI */

    /* look for passed-down data */
    if ((em_ptr = unlinkb(cbptr->ip_msg)) == NULL)
	{				/* no data */
    	if ((em_ptr = allocb(sizeof(TR_MAC_HDR), BPRI_MED)) == NULL)
    	    {				/* no buffers - return error */
	    cmn_err(CE_NOTE, "tr_xid_rq: no buffers\n");
	    cbptr->err_prim = prim;
            send_error_ack(cbptr->qptr, cbptr->ip_msg, DL_SYSERR, ENOSR);
	    return(em_ptr);
	    }
        mblk_trace(0, 203, em_ptr);
        fr_ptr = (TR_MAC_HDR *) em_ptr->b_datap->db_base;
	em_ptr->b_rptr = (unsigned char *) fr_ptr;
	em_ptr->b_wptr = em_ptr->b_rptr + sizeof(TR_MAC_HDR);
    	if (ri_ptr && ri_ptr->rii == 0)	/* not set - no RI field */
	    em_ptr->b_wptr -= TR_MAC_RIF_SIZE;
	}
    else
	{				/* use passed block */
	/* ensure room for MAC hddr */
	ASSERT((em_ptr->b_rptr - em_ptr->b_datap->db_base) >= sizeof(TR_MAC_HDR));
    	fr_ptr = (TR_MAC_HDR *)((__psunsigned_t)em_ptr->b_rptr - sizeof(TR_MAC_HDR));
	    						
	LLP(("xid_rqrsp: em_ptr=%x, rptr=%x, fr_ptr=%x, rii=%x\n",
		em_ptr, em_ptr->b_rptr, fr_ptr,ri_ptr->rii));
	    						
    	if ((ri_ptr->rii & TR_RIF_LENGTH) == 0)	/* not set - no RI field */
	    {
	    fr_ptr = (TR_MAC_HDR *)((unsigned char *)fr_ptr+TR_MAC_RIF_SIZE);
	    }
	em_ptr->b_rptr = (unsigned char *)fr_ptr;
	LLP(("xid_rqrsp: now rptr=%x, TR_RII=%d\n", 
		em_ptr->b_rptr, sizeof(TR_RII)));
	}

    em_ptr->b_datap->db_type = M_DATA;
    fr_ptr->t_mac_ac = TR_MAC_AC_PRIORITY;
    fr_ptr->t_mac_fc = TR_MAC_FC_NON_MAC_CTRL;
    sa_ptr = (char *)tr_get_sa(cbptr->ppa);
    for (i = 0; i < 6; i++)
	{
	fr_ptr->t_mac_da[i] = cbptr->mac_addr[i];
    	fr_ptr->t_mac_sa[i] = *sa_ptr++;
	}
    xp_ptr->xmit_data  = em_ptr;
/*    xp_ptr->frame_size = sizeof(TR_MAC_HDR) - TR_MAC_RIF_SIZE; */
    xp_ptr->frame_size = em_ptr->b_wptr - em_ptr->b_rptr;
    xp_ptr->station_id = (cbptr->stn_id & 0xff00);
    xp_ptr->remote_sap = 0x04;
    if (ri_ptr->rii & TR_RIF_LENGTH)    /* include RI if exists */
	{
	fr_ptr->t_mac_ri = *ri_ptr;
	/* set SA bit 0 */
	fr_ptr->t_mac_sa[0] |= 0x80;
/*	xp_ptr->frame_size += TR_MAC_RIF_SIZE; */
	}
    LLP(("xid_rqrsp: new em_ptr=%x, frame_size=%d, dsap=%x, ssap=%x\n", 
		em_ptr, xp_ptr->frame_size, cbptr->dsap_id, cbptr->ssap_id));
    HEXDUMP((char *)fr_ptr, 32);
    return(em_ptr);
}

#ifdef TRDBG
/* lli_uctb(cbptr, currentAction) - update circular trace buffer.
 *
 * Store event in circular trace buffer
 */
void
lli_uctb(
	 LCB        	*cbptr,
	 u_int	    	currentAction)
{
    LLI_CTB 	    	*ctbptr = lli_ctb + lli_ctbx++;

    ctbptr->interface = (u_short)cbptr->ppa;
    ctbptr->station   = (u_short)cbptr->stn_id;
    ctbptr->event     = (u_short)cbptr->ctrig;
    ctbptr->cstate    = (u_short)cbptr->cstate;
    ctbptr->action    = (u_short)currentAction;
    if (lli_ctbx >= LLI_CTB_SIZE)
	  lli_ctbx = 0;
}
#endif  /* TRDBG */

void
lli_wtime(queue_t *wq)
{
    int s1 = spl1();
    int s = splimp();
    LCB *cbptr = (LCB*)wq->q_ptr;

    lli_timeout = 0;
    LLP(("lli_wtime: timeout wq=%x\n",wq));
    /* If the stream has been closed when the timer hits, exit */
    if (cbptr == (LCB*)0)
	{
	goto lli_wt_ret;
	}
    if (cbptr->ifr_state==IFR_STATE_WAIT)
	{
	lli_timeout = STREAMS_TIMEOUT(lli_wtime,wq,50);
	}
    else
	{
	LLP(("lli_wtime: IFR_STATE_WAIT cleared\n"));
        enableok(wq);
        qenable(wq);
	}

lli_wt_ret:
	splx(s);
	splx(s1);
}


/**************************************************************************
 * LLI state machine processor
 **************************************************************************/

execute_DLPI_FSM(LCB	*cbptr)
{
    
    u_int                 currState;
    u_int                 currEvent;
    u_int                 currAction;
    union DL_primitives   *dl_ptr;
    u_int		  i,j;
    mblk_t		  *mp = cbptr->ip_msg;
    u_int                 nextState;
    int                   rc = 0;
    queue_t		  *wq = cbptr->qptr;

    ASSERT(cbptr->fsm_lock == 0);
    cbptr->fsm_lock = 1;      	    	    /* lock out recursive calls */
    currState  = cbptr->cstate;
    currEvent  = cbptr->ctrig;
    LLP(("execute_DLPI_FSM(%x): currState = %d, currEvent = %d\n", cbptr, currState, currEvent));
    currAction = LLI_SAT[currEvent][currState];
    nextState  = LLI_STT[currEvent][currState];    

    LLP(("execute_DLPI_FSM: entry: cstate = %d, pstate = %d, ctrig = %d, ptrig = %d\n",cbptr->cstate, cbptr->pstate, cbptr->ctrig, cbptr->ptrig));
    
#ifdef TRDBG
    lli_uctb(cbptr, currAction);
#endif /* TRDBG */

    switch (currAction)
        {

        case A_ABRT0:	    	    	    	/* process CLOSE on stream */
	    {
	    u_int   	    stn_id;

	    if (cbptr->ip_msg)
		{
		mblk_trace(3,3, cbptr->ip_msg);
		freemsg(cbptr->ip_msg);
		}
	    
	    if (cbptr->mgt_sap_flag)
		{
		for (i = 0; i < 4; i++)
		    {
	    	    kmem_free(mgt_lcb[i], sizeof(LCB));
		    mgt_lcb[i] = NULL;
		    }
		LLP(("A_ABRT0: Warning - Management CN closed\n"));
	        }
	    else
		{
		stn_id = cbptr->stn_id;
		i = (stn_id >> 8) - 1;    /* get SAP_ID -> table offset */
		stn_id &= 0xff;		  /* get STATION_ID -> offset */
		stn_id--;
		i = (i * MAX_STNS) + stn_id; /* i = offset w/n STN_TBL entry */
		stn_tbl[cbptr->ppa].WR_PTR(i) = NULL; /* clr stn_tbl entry */
#ifndef FRAME_ON_SAP
		stn_tbl[cbptr->ppa].MACPTR(i) = NULL;
#endif /* FRAME_ON_SAP */
	        kmem_free(cbptr, sizeof(LCB));
	        }
	    }

	    break;

	case A_ABRT3:	    	    	    	/* Streams CLOSE - decr outcnt */
	    ASSERT(outcnt > 0);
	    outcnt--;
	    /* goto A_ABRT1; ... fall thru */

        case A_ABRT1:	    	    	    	/* Streams CLOSE - active */
	    /*
	     * A CLOSE has been issued on an active stream.
	     * Issue CLOSE.STN to TRD for it.
	     */
	    if (cbptr->mgt_sap_flag)
		{
		for (i = 0; i < 4; i++)
		    {
	    	    kmem_free(mgt_lcb[i], sizeof(LCB));
		    mgt_lcb[i] = NULL;
		    }
		LLP(("A_ABRT1: Warning - Management CN closed\n"));
	        }
	    else
	        {
		/* format CLOSE.STATION to TRD */
		LLP(("TR_LLI: A_ABRT1:issuing close station\n"));
		if (!tr_cstn(cbptr))
		    {
		    cmn_err(CE_NOTE, "DLPI_FSM: bad internal CLOSE issued\n");
		    goto skip_fsm;		/* avoids state change */
		    }
		cbptr->ip_msg = NULL;
	        }

	    break;
	    
	case A_ABRT2:				/* Streams CLOSE - active */
	    /*
	     * A CLOSE has been issued on an opening SAP, and its state
	     * dictates that a RESET is needed.
	     * Issue LLC.RESET to TRD on this station.
	     * NOTE: Because an individual SAP is being opened, there
	     * will not be any associated link stations to close.
	     */
	    {
	    SCB				stn_scb;  /* build cmd in this */

	    if (cbptr->mgt_sap_flag)
		{
		LLP(("A_ABRT2: management sap, cbptr=%x\n",cbptr));
		if (cbptr->ip_msg)
		    {
		    mblk_trace(3,3, cbptr->ip_msg);
		    freemsg(cbptr->ip_msg);
		    }
	    
		for (i = 0; i < 4; i++)
		    {
	    	    kmem_free(mgt_lcb[i], sizeof(LCB));
		    mgt_lcb[i] = NULL;
		    }
		LLP(("A_ABRT2: Warning - Management CN closed\n"));
		break;
	        }

	    LLP(("A_ABRT2: cbptr=%x: issuing LLC_RESET command\n",cbptr));
	    for (i=0; i<MAX_SAPS; i++)
		{
		LLP(("clearing sapid_2_sap[%d][%d]\n",cbptr->ppa,i));
		sapid_2_sap[cbptr->ppa][i] = 0;
		sapid_2_q[cbptr->ppa][i] = NULL;
		}
	    stn_scb.scb_cmd = TR_CMD_LLC_RESET;
	    stn_scb.scb_parm1 = (u_short) cbptr->sap_stnid;
	    stn_scb.scb_parm2 = 0;
		
	    /* send cmd to TRD */
	    if (!(tr_lli_output(cbptr->ppa, &stn_scb)))
	        {
		cmn_err(CE_NOTE, "DLPI_FSM: bad internal CLOSE issued\n");
		goto skip_fsm;		/* avoids state change */
		}
	    }

	    break;

	case A_ATTACH:				/* DL_ATTACH processing */
    	    dl_ptr = (void *) mp->b_rptr;
	    cbptr->ppa = ((dl_attach_req_t *) dl_ptr)->dl_ppa;
	    cbptr->ip_msg = NULL;
	    send_ok_ack(wq, mp, DL_ATTACH_REQ);	/* ok_ack to sender */
	    break;

	case A_ATTBAD:				/* bad PPA given in DL_ATTACH */
	    cbptr->err_prim = DL_ATTACH_REQ;
	    if (!send_error_ack(wq, mp, DL_BADPPA, 0)) 
		{
		goto skip_fsm;		/* no buffers - try later */
		}

	    break;

	case A_BADRSP:				/* send DL_ERROR_ACK up */
	    /*
	     * LCB has errno &/or syserr and err_prim already set.
	     */
	    if (!send_error_ack(wq, mp, cbptr->errno, cbptr->syserr))
		{
		goto skip_fsm;		/* no buffers - try later */
		}

	    break;

	case A_CNSRSP:				/* CONN.STN rsp - outcnt = 0 */
	    {
	    /*
	     * Issue DL_OK_ACK to user
	     * This is a response on the stream that is accepting the connection
	     */
	    /* REFERENCED */
	    SSB			*ssb_ptr;
	    	
    	    dl_ptr = (void *) mp->b_rptr;
	    ssb_ptr = (SSB *) (((LLI_CB *) dl_ptr)->param_blk);
	    ASSERT(ssb_ptr->ssb_stn_id == cbptr->stn_id); /* LCB stn set ok? */
	    send_ok_ack(wq, mp, DL_CONNECT_RES);
	    outcnt--;

	    }
	    break;

	case A_CNSRS2:				/* CONN.STN rsp on MGT cn */
	    {
	    /*
	     * A CONN.STN rsp has been queued to the MGT cn because the token
	     * field in the real CN LCB is set, indicating an unsolicited
	     * connection is occurring. 2 events need to be executed here:
	     * 1. DL_OK_ACK on this (MGT) stream.
	     * 2. PASS_CON on the new connection (from the SSB).
	    SSB			*ssb_ptr; XXX not used
	     */
	
    	    dl_ptr = (void *) mp->b_rptr;
	    send_ok_ack(wq, mp, DL_CONNECT_RES);
	    outcnt--;

	    }
	    break;

	    

	case A_CNSTN:				/* DL_CONNECT_REQ processing */
	    /*
	     * dl_dest_addr_.. fields are useless - all data is predetermined
	     * by the Stream the request comes on.
	     */
	    {
	    TR_CONNECT_STN_BLK   	cstn_blk; /* build CONN.STATION here */
	    SCB				stn_scb;  /* build cmd in this */
	    
	    stn_scb.scb_cmd = TR_CMD_CONNECT_STATION;
	    stn_scb.scb_parm1 = (u_short) ((__psunsigned_t)&cstn_blk >> 16);
	    stn_scb.scb_parm2 = (u_short) ((__psunsigned_t)&cstn_blk & 0xffff);
	    cstn_blk.station_id = cbptr->stn_id;
	    if (cbptr->src_ri[0])
		{
	    	cstn_blk.routing_info_high = (__psunsigned_t)(cbptr->src_ri) >> 16;
	    	cstn_blk.routing_info_low  = (__psunsigned_t)(cbptr->src_ri) & 0xffff;
		}
	    else
		{
	    	cstn_blk.routing_info_high = 0;
	    	cstn_blk.routing_info_low  = 0;
		}
		
	    /* send cmd to TRD */
	    if (!(tr_lli_output(cbptr->ppa, &stn_scb)))
	        {
		cbptr->err_prim = DL_CONNECT_REQ;
	        send_error_ack(wq, mp, DL_BADPRIM, 0); /* always uses ip_msg */
	        goto skip_fsm;		/* avoids state change */
		}

	    }

	    mblk_trace(3,4, cbptr->ip_msg);
	    freemsg(cbptr->ip_msg);
	    cbptr->ip_msg = NULL;

	    break;

	case A_CONIND:				/* process Connect indicn */
	    /* 
	     * Code assumes that mp contains an ICB structure. 
	     * Issue DL_CONNECT_IND to user.
	     * ???????? need proof that have an ICB here!
	     */
	    {
	    LLI_CB		*cmd_ptr;
	    mblk_t		*em_ptr;
	    TR_ICB		*icb_ptr;
	    typedef struct {
		u_short			rsap;
		u_short			stn_id_s;
	    } RSTN_ADDR;		/* remote stn address format */

	    RSTN_ADDR			*addr_ptr;

	    cmd_ptr = (LLI_CB *) mp->b_rptr;
	    icb_ptr = (TR_ICB *) cmd_ptr->param_blk;

	    LLP(("A_CONIND: mp=%x,cmd_ptr=%x,icb_ptr=%x\n",
		mp, cmd_ptr, icb_ptr));
	    /* get blk for CONNECT_IND (definitely wont fit ICB etc ) */
	    i = sizeof(dl_connect_ind_t) + sizeof(RSTN_ADDR);
	    if ((em_ptr = (mblk_t *)allocb(i, BPRI_MED)) == (mblk_t *)NULL)
	    	{			/* no buffers - use bufcall() */
	    	if (bufcall(i,
			    BPRI_MED,
			    prcs_bufc,
			    (long)cbptr) == 0)/* param is LCB ptr */
		    {		/* call failed - just send error_ack */
		    cmn_err(CE_NOTE, "DLPI_FSM: bufcall() failure\n");
    		    dl_ptr = (void *) mp->b_rptr;
    		    dl_ptr->dl_primitive = DL_ERROR_ACK;
  	    	    mblk_trace(2,610,mp);
    		    qreply(wq, mp);
		    cbptr->ip_msg = NULL;
		    goto skip_fsm;
		    }
	    	goto skip_fsm;	/* so caller will exit FSM w/o st chg */
	        }

	    mblk_trace(0,204, em_ptr);
	    if (cbptr->mgt_sap_flag)
		{
	    	/* save mac address for later - canonical format? */
		LLP(("A_CONIND: management sap\n"));
		ASSERT(cbptr->mac_addr[0] == NULL);/* only 1 at a time on MGT cn */
	    	for (i = 0; i < 6; i++)
		    cbptr->mac_addr[i] = icb_ptr->remote_address[i];
		}

	    /* build DL_CONNECT_IND block */
	    tr_build_hdr(em_ptr, DL_CONNECT_IND, i);
	    dl_ptr = (void *)em_ptr->b_rptr;
    	    ((dl_connect_ind_t *) dl_ptr)->dl_called_addr_offset = 0;
    	    ((dl_connect_ind_t *) dl_ptr)->dl_called_addr_length = 0; 
    	    ((dl_connect_ind_t *) dl_ptr)->dl_calling_addr_offset 
					= sizeof(dl_connect_ind_t);
    	    ((dl_connect_ind_t *) dl_ptr)->dl_calling_addr_length 
					= sizeof(RSTN_ADDR);
    	    addr_ptr = (RSTN_ADDR *)((dl_connect_ind_t *) dl_ptr + 1);
	    addr_ptr->rsap = icb_ptr->remote_sap;
	    addr_ptr->stn_id_s = icb_ptr->station_id_s;
	    LLP(("A_CONIND: icb station_id_s=%x\n", icb_ptr->station_id_s));

    	    ((dl_connect_ind_t *) dl_ptr)->dl_correlation  =
				((u_int)cmd_ptr->port << 16) + 
			 	 icb_ptr->station_id_s;
	    outcnt++;			/* bump o/s indications count */
		
  	    mblk_trace(2,610,em_ptr);
    	    qreply(wq, em_ptr);
	    mblk_trace(3,5, mp);
	    freemsg(mp);
    	    cbptr->ip_msg = NULL;
	    }
		
	    break;
	    
	case A_CONCON:				/* process CONN.STN rsp OK */
	    /*
	     * 2 possible sources - 1. SABME_RCVD  2. CONN.STN rsp
	     * Issue DL_CONNECT_CON to user.
	     */
	    {
	    mblk_t		*em_ptr;
	    u_short		*jj;

	    ASSERT(cbptr->mgt_sap_flag == NULL);
	    i = sizeof(dl_connect_con_t) + sizeof(short);
	    if (i > (mp->b_datap->db_lim - mp->b_datap->db_base))
		{
	        if ((em_ptr = (mblk_t *)allocb(i, BPRI_MED)) == (mblk_t *)NULL) 
	    	    {			/* no buffers - use bufcall() */
	    	    if (bufcall(i,
		    		BPRI_MED,
				prcs_bufc,
				(long)cbptr) == 0)/* param is LCB ptr */
		    	{		/* call failed - just send error_ack */
		    	cmn_err(CE_NOTE, "DLPI_FSM: bufcall() failure\n");
    		    	dl_ptr = (void *) mp->b_rptr;
    		    	dl_ptr->dl_primitive = DL_ERROR_ACK;
  	    		mblk_trace(2,610,mp);
    		    	qreply(wq, mp);
		    	cbptr->ip_msg = NULL;
			goto skip_fsm;
		        }
	    	    goto skip_fsm;	/* so caller will exit FSM w/o st chg */
	            }
	    	    mblk_trace(0,205, em_ptr);
		}			/* rcvd msg not big enough */
    	    else
		{			/* recvd msg is big enough */
		em_ptr = mp;
    		ASSERT(mp->b_next == NULL);
    		ASSERT(unlinkb(mp) == NULL); 	/* only 1 block expected */
		}
	    tr_build_hdr(em_ptr, DL_CONNECT_CON, i);
    	    dl_ptr = (void *) em_ptr->b_rptr;
    	    ((dl_connect_con_t *) dl_ptr)->dl_resp_addr_offset 
					= sizeof(dl_connect_con_t);
    	    ((dl_connect_con_t *) dl_ptr)->dl_resp_addr_length = 2; 
    	    jj = (u_short *)((dl_connect_con_t *) dl_ptr + 1);
    	    *jj = (u_short) cbptr->stn_id;
  	    mblk_trace(2,611,em_ptr);
    	    qreply(wq, em_ptr);
    	    if (em_ptr != mp)
		{
	        mblk_trace(3,6, mp);
		freemsg(mp);
		}
    	    cbptr->ip_msg = NULL;
	    }
		
	    break;
	    
	case A_CONSTN:				/* DL_CONNECT_RES processing */
	    /*
	     * Issue CONNECT.STATION on stn in dl_correlation field.
	     */
	    {
	    TR_CONNECT_STN_BLK   	cstn_blk; /* build CONN.STATION here */
	    u_int			i, j;
	    u_short			stn_id;
	    SCB				stn_scb;  /* build cmd in this */
	    
    	    dl_ptr = (void *) mp->b_rptr;
	    stn_scb.scb_cmd = TR_CMD_CONNECT_STATION;
	    stn_scb.scb_parm1 = (u_short) ((__psunsigned_t)&cstn_blk >> 16);
	    stn_scb.scb_parm2 = (u_short) ((__psunsigned_t)&cstn_blk & 0xffff);
	    stn_id = ((dl_connect_res_t *) dl_ptr)->dl_correlation & 0xffff;
	    cstn_blk.station_id = stn_id;

	    /*
	     * MGT cn should have no RI info, tho it may have a remote addr
	     * of a previous Connect Indication passed up on this MGT cn.
	     */
	    if (cbptr->src_ri[0])
		{
		ASSERT(cbptr->mgt_sap_flag == 0);
	    	cstn_blk.routing_info_high = ((__psunsigned_t)cbptr->src_ri) >> 16;
	    	cstn_blk.routing_info_low  = ((__psunsigned_t)cbptr->src_ri) & 0xffff;
		}
	    else
		{
	    	cstn_blk.routing_info_high = 0;
	    	cstn_blk.routing_info_low  = 0;
		}
		
	    /*
	     * If dl_resp_token set, set queue_t in stn_tbl (must be MGT cn).
	     * Also move remote_address from MGT cn to new LCB.
	     */
	    if ((j = ((dl_connect_res_t *) dl_ptr)->dl_resp_token))
		{				/* token is CN stream mdev */
		LCB		*ncbptr;

		ncbptr = (LCB *)(mdev_tbl[j])->q_ptr;
		ASSERT(cbptr->mgt_sap_flag);
		ASSERT(ncbptr != NULL);
		ASSERT(ncbptr->token != 0);
    		i = (stn_id >> 8) - 1;		/* get SAP_ID -> table offset */
    		stn_id &= 0xff;			/* get STATION_ID -> offset */
    		stn_id--;	
    		i = (i * MAX_STNS) + stn_id;	/* i= offset in STN_TBL entry */
    		ASSERT(stn_tbl[ncbptr->ppa].WR_PTR(i) == NULL);
    		stn_tbl[cbptr->ppa].WR_PTR(i) = mdev_tbl[j];
#ifndef FRAME_ON_SAP
    		stn_tbl[cbptr->ppa].MACPTR(i) = ncbptr->mac_addr; /* ptr to MAC */
#endif /* FRAME_ON_SAP */
	    	for (i = 0; i < 6; i++)
		    {
		    ncbptr->mac_addr[i] = cbptr->mac_addr[i];
		    cbptr->mac_addr[i] = NULL;
		    }
		}
		
	    /* send cmd to TRD */
	    if (!(tr_lli_output(
			((dl_connect_res_t *) dl_ptr)->dl_correlation >> 16,
			&stn_scb)))
	        {
		cbptr->err_prim = DL_CONNECT_RES;
	        send_error_ack(wq, mp, DL_BADPRIM, 0); /* always uses ip_msg */
	        goto skip_fsm;		/* avoids state change */
		}

	    mblk_trace(3,7, cbptr->ip_msg);
	    freemsg(cbptr->ip_msg);
	    cbptr->ip_msg = NULL;
	    }

	    break;

	case A_CSAP:				/* DL_UNBIND_REQ processing */

	    if (cbptr->mgt_sap_flag == 0)
		{
	    	SCB	        sap_scb;	/* build cmd in this */

	    	/* format CLOSE.SAP to TRD */
	    	sap_scb.scb_cmd = TR_CMD_CLOSE_SAP;
	    	sap_scb.scb_parm1 = cbptr->sap_stnid;
	    	sap_scb.scb_parm2 = 0;
	    	if (!(tr_lli_output(cbptr->ppa, &sap_scb))) /* cmd error */
	            {
		    cbptr->err_prim = DL_UNBIND_REQ;
	            send_error_ack(wq, mp, DL_BADPRIM, 0); /* uses ip_msg */
	            goto skip_fsm;		/* avoids state change */
		    }
		mblk_trace(3,8, cbptr->ip_msg);
		freemsg(cbptr->ip_msg);
		cbptr->ip_msg = NULL;
		break;
		}

	    /* MGT SAP - force state change & fall thru to Close SAP rsp */
	    nextState = S_UNBND;		/* ????? maybe event later */
	    /* goto A_CSAPRS; ... fall thru */

	case A_CSAPRS:				/* Close SAP successful */
	    /* send DL_OK_ACK to user */

	    putctl1((RD(wq))->q_next, M_FLUSH, FLUSHRW); /* as per DLPI spec */
	    send_ok_ack(wq, mp, DL_UNBIND_REQ);	/* ok_ack to user */
	    if (cbptr->mgt_sap_flag == 0)
		{
		int i;
		i = (cbptr->sap_stnid >> 8) - 1; /* get SAP_ID -> table offset */
		LLP(("CLOSE_SAP OK:sapid_2_q=%x,sapid_2_sap=%x,ppa=%d,i=%d\n",
			sapid_2_q,sapid_2_sap,cbptr->ppa,i));
		if (i>=0)
		    {
		    sapid_2_sap[cbptr->ppa][i] = 0;
		    sapid_2_stn[cbptr->ppa][i] = 0;
		    sapid_2_q[cbptr->ppa][i]   = NULL;
		    }
	        }
	    else
		{
		for (i = 0; i < 4; i++)
		    {
		    if (i > 0)
	    	    	kmem_free(mgt_lcb[i], sizeof(LCB));
		    mgt_lcb[i] = NULL;
		    }
		LLP(("CLOSE_SAP OK: Management SAP lcb=%x\n", &mgt_lcb[0]));
		}
	    cbptr->ssap_id = cbptr->dsap_id = cbptr->sap_stnid = 0;
	    cbptr->max_conind = cbptr->sap_stnid = cbptr->token = 0;
	    cbptr->src_ri[0] = 0;
	    cbptr->ifr_first = cbptr->ifr_last = 0;
	    cbptr->sap_stnid = cbptr->mgt_sap_flag = 0;
	    cbptr->ip_msg = NULL;

	    break;

	case A_CSTN:				/* DL_SUBS_UNBIND_REQ prcsg */
	    if (cbptr->mgt_sap_flag == 0)
		{
	    	/* format CLOSE.STATION to TRD */
		if (!tr_cstn(cbptr)) 
		    {
	            goto skip_fsm;		/* avoids state change */
		    }
		mblk_trace(3,9, cbptr->ip_msg);
		freemsg(cbptr->ip_msg);
		cbptr->ip_msg = NULL;
		break;
		}

	    /* MGT SAP - force state change & fall thru to Close Stn rsp */
	    nextState = S_UNBND;		/* ????? maybe event later */
	    goto L_CSTNRS;

	case A_CSTN2:	    	    	    	/* DISC ind , outcnt > 1 */
	    /* Decrement outcnt */
	    outcnt--;
	    /* goto A_CSTN1; ... fall thru */

	case A_CSTN1:	    	    	    	/* DISC ind , outcnt = 1 */
	    /*
	     * Issue DL_DISCONNECT_IND to user.
	     */
	    LLP(("tr_lli: Received DISCONNECT_IND\n"));
	    tr_discind(cbptr);
	    
    	    /* clear approp LCB fields */
	    cbptr->correln = cbptr->token = 0;
	    cbptr->ifr_first = cbptr->ifr_last = 0;
	    /* return mblks for these */
	    tr_free_iframes(cbptr);

	    break;
	    
	case A_CSTNRS:				/* Close Stn successful */
	  L_CSTNRS:
	    /* send DL_OK_ACK to user */
	    send_ok_ack(wq, mp, DL_SUBS_UNBIND_REQ); /* ok_ack to user */
	    cbptr->stn_id = 0;
	    cbptr->ip_msg = NULL;

	    break;

	case A_CSTRS1:				/* Close Stn OK - Discon Pnd */
	    /* Decrement outcnt, then process as for normal CLOSE.STATION */
	    ASSERT(outcnt != 0);
	    outcnt--;
	    /* goto A_CSTNRS; */
	    goto L_CSTNRS;

	case A_DIND:				/* process rcvd data */	    	    	    	
	    /*
	     * Generate DL_DATA_IND to user.
	     * Input blk consists of LLI_CB, then ICB, then data.
	     * Simply adjust b_rptr to 1st byte of data beyond the MAC hdr.
	     */
	    {
	    TR_MAC_HDR		*fr_ptr;

	    ASSERT(cbptr->mgt_sap_flag == NULL);

	    LLP(("A_DIND: cbptr=%x, mp=%x\n",cbptr,mp));
	    /* point to returned frame */
	    fr_ptr=(TR_MAC_HDR *)(mp->b_rptr + sizeof(LLI_CB) + sizeof(TR_ICB));
    	    mp->b_datap->db_type = M_DATA;
	    mp->b_rptr = (unsigned char *)((__psunsigned_t)fr_ptr + 
		(u_int)sizeof(TR_MAC_HDR)) - TR_MAC_RIF_SIZE;
#ifdef RIFDBG
	    printf("A_DIND: fr_ptr=%x, b_rptr=%x\n",fr_ptr,mp->b_rptr);
	    fv_hexdump(fr_ptr, 32);
	    fv_hexdump(mp->b_rptr, 12);
#endif
	    if ((fr_ptr->t_mac_sa[0] & 0x80) != 0) /* frame has RI */
		mp->b_rptr += cbptr->rif_len;
  	    mblk_trace(2,612,mp);
    	    qreply(wq, mp);
    	    cbptr->ip_msg = NULL;
	    }
		
	    break;
	    
	case A_DREQ:	    	    	    	/* Xmit an I-frame */
	    /*
	     * MAC header is created by the system (LLC hdr also).
	     */
	    {
	    SCB			stn_scb;  	/* build cmd in this */
	    TR_XMIT_PARMS	xmit_parms;	/* set xmit params here */
	    
	    ASSERT(cbptr->mgt_sap_flag == NULL);
	    stn_scb.scb_cmd = TR_CMD_TRANSMIT;
	    stn_scb.scb_parm1 = (u_short) ((__psunsigned_t)&xmit_parms >> 16);
	    stn_scb.scb_parm2 = (u_short) ((__psunsigned_t)&xmit_parms & 0xffff);

	    xmit_parms.xmit_data  = mp;
	    xmit_parms.frame_size = mp->b_wptr - mp->b_rptr;
	    xmit_parms.station_id = cbptr->stn_id;
	    xmit_parms.remote_sap = cbptr->ssap_id;
	    xmit_parms.frame_type = TR_FTYPE_I_FRAME;

	    /* send cmd to TRD */
#ifdef IFRMDBG
	    {
	    char *cp;
	    cp=(char*)xmit_parms.xmit_data->b_rptr;
	    if (cp[0]==0 && cp[1]==0 && cp[2]==0 && cp[3]==0)
		printf("tr_lli: Bad I-Frames\n");
	    }
#endif
	    if (cbptr->ifr_state == IFR_STATE_WAIT)
		{
		LLP(("tr_lli: I-Frame State == Wait\n"));
		noenable(wq);
		putbq(wq,mp);
		cbptr->ip_msg = NULL;
		LLP(("tr_lli: wq=%x, mp=%x\n", wq, mp));
		if (lli_timeout == 0) 
		    {
		    lli_timeout = STREAMS_TIMEOUT(lli_wtime,wq,50);
		    }
	        goto skip_fsm;		/* avoids state change */
		}
	    else if (!(tr_lli_output(cbptr->ppa, &stn_scb)))
	        {
	        mblk_trace(3,10, mp);
		LLP(("tr_lli: can not send I-Frames\n"));
		noenable(wq);
		putbq(wq,mp);
		cbptr->ip_msg = NULL;
		LLP(("tr_lli: wq=%x, mp=%x\n", wq, mp));
		if (lli_timeout == 0) 
		    {
		    lli_timeout = STREAMS_TIMEOUT(lli_wtime,wq,50);
		    }
	        goto skip_fsm;		/* avoids state change */
		}
	    else
		{
		cbptr->ip_msg = NULL;
		LLP(("tr_lli:send I-Frames %d\n", xmit_parms.frame_size));
		}

	    }
	    break;

	case A_DROPIT:	    	    	    	/* drop XMIT-request frames */
	    mblk_trace(3,11, mp);
	    freemsg(mp);
	    cbptr->ip_msg = NULL;

	    break;
	    
	case A_DTTCH:				/* DL_DETTACH processing */
	    send_ok_ack(wq, mp, DL_DETACH_REQ); /* ok_ack to user */
	    cbptr->ip_msg = NULL;
	    cbptr->ppa = 0;
	    cbptr->stn_id = 0;
	    cbptr->mgt_sap_flag = 0;
	    
	    break;
                    
        case A_NOOP:                            /* do nothing */
            break;

	case A_OSAPRS:				/* process OPEN.SAP OK rsp */
	    /* Set STN_ID in approp sapid table */
	    {
	    LLI_CB		*cmd_ptr;
	    SSB			*ssb_ptr;

    	    dl_ptr = (void *) mp->b_rptr;
	    cmd_ptr = (LLI_CB *) dl_ptr;
	    ssb_ptr = (SSB *) cmd_ptr->param_blk;

	    i = (ssb_ptr->ssb_stn_id >> 8) - 1;
	    ASSERT(i < MAX_SAPS);
	    sapid_2_stn[cmd_ptr->port][i] = ssb_ptr->ssb_stn_id;
	    cbptr->sap_stnid = ssb_ptr->ssb_stn_id;
	    ASSERT(sapid_2_sap[cmd_ptr->port][i] == cbptr->ssap_id);
	    if (!bind_ack_rply(wq, mp))		/* format & send BIND_ACK */
		{
		goto skip_fsm;		/* no buffers - try later */
		}

	    }
	    break;
	
	case A_OOSAP:				/* process existing OPEN.SAP */
	    /*
	     * Update LCB fields and send ok_ack to caller
	     */

    	    dl_ptr = (void *) mp->b_rptr;
	    if (((dl_bind_req_t *) dl_ptr)->dl_conn_mgmt) /* is MGT cn */
		{
		LCB		*ncbptr;

	        LLP(("SAP is management sap\n"));
		mgt_lcb[0] = cbptr;
		for (i = 0; i < 4; i++)
		    {
		    if (i > 0)
			{
		    	if ((ncbptr = kmem_zalloc(sizeof(LCB), KM_NOSLEEP))
								 == NULL)
			    {
			    cmn_err(CE_NOTE,"DLPI_FSM: no KMEM for LLI\n");
            		    putctl1(RD(wq)->q_next, M_ERROR, ENOSR);
			    goto skip_fsm;
			    }
		 	ncbptr->qptr   = cbptr->qptr;
		 	ncbptr->ctrig  = cbptr->ctrig;
		 	ncbptr->pstate = currState;
		 	ncbptr->ptrig  = currEvent;
		 	ncbptr->cstate = nextState;
		 	ncbptr->mdev   = cbptr->mdev;
		    	mgt_lcb[i]     = ncbptr;
			}	/* if (i > 0) */
		    else
			ncbptr = cbptr;
		    ncbptr->ppa = i;
	    	    LLP(("OPEN_SAP: Setting PPA to %d, cbptr = %x\n", 
			ncbptr->ppa, ncbptr));
	    	    ncbptr->mgt_sap_flag = 1;
	    	    ncbptr->max_conind = ((dl_bind_req_t *) dl_ptr)->dl_max_conind;
	    	    ncbptr->ssap_id = 0;
	    	    ncbptr->sap_stnid = 0;
		    ncbptr->stn_id = 0;
	 	    for (j=0; j < MAX_SAPS; j++)
			{
			sapid_2_sap[i][j]=0;
			sapid_2_stn[i][j]=0;
			sapid_2_q[i][j]=NULL;
			}
		    }			/* for (i = 0; ) .. */

	    	if(!bind_ack_rply(wq, mp))
		    {
		    for (i = 1; i < 4; i++)
			kmem_free(mgt_lcb[i], sizeof(LCB));
		    goto skip_fsm;		/* no buffers - try later */
		    }
		}
	    else
		{
	        LLP(("SAP is not management sap\n"));
	    	cbptr->max_conind = 1;
	    	cbptr->ssap_id = ((dl_bind_req_t *) dl_ptr)->dl_sap;

	    	/* find SAP in sapid table */
	    	for (i = 0; i < MAX_SAPS; i++)
	            {
	            if (sapid_2_sap[cbptr->ppa][i] == cbptr->ssap_id)
		    	break;
	            }
	    	ASSERT(i < MAX_SAPS);
	    	cbptr->sap_stnid = sapid_2_stn[cbptr->ppa][i];
	    	ASSERT((cbptr->sap_stnid >> 8) == i + 1); /* SAP_ID correct? */
	    	if(!bind_ack_rply(wq, mp))
		    {
		    goto skip_fsm;		/* no buffers - try later */
		    }
		}
	    
	    break;

	case A_OSAP:				/* process new OPEN.SAP */
	    /*
	     * Store Sap value into next sapid table entry, and wq address
	     * into sapid_2_q so can find WR queue when sap response arrives. 
	     */
	    {
	    TR_OPEN_SAP_BLOCK	osap_blk;	/* build param block in this */
	    SCB		        sap_scb;	/* build cmd in this */

    	    dl_ptr = (void *) mp->b_rptr;
	    for (i = 1; i < MAX_SAPS; i++)
	        {
	        if (sapid_2_sap[cbptr->ppa][i] == 0)
		    break;			/* next available entry */
	        }
	    LLP(("OPEN_SAP: ppa=%d, i=%d, dl_sap=%x, wq=%x\n",
			cbptr->ppa, i, ((dl_bind_req_t *)dl_ptr)->dl_sap, wq));
	    LLP(("\tsapid_2_sap=%x, sapid_2_q=%x\n",sapid_2_sap,sapid_2_q));
	    sapid_2_sap[cbptr->ppa][i] = ((dl_bind_req_t *) dl_ptr)->dl_sap;
	    ASSERT(sapid_2_q[cbptr->ppa][i] == NULL);
	    sapid_2_q[cbptr->ppa][i] = wq;
	    
	    /* format OPEN.SAP to TRD */
	    sap_scb.scb_cmd = TR_CMD_OPEN_SAP;
	    sap_scb.scb_parm1 = (u_short) ((__psunsigned_t)&osap_blk >> 16);
	    sap_scb.scb_parm2 = (u_short) ((__psunsigned_t)&osap_blk & 0xffff);
	    osap_blk.timer_t1 = osap_blk.timer_t2 = osap_blk.timer_ti = 0;
	    osap_blk.maxin = osap_blk.maxout_incr =osap_blk.max_retry_count = 0;
	    osap_blk.sap_value = ((dl_bind_req_t *) dl_ptr)->dl_sap;
	    osap_blk.station_count = MAX_STNS;
	    osap_blk.max_i_field = 0;		/* (600 dflt) ????? set this? */
	    osap_blk.maxout = 7;
	    osap_blk.group_member_count = osap_blk.gsap_max_number = 0;
	    osap_blk.gsap_list_ptr_hi = osap_blk.gsap_list_ptr_lo = 0;
	    osap_blk.sap_options = TR_INDIVIDUAL_SAP;
	    if (!(((dl_bind_req_t *) dl_ptr)->dl_xidtest_flg & DL_AUTO_XID))
 	        osap_blk.sap_options |= TR_XID_HANDLER;
		
	    if (!(tr_lli_output(cbptr->ppa, &sap_scb)))
	        {
		cbptr->err_prim = DL_BIND_REQ;
	        send_error_ack(wq, mp, DL_BADPRIM, 0); /* always uses ip_msg */
	        sapid_2_sap[cbptr->ppa][i] = 0;
	        sapid_2_q[cbptr->ppa][i] = NULL;
	        goto skip_fsm;		/* avoids state change */
		}

	    /* save fields for when reply from TRD arrives */
	    cbptr->ssap_id = ((dl_bind_req_t *) dl_ptr)->dl_sap;
	    cbptr->max_conind = ((dl_bind_req_t *) dl_ptr)->dl_max_conind;
	    mblk_trace(3,12, mp);
	    freemsg(mp);
	    cbptr->ip_msg = NULL;
	    }

	    break;
	
	case A_OSTN:				/* process OPEN.STATION */
	    /* dl_subs_sap_offset & _length point to DSAP + MAC address. */
	    {
	    TR_OPEN_STATION_BLOCK	ostn_blk; /* build OPEN.STATION here */
	    SCB				stn_scb;  /* build cmd in this */
	    typedef struct 
		{
		u_short			max_i_fld;
		u_char 			rsap;
		char			mac[6];
		u_char			t1;
		u_char			t2;
		u_char			ti;
		} RSTN_ADDR;		/* remote stn address format */

	    RSTN_ADDR			*addr_ptr;
	    
    	    dl_ptr = (void *) mp->b_rptr;
	    stn_scb.scb_cmd = TR_CMD_OPEN_STATION;
	    stn_scb.scb_parm1 = (u_short) ((__psunsigned_t)&ostn_blk >> 16);
	    stn_scb.scb_parm2 = (u_short) ((__psunsigned_t)&ostn_blk & 0xffff);
	    ostn_blk.station_id = cbptr->sap_stnid;
	    ostn_blk.maxout = ostn_blk.maxin = ostn_blk.maxout_incr = 0;
	    ostn_blk.station_options = 0;
	    addr_ptr = (RSTN_ADDR *) ((__psunsigned_t)dl_ptr + (__psunsigned_t)
			(((dl_subs_bind_req_t *)dl_ptr)->dl_subs_sap_offset));
	    ASSERT(sizeof(RSTN_ADDR) == ((dl_subs_bind_req_t *)dl_ptr)->dl_subs_sap_len);
	    ostn_blk.rsap_value = (u_short) (addr_ptr->rsap);
	    ostn_blk.max_i_field = addr_ptr->max_i_fld;
	    ostn_blk.timer_t1 = addr_ptr->t1;
	    ostn_blk.timer_t2 = addr_ptr->t2;
	    ostn_blk.timer_ti = addr_ptr->ti;
	    ostn_blk.rnode_addr_high = ((__psunsigned_t)addr_ptr->mac) >> 16;
	    ostn_blk.rnode_addr_low  = ((__psunsigned_t)addr_ptr->mac) & 0xffff;

	    /* save mac address for later ??????? canonical format? */
	    cbptr->dsap_id = (u_short)(addr_ptr->rsap);
	    for (i = 0; i < 6; i++)
		cbptr->mac_addr[i] = addr_ptr->mac[i];

	    /* send cmd to TRD */
	    if (!(tr_lli_output(cbptr->ppa, &stn_scb)))
	        {
		cbptr->err_prim = DL_SUBS_BIND_REQ;
	        send_error_ack(wq, mp, DL_BADPRIM, 0); /* always uses ip_msg */
	        goto skip_fsm;		/* avoids state change */
		}

	    mblk_trace(3,13, cbptr->ip_msg);
	    freemsg(cbptr->ip_msg);
	    cbptr->ip_msg = NULL;
	    }

	    break;

	case A_OSTNRS:
	    /*
	     * Format and send DL_SUBS_BIND_ACK to user. For MGT SAP, there
	     * is no response block from the TRD, just the DL_SUBS_BIND_REQ
	     * from the user.
	     */
	    {
	    LLI_CB		*cmd_ptr;
	    mblk_t		*em_ptr;
	    u_short		*jj;
	    SSB			*ssb_ptr;
	    u_short		stn_id;

	    cmd_ptr = (LLI_CB *) mp->b_rptr;
	    ssb_ptr = (SSB *) cmd_ptr->param_blk;

	    i = sizeof(dl_subs_bind_ack_t) + sizeof(short);
	    if (i > (mp->b_datap->db_lim - mp->b_datap->db_base))
		{
	        if ((em_ptr = (mblk_t *)allocb(i, BPRI_MED)) == (mblk_t *)NULL)
	    	    {			/* no buffers - use bufcall() */
	    	    if (bufcall(i,
		    		BPRI_MED,
				prcs_bufc,
				(long)cbptr) == 0)/* param is LCB ptr */
		    	{		/* call failed - just send error_ack */
		    	cmn_err(CE_NOTE, "DLPI_FSM: bufcall() failure\n");
    		    	dl_ptr = (void *) mp->b_rptr;
    		    	dl_ptr->dl_primitive = DL_ERROR_ACK;
  	    		mblk_trace(2,613,mp);
    		    	qreply(wq, mp);
		    	cbptr->ip_msg = NULL;
			goto skip_fsm;
		        }
	    	    goto skip_fsm;	/* so caller will exit FSM w/o st chg */
	            }
	    	    mblk_trace(0,206, em_ptr);
		}			/* rcvd msg not big enough */
    	    else
		{			/* recvd msg is big enough */
		em_ptr = mp;
    		ASSERT(mp->b_next == NULL);
    		ASSERT(unlinkb(mp) == NULL); 	/* only 1 block expected */
		}
    	    em_ptr->b_datap->db_type = M_PCPROTO;
    	    em_ptr->b_rptr = em_ptr->b_datap->db_base;
    	    em_ptr->b_wptr = em_ptr->b_rptr + i;
    	    dl_ptr = (void *) em_ptr->b_rptr;
    	    dl_ptr->dl_primitive = DL_SUBS_BIND_ACK;
	    if (cbptr->mgt_sap_flag)		/* no SSB... */
		{
    	    	((dl_subs_bind_ack_t *) dl_ptr)->dl_subs_sap_offset = 0; 
    	    	((dl_subs_bind_ack_t *) dl_ptr)->dl_subs_sap_len = 0; 
		}
	    else
		{
	    	stn_id = cbptr->stn_id = ssb_ptr->ssb_stn_id;

		/* update stn_tbl for this station */
    		i = (stn_id >> 8) - 1;		/* get SAP_ID -> table offset */
    		stn_id &= 0xff;			/* get STATION_ID -> offset */
    		stn_id--;
    		i = (i * MAX_STNS) + stn_id;	/* i= offset in STN_TBL entry */
    		stn_tbl[cbptr->ppa].WR_PTR(i) = wq;
		LLP(("WR_PTR ppa=%d, index=%d, ptr=%x\n", cbptr->ppa, i, wq));
#ifndef FRAME_ON_SAP
    		stn_tbl[cbptr->ppa].MACPTR(i) = cbptr->mac_addr;
#endif /* FRAME_ON_SAP */

    	    	((dl_subs_bind_ack_t *) dl_ptr)->dl_subs_sap_offset 
					= sizeof(dl_subs_bind_ack_t);
    	    	((dl_subs_bind_ack_t *) dl_ptr)->dl_subs_sap_len = 2; 
    	    	jj = (u_short *)((dl_subs_bind_ack_t *) dl_ptr + 1);
    	    	*jj = (u_short) cbptr->stn_id;
		}
  	    mblk_trace(2,614,em_ptr);
    	    qreply(wq, em_ptr);
    	    if (em_ptr != mp)
		{
	    	mblk_trace(3,14, mp);
		freemsg(mp);
		}
    	    cbptr->ip_msg = NULL;
	    }
		
	    break;
	    
	case A_PASSCN:				/* PASS_CON processing */
	    /*
	     * Connection is to be passed to the LCB indicated by cbptr.
	     * Input blk is an SSB response from the TRD.
	     * Action does not return the input mblk_t or send any response.
	     */
	    {
	    SSB			*ssb_ptr;

    	    dl_ptr = (void *) mp->b_rptr;
	    ssb_ptr = (SSB *) (((LLI_CB *) dl_ptr)->param_blk);
	    cbptr->stn_id = ssb_ptr->ssb_stn_id;
	    cbptr->token = 0;
	    cbptr->ip_msg = NULL;
	    }

	    break;

	case A_RSTRQ:				/* LLC.RESET processing */
	    /*
	     * Issue LLC.RESET to TRD on this station.
	     * Does not return input mblk.
	     */
	    {
	    SCB				stn_scb;  /* build cmd in this */

	    LLP(("TR_LLI: A_RSTRQ: issuing LLC_RESET command\n"));
	    ASSERT(cbptr->mgt_sap_flag == NULL);
	    stn_scb.scb_cmd = TR_CMD_LLC_RESET;
	    stn_scb.scb_parm1 = (u_short) cbptr->sap_stnid;
	    stn_scb.scb_parm2 = 0;
		
	    /* send cmd to TRD */
	    if (!(tr_lli_output(cbptr->ppa, &stn_scb)))
	        {
		cbptr->err_prim = DL_RESET_REQ;
	        send_error_ack(wq, mp, DL_BADPRIM, 0); /* always uses ip_msg */
		rc = Error;
	        goto skip_fsm;			/* = -1, avoids state change */
		}

	    rc = NoError;   	    	    	/* = 0 */
	    }

	    break;

	case A_RSTND:	    	    	    	/* process LOST_LINK ind */
	    /* Clear approp LCB fields and issue DL_DISCONNECT_IND to UI */

	    tr_discind(cbptr);	    	    	/* issue DISC ind */

    	    /* clear approp LCB fields */
	    cbptr->correln = cbptr->token = 0;
	    cbptr->ifr_first = cbptr->ifr_last = 0;
	    /* return mblks for these */
	    tr_free_iframes(cbptr);
	    
	    break;
	    
	case A_RSTRS:	    	    	    	/* process LLC.RESET rsp */
	    /* Clear approp LCB fields and issue DL_RESET_IND to UI */

	    {
	    mblk_t		*em_ptr;
	    u_int		i, k, stn_id;

	    /*
	     * get blk for RESET_IND (cannot use i/p block - needed for
	     * multiple calls).
    	     */
	    i = sizeof(dl_reset_res_t);
	    if ((em_ptr = (mblk_t *)allocb(i, BPRI_MED)) == (mblk_t *)NULL)
	    	{			/* no buffers - use bufcall() */
	    	if (bufcall(i,
			    BPRI_MED,
			    prcs_bufc,
			    (long)cbptr) == 0)/* param is LCB ptr */
		    {		/* call failed - just send error_ack */
		    cmn_err(CE_NOTE, "DLPI_FSM: bufcall() failure\n");
    		    dl_ptr = (void *) mp->b_rptr;
    		    dl_ptr->dl_primitive = DL_ERROR_ACK;
  	    	    mblk_trace(2,615,mp);
    		    qreply(wq, mp);
		    cbptr->ip_msg = NULL;
		    goto skip_fsm;
		    }
	    	goto skip_fsm;			/* so caller will exit FSM w/o st chg */
	        }
	    mblk_trace(0,207, em_ptr);
	    /* build DL_RESET_IND block */
	    tr_build_hdr(em_ptr, DL_RESET_RES, i);
    	    dl_ptr = (void *) em_ptr->b_rptr;
/*  	    ((dl_reset_ind_t *) dl_ptr)->dl_originator = DL_PROVIDER;    */
/*  	    ((dl_reset_ind_t *) dl_ptr)->dl_reason = DL_RESET_LINK_ERROR;*/
		
  	    mblk_trace(2,616,em_ptr);
    	    qreply(wq, em_ptr);

    	    /* clear approp LCB fields */
	    stn_id = cbptr->stn_id;
	    i = k = (stn_id >> 8) - 1;    /* get SAP_ID -> table offset */
	    stn_id &= 0xff;		  /* get STATION_ID -> offset */
	    stn_id--;
	    i = (i * MAX_STNS) + stn_id;  /* i = offset within STN_TBL entry */
	    stn_tbl[cbptr->ppa].WR_PTR(i) = NULL; /* clr stn_tbl entry */
#ifndef FRAME_ON_SAP
	    stn_tbl[cbptr->ppa].MACPTR(i) = NULL;
#endif /* FRAME_ON_SAP */
	    sapid_2_sap[cbptr->ppa][k] = 0;
	    sapid_2_stn[cbptr->ppa][k] = 0;
	    sapid_2_q[cbptr->ppa][k]   = NULL;
	    cbptr->ssap_id = cbptr->dsap_id = cbptr->stn_id = 0;
	    cbptr->max_conind = cbptr->sap_stnid = cbptr->token = 0;
	    cbptr->src_ri[0] = NULL;
	    /* return mblks for these */
	    freemsg(mp);
	    cbptr->ip_msg = NULL;
	    cbptr->ifr_first = cbptr->ifr_last = 0;
	    tr_free_iframes(cbptr);
	    
	    }
		
	    break;
	    
        case A_TESTRQ:
	    /*
	     * Create MAC header as follows:
	     * DA from LCB; SA from TRD; AC - default; FC - default
	     * RII - based on dl_flag value; RSAP = 0.
	     * If data is passed down to be sent, assumes room at begin 
	     * of buffer for MAC hddr.
	     */
	    {
	    mblk_t		*em_ptr;
	    TR_MAC_HDR		*fr_ptr;
	    SCB			stn_scb;  	/* build cmd in this */
	    TR_XMIT_PARMS	xmit_parms;	/* set xmit params here */
	    char		*sa_ptr;
	    
    	    dl_ptr = (void *) mp->b_rptr;
	    stn_scb.scb_cmd = TR_CMD_TRANSMIT;
	    stn_scb.scb_parm1 = (u_short) ((__psunsigned_t)&xmit_parms >> 16);
	    stn_scb.scb_parm2 = (u_short) ((__psunsigned_t)&xmit_parms & 0xffff);

	    /* look for passed-down data */
	    if ((em_ptr = unlinkb(mp)) == NULL)
		{				/* no data */
	    	if ((em_ptr = allocb(sizeof(TR_MAC_HDR), BPRI_MED)) == NULL)
	    	    {				/* no buffers - return error */
		    cmn_err(CE_NOTE, "DLPI_FSM: no buffers\n");
		    cbptr->err_prim = DL_TEST_REQ;
	            send_error_ack(wq, mp, DL_SYSERR, ENOSR);
		    goto skip_fsm;
		    }
		mblk_trace(0, 208, em_ptr);
    	        fr_ptr = (TR_MAC_HDR *) em_ptr->b_datap->db_base;
		em_ptr->b_rptr = (unsigned char *) fr_ptr;
		em_ptr->b_wptr = em_ptr->b_rptr + sizeof(TR_MAC_HDR);
	    	if (!((dl_test_req_t *) dl_ptr)->dl_flag) /* not set - no RI */
		    em_ptr->b_wptr -= TR_MAC_RIF_SIZE;
		}
	    else
		{				/* use passed block */
		/* ensure room for MAC hddr */
		ASSERT((em_ptr->b_rptr - em_ptr->b_datap->db_base) >= sizeof(TR_MAC_HDR));
    	    	fr_ptr = (TR_MAC_HDR *)((__psunsigned_t)em_ptr->b_rptr - sizeof(TR_MAC_HDR));
	    	if (!((dl_test_req_t *) dl_ptr)->dl_flag) /* not set - no RI */
		    fr_ptr += sizeof(TR_RII);
		em_ptr->b_rptr = (unsigned char *)fr_ptr;
		}

	    em_ptr->b_datap->db_type = M_DATA;
	    fr_ptr->t_mac_ac = TR_MAC_AC_PRIORITY;
	    fr_ptr->t_mac_fc = TR_MAC_FC_NON_MAC_CTRL;
	    sa_ptr = (char *)tr_get_sa(cbptr->ppa);
	    for (i = 0; i < 6; i++)
		{
		fr_ptr->t_mac_da[i] = cbptr->mac_addr[i];
	    	fr_ptr->t_mac_sa[i] = *sa_ptr++;
		}
	    xmit_parms.xmit_data  = em_ptr;
	    xmit_parms.frame_size = sizeof(TR_MAC_HDR) - TR_MAC_RIF_SIZE;
	    xmit_parms.station_id = (cbptr->stn_id & 0xff00);
	    xmit_parms.remote_sap = 0;
	    xmit_parms.frame_type = TR_FTYPE_TEST_COM;
	    if (((dl_test_req_t *) dl_ptr)->dl_flag) /* if set, send BC frame */
		{
		fr_ptr->t_mac_rii = TR_RIF_ALL_ROUTES | TR_RIF_LF_4472 | 0x0200;
		/* set SA bit 0 */
		fr_ptr->t_mac_sa[0] |= 0x80;
		xmit_parms.frame_size += TR_MAC_RIF_SIZE;
		*(u_short*)cbptr->src_ri = fr_ptr->t_mac_rii;
		}

#ifdef RIFDBG
	    fv_hexdump((char *)fr_ptr, em_ptr->b_wptr - em_ptr->b_rptr);
#endif

	    /* send cmd to TRD */
	    if (!(tr_lli_output(cbptr->ppa, &stn_scb)))
	        {
	    	mblk_trace(3,15, em_ptr);
	    	freemsg(em_ptr);
		cbptr->err_prim = DL_TEST_REQ;
	        send_error_ack(wq, mp, DL_BADPRIM, 0); /* always uses ip_msg */
	        goto skip_fsm;			/* avoids state change */
		}
	    else
		{
	    	mblk_trace(3,16, em_ptr);
		freemsg(em_ptr);
	    	mblk_trace(3,17, mp);
		freemsg(mp);
		cbptr->ip_msg = NULL;
		}
	    }

	    break;

        case A_TESTCN:				/* process TEST reply rcvd */
	    /*
	     * Generate DL_TEST_CON to user; save any Routing Info.
	     * Input blk consists of LLI_CB, then ICB, then TEST data.
	     * Use input block for M_DATA block and get M_PROTO block for
	     * primitive.
	     */
	    {
	    u_long 		*jj;
	    mblk_t		*em_ptr;

    LLP(("A_TESTCN: b_rptr=%x, b_base=%x\n",mp->b_rptr,mp->b_datap->db_base));
	    ASSERT(cbptr->mgt_sap_flag == NULL);
	    /* get a block for the DL_TEST_CON primitive */
	    i = sizeof(dl_test_con_t) + sizeof(long);
	    if ((em_ptr = tr_get_blk(cbptr, i)) == NULL)
		{
	    	goto skip_fsm;			/* so caller will exit FSM w/o st chg */
		}
	    
	    /* extract RI if exists */
	    HEXDUMP((mp->b_rptr+sizeof(LLI_CB)+sizeof(TR_ICB)), 32);
	    tr_update_ri(cbptr, mp);

	    /* set to point to MAC hdr in input block */
    	    mp->b_rptr += (sizeof(LLI_CB) + sizeof(TR_ICB));
	    mp->b_datap->db_type = M_DATA; 
	    
    	    ASSERT(mp->b_next == NULL);

	    /* build DL header in acquired block */
	    tr_build_hdr(em_ptr, DL_TEST_CON, i);
    	    dl_ptr = (void *) em_ptr->b_rptr;
    	    ((dl_test_con_t *) dl_ptr)->dl_src_addr_offset = 0;
    	    ((dl_test_con_t *) dl_ptr)->dl_src_addr_length = 0;
    	    ((dl_test_con_t *) dl_ptr)->dl_dest_addr_offset 
					= sizeof(dl_test_con_t);
    	    ((dl_test_con_t *) dl_ptr)->dl_dest_addr_length = sizeof(long);
    	    jj = (u_long *)((dl_test_con_t *) dl_ptr + 1);
    	    *jj = (u_long) cbptr->stn_id;
	    linkb(em_ptr, mp);
  	    mblk_trace(2,616,em_ptr);
    	    qreply(wq, em_ptr);
    	    cbptr->ip_msg = NULL;
	    }
		
	    break;
	    
        case A_XIDRQ:
	    /*
	     * Create MAC header as follows:
	     * DA from LCB; SA from TRD; AC - default; FC - default
	     * RII - based on dl_flag value; RSAP = 0.
	     * If data is passed down to be sent, assumes room at begin 
	     * of buffer for MAC hddr.
	     */
	    {
	    mblk_t		*em_ptr;
	    SCB			stn_scb;  	/* build cmd in this */
	    TR_XMIT_PARMS	xmit_parms;	/* set xmit params here */
	    
	    if ((em_ptr = tr_xid_rqrsp(cbptr, 
				       &stn_scb, 
				       &xmit_parms, 
				       DL_XID_REQ)) == NULL)
		{
		goto skip_fsm;				/* could not get block */
		}
	    xmit_parms.frame_type = TR_FTYPE_XID_COM;

	    /* send cmd to TRD */
	    if (!(tr_lli_output(cbptr->ppa, &stn_scb)))
	        {
	    	mblk_trace(3,18, em_ptr);
	    	freemsg(em_ptr);
		cbptr->err_prim = DL_XID_REQ;
	        send_error_ack(wq, mp, DL_BADPRIM, 0); /* always uses ip_msg */
	        goto skip_fsm;				/* avoids state change */
		}
	    else
		{
	    	mblk_trace(3,19, em_ptr);
		freemsg(em_ptr);
		if (em_ptr != mp)
		    {
	    	    mblk_trace(3,20, mp);
		    freemsg(mp);
		    }
		cbptr->ip_msg = NULL;
		}
	    }

	    break;

        case A_XIDCN:				/* process XID reply */
	    /*
	     * Generate DL_XID_CON to user; save any Routing Info.
	     * Input blk consists of LLI_CB, then ICB, then XID data.
	     * Use input block for M_DATA block and get M_PROTO block for
	     * primitive.
	     */
	    {
	    mblk_t		*em_ptr;
	    TR_ICB		*icb_ptr;
	    u_long 		*jj;

	    /* get a block for the DL_XID_CON primitive */
    LLP(("A_XIDCN: b_rptr=%x, b_base=%x\n",mp->b_rptr,mp->b_datap->db_base));
	    i = sizeof(dl_xid_con_t) + sizeof(long);
	    if ((em_ptr = tr_get_blk(cbptr, i)) == NULL)
		{
	    	goto skip_fsm;			/* so caller will exit FSM w/o st chg */
		}

	    ASSERT(cbptr->mgt_sap_flag == NULL);
	    icb_ptr = (TR_ICB *)(((LLI_CB *)mp->b_rptr)->param_blk);

	    /* extract RI if exists */
	    HEXDUMP((mp->b_rptr+sizeof(LLI_CB)+sizeof(TR_ICB)), 32);
	    tr_update_ri(cbptr, mp);
	    
	    /* set to point to MAC hdr */
    	    mp->b_rptr += (sizeof(LLI_CB) + sizeof(TR_ICB));
/*	    mp->b_rptr -= 18;  TEMPORARY ADJUST FOR RI */
	    mp->b_datap->db_type = M_DATA;

    	    ASSERT(mp->b_next == NULL);
	    i = icb_ptr->frame_type;		/* get for FINAL=0/1 flag */
	    tr_build_hdr(em_ptr, DL_XID_CON, 
		sizeof(dl_xid_con_t) + sizeof(long));
    	    dl_ptr = (void *) em_ptr->b_rptr;

	    if (i == TR_XID_RESP_F1)		/* FINAL bit set */
		i = DL_POLL_FINAL;
	    else
		i = 0;
    	    ((dl_xid_con_t *) dl_ptr)->dl_flag = i;
    	    ((dl_xid_con_t *) dl_ptr)->dl_src_addr_offset = 0;
    	    ((dl_xid_con_t *) dl_ptr)->dl_src_addr_length = 0;
    	    ((dl_xid_con_t *) dl_ptr)->dl_dest_addr_offset 
					= sizeof(dl_xid_con_t);
    	    ((dl_xid_con_t *) dl_ptr)->dl_dest_addr_length = sizeof(long);
    	    jj = (u_long *)((dl_xid_con_t *) dl_ptr + 1);
    	    *jj = (u_long) cbptr->stn_id;
	    linkb(em_ptr, mp);
  	    mblk_trace(2,617,em_ptr);
    	    qreply(wq, em_ptr);
    	    cbptr->ip_msg = NULL;
	    }
		
	    break;
	    
        case A_XIDCMD:				/* process XID indication */
	    /*
	     * Generate DL_XID_IND to user; save any Routing Info.
	     * Input blk consists of LLI_CB, then ICB, then XID data.
	     */
	    {
	    mblk_t		*em_ptr;
	    TR_ICB		*icb_ptr;
	    u_long 		*jj;

    LLP(("A_XIDCMD: b_rptr=%x, b_base=%x\n",mp->b_rptr,mp->b_datap->db_base));
	    /* get a block for the DL_XID_IND primitive */
	    i = sizeof(dl_xid_ind_t) + sizeof(long);
	    if ((em_ptr = tr_get_blk(cbptr, i)) == NULL)
		{
	    	goto skip_fsm;			/* so caller will exit FSM w/o st chg */
		}
	    
	    ASSERT(cbptr->mgt_sap_flag == NULL);
	    icb_ptr = (TR_ICB *)(((LLI_CB *)mp->b_rptr)->param_blk);

	    /* extract RI if exists */
	    HEXDUMP((mp->b_rptr+sizeof(LLI_CB)+sizeof(TR_ICB)), 32);
	    tr_update_ri(cbptr, mp);
	    
	    /* set to point to MAC hdr */
    	    mp->b_rptr += (sizeof(LLI_CB) + sizeof(TR_ICB));
/*	    mp->b_rptr -= 18;  TEMPORARY ADJUST FOR RI */
	    mp->b_datap->db_type = M_DATA;

    	    ASSERT(mp->b_next == NULL);
	    i = icb_ptr->frame_type;		/* get for POLL=0/1 flag */
	    tr_build_hdr(em_ptr, DL_XID_IND, 
			sizeof(dl_xid_ind_t) + sizeof(long));
    	    dl_ptr = (void *) em_ptr->b_rptr;
	    if (i == TR_XID_CMD_P1)		/* POLL bit set */
		i = DL_POLL_FINAL;
	    else
		i = 0;
    	    ((dl_xid_ind_t *) dl_ptr)->dl_flag = i;
    	    ((dl_xid_ind_t *) dl_ptr)->dl_src_addr_offset = 0;
    	    ((dl_xid_ind_t *) dl_ptr)->dl_src_addr_length = 0;
    	    ((dl_xid_ind_t *) dl_ptr)->dl_dest_addr_offset 
					= sizeof(dl_xid_ind_t);
    	    ((dl_xid_ind_t *) dl_ptr)->dl_dest_addr_length = sizeof(long);
    	    jj = (u_long *)((dl_xid_ind_t *) dl_ptr + 1);
    	    *jj = (u_long) cbptr->stn_id;
	    linkb(em_ptr, mp);
  	    mblk_trace(2,618,em_ptr);
    	    qreply(wq, em_ptr);
    	    cbptr->ip_msg = NULL;
	    }
		
	    break;
	    
        case A_XIDRES:				/* process XID resp from user */
	    /*
	     * Create MAC header as follows:
	     * DA from LCB; SA from TRD; AC - default; FC - default
	     * RII - if exists; RSAP = 0; POLL bit value from dl_flag.
	     * If data is passed down to be sent, assumes room at begin 
	     * of buffer for MAC hddr.
	     */
	    {
	    mblk_t		*em_ptr;
	    SCB			stn_scb;  	/* build cmd in this */
	    TR_XMIT_PARMS	xmit_parms;	/* set xmit params here */
	    
    	    dl_ptr = (void *) mp->b_rptr;
	    if ((em_ptr = tr_xid_rqrsp(cbptr, 
				       &stn_scb, 
				       &xmit_parms, 
				       DL_XID_REQ)) == NULL)
		{
		goto skip_fsm;				/* could not get block */
		}

	    /* determine frame type */
	    if (((dl_xid_res_t *) dl_ptr)->dl_flag == DL_POLL_FINAL)
	        xmit_parms.frame_type = TR_FTYPE_XID_RESP_F;
	    else
	        xmit_parms.frame_type = TR_FTYPE_XID_RESP_NF;

	    /* send cmd to TRD */
	    if (!(tr_lli_output(cbptr->ppa, &stn_scb)))
	        {
	    	mblk_trace(3,21, em_ptr);
	    	freemsg(em_ptr);
		cbptr->err_prim = DL_XID_RES;
	        send_error_ack(wq, mp, DL_BADPRIM, 0); /* always uses ip_msg */
	        goto skip_fsm;				/* avoids state change */
		}
	    else
		{
	    	mblk_trace(3,22, em_ptr);
		freemsg(em_ptr);
	    	mblk_trace(3,23, mp);
	        freemsg(mp);
		cbptr->ip_msg = NULL;
		}
	    }

	    break;

        case X:                                 /* invalid action */
    	    dl_ptr = (void *) mp->b_rptr;
/*          cmn_err(CE_NOTE,"execute_DLPI_FSM: invalid action detected"); */
	    LLP(("tr_lli: execute_DLPI_FSM: invalid action detected\n"));
            putctl1(RD(wq)->q_next, M_ERROR, EPROTO);
	    LLP(("\tcstate = %d, tpstate = %d\n", cbptr->cstate, 
		cbptr->pstate));
	    LLP(("\tctrig  = %d, ptrig = %d\n", cbptr->ctrig, cbptr->ptrig));
	    freemsg(mp);
	    cbptr->ip_msg = NULL;
	    nextState = currState;
#ifdef TEST
	    ASSERT(mp);
	    cbptr->err_prim = dl_ptr->dl_primitive;
	    if (!send_error_ack(wq, mp, DL_OUTSTATE, 0))
		{
		goto skip_fsm;				/* no buffers - try later */
		}
	    nextState = currState;
#endif /* TRDBG */
            rc = -1;
            break;

        }  /* end of switch (currAction) */

    if ((nextState == X) && (currAction != X))
        {
/*      cmn_err(CE_NOTE,"execute_DLPI_FSM: invalid state transition\n"); */
	LLP(("tr_lli: execute_DLPI_FSM: invalid state transition\n"));
	putctl1(RD(wq)->q_next, M_ERROR, EPROTO);
	cmn_err(CE_NOTE,"cstate = %d\n", cbptr->cstate);
	cmn_err(CE_NOTE,"pstate = %d\n", cbptr->pstate);
	cmn_err(CE_NOTE,"ctrig  = %d\n", cbptr->ctrig);
	cmn_err(CE_NOTE,"ptrig  = %d\n", cbptr->ptrig);
#ifdef TEST
	ASSERT(mp);
	ASSERT(dl_ptr);
	cbptr->err_prim = dl_ptr->dl_primitive;
	if (!send_error_ack(wq, mp, DL_OUTSTATE, 0))
	    {
	    goto skip_fsm;				/* no buffers - try later */
	    }
#endif /* TRDBG */
	cbptr->pstate = currState;
	cbptr->ptrig  = currEvent;
	cbptr->cstate = currState;
        rc = -1;
        }
    else
        {
        cbptr->pstate = currState;
        cbptr->ptrig  = currEvent;
        cbptr->cstate = nextState;
        }

    LLP(("execute_DLPI_FSM: exiting.. cstate = %d, pstate = %d, ctrig = %d, ptrig = %d\n", cbptr->cstate, cbptr->pstate, cbptr->ctrig, cbptr->ptrig));

skip_fsm:	/* avoid state machine changes in some case, and reset lock */
    cbptr->fsm_lock = 0;
    return(rc);
} /* end of execute_DLPI_FSM */


/* ARGSUSED */
lli_enque(LCB *cbptr, mblk_t *mp)
{
	return 0;
}

/* ARGSUSED */
lli_deque(LCB *cbptr, mblk_t *mp)
{
	return 0;
}


/**************************************************************************
 * STREAMS open processor
 **************************************************************************/

/* Open STREAMS device. Must be clone open.
 * All streams will have a slot in the mdev_tbl allocated at open time.
 */

/*ARGSUSED*/
int
lli_open(
	queue_t 	*rq,
	dev_t 		*inputdev,
	int 		flag,
	int 		sflag,
	struct cred	*cred)
{
    LCB			*cbptr;
    long		dev;

    /* Check for non-driver open */
    if (sflag != CLONEOPEN)
        {
	cmn_err(CE_NOTE, "lli_open: not CLONE open\n");
        return (ENXIO);
        }

    for (dev = 0; dev < MAX_LCB; dev++)
	{
	if (mdev_tbl[dev] == NULL)
		break;
	}
    if (dev >= MAX_LCB)
	{
	cmn_err(CE_NOTE, "lli_open: out of minor device numbers\n");
	return(ENOSR);
	}

    /* Check for exclusive access */
    if (WR(rq)->q_ptr)
        {
        return(EBUSY);
        }

    /* 
     * Set this slot.
     */
    *inputdev = dev;
    mdev_tbl[dev] = WR(rq);			/* point to Write q */
    rq->q_ptr = (caddr_t) dev;     		/* point back to Dev tbl */

    /* get LCB memory already zeroed - OK to sleep in open */
    ASSERT(WR(rq)->q_ptr == NULL);
    cbptr = (LCB *) kmem_zalloc(sizeof(LCB), KM_SLEEP);
    WR(rq)->q_ptr = (caddr_t) cbptr;
    cbptr->qptr = WR(rq);
    cbptr->cstate = S_UNATT;
    cbptr->pstate = S_UNATT;
    cbptr->mdev   = dev;

    return(0);
}


/**************************************************************************
 * STREAMS close processor
 **************************************************************************/

/* Close STREAMS device */
/*ARGSUSED*/
int
lli_close(queue_t *rq, int flags, struct cred *cred)
{

    LCB			*lcb_ptr;
    long                dev;

    LLP(("lli_close: minor device = %x\n", rq->q_ptr));

    if (lli_timeout)
	untimeout(lli_timeout);
    lli_timeout = 0;
    dev = (ulong) rq->q_ptr;
    ASSERT(mdev_tbl[dev] == WR(rq));
    if ((lcb_ptr = (LCB *)((WR(rq))->q_ptr)) != NULL)
	{
	int s;

	STR_LOCK_SPL(s);
	lcb_ptr->ctrig = E_SCLOS;
        execute_DLPI_FSM(lcb_ptr);
	WR(rq)->q_ptr = 0;
	STR_UNLOCK_SPL(s);
	}
    mdev_tbl[dev] = NULL;
    return 0;
}


/**************************************************************************
 * STREAMS read service procedure
 **************************************************************************/

/* 
 * Messages are queued to the READ side by tr_lli_input() at interrupt
 * level. This procedure dequeues these messages and processes them.
 */

int
lli_rsrv(queue_t	*rq)		/* read queue */	
{

    LCB                 *cbptr = (LCB *) (WR(rq))->q_ptr;
    LLI_CB		*cmd_ptr;
    int                 i;
    mblk_t              *mp;
    int			s;

    STR_LOCK_SPL(s);
    if (!canput(rq->q_next))
	{
	cmn_err(CE_NOTE,"lli_rsrv: can not send upstream\n");
	noenable(rq);
	goto lli_rs_ret;
	}

    /*
     * Loop while upline flow-control not on and have RCV data
     */
    while ((mp = getq(rq)) != NULL)
        {
	switch (mp->b_datap->db_type)
	    {
	    default:
		cmn_err(CE_NOTE, "lli_rsrv: unexpected msg rcvd - %x\n",
			mp->b_datap->db_type);
	    	mblk_trace(3,24, mp);
		freemsg(mp);
		continue;

	    case M_PROTO:			/* only allowable msg type */
		cmd_ptr = (LLI_CB *) mp->b_rptr;
		switch (cmd_ptr->command)
	    	    {
	    	    default:
			cmn_err(CE_NOTE, "lli_rsrv: unexpected msg rcvd - %x\n",
				mp->b_datap->db_type);
	    		mblk_trace(3,25, mp);
			freemsg(mp);
			continue;

	    	    case LLI_CMD_RSP:		/* response to previous cmd */
			/* generate event from SSB response */
			{
			SSB	*ssb_ptr;

			ssb_ptr = (SSB *) cmd_ptr->param_blk;
			ASSERT(cbptr->ip_msg == NULL);
			cbptr->ip_msg = mp;
			switch (ssb_ptr->ssb_command)
		    	    {
		    	    case TR_CMD_TRANSMIT:
				/* if_fv does not generate this */
				cmn_err(CE_NOTE,"lli_rsrv,TR_CMD_TRSNAMIT response received\n");
	    			mblk_trace(3,25, mp);
				freemsg(mp);
				cbptr->ip_msg = NULL;
				continue;

		    	    case TR_CMD_OPEN_SAP:
				if (ssb_ptr->ssb_parm0 == TR_OPEN_SAP_OK)
				    cbptr->ctrig = E_OSAPOK;
				else
				    {
				    cbptr->ctrig = E_BADCMD;
				    switch (ssb_ptr->ssb_parm0)
					{
					default:
					    cbptr->errno = DL_BADADDR;
					    break;
					case TR_OPEN_SAP_UNAUTH:
					    cbptr->errno = DL_ACCESS;
					    break;
					}
				    cbptr->syserr = 0;
				    }
				break;

		    	    case TR_CMD_LLC_RESET:
				{
				queue_t	    	*nwq;

				if (ssb_ptr->ssb_parm0 != TR_LLC_RESET_OK)
				    {
				    send_error_ack(WR(rq), mp, DL_BADPRIM, 0);
				    cbptr->ctrig = E_BADCMD;
				    cbptr->cstate = cbptr->pstate;
				    continue;	    /* loop for next msg */
				    }
				i = (cbptr->stn_id >> 8) - 1;
				i *= MAX_STNS; /* i= offset in STN_TBL entry */
				for (; i < MAX_STNS; i++)
				    {
				      nwq = stn_tbl[cbptr->ppa].WR_PTR(i);
				      if (nwq)
					  {
					    cbptr = (LCB *)nwq->q_ptr;
					    cbptr->ctrig = E_RSTRS;
					    cbptr->ip_msg = mp;
					    execute_DLPI_FSM(cbptr);
					  }
				    }
	    			mblk_trace(3,26, mp);
				freemsg(mp);
				cbptr->ip_msg = NULL;
				continue;
			        }

		    	    case TR_CMD_CLOSE_SAP:
				if (ssb_ptr->ssb_parm0 == TR_CLOSE_SAP_OK)
				    cbptr->ctrig = E_CSAPOK;
				else
				    {
				    cbptr->ctrig = E_BADCMD;
				    cbptr->errno = 0; /* ??????? */
				    cbptr->syserr = 0;
				    }
				break;

		    	    case TR_CMD_OPEN_STATION:
				if (ssb_ptr->ssb_parm0 == TR_OPEN_STA_OK)
				    cbptr->ctrig = E_OSTNOK;
				else
				    {
				    cbptr->ctrig = E_BADCMD;
				    switch (ssb_ptr->ssb_parm0)
					{
					default:
					    cbptr->errno = DL_BADADDR;
					    break;
					case TR_OPEN_STA_UNAUTH:
					    cbptr->errno = DL_ACCESS;
					    break;
					case TR_OPEN_STA_RES_NA:
					    cbptr->errno = DL_TOOMANY;
					    break;
					}
				    cbptr->syserr = 0;
				    }
				break;

		    	    case TR_CMD_CLOSE_STATION:
				if (ssb_ptr->ssb_parm0 == TR_CLOSE_STA_OK)
				    cbptr->ctrig = E_CSTNOK;
				else
				    {
				    cbptr->ctrig = E_BADCMD;
				    cbptr->errno = 0; /* ??????? */
				    cbptr->syserr = 0;
				    }
				break;

		    	    case TR_CMD_CONNECT_STATION:
				if (ssb_ptr->ssb_parm0 == TR_CONNECT_STA_OK)
				    {
				    if (outcnt == 0)	/* no O/S Cn indics */
					{
					ASSERT(ssb_ptr->ssb_parm1 == cbptr->stn_id);
				    	cbptr->ctrig = E_SABRCV;
					break;
					}
				    /* incoming cn on this stream */
				    if ((outcnt == 1) && (cbptr->token == 0))
					{
					ASSERT(ssb_ptr->ssb_parm1 == cbptr->stn_id);
					cbptr->ctrig = E_CONSOK;
					break;
					}
				    /* unsolicited CN response - pass to MGT */
				    if ((outcnt == 1) && (cbptr->token != 0))
					{
					/* This stn shouldn't have STN_ID yet */
					ASSERT(cbptr->stn_id == 0);
					cbptr->ip_msg = mp;
					cbptr->ctrig = E_PASSCN;
					execute_DLPI_FSM(cbptr);/* for new CN */

					cbptr = mgt_lcb[cmd_ptr->port];
					cbptr->ctrig = E_CONSOK1;
					cbptr->ip_msg = mp;
					break;
					}
				    /* unsolicited CN response - pass to MGT */
				    if ((outcnt > 1) && (cbptr->token != 0))
					{
					/* This stn shouldn't have STN_ID yet */
					ASSERT(cbptr->stn_id == 0);
					cbptr->ip_msg = mp;
					cbptr->ctrig = E_PASSCN;
					execute_DLPI_FSM(cbptr);/* for new CN */

					cbptr = mgt_lcb[cmd_ptr->port];
					cbptr->ctrig = E_CONSOK2;
					cbptr->ip_msg = mp;
					break;
					}
				    }	/* if CONNECT_STA_OK */
				else
				    {		/* not good completion */
				    switch (ssb_ptr->ssb_parm1)
					{
					case TR_CONNECT_NOT_OK:
					    cbptr->ctrig = E_DSCIND;
					    break;
					default:
				    	    cbptr->ctrig = E_BADCMD;
				    	    cbptr->errno = 0; /* ??????? */
				    	    cbptr->syserr = 0;
					    break;
					}
				    }
				break;

		    	    case TR_CMD_XMIT_IFRAME:
				/* if_fv does not send us this */
				LLP(("lli_rsrv: TR_CMD_XMIT_IFRAME RSP\n"));
	    			mblk_trace(3,27, mp);
	    			freemsg(mp);
				cbptr->ip_msg = NULL;
				continue;	/* loop for next msg */

			    default:
	    			cmn_err(CE_NOTE, 
                   		"lli_rsrv: unexpected SSB response - %x\n",
		      			ssb_ptr->ssb_command);
				HEXDUMP((char *)&ssb_ptr, 8);
				HEXDUMP((char *)cmd_ptr,32);
	    			mblk_trace(3,27, mp);
	    			freemsg(mp);
				cbptr->ip_msg = NULL;
				continue;	/* loop for next msg */
         		    }	/* end of switch (ssb_ptr->ssb_command) */
		        }
			break;			/* exit to execute the FSM */

                    case LLI_RCV_PENDING:
			/* generate event from ICB */
			{
			TR_ICB	*icb_ptr = (TR_ICB *) cmd_ptr->param_blk;

			ASSERT(cbptr->ip_msg == NULL);
			cbptr->ip_msg = mp;
			
			switch (icb_ptr->frame_type)
			    {
			    case TR_I_FRAME:
				cbptr->ctrig = E_RCVD;
				break;
			    case TR_XID_CMD_P1:
			    case TR_XID_CMD_P0:
				cbptr->ctrig = E_XIDIND;
				break;
			    case TR_XID_RESP_F1:
			    case TR_XID_RESP_F0:
				cbptr->ctrig = E_XIDCON;
				break;
			    case TR_TEST_RESP_F1:
			    case TR_TEST_RESP_F0:
				cbptr->ctrig = E_TESTCON;
				break;
			    default:
	    			cmn_err(CE_NOTE, 
                   		"lli_rsrv: unexpected RCV frame type - %x\n",
		      			icb_ptr->frame_type);
	    			mblk_trace(3,28, mp);
	    			freemsg(mp);
				cbptr->ip_msg = NULL;
				continue;	/* loop for next msg */
         		    }	/* end of switch (icb_ptr->frame_type) */

			}
        		break;

		    case LLI_LLC_STATUS:
			/* generate event from ICB - use LLC status info */
			{
			TR_ICB	*icb_ptr = (TR_ICB *) cmd_ptr->param_blk;

			ASSERT(cbptr->ip_msg == NULL);
			cbptr->ip_msg = mp;
	
			/* Multiple status in 1 ICB?; if one of the ASSERTs
			   fails, use dupb/copyb and make multiple FSM calls */
			if (icb_ptr->llc_stat_code & TR_LOST_LINK)
			    {
			    ASSERT(icb_ptr->llc_stat_code == TR_LOST_LINK);
			    ASSERT(icb_ptr->station_id_s == cbptr->stn_id);
			    cbptr->ctrig = E_LSTLNK;
			    break;
			    }
			if (icb_ptr->llc_stat_code & TR_DISC)
			    {
			    LLP(("tr_lli: DSCIND from LLC Status msg\n"));
			    ASSERT(icb_ptr->llc_stat_code == TR_DISC);
			    ASSERT(icb_ptr->station_id_s == cbptr->stn_id);
			    if (outcnt == 0)
				{
			    	cbptr->ctrig = E_DSCIND;
			    	break;
			    	}
			    if (outcnt == 1)
				{
			    	cbptr->ctrig = E_DSCIND1;
			    	break;
			    	}
			    ASSERT(outcnt > 1);
			    cbptr->ctrig = E_DSCIND2;
			    break;
			    }
			if (icb_ptr->llc_stat_code & TR_SABME_RCVD)
			    {
			    ASSERT(icb_ptr->llc_stat_code == TR_SABME_RCVD);
			    ASSERT(icb_ptr->station_id_s == cbptr->stn_id);
			    cbptr->ctrig = E_SABRCV;
			    break;
			    }
			if (icb_ptr->llc_stat_code & TR_SABME_OPEN_LINK)
			    {
			    if (outcnt < MAX_CONIND)
				{
			    	ASSERT(icb_ptr->llc_stat_code == TR_SABME_OPEN_LINK);
			    	cbptr->ctrig = E_SABOL;
			    	break;
			    	}
			    else
				{
				/* ?????????? wot to do- MAX_CONIND exceeded */
				mblk_trace(3,28,mp);
				freemsg(mp);
				cbptr->ip_msg = NULL;
				continue;
				}
			    }
			if ((icb_ptr->llc_stat_code & TR_LOCAL_BUSY) ||
			    (icb_ptr->llc_stat_code & TR_REMOTE_BUSY))
			    {
LLP(("lli_rsrv: remote/local busy, stat_code=%x\n",icb_ptr->llc_stat_code));
			    mblk_trace(3,28, mp);
			    freemsg(mp);
			    cbptr->ip_msg = NULL;
			    continue;       /* loop for next message */
			    }
			if (icb_ptr->llc_stat_code & TR_REMOTE_NOT_BUSY)
			    {
			    LLP(("lli_rsrv: REMOTE_NOT_BUSY\n"));
			    enableok(WR(rq));
			    mblk_trace(3,28, mp);
			    freemsg(mp);
			    cbptr->ip_msg = NULL;
/*			    putctl(rq->q_next,M_START); */
			    continue;       /* loop for next message */
			    }
			if ((icb_ptr->llc_stat_code & TR_FRMR_RCVD) ||
			    (icb_ptr->llc_stat_code & TR_FRMR_SENT) ||
			    (icb_ptr->llc_stat_code & TR_TI_EXPIRED) ||
			    (icb_ptr->llc_stat_code & TR_LLC_COUNT_OVFL) ||
			    (icb_ptr->llc_stat_code & TR_ACCESS_PRIORITY))
			    {
LLP(("lli_rsrv: other llc status, stat_code=%x\n",icb_ptr->llc_stat_code));
/* lli_brk(2); */

			    mblk_trace(3,28, mp);
			    freemsg(mp);
			    cbptr->ip_msg = NULL;
			    continue;       /* loop for next message */
			    }
			}

			break;

		    }	/* end of switch (cmd_ptr->command) */

#ifdef IFRMDBG
		if (cbptr->ctrig == E_DATARQ) {
		    printf("tr_lli: lli_rsrv,E_DATARQ,cmd=%d\n",
			cmd_ptr->command);
/* lli_brk(3); */
		    mblk_trace(3,28,mp);
		    freemsg(mp);
		    cbptr->ip_msg = NULL;
		    continue;
		}
#endif
		execute_DLPI_FSM(cbptr);
		continue;
	    }	/* end of switch (mp->b_datap->db_type) */
                
        } /* while ((mp = get(rq))... */
 
lli_rs_ret:
    STR_UNLOCK_SPL(s);
    return(0);
}


/**************************************************************************
 * STREAMS write service procedure
 **************************************************************************/
/*
 * Messages are queued to the WR q by lli_wput when it is passed a message
 * and the FSM for this STREAM is currently executing a previous message.
 */

static int
lli_wsrv(queue_t	*wq)		    /* write queue */
{
    mblk_t              *mp;
    LCB                	*cbptr = (LCB *) wq->q_ptr;
    int			s;

    STR_LOCK_SPL(s);
    /*
     * Loop while have data to process
     */
    ASSERT(cbptr);  	       /* LCB must exist by now */
    LLP(("lli_wsrv called, wq = %x\n", wq));
    if (cbptr->ifr_state == IFR_STATE_WAIT)
	{
	noenable(wq);
	if (lli_timeout == 0) 
	    {
	    lli_timeout = STREAMS_TIMEOUT(lli_wtime,wq,50);
	    }
	goto lli_ws_ret;
	}

    while ((mp = getq(wq)) != NULL)
        {
    	union DL_primitives	*dl_ptr;
    	int                 	i;

        if (cbptr->fsm_lock)	    	    /* FSM is already executing */
            {
	    mblk_trace(2,209,mp);
	    putbq(wq, mp);		    /* q so process after FSM done */
	    goto lli_ws_ret;
	    }

	switch (mp->b_datap->db_type)	    /* switch on msg type */
    	    {
    	    case M_IOCTL:                   /* use d/stream blk for reply */
	        ASSERT(mp->b_next == (mblk_t *) NULL);
    	        mp->b_datap->db_type = M_IOCNAK;
	        mblk_trace(2,601,mp);
    	        qreply(wq, mp);
    	        cmn_err(CE_NOTE, "lli_wput: M_IOCTL received\n");
    	        break;

    	    case M_PROTO:
	        {
	        dl_ptr = (void *) mp->b_rptr; 	/* point to Ctl blk*/
  	        ASSERT(cbptr->ip_msg == NULL);	/* expected to be NULL */

    	        switch (dl_ptr->dl_primitive) /* DLPI message type */
    	            {
	            case DL_ATTACH_REQ:
		      /* validate dl_ppa via TRD - set approp event */
		      if (tr_if_valid(((dl_attach_req_t *) dl_ptr)->dl_ppa))
		          {
		          cbptr->ctrig  = E_ATTRQV;
		          }
		      else
		          {
		          cbptr->ctrig = E_ATTRQI;
		          }
		      cbptr->ip_msg = mp;

		      break;

	            case DL_DETACH_REQ:
		      /* no dl_ppa passed - implicit in stream */
		      cbptr->ip_msg = mp;
		      cbptr->ctrig = E_DTTRQV;
		
		      break;

	            case DL_BIND_REQ:
		      /*
		       * OPEN.SAP cmd response does not contain the value of the
		       * opened SAP. However, SAP_IDs are allocated sequentially
		       * so check SAP not already opened or open-pending via 
		       * sapid_2_sap table.
		       * If MGT SAP, do not issue OPEN.STATION to TRD.
		       */
		      LLP(("DL_BIND_REQ\n"));
		      cbptr->ip_msg = mp;

		      /* check if opening Mgt connection */
		      if (((dl_bind_req_t *) dl_ptr)->dl_conn_mgmt)
		          {
		          if (mgt_lcb[cbptr->ppa] == NULL) {
			      LLP(("DL_BIND_REQ: management SAP\n"));
			      cbptr->ctrig = E_BNDOLD;
			      }
		          else
		    	      {			/* only 1 MGT cn allowed */
		    	      cbptr->ctrig = E_BADCMD;
		    	      cbptr->errno = DL_BOUND;
		    	      cbptr->syserr = 0;
		    	      }
		          break;
		          }

		      /* dl_max_conind validation??? 1 for normal, x for MGT */

		      for (i = 0; i < MAX_SAPS; i++)
		          {
	                  if (sapid_2_sap[cbptr->ppa][i] == 
				((dl_bind_req_t *) dl_ptr)->dl_sap)
			      break;		/* already opened/opening */
		          if (sapid_2_sap[cbptr->ppa][i] == 0)
			      break;		/* next available entry */
		          }
		      if (i >= MAX_SAPS)	/* no space in table */
		          {
		          cbptr->ctrig = E_BADCMD;
		          cbptr->errno = DL_SYSERR;
		          cbptr->syserr = EBUSY;
		          break;
		          }    

		      if (sapid_2_sap[cbptr->ppa][i] == 0) /* SAP not opened */
		          cbptr->ctrig = E_BINDRQ;
		      else
		          cbptr->ctrig = E_BNDOLD;

		      break;

	            case DL_SUBS_BIND_REQ:
		      /*
		       * Issue OPEN.STATION to TRD.
		       */
		      cbptr->ip_msg = mp;
		      if (cbptr->mgt_sap_flag == 0)
		          cbptr->ctrig = E_SBINDRQ;
		      else
		          cbptr->ctrig = E_SBINDMG;
	
		      break;

	            case DL_UNBIND_REQ:
		      /*
		       * Issue CLOSE.SAP to TRD
		       */
		      cbptr->ip_msg = mp;
		      cbptr->ctrig = E_UBINDRQ;

		      break;

	            case DL_SUBS_UNBIND_REQ:
		      /* 
		       * Issue CLOSE.STATION to TRD
		       */
		      cbptr->ip_msg = mp;
		      cbptr->ctrig = E_SUBNDRQ;

		      break;

	            case DL_CONNECT_REQ:
		      /*
		       * Issue CONNECT.STATION to TRD
		       */
		      if (cbptr->mgt_sap_flag==0) /* CONNECT OK for this stn */
		          {
		          cbptr->ip_msg = mp;
		          cbptr->ctrig = E_CONNRQ;
		          }
		      else
		          {			/* no CONNECTS on MGT SAP */
		          cbptr->err_prim = dl_ptr->dl_primitive;
		          send_error_ack(wq, mp, DL_BADPRIM, 0);
		          goto lli_ws_ret;
		          }

		      break;

	            case DL_CONNECT_RES:
		      /* Users response to a CONNECT_IND from us */
		      cbptr->ip_msg = mp;
		      cbptr->ctrig  = E_CONNRES;

		      break;

	            case DL_TEST_REQ:
		      cbptr->ip_msg = mp;
		      cbptr->ctrig = E_TESTRQ;

		      break;

	            case DL_XID_REQ:
		      cbptr->ip_msg = mp;
		      cbptr->ctrig = E_XIDRQ;

		      break;

	            case DL_XID_RES:
		      cbptr->ip_msg = mp;
		      cbptr->ctrig = E_XIDRES;

		      break;

	            case DL_DISCONNECT_REQ:
		      if ( (cbptr->cstate == S_INCPND &&
			(!((dl_disconnect_req_t *)(dl_ptr))->dl_correlation))
		      || (cbptr->cstate != S_INCPND &&
			((dl_disconnect_req_t *)(dl_ptr))->dl_correlation)  )
		          {
		          cbptr->err_prim = dl_ptr->dl_primitive;
		          send_error_ack(wq, mp, DL_BADPRIM, 0);
		          goto lli_ws_ret;
		          }
		      cbptr->ip_msg = mp;
		      cbptr->ctrig = E_DISCNRQ;

		      break;

	            case DL_RESET_REQ:
		      /*
		       * Resets a SAP and all stns on that SAP - so need
		       * to generate triggers for all stns on the SAP.
		       * Issue LLC.RESET for this stn, and separate event
		       * for all others on the SAP.
		       */
		      {
		      queue_t	    	*nwq;

		      cbptr->ip_msg = mp;
		      cbptr->ctrig = E_RSTREQ;
		      if (execute_DLPI_FSM(cbptr) == Error)
		          {
		          goto lli_ws_ret;		/* initial LLC.RESET failed */
		          }
		      /* get SAP_ID -> table offset */
    		      i = (cbptr->stn_id >> 8) - 1;
    		      i *= MAX_STNS;        /* i= offset in STN_TBL entry */
		      for (; i < MAX_STNS; i++)
		          {
		          nwq = stn_tbl[cbptr->ppa].WR_PTR(i);
		          if (nwq)
			      {
			      cbptr = (LCB *)nwq->q_ptr;
			      cbptr->ctrig = E_SAPRS;
			      cbptr->ip_msg = mp;
			      execute_DLPI_FSM(cbptr);
		              }
		          }
	              }

		      break;
	
	            default:        	/* unknown message type */
		      /* issue NOT_SUPPORTED error_ack to user */
		      /* ?? need to solve no-msg-avail situation in following */
		      cbptr->err_prim = dl_ptr->dl_primitive;
		      send_error_ack(wq, mp, DL_NOTSUPPORTED, 0);
		      cmn_err(CE_NOTE,
			      "lli_wput: unsupported/bad primitive rcvd, %x\n",
			      dl_ptr->dl_primitive);
		      goto lli_ws_ret;

	            }  /* end of switch (dl_ptr->dl_primitive) */

	        execute_DLPI_FSM(cbptr);

	        }
    	        break;     /* for case M_PROTO */

	    case M_DATA:
	        /* These msgs can only be DL_DATA_REQ msgs */
	        ASSERT(cbptr->ip_msg == NULL);
	        cbptr->ip_msg = mp;
	        cbptr->ctrig = E_DATARQ;
	        execute_DLPI_FSM(cbptr);
	        break;

    	    default:                        /* unexpected STREAMS msg */
	        cmn_err(CE_NOTE,
		        "lli_wput: FIFO1 unexpected db_type %d\n",
		        mp->b_datap->db_type);
	        mblk_trace(3,30, mp);
    	        freemsg(mp);
    	        break;

    	    }  /* end of switch (db_type)... */
            
        } /* while ((mp = get(rq))... */
lli_ws_ret:
    STR_UNLOCK_SPL(s);
    return(0);
}


/**************************************************************************
 * STREAMS write put procedure
 **************************************************************************/

/*
 * Processes requests and responses from the DLPI user.
 */

int
lli_wput(queue_t		*wq,
         mblk_t 	        *mp)
{

if (mp->b_datap->db_type > QPCTL)
    {
    int s;

    STR_LOCK_SPL(s);
    switch (mp->b_datap->db_type)	    /* switch on msg type */
	{
	case M_FLUSH:
	    if (*mp->b_rptr & FLUSHW)
    	      	{
	      	flushq(wq, FLUSHDATA);
	      	enableok(wq);
	      	}
    	    if (*mp->b_rptr & FLUSHR)
    	      	{
	      	flushq(RD(wq), FLUSHDATA);
	      	enableok(RD(wq));
	      	*mp->b_rptr &= ~FLUSHW;
	      	mblk_trace(2,600,mp);
	      	qreply(wq, mp);
	      	}
	    else 
	      	{
	      	mblk_trace(3,29, mp);
	      	freemsg(mp);
	      	}
	    break;

	case M_PCPROTO:
	    {
    	    union DL_primitives		*dl_ptr;
    	    LCB                 	*cbptr = (LCB *) wq->q_ptr;
	    dl_ptr = (void *) mp->b_rptr;
	    ASSERT(cbptr->ip_msg == NULL);

	    switch (dl_ptr->dl_primitive)
	        {
	        case DL_TOKEN_REQ:
		/*
		 * If token not already set, assign one. Either way, return
		 * token value to user. Use minor dev # as token.
		 * LCB must not be OPENed (stn_id == 0), and not the MGT LCB.
		 */
		    if ((cbptr->mgt_sap_flag == 0)
		      && (cbptr->stn_id == 0))
		        {
		        cbptr->token = 0xf;

    		        /* ensure room for ack */
    		        ASSERT(sizeof(dl_token_ack_t) <= (mp->b_datap->db_lim - mp->b_datap->db_base));
    		        ASSERT(mp->b_next == NULL);
    		        ASSERT(unlinkb(mp) == NULL); /* only 1 block expected */
    		        mp->b_datap->db_type = M_PCPROTO;
    		        mp->b_rptr = mp->b_datap->db_base;
    		        mp->b_wptr = mp->b_rptr + sizeof(dl_token_ack_t);
    		        dl_ptr = (void *) mp->b_rptr;
    		        ((dl_token_ack_t *) dl_ptr)->dl_token = cbptr->mdev;
    		        dl_ptr->dl_primitive = DL_TOKEN_ACK;
	  	        mblk_trace(2,602,mp);
		        qreply(wq, mp);
		        }
		    else
		        {			/* no tokens on MGT SAP cn */
		        cbptr->err_prim = dl_ptr->dl_primitive;
		        send_error_ack(wq, mp, DL_BADPRIM, 0);
		        }
		    cbptr->ip_msg = NULL;
		    goto lli_wp_ret;
	
	        default:         		/* unknown message type */
		    /* issue NOT_SUPPORTED error_ack to user */
		    /* ??????? need to solve no-msg-avail situation in following */
		    cbptr->err_prim = dl_ptr->dl_primitive;
		    send_error_ack(wq, mp, DL_NOTSUPPORTED, 0);
		    cmn_err(CE_NOTE,
			"lli_wput: unsupported/bad primitive rcvd, %x\n",
			dl_ptr->dl_primitive);
		    goto lli_wp_ret;

	        }  /* end of switch (dl_ptr->dl_primitive) */

	    /* NOTREACHED */
	    }

    	default:                        /* unexpected STREAMS msg */
	    cmn_err(CE_NOTE,
		  "lli_wput: FIFO1 unexpected db_type %d\n",
		  mp->b_datap->db_type);
	    mblk_trace(3,30, mp);
    	    freemsg(mp);
    	    break;

        }
lli_wp_ret:
    	STR_UNLOCK_SPL(s);
    }
else
    putq(wq,mp);
    return 0;
}



/**************************************************************************
 * LLI TRD input processing
 **************************************************************************/

/*
 * tr_lli_input(mblk_t *) - routine to receive input from the TRD.
 * Enter here at splimp().
 * It would be nice to pass the input directly to the FSM now, but that is
 * probably not acceptable. So instead it is queued to the RD queue and that 
 * queue is enabled.
 *
 * Input is a pointer to the input cmd/data block formatted by the TRD.
 * The block must be an M_PROTO block containing the command or response
 * in LLI_CB format in the control info part of the message, and any associated
 * data in the data part of the message (see STREAMS Programmers Guide on
 * M_PROTO messages).
 */
void
tr_lli_input(mblk_t      *mp)
{

    register LLI_CB	*cmd_ptr;		/* ptr to rsp block */
    u_int		i, j;
    register TR_ICB	*icb_ptr;
    queue_t		*qptr;
    SSB			*ssb_ptr;
    u_short		stn_id;			/* 15-8=SAP_ID, 0-7=STN_ID */

    ASSERT(mp->b_datap->db_type == M_PROTO);
    cmd_ptr = (LLI_CB *) mp->b_rptr;            /* point to cmd in block */
    ASSERT(cmd_ptr);

    /*
     * Determine interface # and STREAMS queue_t for this status/rsp, so it
     * can be queued appropriately.
     * Extract SAP_ID from response.
     */
    ASSERT(cmd_ptr->port < MAX_ADAPTORS);
    switch (cmd_ptr->command)
	{
	case LLI_CMD_RSP:			/* use SSB_CMD */
	    ssb_ptr = (SSB *) cmd_ptr->param_blk;
	    switch (ssb_ptr->ssb_command)
		{
		case TR_SSB_COMMAND_REJ:	/* bad command issued */
		    cmn_err(CE_NOTE, 
		      "tr_lli_input: COMMAND REJECT rsp received - cmd = %x\n",
		        ssb_ptr->ssb_parm1);
		    freemsg(mp);
		    return;

		case TR_CMD_RECEIVE:		/* no use for this */
		    freemsg(mp);
		    return;

		case TR_CMD_LLC_RESET:
		    freemsg(mp);
		    return;

		case TR_CMD_CLOSE_STATION:
		case TR_CMD_CONNECT_STATION:
		case TR_CMD_XMIT_IFRAME:
		    stn_id = ssb_ptr->ssb_parm1;
		    LLP(("tr_lli_input: cmd_ptr=%x, stn_id=%x\n",cmd_ptr,stn_id));
		    break;

		case TR_CMD_OPEN_STATION:
		    if (ssb_ptr->ssb_stat) {	/* returned error */
	    		cmn_err(CE_NOTE, 
                   	    "tr_lli_input: OPEN STATION failed, err = %x\n",
		      	    ssb_ptr->ssb_stat);
	  		mblk_trace(3,31, mp);
	    		freemsg(mp);
	    		return;
		    }
		    goto op_cl_sap;

		case TR_CMD_CLOSE_SAP:
		    if (ssb_ptr->ssb_stat) {	/* returned error */
	    		cmn_err(CE_NOTE, 
                   	    "tr_lli_input: CLOSE SAP failed, err = %x\n",
		      	    ssb_ptr->ssb_stat);
	  		mblk_trace(3,31, mp);
	    		freemsg(mp);
	    		return;
		    }
		    goto op_cl_sap;

		case TR_CMD_OPEN_SAP:
		    if (ssb_ptr->ssb_stat) {	/* returned error */
	    		cmn_err(CE_NOTE, 
                   	    "tr_lli_input: OPEN SAP failed, err = %x\n",
		      	    ssb_ptr->ssb_stat);
	  		mblk_trace(3,31, mp);
	    		freemsg(mp);
	    		return;
		    }

op_cl_sap:
		    /* sapid_2_q table contains WR q address of OPEN.SAP */
	    	    stn_id = ssb_ptr->ssb_stn_id;
		    ASSERT(stn_id != 0);	/* no NULL SAP stuff */
		    i = (stn_id >> 8) - 1;
		    qptr = sapid_2_q[cmd_ptr->port][i];
		    LLP(("tr_lli_input: rcv'd OPEN/CLOSE SAP response, qptr = %x\n", qptr));
	 	    LLP(("\tcmd_ptr=%x,i=%d,sapid_2_q=%x\n",cmd_ptr,i,sapid_2_q));
		    /* queue to this queue_t */
		    mblk_trace(2,210,mp);
    		    STREAMS_INTERRUPT(putq, RD(qptr), mp, NULL);
		    return;

		default:
		    cmn_err(CE_NOTE, 
                      "tr_lli_input: unexpected CMD response - cmd %x\n",
		      ssb_ptr->ssb_command);
		    return;
		};	/* end of switch (ssb_ptr->ssb_command) */
	    
	    break;

	case LLI_RCV_PENDING:			/* use ICB */
	    icb_ptr = (TR_ICB *) cmd_ptr->param_blk;
	    stn_id = icb_ptr->station_id_r;
	    LLP(("ICB(RCV_PENDING): station=%x, LLC_STATUS=%x\n", 
			stn_id, icb_ptr->llc_stat_code));

	    if ((stn_id & 0xff) == 0)		/* sent to SAP only */
		{
		/* get stn_tbl macptr & use to find link stn */
		u_int		found = FALSE;
		u_int		k, octet;
		u_char		*macptr;
		TR_MAC_HDR	*fr_ptr;

		i = (stn_id >> 8) - 1;
		j = i * MAX_STNS;
		LLP(("FRAME_ON_SAP %d\n", i+1));
		for (i = j; i < (j + MAX_STNS); i++)
		    {
		    macptr = stn_tbl[cmd_ptr->port].MACPTR(i);
		    if (macptr)			/* this slot has a station */
			{
			LLP(("LLI_RCV_PENDING: this slot has a station\n"));
			fr_ptr = (TR_MAC_HDR *) (mp->b_rptr + sizeof(LLI_CB)
							    + sizeof(TR_ICB));
			for (k = 0; k < 6; k++)
			    {
			    octet = fr_ptr->t_mac_sa[k];
			    if (k == 0)
				octet &= 0x7f;	/* clear possible RI bit */
			    if (octet != *macptr++)
				break;
			    }   /* for (k = 0; ... */
			if (k >= 6)		/* MAC address found */
			    {
			    found = TRUE;
			    break;
			    }
			}   /* if (macptr) */
		    }    /* for (i = j; ... */
		if (!found)
		    {
	    	    cmn_err(CE_NOTE, 
			    "tr_lli_input: frame rcvd on unknown station\n");
	  	    mblk_trace(3,32, mp);
	    	    freemsg(mp);
		    return;
		    }
		qptr = stn_tbl[cmd_ptr->port].WR_PTR(i);
		LLP(("LLI_RCV_PENDING: wq=%x, rq=%x\n", qptr, RD(qptr)));
		ASSERT(qptr);
		LLP(("tr_lli_input: putq mp %x to RD %x\n", mp, RD(qptr)));
		mblk_trace(2,211,mp);
		ASSERT(mp->b_datap);
		TIMEDUMP(4,icb_ptr->frame_len,time);
		STREAMS_INTERRUPT(putq, RD(qptr), mp, NULL);
		return;
		}
	    break;

	case LLI_LLC_STATUS:			/* use ICB */
	    icb_ptr = (TR_ICB *) cmd_ptr->param_blk;
	    stn_id = icb_ptr->station_id_s;
		if (icb_ptr->llc_stat_code != 128)
	    	LLP(("ICB(LLC_STATUS): station=%x, LLC_STATUS=%x\n", stn_id, 
				icb_ptr->llc_stat_code));

	    /* Check for new stn_id - won't be in our tables; queue to MGT cn */
	    if (icb_ptr->llc_stat_code & TR_SABME_OPEN_LINK)
		{
		LLP(("ICB: station=%x, TR_SABME_OPEN_LINK\n", stn_id));
		ASSERT(mgt_lcb[cmd_ptr->port] != NULL);
		/* ????? validate that STN_ID is not in stn_tbl? */
		mblk_trace(2,212,mp);
		STREAMS_INTERRUPT(putq, RD((mgt_lcb[cmd_ptr->port])->qptr),
				  mp, NULL);
	    	return;
		}

	    ASSERT(stn_id);
 	    i = (stn_id >> 8) - 1;		/* get SAP_ID -> table offset */
	    stn_id &= 0xff;			/* get STATION_ID -> offset */
	    stn_id--;
	    i = (i * MAX_STNS) + stn_id;   /* i = offset within STN_TBL entry */
	    qptr = stn_tbl[cmd_ptr->port].WR_PTR(i);
	    /* LLC_STATUS is meaningless if station is not up yet */
	    if (qptr)
		{
	    	if (icb_ptr->llc_stat_code & TR_TI_EXPIRED)
		    {
	  	    mblk_trace(3,33, mp);
		    freemsg(mp);
		    return;
		    }
		else 	/* queue to this queue_t */
		    {
		    mblk_trace(2,213,mp);
		    STREAMS_INTERRUPT(putq, RD(qptr), mp, NULL);
		    }
		}
	    else
		{
		LLP(("LLC_STATUS discarded\n"));
	  	mblk_trace(3,34, mp);
	    	freemsg(mp);
		return;
		}
	    return;

	default:
	    cmn_err(CE_NOTE, 
                   "tr_lli_input: unexpected LLI CMD - cmd %x\n",
		      cmd_ptr->command);
	    mblk_trace(3,35, mp);
	    freemsg(mp);
	    return;
	}
    
    /* Use STN_ID to get queue_t ptr to WR q from STN_TBL */
    i = (stn_id >> 8) - 1;		/* get SAP_ID -> table offset */
    stn_id &= 0xff;			/* get STATION_ID -> offset */
    stn_id--;
    i = (i * MAX_STNS) + stn_id;	/* i = offset within STN_TBL entry */
    qptr = stn_tbl[cmd_ptr->port].WR_PTR(i);
    LLP(("stn_id = %x, i = %x, qptr = %x\n", stn_id, i, qptr));
    ASSERT(qptr);			/* TRD input for unknown stn */
    /* queue to this queue_t */
    mblk_trace(2,214,mp);
    STREAMS_INTERRUPT(putq, RD(qptr), mp, NULL);
}


int
lli_rput(
	queue_t 	*wq, 
	mblk_t 		*mp)
{

	int s;

    	STR_LOCK_SPL(s);
	mblk_trace(2,215,mp);
	putq(wq, mp);
    	STR_UNLOCK_SPL(s);
	return 0;
}

/*
 * LLI BANDAGE:
 */
/*--------------------------------------------------------------------------*
 *	tr_find_WR_q 							    *
 *									    *
 *	These 2 routines are used in conjunction with the interface to LLI  *
 *	and eventually the SNA protocol stack.  In order to send data	    *
 *	to the correct interface, we must first find the LCB associated     *
 *	with a specific station id.  This algorithm is also used in LLI.    *
 *--------------------------------------------------------------------------*/
LCB *
tr_find_WR_q(struct ifnet *ifp, u_short sid)
{
	int i;
	LCB *cbptr;
        queue_t	*qptr;

	/* Use sid to get queue_t ptr to WR q from STN_TBL */
	i = (sid >> 8) - 1;
	sid &= 0xff;
	sid--;
	i = (i * MAX_STNS) + sid;
	qptr = stn_tbl[ifp->if_unit].WR_PTR(i); 
	if (qptr == 0) {
		LP(("%s%d: NULL qptr, sid = %x\n",
				ifp->if_name, ifp->if_unit,(u_long)sid));
		return(0);
	}

	cbptr = (LCB *)qptr->q_ptr;
	return(cbptr);

}

/*--------------------------------------------------------------------------*
 *	tr_find_iframe_q						    *
 *									    *
 *  When the SNA protocol stack wishes to send an iframe over    	    *
 *  token ring, the driver may be instructed by the board to wait.  The     *
 *  driver must then save the iframe information until the board indicates  *
 *  that it's time to do so.  The iframe information is stored in each      *
 *  connection's LCB; each LCB may have seven outstanding iframe requests.  *
 *  This routine finds the correct queue based on station id.               *
 *--------------------------------------------------------------------------*/
struct iframes *
tr_find_iframe_q(struct ifnet *ifp, u_short station_id, u_short sts)
{
	LCB *cbptr;
	u_short	i, idx;
	IFRAMES *ifr_ptr;

	/* Use station_id to get queue_t ptr to WR q from STN_TBL */
	if ((cbptr = tr_find_WR_q(ifp, station_id)) == 0)
		return(0);

	idx = cbptr->ifr_first;
	for (i = 0; i < NIFRAMES; i++) {
		ifr_ptr = &cbptr->iframes[idx];
		if (ifr_ptr->state == sts) {
			LP(("cmd_status: found iframes entry \n"));
			return(ifr_ptr);
		}
		if (idx == cbptr->ifr_last) { /* can't find it? */
			break;			   /* terminate loop */
		}
		if (++idx >= NIFRAMES)
	    		idx = 0;
	}

	DP(("cmd_status: CANNOT FIND IFRAMES ENTRY, first=%d, last=%d \n",
			cbptr->ifr_first, cbptr->ifr_last));
	return(0);
}

/*----------------------------------------------------------------------*
 *	The stream is being closed, so we have to clean out all of      *
 *	outstanding iframe requests and free the mblk's associated	*
 *	with them.							*
 *----------------------------------------------------------------------*/
void
tr_free_iframes(LCB *lcb)
{
	int i;
	IFRAMES *ifr_ptr;

	for (i = 0; i < NIFRAMES; i++) {
		ifr_ptr = &lcb->iframes[i]; 
		if (ifr_ptr->xmit_parms.xmit_data) {
			DP(("tr_free_iframes: freeing %x\n",
					ifr_ptr->xmit_parms.xmit_data));
			freemsg(ifr_ptr->xmit_parms.xmit_data);
	        	ifr_ptr->xmit_parms.xmit_data = NULL;
		}
	}
	lcb->ifr_first = lcb->ifr_last = 0;

}

static struct trlliops *trifops = 0;
int
tr_lli_register(struct trlliops *ops)
{
	if (trifops == 0) {
		trifops = ops;
		return(1);
	}
	return(0);
}

/*----------------------------------------------------------------------*
 * 	check to see if "unit" is valid					*
 *----------------------------------------------------------------------*/
int
tr_if_valid(int unit)
{
	if (!trifops) {
		return(0);
	}
	return((*trifops->lli_valid)(unit));
}

/*----------------------------------------------------------------------*
 * 	return MAC source address to upper layers			*
 *----------------------------------------------------------------------*/

char *
tr_get_sa(int unit)
{
	if (trifops) {
		return((*trifops->lli_get_sa)(unit));
	}
	printf("tr_get_sa: interface not set\n");
	return(0);
}

int
tr_lli_output(u_short unit, SCB *scb)
{
	if (trifops) {
		return((*trifops->lli_output)(unit, scb));
	}
	printf("tr_lli_output: interface not set\n");
	return(0);
}

u_short
tr_addr2sid(struct ifnet *ifp, char *addr, u_char sap)
{
	int i, j;
	STN_ENTRY *se;
	STN_TBL *stbl = &stn_tbl[ifp->if_unit];

	for (i = 0; i < MAX_SAPS; i++) {
		if (sapid_2_sap[ifp->if_unit][i] == sap)
			break;
	}
	if (i >= MAX_SAPS) {
		DP(("addr2sid: no sap found\n"));
		return(0);
	}
	se = &stbl->stn_slot[MAX_STNS*i];
	for (j = 0; j < MAX_STNS; j++, se++) {
		ASSERT(se != 0);
		if (!bcmp(addr, se->pmacaddr, 6))
			break;
	}
	if (j >= MAX_STNS)
		j = -1;
	return(((i+1)<<8)+(j+1));
}
