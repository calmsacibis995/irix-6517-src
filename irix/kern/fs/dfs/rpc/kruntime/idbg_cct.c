/*
 *	(C) COPYRIGHT CRAY RESEARCH, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */

#include	"errno.h"
#include        "sys/vfs.h"
#include        "sys/vnode.h"
#include        "sys/mount.h"
#include        "sys/buf.h"
#include        "sys/map.h"
#include        "sys/conf.h"
#include        "sys/fsid.h"
#include        "sys/fstyp.h"
#include        "limits.h"
#include        "sys/fcntl.h"
#include        "sys/flock.h"
#include	"sys/idbgentry.h"

#include 	"dce/ker/pthread.h"
#include 	"dce/nbase.h"
#include 	"dg.h"

extern int rpc_PrintedOne;		/* Flag to say we found entries */
extern  char *token();

extern void
pr_uuid_ent (
uuid_t  *uuid,
int     verboseLevel,
char    *spstrin);

/*
 *	Print a single entry
 */
pr_rpc_key_info (
rpc_key_info_t  *keyent,
int	verboseLevel,
char	*spstr)
{
   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;

   qprintf ("%sRPC_KEY_INFO_T AT ADDR=0x%x\n", spstr, keyent);
   strcat (spstr, "   ");

   qprintf ("%srefcnt=%d authn_level=0x%x is_server=0x%x ", spstr, 
      keyent->refcnt, keyent->authn_level, keyent->is_server);
   qprintf ("auth_info=0x%x\n", keyent->auth_info);

   spstr[strlen(spstr) - 3] = '\0';
   return(0);
}



/*
 *	Print a single entry
 */
pr_rpc_auth_info (
rpc_auth_info_t  *authent,
int	verboseLevel,
char	*spstr)
{

   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;

   qprintf ("%sRPC_AUTH_INFO AT ADDR=0x%x \n", spstr, authent);
   strcat (spstr, "   ");

   /* typedef struct
    * {
    *     rpc_list_t              cache_link;
    *     unsigned16              refcount;
    *     unsigned_char_p_t       server_princ_name;
    *     rpc_authn_level_t       authn_level;
    *     rpc_authn_protocol_id_t authn_protocol;
    *     rpc_authz_protocol_id_t authz_protocol;
    *     unsigned                is_server: 1;
    *     union
    *     {
    *         rpc_auth_identity_handle_t  auth_identity; #If !"is_server"
    *         rpc_authz_handle_t          privs;	 #If "is_server"
    *           (typedef idl_void_p_t rpc_authz_handle_t)
    *     } u;
    * } rpc_auth_info_t, *rpc_auth_info_p_t;
    */ 

   qprintf ("%srefcnt=%d is_server=0x%x\n", spstr, 
      authent->refcount, authent->is_server);

   if (authent->server_princ_name) {
         qprintf ("%sserver_princ_name=0x%x\n%s   %s\n", spstr, 
	    authent->server_princ_name, spstr, authent->server_princ_name);
   } else {
      qprintf ("%sserver_princ_name=0x%x\n", spstr, authent->server_princ_name);
   }


   qprintf ("%sauthn_level=%d authn_protocol=%d authz_protocol=%d\n", spstr, 
      authent->authn_level, authent->authn_protocol, 
      authent->authz_protocol);

   spstr[strlen(spstr) - 3] = '\0';
   return(0);
}

/*
 *	Print a single cct entry
 */
pr_ccte_entry (
rpc_dg_cct_elt_t  *ccte,
int	verboseLevel,
char	*spstrin)
{
   char			*spstr;

   /* Set up the spstr */
      spstr = spstrin;

   /* Mark we got one */
   if (!rpc_PrintedOne) ++rpc_PrintedOne;

   qprintf ("%sRPC_DG_CCT_ELT_P_T AT ADDR=0x%x next=0x%x\n", 
      spstr, ccte, ccte->next);
   strcat (spstr, "   ");

   /* typedef struct rpc_dg_cct_elt_t {
    *    struct rpc_dg_cct_elt_t *next;       * -> next in hash chain
    *    rpc_auth_info_p_t auth_info;         * auth info to be used with server
    *    rpc_key_info_p_t key_info;           * key to use
    *    struct rpc_dg_auth_epv_t *auth_epv;  * auth epv
    *    uuid_t actid;                        * activity ID to use in msgs to server
    *    unsigned32 actid_hash;               * uuid_hash(.actid)
    *    unsigned32 seq;                      * seq # to use in next call to server
    *    rpc_clock_t timestamp;               * last time connection used in a call
    *    unsigned8 refcnt;                    * # of references to the CCTE
    * } rpc_dg_cct_elt_t, *rpc_dg_cct_elt_p_t;
    */
   
   qprintf ("%srefcnt=%d, last time used = 0x%x\n", spstr, ccte->refcnt,
		(int)ccte->timestamp);
   qprintf ("%sauth_epv=0x%x actid_hash=%d seq=%d\n", spstr, 
      ccte->auth_epv, ccte->actid_hash, ccte->seq);
   pr_uuid_ent (&ccte->actid, verboseLevel, spstr);

   if (ccte->auth_info) {
         pr_rpc_auth_info (ccte->auth_info, verboseLevel, spstr);
   }
   else {
      qprintf ("%sauth_info=0x%x\n", spstr, ccte->auth_info);
   }

   if (ccte->key_info) {
         pr_rpc_key_info (ccte->key_info, verboseLevel, spstr);
   } else {
      qprintf ("%skey_info=0x%x\n", spstr, ccte->key_info);
   }
   spstr[strlen(spstr) - 3] = '\0';
   return(0);
}

/*
 *	Print all cct entries
 */
void
pr_all_cct_entries (
int	verboseLevel,
char	*spstr)
{
   rpc_dg_cct_elt_p_t	ccte_p;			/* Byte offset of cct entry */
   int			i;
   void 	*addr;

   /*
    * #define RPC_DG_CCT_SIZE 29
    * 
    * typedef struct rpc_dg_cct_t {
    *     unsigned32 gc_count;
    *     struct {
    *         rpc_dg_cct_elt_p_t first;
    *         rpc_dg_cct_elt_p_t last;
    *     } t[RPC_DG_CCT_SIZE];
    * } rpc_dg_cct_t;
    */

   addr = &rpc_g_dg_cct;
   ++rpc_PrintedOne;
   qprintf ("%sGlobal cct table: 0x%x, Maximum of %d entries.\n", 
		spstr, &rpc_g_dg_cct, RPC_DG_CCT_SIZE);
   qprintf ("%sgc_count=%d\n\n", spstr, (int)rpc_g_dg_cct.gc_count); 

   if (rpc_g_dg_cct.gc_count == 0) {
	qprintf("No entries\n");
        return;
   }
   for (i = 0; i < RPC_DG_CCT_SIZE; i++) {
      ccte_p = rpc_g_dg_cct.t[i].first;
      qprintf("\n%sEntries from rpc_g_dg_cct.t[%d]\n", spstr, i);
      while (ccte_p != NULL) {
        pr_ccte_entry (ccte_p, verboseLevel, spstr);
	ccte_p = ccte_p->next;
      }
   }
   return;
}




void
idbg_dfs_cct(__psint_t addr)
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
      qprintf("Printing single cct entry from 0x%x\n", addr);
      strcat (spstr, "   ");
      pr_ccte_entry ((rpc_dg_cct_elt_t *)addr, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }
   else {
      strcat (spstr, "   ");
      pr_all_cct_entries(verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   if (!rpc_PrintedOne) {
      qprintf ("No active cct entries.\n");
   }

   return;


}
