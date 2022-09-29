/*
 *	(C) COPYRIGHT CRAY RESEARCH, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */

#include	"sgidefs.h"

#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include	<sys/socket.h>

#include <netdb.h>
#include	"dce/nbase.h"
#include	"dcedfs/common_data.h"


void
idbg_krpc_help(__psint_t addr)
{
	qprintf("Give commands a 0 to get more inforamtion about a particular one\n\n");
	qprintf("rpc_lstate		Dumps listener_state\n");
	qprintf("rpc_cct 0 		Dumps client call table\n");
	qprintf("rpc_cct <addr>		Dumps one ccte\n");
	qprintf("rpc_ccall\n");
	qprintf("rpc_call\n");
	qprintf("rpc_sct\n");
	qprintf("rpc_rqe\n");
	qprintf("rpc_scall\n");
	qprintf("rpc_helper 0		Dumps global information\n");
	qprintf("rpc_helper_queues      Dumps entries on the pending and in_service queues\n");
	qprintf("rpc_help		This help information\n");
}


char *
get_host_name(struct in_addr *ip)
{
      struct hostent *host;
#if 0
      host = gethostbyaddr((char *)ip, sizeof(struct in_addr),
			   AF_INET);
      return(host->h_name);
#endif
      return (NULL);
}



/*
 *	Print out a uuid_t as defined in dce/nbase.h
 *	Also used by dfs idbg commands.
 */
void
pr_uuid_ent (
uuid_t  *uuid,
int     verboseLevel,
char    *spstrin)
{
   char *spstr;
   spstr = spstrin;

   /* Mark we got on if we havn't already (for entry fm "tkm")*/

   qprintf ( "%sUUID: %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n", spstr,
            uuid->time_low, uuid->time_mid, uuid->time_hi_and_version,
            uuid->clock_seq_hi_and_reserved, uuid->clock_seq_low,
            (unsigned8) uuid->node[0], (unsigned8) uuid->node[1],
            (unsigned8) uuid->node[2], (unsigned8) uuid->node[3],
            (unsigned8) uuid->node[4], (unsigned8) uuid->node[5]);
}

#if 0
/*
 *	Print out sockaddr information.
 */
void
pr_sockaddr_in_ent (
struct sockaddr_in  *saent,
int     verboseLevel,
char    *spstrin)
{
   char *spstr;
   struct  in_addr *sin_addr=&saent->sin_addr;
   struct hostent *host;

   spstr = spstrin;
   if (saent) qprintf ("%sSOCKADDR_IN at %#x:", spstr, (struct sockaddr_in *)saent);
   else  qprintf ("%sSOCKADDR_IN: ", spstr);

   qprintf ("s_addr=%d.%d.%d.%d", 
      (sin_addr->s_addr >> 24) & 0xFF,
      (sin_addr->s_addr >> 16) & 0xFF,
      (sin_addr->s_addr >>  8) & 0xFF,
       sin_addr->s_addr        & 0xFF);

   host = gethostbyaddr((char *)sin_addr, sizeof(struct in_addr), AF_INET);
   if (host) {
	qprintf(" (%s)\n", host->h_name);
   } else {
	putchar('\n');
   }
}
#endif

#if 0
void
print_afsfid(afsFid  *id, char *prefix)
{
    qprintf("%sFID: Cell/Vol=%lx.%lx.%lx.%lx Vnode=%#lx Unique=%#lx\n",
           prefix, id->Cell.high, id->Cell.low,
           id->Volume.high, id->Volume.low,
           id->Vnode, id->Unique);
}


#endif

