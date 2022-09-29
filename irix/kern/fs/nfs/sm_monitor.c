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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include "types.h"
#include "xdr.h"
#include "auth.h"
#include "clnt.h"
#include <rpcsvc/nlm_prot.h>
#include <rpcsvc/sm_inter.h>

#define RETRY_NOTIFY	10

static struct sockaddr_in sm_sa;

extern int first_retry;
extern int sm_timeout;

/*
 * call the local status monitor and simulate a crash
 */
void
cancel_mon(void)
{
	int error;
	enum clnt_stat clnt_stat;
	struct timeval timeout;
	CLIENT *client;
	int mon_retries = 0;

	sm_sa.sin_family = AF_INET;
	sm_sa.sin_port = 0;
	sm_sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	/*
	 * This is only done when lockd is started, so always get the
	 * port at least once.
	 */
	error = getport_loop(&sm_sa, (u_long)SM_PROG, (u_long)SM_VERS,
		(u_long)IPPROTO_UDP, 0);
	switch (error) {
		case 0:
			break;
		case EINTR:
		case ENOENT:
			/*
			 * The status monitor is not registered.  Just return
			 * in this case because, the status monitor will do
			 * what we want it to when it starts.
			 */
		case ETIMEDOUT:
			return;
		default:
			/*
			 * give up
			 */
			cmn_err(CE_WARN,
				"Failed to get port number for local status monitor, "
				"error %d.\n", error);
			return;
	}
	if ((client = clntkudp_create(&sm_sa, (u_long)SM_PROG, (u_long)SM_VERS,
		first_retry, KUDP_INTR, KUDP_XID_PERCALL, sys_cred)) == NULL) {
			return;
	}
	do {
		timeout.tv_sec = sm_timeout / 10;
		timeout.tv_usec = 100000 * (sm_timeout % 10);
		clnt_stat = CLNT_CALL(client, SM_SIMU_CRASH, xdr_void, NULL,
			xdr_void, NULL, timeout);
		if (clnt_stat == RPC_SUCCESS) {
			break;
		} else if (clnt_stat == RPC_TIMEDOUT) {
			continue;
		} else {
			cmn_err(CE_WARN, "Error contacting local status monitor: %s",
				clnt_sperrno(clnt_stat));
		}
	} while (++mon_retries <= RETRY_NOTIFY);
	CLNT_DESTROY(client);
}
