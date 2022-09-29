/*
 *	(C) COPYRIGHT CRAY RESEARCH, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */

#include	"errno.h"
#include        "sys/types.h"
#include        "sys/mount.h"
#include        "sys/buf.h"
#include        "sys/map.h"
#include        "sys/conf.h"
#include        "sys/fsid.h"
#include        "sys/fstyp.h"
#include        "limits.h"
#include        "sys/fcntl.h"
#include        "sys/flock.h"
#include <dg.h>
#include <dgpkt.h>
#include <dgsct.h>
#include <dgcall.h>
#include <dgscall.h>
#include <dgccall.h>
#include <dgccallt.h>
#include <dgslsn.h>
#include <dgclsn.h>
#include <comfwd.h>
#include <dgfwd.h>
#include <dgxq.h>

#include <dce/conv.h>
#include <dce/convc.h>
#include <dce/ep.h>
#include <dce/mgmt.h>

#include	"comprot.h"
#include	"comnetp.h"
#include	"sys/idbgentry.h"

/*
*  listener_state not exported
extern rpc_listener_state_t listener_state;
*/


int	rpc_PrintedOne;		/* Flag to say we found entries */

/*
 *	Print out a uuid_t as defined in dce/nbase.h
 */
void
pr_rpc_lstate (
	rpc_listener_state_t *lstate, 
	int verboseLevel, 
	char *spstrin)
{
   int i;
   char *spstr;

   spstr=spstrin;

   qprintf("%slistener state : mutex *0x%x, cond*0x%x\n",spstr,
		&lstate->mutex,  &lstate->cond);
   qprintf ("%slistener state : num_desc=%d high_water=%d status=%d reload=%d\n",
                        spstr, lstate->num_desc, lstate->high_water, 
				lstate->status, lstate->reload_pending);
   qprintf("%slistener state : Maximum sockets=%d socks[0]=0x%x\n",spstr,
		RPC_C_SERVER_MAX_SOCKETS,  &lstate->socks);
   
   for (i=0; i<RPC_C_SERVER_MAX_SOCKETS; i++) {
	qprintf("%s desc=socket *=0x%x ", spstr, lstate->socks[i].desc);
	if (lstate->socks[i].busy) {
		qprintf(" busy ");
	}
	if (lstate->socks[i].is_server) {
		qprintf(" is_server ");
	}
	if (lstate->socks[i].is_dynamic) {
		qprintf(" is_dynamic ");
	}
	if (lstate->socks[i].is_active) {
		qprintf(" is_active ");
	}
	qprintf("\n");
   }

}

void
idbg_lstate(__psint_t addr)
{
   int          addrset=0;
   char         spstr[40];
   int		verboseLevel=0;


   spstr[0]='\0';
   if (addr != 0) {
      addrset++;
   }

   rpc_PrintedOne = 0;
   if (addrset) {
      ++rpc_PrintedOne;
      strcat (spstr, "   ");
      qprintf("%sRPC_LSTATE at addr=0x%x\n",spstr, addr);
      pr_rpc_lstate ((rpc_listener_state_t *)addr,
                verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   } else {
      ++rpc_PrintedOne;
      qprintf("listener_state not exported.  Find address using\n");
      qprintf(" 'hx listener_state' \n");
      qprintf("Then use 'rpc_lstate <addr>'\n");
   } 

   if (!rpc_PrintedOne) {
      qprintf ("No listener state.\n");
   }

   return;

}

