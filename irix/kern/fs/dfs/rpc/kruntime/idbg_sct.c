/*
 *	(C) COPYRIGHT CRAY RESEARCH, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */

#include 	"dce/ker/pthread.h"
#include 	"dce/nbase.h"
#include 	"dg.h"

#include <sys/idbg.h>
#include <sys/idbgentry.h>

extern int rpc_PrintedOne;		/* Flag to say we found entries */
extern  char *token();

extern void
pr_uuid_ent (
uuid_t  *uuid,
int     verboseLevel,
char    *spstrin);

extern pr_call_entry (
rpc_dg_call_t  *callent,
int     verboseLevel,
char    *spstr);


/*
 *	Print a single scall entry
 */
pr_scall_ent (
rpc_dg_scall_t  *ent,
int	verboseLevel,
char	*spstr)
{

   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;

   /* typedef struct rpc_dg_scall_t {
    *   rpc_dg_call_t c;                    * call handle common header *
    *   rpc_dg_recvq_elt_p_t fwd2_rqe;      * forwarded pkt *
    *   struct rpc_dg_sct_elt_t *scte;
    *   struct rpc_dg_ccall_t *cbk_ccall;   * -> callback paired ccall *
    *   struct rpc_dg_binding_server_t *h;  * binding handle *
    *   unsigned client_needs_sboot: 1;     * T => client needs to be WAY *
    *   unsigned call_is_setup: 1;          * T => "call executor" established *
    *   unsigned has_call_executor_ref: 1;  * T => "call executor" has ref *
    * } rpc_dg_scall_t, *rpc_dg_scall_p_t;
    */
   qprintf ("%sfwd2_rqe=%#x scte=%#x cbk_ccall=%#x\n", spstr,
      ent->fwd2_rqe, ent->scte, ent->cbk_ccall);
   qprintf ("%sh=%#x client_needs_sboot=%#d call_is_setup=%#d\n", spstr,
      ent->h, ent->client_needs_sboot, ent->call_is_setup);
   qprintf ("%shas_call_executor_ref=%#d\n", spstr,
      ent->has_call_executor_ref);
   pr_call_entry (&ent->c, verboseLevel, spstr);

   spstr[strlen(spstr) - 3] = '\0';
   return(0);
}

/*
 *	Print a single sct entry
 */
pr_sct_entry (
rpc_dg_sct_elt_t  *ent,
int	verboseLevel,
char	*spstrin)
{
   char			*spstr;

   spstr = spstrin;

   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;

   qprintf ("%sRPC_DG_SCT_ELT_T AT ADDR=%#x next=%#x\n", spstr, 
      ent, ent->next);
   strcat (spstr, "   ");

   /* typedef struct rpc_dg_sct_elt_t {
    *   struct rpc_dg_sct_elt_t *next;      * -> next in hash chain *
    *   uuid_t actid;                       * activity of of remote client *
    *   unsigned16 ahint;                   * uuid_hash(.actid) % RPC_DG_SCT_SIZE *
    *   unsigned32 high_seq;                * highest known seq *
    *   unsigned high_seq_is_way_validated: 1; * T => .high_seq is WAY validated *
    *   unsigned8 refcnt;                   * scte reference count *
    *   rpc_key_info_p_t key_info;       * key info to be used *
    *   struct rpc_dg_auth_epv_t *auth_epv; * auth epv *
    *   rpc_dg_scall_p_t scall;             * -> server call handle being used in call *
    *   rpc_clock_t timestamp;              * last time connection used in a call *
    *   rpc_dg_client_rep_p_t client;       * -> to client handle   *
    * } rpc_dg_sct_elt_t, *rpc_dg_sct_elt_p_t;
    */

   qprintf ("%sahint=%#x high_seq=%#d high_seq_is_way_validated=%#d\n", spstr, 
      ent->ahint, ent->high_seq, ent->high_seq_is_way_validated);
   qprintf ("%srefcnt=%#d key_info=%#x auth_epv=%#x\n", spstr, 
      ent->refcnt, ent->key_info, ent->auth_epv);
   qprintf ("%stimestamp=%#x client=%#x\n", spstr, 
      ent->timestamp, ent->client);
   pr_uuid_ent (&ent->actid, verboseLevel, spstr);

   if (ent->scall) {
         pr_scall_ent (ent->scall, verboseLevel, spstr);
   }
   else {
      qprintf ("%sscall=%#x timestamp=%#x client=%#x\n", spstr, 
         ent->scall, ent->timestamp, ent->client);
   }

   spstr[strlen(spstr) - 3] = '\0';
   return(0);
}

/*
 *	Print all sct entries
 */
void
pr_all_sct_entries (
int	verboseLevel,
char	*spstr)
{
   rpc_dg_sct_elt_t *sct_table;	/* Address of array table */
   int		i;
   rpc_dg_sct_elt_t	*sctent;

   sct_table = rpc_g_dg_sct[0];
   for (i=0 ; i < RPC_DG_SCT_SIZE ; i++) {
      sctent=&sct_table[i];
      while (sctent) {
	    pr_sct_entry (sctent, verboseLevel, spstr);
	    sctent = (sctent->next);
      }
   }
   return;
}



void
idbg_pr_scall_ent(__psint_t addr)
{
    int verboseLevel=0;
    char spstr[40];
    spstr[0]='\0';
    pr_scall_ent((rpc_dg_scall_t *)addr, verboseLevel, (char *)&spstr);

}

void
idbg_dfs_scall(__psint_t addr)
{
   int		verboseLevel=0;
   char		spstr[40];
   int 		addrset=0;

   spstr[0]='\0';

   if (addr != 0) {
      addrset++;
   }

   rpc_PrintedOne = 0;
   if (addrset) {
      strcat (spstr, "   ");
      pr_sct_entry ((rpc_dg_sct_elt_t *)addr, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }
   else {
      strcat (spstr, "   ");
      pr_all_sct_entries(verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   if (!rpc_PrintedOne) {
      qprintf ("No active sct entries.\n");
   }

   return;

usage:
   qprintf ("usage: sct [-[-[-[-]]]] [-g] [<addr>]\n");
   qprintf ("alias: \n");
   qprintf ("    -  	Increase verbose level\n");
   qprintf ("    -g	Print in format suitable for grepping\n");

}
