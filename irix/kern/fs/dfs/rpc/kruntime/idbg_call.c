/*
 *	(C) COPYRIGHT CRAY RESEARCH, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */

#include        "errno.h"
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

#include <sys/idbg.h>
#include <sys/idbgentry.h>
extern int rpc_PrintedOne;		/* Flag to say we found entries */

pr_rpc_dg_recvq_elt  (
rpc_dg_recvq_elt_t *ent,
int     verboseLevel,
char    *spstr)
{

  while (ent) {

    qprintf ("%sRPC_DG_RECVQ_ELT_T AT ADDR=0x%x\n", spstr, ent);
    strcat (spstr, "   ");

    qprintf ("%snext=0x%x more_data=0x%x\n", spstr,
	ent->next, ent->more_data);
    qprintf ("%shdrp=%x *hdr=0x%x sock=%x\n", spstr,
	ent->hdrp, ent->hdr, ent->sock);
    qprintf ("%sfrag_len=%d from_len=%d pkt_len=%d\n", spstr,
	ent->frag_len, ent->from_len, ent->pkt_len);
    qprintf ("%sfrom*=%d pkt *=0x%x \n", spstr,
	ent->from, ent->pkt);
#if 0	/* INLINE additions */
    qprintf ("%spkt_hdr=%x pkt_body=%x mbuf=%x\n", spstr,
	ent->pkt_hdr, ent->pkt_body, ent->mbuf);
#endif

    spstr[strlen(spstr) - 3] = '\0';
    ent =  ent->next;
  }
  return(0);
}


void
idbg_pr_rpc_dg_recvq_elt  (__psint_t addr)
{
rpc_dg_recvq_elt_t *ent;
int     verboseLevel = 0;
char	spstr[40];
  spstr[0]='\0'; 
  pr_rpc_dg_recvq_elt((rpc_dg_recvq_elt_t *)addr, verboseLevel, spstr);
};

/*
 *	Print the xmit queue
 */
pr_rpc_addr (
rpc_addr_p_t  ent,
int     verboseLevel,
char    *spstr)
{
   struct hostent *host;
   rpc_auth_info_p_t authent;
   struct ad { 
      int port; 
      char ip[4];
   } *ads;

   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;
   strcat (spstr, "   ");

   /* typedef struct
    * {
    *   rpc_protseq_id_t        rpc_protseq_id;
    *   unsigned32              len;
    *   sockaddr_t              sa;
    * } *rpc_addr_p_t;
    */ 

   qprintf ("%srpc_protseq_id=0x%x len=%d\n", spstr, 
     ent->rpc_protseq_id, ent->len);
   if (ent->sa.family == AF_INET) {
      ads = (struct ad *)ent->sa.data;
      qprintf ("%ssa.family=AF_INET port=%d addr=%d.%d.%d.%d", spstr,
	 ads->port, ads->ip[0], ads->ip[1], ads->ip[2], ads->ip[3]); 

#if 0
      host = gethostbyaddr((char *)ads->ip, sizeof(struct in_addr),
			   AF_INET);
      if (host) {
	qprintf(" (%s)\n", host->h_name);
      } else {
	qprintf("\n");
      }
#endif
   }
   else {
      qprintf ("%ssa.family=%d\n", spstr, (u_int)ent->sa.family); 
      qprintf ("%sUnknown data format 0x%x\n", spstr, ent->sa.data); 
   }

   spstr[strlen(spstr) - 3] = '\0';
   return(0);
}



pr_rpc_dg_xmitq_elt  (
rpc_dg_xmitq_elt_t	*ent,
int     verboseLevel,
char    *spstr)
{

   while(ent) {

    qprintf ("%sRPC_DG_XMITQ_ELT_T AT ADDR=0x%x\n", spstr, ent);
    strcat (spstr, "   ");

    qprintf ("%snext=0x%x next_rexmit=0x%x more_data=0x%x\n", spstr,
	ent->next, ent->next_rexmit, ent->more_data);
    qprintf ("%sfrag_len=%d flags=0x%x fragnum=%d serial_num=%d\n", spstr,
	ent->frag_len, ent->flags, ent->fragnum, ent->serial_num);
#if 0
    qprintf ("%sbody_len=%d body_size=%d body=%x\n", spstr,
	ent.body_len, ent.bodysize, ent.body);
    qprintf ("%smbuf=%x funny_mbuf=%x\n", spstr,
	ent.mbuf, ent.funny_mbuf);
#endif

    spstr[strlen(spstr) - 3] = '\0';
    ent = ent->next;
  }
  return(0);
}


/*
 *	Print the xmit queue
 */
pr_rpc_dg_xmitq (
rpc_dg_xmitq_t  *ent,
int     verboseLevel,
char    *spstr)
{
   rpc_auth_info_p_t authent;
   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;

   qprintf ("%sRPC_DG_XMITQ_T AT ADDR=%o\n", spstr, ent);
   strcat (spstr, "   ");

   /* typedef struct {
    *   rpc_dg_xmitq_elt_p_t head;          * -> head pktq element 
    *   rpc_dg_xmitq_elt_p_t first_unsent;  * -> 1st unsent pkt in the queue 
    *   rpc_dg_xmitq_elt_p_t tail;          * -> tail pktq element 
    *   rpc_dg_xmitq_elt_p_t rexmitq;       * -> pkts to be resent 
    *   rpc_dg_xmitq_elt_p_t part_xqe;      * partially filled xmit queue element 
    *   rpc_dg_pkt_hdr_t hdr;               * prototype pkt hdr 
    *   rpc_clock_t awaiting_ack_timestamp; * when awaiting_ack was set 
    *   rpc_clock_t timestamp;              * last (re)transmission time 
    *   rpc_clock_t rexmit_timeout;         * time until next retransmission time 
    *   unsigned16 base_flags;              * header flags constants for all pkts 
    *   unsigned16 base_flags2;             * header flags constants for all pkts 
    *   unsigned16 next_fragnum;            * next fragnum to use 
    *   unsigned16 next_serial_num;         * unique serial # sent in each pkt   
    *   unsigned16 last_fack_serial;        * serial # that induced last recv'd fack 
    *   unsigned16 window_size;             * receiver's window size (in pkts) 
    *   unsigned16 cwindow_size;            * congestion window size (in pkts) 
    *   unsigned32 max_tsdu;                * max pkt size by LOCAL transport API 
    *   unsigned32 max_path_tpdu;           * local max pkt size for entire path 
    *   unsigned32 max_pkt_size;            * bounded by min of tpdu and path_tpdu 
    *   unsigned8 blast_size;               * # of pkts to send as a blast 
    *   unsigned8 max_blast_size;           * limit to the above value 
    *   unsigned16 xq_timer;                * blast size code to delay adjustments 
    *   unsigned16 xq_timer_throttle;       * multiplier to timer, to increase delays 
    *   unsigned8 high_cwindow;             * highest value of .cwindow_size so far 
    *   unsigned8 freqs_out;                * # of outstanding fack requests 
    *   unsigned push: 1;                   * T => don't keep at least one pkt on queue 
    *   unsigned awaiting_ack: 1;           * T => we are waiting for a xq ack event 
    * } rpc_dg_xmitq_t, *rpc_dg_xmitq_p_t;
    */
   qprintf ("%shead=0x%x first_unsent=0x%x tail=0x%x\n", spstr, 
      ent->head, ent->first_unsent, ent->tail);
   qprintf ("%srexmitq=0x%x part_xqe=0x%x hdr=0x%x\n", spstr, 
      ent->rexmitq, ent->part_xqe, 
      ent->hdr);
   qprintf ("%sawaiting_ack_ts=0x%x timestamp=0x%x rexmit_timeout=0x%x\n", spstr, 
      ent->awaiting_ack_timestamp, ent->timestamp, ent->rexmit_timeout);
   qprintf ("%sbase_flags=0x%x base_flags2=0x%x next_fragnum=%d\n", spstr, 
      ent->base_flags, ent->base_flags2, ent->next_fragnum);
   qprintf ("%snext_serial_num=%d last_fack_serial=%d\n", spstr, 
      ent->next_serial_num, ent->last_fack_serial);
   qprintf ("%swindow_size=%d cwindow_size=%d\n", spstr, 
         ent->window_size, ent->cwindow_size);
   qprintf ("%smax_rcv_tsdu=%d max_snd_tsdu=%d\n", spstr, 
      ent->max_rcv_tsdu, ent->max_snd_tsdu);
   qprintf ("%smax_frag_size=%d snd_frag_size=%d\n", spstr, 
      ent->max_frag_size, ent->snd_frag_size);
   qprintf ("%sblast_size=%d max_blast_size=%d xq_timer=0x%x\n", spstr, 
      ent->blast_size, ent->max_blast_size, ent->xq_timer);
   qprintf ("%sxq_timer_throttle=%d high_cwindow=%d freqs_out=%d\n", spstr, 
      ent->xq_timer_throttle, (u_int)ent->high_cwindow, (u_int)ent->freqs_out);
   qprintf ("%spush=%d awaiting_ack=%d\n", spstr, 
      (u_int)ent->push, (u_int)ent->awaiting_ack);

   pr_rpc_dg_xmitq_elt(ent->head, verboseLevel, spstr);
   spstr[strlen(spstr) - 3] = '\0';
   return(0);
}

/*
 *	Print the xmit queue
 */
pr_rpc_dg_recvq (
rpc_dg_recvq_t  *ent,
int     verboseLevel,
char    *spstr)
{
   rpc_auth_info_p_t authent;
   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;

   qprintf ("%sRPC_DG_RECVQ_T AT ADDR=0x%x\n", spstr, ent);
   strcat (spstr, "   ");

   /* typedef struct {
    *   rpc_dg_recvq_elt_p_t head;          * -> head pktq element *
    *   rpc_dg_recvq_elt_p_t last_inorder;  * -> last in-order frag in the queue *
    *   unsigned16 next_fragnum;            * next in-order frag we want to see *
    *   unsigned16 high_fragnum;            * highest numbered fragnum so far *
    *   unsigned16 high_serial_num;         * highest serial number seen so far *
    *   unsigned16 window_size;             * our receive window size (in pkts) *
    *   unsigned16 wake_thread_qsize;       * # of recvqe's to q bef wak thread *
    *   unsigned8 queue_len;                * total frags on queue *
    *   unsigned8 inorder_len;              * total inorder frags on queue *
    *   unsigned recving_frags: 1;          * T => we are receiving frags *
    *   unsigned all_pkts_recvd: 1;         * T => we've recv'd all data pkts *
    *   unsigned is_way_validated: 1;       * T => we've passed the WAY check *
    *   } rpc_dg_recvq_t, *rpc_dg_recvq_p_t;
    */

   qprintf ("%shead=0x%x last_inorder=0x%x\n", spstr, 
      ent->head, ent->last_inorder);
   qprintf ("%snext_fragnum=%d high_fragnum=%d high_serial_num=%d\n", spstr, 
      ent->next_fragnum, ent->high_fragnum, ent->high_serial_num);
   qprintf ("%swindow_size=%d wake_thread_qsize=%d queue_len=%d max_queue_len=%d\n", spstr, 
      ent->window_size, ent->wake_thread_qsize, (u_int)ent->queue_len,
      ent->max_queue_len);
   qprintf ("%shigh_rcv_frag_size=%d inorder_len=%d recving_frags=%d\n",
      spstr, (u_int)ent->high_rcv_frag_size,
      (u_int)ent->inorder_len, (u_int)ent->recving_frags);
   qprintf ("%sall_pkts_recvd=%d is_way_validated=0x%x\n", spstr, 
      (u_int)ent->all_pkts_recvd, (u_int)ent->is_way_validated);

   pr_rpc_dg_recvq_elt(ent->head, verboseLevel, spstr);

   spstr[strlen(spstr) - 3] = '\0';
   return(0);
}

/*
 *	Print a single entry
 */
pr_call_entry (
rpc_dg_call_t  *callent,
int	verboseLevel,
char	*spstr)
{
   rpc_auth_info_p_t authent;
#if 0
   rpc_dg_sock_pool_elt_t	sock_elt;
#endif

   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;

   qprintf ("%sRPC_DG_CALL_T AT ADDR=0x%x\n", spstr, callent);
   strcat (spstr, "   ");

   /* typedef struct rpc_dg_call_t {
    *     rpc_call_rep_t c;                   
    *     struct rpc_dg_call_t *next;         
    *     rpc_dg_call_state_t state;          
    *     unsigned32 status;                  
    *     rpc_clock_t state_timestamp;        
    *     rpc_cond_t cv;                      
    *     rpc_dg_xmitq_t xq;                  
    *     rpc_dg_recvq_t rq;                  
    *     struct rpc_dg_sock_pool_elt_t *sock_ref;   
    *     unsigned32 actid_hash;              
    *     rpc_key_info_p_t key_info;       
    *     struct rpc_dg_auth_epv_t *auth_epv; 
    *     rpc_addr_p_t addr;                  
    *     rpc_timer_t timer;                  
    *     rpc_clock_t last_rcv_timestamp;     
    *     rpc_clock_t start_time;             
    *     unsigned32 high_seq;                
    *     struct rpc_dg_call_t *pkt_chain;    
    *     unsigned32 com_timeout_knob;        
    *     unsigned8 refcnt;                   
    *     unsigned stop_timer: 1;             
    *     unsigned is_cbk: 1;                 
    *     unsigned : 0;                       
    *     unsigned is_in_pkt_chain: 1;        
    *     unsigned : 0;                       
    * } rpc_dg_call_t, *rpc_dg_call_p_t;
    */


   qprintf ("%snext=0x%x \n", spstr, callent->next);
   qprintf ("%sstate=%d ", spstr, callent->state);
   switch ((unsigned int)callent->state) {
      case rpc_e_dg_cs_init:
	 qprintf (" rpc_e_dg_cs_init (initialized and in use)\n");
	 break;
      case rpc_e_dg_cs_xmit:
	 qprintf (" rpc_e_dg_cs_xmit (currently sending data)\n");
	 break;
      case rpc_e_dg_cs_recv:
	 qprintf (" rpc_e_dg_cs_recv (awaiting data)\n");
	 break;
      case rpc_e_dg_cs_final:
	 qprintf (" rpc_e_dg_cs_final (delayed ack pending)\n");
	 break;
      case rpc_e_dg_cs_idle:
	 qprintf (" rpc_e_dg_cs_idle (call NOT in use)\n");
	 break;
      case rpc_e_dg_cs_orphan:
	 qprintf (" rpc_e_dg_cs_orphan (call orphaned; waiting to exit)\n");
	 break;
      default:
	 qprintf (" rpc_e_dg_cs_xmit (BAD STATE CODE!!)\n");
	 break;
   }
   qprintf ("%sstatus=%d state_timestamp=0x%x cv=0x%x\n", spstr,
      (int)callent->status, (int)callent->state_timestamp, 
      &callent->cv);
   qprintf ("%ssock_ref=0x%x actid_hash=0x%d key_info=0x%x\n", spstr,
      callent->sock_ref, callent->actid_hash, callent->key_info);
#if 0	/* Security label stuff... */
	sock_ent = callent->sock_ref;
	qprintf ("%scurrent socket level: %d, min/max: %d/%d\n", spstr,
		callent->sock_ref->sock->so_actlabel.lt_level,
		callent->sock_ref.so_minlabel.lt_level,
		callent->sock_ref.so_maxlabel.lt_level);
	qprintf ("%scompartment: %x, valcmp: %x\n", spstr,
		callent->sock_ref.so_actlabel.lt_compart,
		callent->sock_ref.so_maxlabel.lt_compart);
#endif

   qprintf ("%sauth_epv=0x%x timer=0x%x\n", spstr,
      callent->auth_epv, callent->timer);

   qprintf ("%slast_rcv_timestamp=0x%x start_time=0x%x high_seq=0x%x\n", spstr,
      callent->last_rcv_timestamp, callent->start_time, callent->high_seq);
   qprintf ("%spkt_chain=0x%x com_timeout_knob=0x%x refcnt=0x%x\n", spstr,
      callent->pkt_chain, callent->com_timeout_knob, (u_int)callent->refcnt);
   qprintf ("%sstop_timer=0x%x is_cbk=0x%x\n", spstr,
      (u_int)callent->stop_timer, (u_int)callent->is_cbk);
   qprintf ("%sn_resvs=%d n_resvs_wait=%d max_resvs=%d\n", spstr,
      callent->n_resvs, callent->n_resvs_wait, callent->max_resvs);
   qprintf ("%sis_in_pkt_chain=0x%x\n", spstr, (u_int)callent->is_in_pkt_chain);

#if 0	/* Security label stuff... */
   qprintf ("%scurrent call level: %d, min/max: %d/%d\n", spstr,
	callent->c.soc_labels.ss_actlabel.lt_level,
	callent->c.soc_labels.ss_minlabel.lt_level,
	callent->c.soc_labels.ss_maxlabel.lt_level);
   qprintf ("%scompartment: %x, valcmp: %x\n", spstr,
	callent->c.soc_labels.ss_actlabel.lt_compart,
	callent->c.soc_labels.ss_maxlabel.lt_compart);
#endif

     qprintf ("%saddr at 0x%x:\n", spstr, callent->addr);
     pr_rpc_addr (callent->addr, verboseLevel, spstr);


   pr_rpc_dg_xmitq ( &callent->xq, verboseLevel, spstr); 
   pr_rpc_dg_recvq ( &callent->rq, verboseLevel, spstr); 

  spstr[strlen(spstr) - 3] = '\0';
  return(0);
}

void
idbg_pr_call_entry(__psint_t addr)
{
  int	verboseLevel=0;
  char	spstr[40];
  pr_call_entry((rpc_dg_call_t *)addr, verboseLevel, (char *)&spstr);
}

/*
 *	Print a single ccall entry
 */
pr_ccall_entry (
rpc_dg_ccall_t  *ccall,
int	verboseLevel,
char	*spstrin)
{
   char			*spstr;

   /* Set up the spstr */
      spstr = spstrin;

   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;

   qprintf ("%sRPC_DG_CCALL_T AT addr=0x%x\n", spstr, ccall);
   strcat (spstr, "   ");

   /* 
    * typedef struct rpc_dg_ccall_t {
    *     rpc_dg_call_t c;                
    *     rpc_dg_recvq_elt_t *fault_rqe; 
    *     unsigned32 reject_status;       
    *     unsigned cbk_start: 1;          
    *     unsigned response_info_updated: 1; 
    *     unsigned server_bound: 1;       
    *     rpc_dg_scall_t *cbk_scall;     
    *     rpc_dg_ccte_ref_t ccte_ref;     
    *     struct rpc_dg_binding_client_t *h;
    *     rpc_dg_ping_info_t ping;        
    *     rpc_dg_quit_info_t quit;        
    *     rpc_dg_cancel_info_t cancel;    
    *     rpc_clock_t timeout_stamp;      
    * } rpc_dg_ccall_t, *rpc_dg_ccall_p_t;
    */
   qprintf ("%sfault_rqe=0x%x   reject status=%d\n", spstr, 
		ccall->fault_rqe, ccall->reject_status);
   if (ccall->cbk_start) {
	qprintf("%sawakened to start a callback, cbk_start=1\n", spstr);
   }
   if (ccall->response_info_updated) {
	qprintf("%sahint, etc.. has been updated, response_info_updated=1\n",
		spstr);
   }
   if (ccall->server_bound) {
	qprintf("%sreal server address is known, server_bound=1\n", spstr);
   }
   qprintf ("%s cbk_scall=0x%x\n", spstr, ccall->cbk_scall);
   qprintf ("%sccte_ref.ccte=0x%x ccte_ref.gc_count=%d\n", spstr,
      ccall->ccte_ref.ccte, (int)ccall->ccte_ref.gc_count);
   qprintf ("%sh=0x%x timeout_stamp=0x%x\n", spstr, 
		ccall->h, ccall->timeout_stamp); 
   qprintf ("%sping.next_time=0x%x ping.start_time=0x%x\n", spstr,
      ccall->ping.next_time, ccall->ping.start_time);
   qprintf ("%sping.count=%d ping.pinging=%d\n", spstr,
      ccall->ping.count, (unsigned int)ccall->ping.pinging);

   qprintf ("%squit.next_time=0x%x quit.quack_rcvd=0x%x\n", spstr,
      ccall->quit.next_time, (unsigned int)ccall->quit.quack_rcvd);

   qprintf ("%scancel.next_time=0x%x cancel.local_count=0x%x\n", spstr,
      ccall->cancel.next_time, ccall->cancel.local_count);
   qprintf ("%scancel.server_count=0x%x cancel.timeout_time=0x%x\n", spstr,
      ccall->cancel.server_count, (unsigned int)ccall->cancel.timeout_time);
   qprintf ("%scancel.server_is_accepting=0x%x cancel.server_had_pending=0x%x\n", spstr,
      (unsigned int)ccall->cancel.server_is_accepting, 
      (unsigned int)ccall->cancel.server_had_pending);

   pr_call_entry( &ccall->c, verboseLevel, spstr);

   spstr[strlen(spstr) - 3] = '\0';
   return(0);
}


/*
 *	Print all ccall entries
 */
void
pr_all_ccall_entries (
rpc_dg_ccall_p_t ccall,
int	verboseLevel,
char	*spstr)
{
   int		i;
   ccall = rpc_g_dg_ccallt[0];

   while (ccall) {
	pr_ccall_entry (ccall, verboseLevel, spstr);
	ccall = (rpc_dg_ccall_p_t) ccall->c.next;
   }
   return;
}



void
idbg_dfs_ccall(__psint_t addr)
{
   int 		addrset=0;
   int		verboseLevel=0;
   char		spstr[40];


   spstr[0]='\0';

   if (addr != 0) {
      addrset++;
   }

   rpc_PrintedOne = 0;
   if (addrset) {
      strcat (spstr, "   ");
      pr_ccall_entry ((rpc_dg_ccall_t *)addr, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }
   else {
      strcat (spstr, "   ");
      pr_all_ccall_entries((rpc_dg_ccall_p_t)addr, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   if (!rpc_PrintedOne) {
      qprintf ("No active ccall entries.\n");
   }

   return;


}
