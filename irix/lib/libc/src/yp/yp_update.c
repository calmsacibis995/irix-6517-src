/*
 * Copyright (C) 1990, Sun Microsystems, Inc.
 * Copyright (c) 1988,1990 by Sun Microsystems, Inc.
 * 1.10 87/10/30 
 */

/*
 * Network Information Services updater interface
 */
#ifdef __STDC__
	#pragma weak yp_update = _yp_update
#endif
#include "synonyms.h"

#include <rpc/rpc.h>
#include <netdb.h>
#include <sys/socket.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/ypupdate_prot.h>
#include <sys/time.h>

#define WINDOW (60*60)
#define TOTAL_TIMEOUT	300	

/*
 * Turn off debugging 
 */
#define debugging 0
#define debug(msg)

int
yp_update(char *domain,
	char *map,
	unsigned op,
	char *key,
	int keylen,
	char *data,
	int datalen)
{
	struct ypupdate_args args;
	u_int rslt;	
	struct timeval total;
	CLIENT *client;
	struct sockaddr_in server_addr;
	char *ypmaster;
	char ypmastername[MAXNETNAMELEN+1];
	enum clnt_stat stat;
	u_int proc;

	switch (op) {
	case YPOP_DELETE:
		proc = YPU_DELETE;
		break;	
	case YPOP_INSERT:
		proc = YPU_INSERT;
		break;	
	case YPOP_CHANGE:
		proc = YPU_CHANGE;
		break;	
	case YPOP_STORE:
		proc = YPU_STORE;
		break;	
	default:
		return(YPERR_BADARGS);
	}
	if (yp_master(domain, map, &ypmaster) != 0) {
		debug("no master found");
		return (YPERR_BADDB);	
	}

	client = clnt_create(ypmaster, YPU_PROG, YPU_VERS, "tcp");
	if (client == NULL) {
#if debugging == 1
		clnt_pcreateerror("client create failed");
#endif
		free(ypmaster);
		return (YPERR_RPC);
	}

	if (! host2netname(ypmastername, ypmaster, domain)) {
		free(ypmaster);
		return (YPERR_BADARGS);
	}
	free(ypmaster);
	clnt_control(client, CLGET_SERVER_ADDR, &server_addr);
	client->cl_auth = authunix_create_default();
	if (client->cl_auth == NULL) {
		debug("auth create failed");
		clnt_destroy(client);
		return (YPERR_RESRC);	/* local auth failure */
	}

	args.mapname = map;	
	args.key.yp_buf_len = keylen;
	args.key.yp_buf_val = key;
	args.datum.yp_buf_len = datalen;
	args.datum.yp_buf_val = data;

	total.tv_sec = TOTAL_TIMEOUT; total.tv_usec = 0;
	clnt_control(client, CLSET_TIMEOUT, &total);
	stat = clnt_call(client, proc,
		(xdrproc_t) xdr_ypupdate_args, &args,
		(xdrproc_t) xdr_u_int, &rslt, total);

	if (stat != RPC_SUCCESS) {
		debug("ypu call failed");
#if debugging == 1
		clnt_perror(client, "ypu call failed");
#endif
		if (stat == RPC_AUTHERROR ){
			rslt = YPERR_ACCESS;	/*remote auth failure*/
			} else {
			rslt = YPERR_RPC;
			}
	}
	auth_destroy(client->cl_auth);
	clnt_destroy(client);
	return ((int)rslt);
}
