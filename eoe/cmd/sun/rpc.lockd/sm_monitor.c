#if 0
static char sccsid[] = "@(#)sm_monitor.c	1.3 90/11/09 NFSSRC4.0 1.11 88/02/07 Copyr 1984 Sun Micro";
#endif

	/*
	 * Copyright (c) 1988 by Sun Microsystems, Inc.
	 */

	/*
	 * sm_monitor.c:
	 * simple interface to status monitor
	 */

#include <syslog.h>
#include <string.h>
#include <bstring.h>
#include <netdb.h>
#include <sys/param.h>
#include "prot_lock.h"
#include <rpcsvc/sm_inter.h>
#include "priv_prot.h"
#include "sm_res.h"

#define LM_UDP_TIMEOUT 15
#define RETRY_NOTIFY	10

extern int debug;
extern char hostname[];

struct stat_res *
stat_mon(sitename, svrname, my_prog, my_vers, my_proc, func, len, priv)
	char *sitename;
	char *svrname;
	int my_prog, my_vers, my_proc;
	int func;
	int len;
	char *priv;
{
	static struct stat_res Resp;
	static sm_stat_res resp;
	mon mond, *monp;
	mon_id *mon_idp;
	my_id *my_idp;
	char *svr;
 	int 	mon_retries;	/* Times we have tried to contact statd */
	int rpc_err;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *ip;
	int valid;

	if (debug)
		printf("enter stat_mon: sitename=%s svrname=%s func=%d\n",
			sitename, svrname, func);
	monp = &mond;
	mon_idp = &mond.mon_id;
	my_idp = &mon_idp->my_id;
	bzero(monp, sizeof (mon));

	if (svrname == NULL)
		svrname = hostname;
	if ((svr = strdup(svrname)) == NULL || 
	    (sitename && (mon_idp->mon_name = strdup(sitename)) == NULL) ||
	    (my_idp->my_name = strdup(hostname)) == NULL) {
		syslog(LOG_ERR, "stat_mon: can't copy names");
		Resp.res_stat = stat_fail;
		goto done;
	}


	my_idp->my_prog = my_prog;
	my_idp->my_vers = my_vers;
	my_idp->my_proc = my_proc;
	if (len > SM_PRIVLEN) {
		syslog(LOG_ERR, "stat_mon: len(=%d) is greater than SM_PRIVLEN!", len);
		Resp.res_stat = stat_fail;
		return (&Resp);
	}
	if (len > 0) {
		bcopy(priv, monp->priv, len);
	}

	switch (func) {
	case SM_STAT:
		xdr_argument = xdr_sm_name;
		xdr_result = xdr_sm_stat_res;
		ip =  (char *) &mon_idp->mon_name;
		break;

	case SM_MON:
		xdr_argument = xdr_mon;
		xdr_result = xdr_sm_stat_res;
		ip = (char *)  monp;
		break;

	case SM_UNMON:
		xdr_argument = xdr_mon_id;
		xdr_result = xdr_sm_stat;
		ip =  (char *) mon_idp;
		break;

	case SM_UNMON_ALL:
		xdr_argument = xdr_my_id;
		xdr_result = xdr_sm_stat;
		ip = (char *) my_idp;
		break;

	case SM_SIMU_CRASH:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		ip = NULL;
		break;

	default:
		syslog(LOG_ERR, "stat_mon proc(%d) not supported", func);
		Resp.res_stat = stat_fail;
		return (&Resp);
	}

	if (debug)
		printf("request monitor: svr=%s my_name=%s\n",
			svr, my_idp->my_name);
	valid = 1;
	mon_retries = 0;

again:

	if ((rpc_err = call_udp(svr, SM_PROG, SM_VERS, func, xdr_argument, ip,
		xdr_result, &resp, valid, LM_UDP_TIMEOUT)) != (int) RPC_SUCCESS) {
		if (rpc_err == (int) RPC_TIMEDOUT) {
			if (debug)
			   printf("timeout, retry contacting status monitor\n");
			mon_retries++;
			if (mon_retries <= RETRY_NOTIFY) {
			    valid = 0;
			    goto again;
			}
			syslog(LOG_ERR,"Cannot contact status monitor!");
		} else if (debug) {
			printf("stat mon call failed: %s\n",
				clnt_sperrno(rpc_err));
		}
		Resp.res_stat = stat_fail;
		Resp.u.rpc_err = rpc_err;
	} else {
		Resp.res_stat = stat_succ;
		Resp.u.stat = resp;
	}
done:
	if (mon_idp->mon_name)
		xfree(&mon_idp->mon_name);
	if (my_idp->my_name)
		xfree(&my_idp->my_name);
	if (svr)
		xfree(&svr);
	return (&Resp);
}

void
cancel_mon(void)
{
	struct stat_res *resp;

	resp = stat_mon(NULL, hostname, PRIV_PROG, PRIV_VERS, PRIV_CRASH, SM_UNMON_ALL, NULL, NULL);
	if (resp->res_stat == stat_fail)
		return;
	(void) stat_mon(NULL, hostname, PRIV_PROG, PRIV_VERS,
			PRIV_RECOVERY, SM_UNMON_ALL, NULL, NULL);
	(void) stat_mon(NULL, NULL, 0, 0, 0, SM_SIMU_CRASH, NULL, NULL);
}
