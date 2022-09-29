/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)librpc:key_call.c	1.4.3.1"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 
/*
 * key_call.c, Interface to keyserver
 *
 * setsecretkey(key) - set your secret key
 * encryptsessionkey(agent, deskey) - encrypt a session key to talk to agent
 * decryptsessionkey(agent, deskey) - decrypt ditto
 * gendeskey(deskey) - generate a secure des key
 */
#if defined(__STDC__) 
        #pragma weak key_setsecret	= _key_setsecret
        #pragma weak key_encryptsession	= _key_encryptsession
        #pragma weak key_decryptsession	= _key_decryptsession
        #pragma weak key_gendes		= _key_gendes
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#include <stdio.h>
#include <netconfig.h>

#define	KEY_TIMEOUT	5	/* per-try timeout in seconds */
#define	KEY_NRETRY	12	/* number of retries */

#ifdef DEBUG
#define	debug(msg)	(void) fprintf(stderr, "%s\n", msg);
#else
#define	debug(msg)
#endif /* DEBUG */

static key_call();

key_setsecret(secretkey)
	char *secretkey;
{
	keystatus status;

	if (!key_call((u_long) KEY_SET, xdr_keybuf, secretkey,
			xdr_keystatus, (char *) &status)) {
		return (-1);
	}
	if (status != KEY_SUCCESS) {
		debug("set status is nonzero");
		return (-1);
	}
	return (0);
}

key_encryptsession(remotename, deskey)
	char *remotename;
	des_block *deskey;
{
	cryptkeyarg arg;
	cryptkeyres res;

	arg.remotename = remotename;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_ENCRYPT, xdr_cryptkeyarg, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("encrypt status is nonzero");
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	return (0);
}

key_decryptsession(remotename, deskey)
	char *remotename;
	des_block *deskey;
{
	cryptkeyarg arg;
	cryptkeyres res;

	arg.remotename = remotename;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_DECRYPT, xdr_cryptkeyarg, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("decrypt status is nonzero");
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	return (0);
}

key_gendes(key)
	des_block *key;
{
	if (!key_call((u_long)KEY_GEN, xdr_void, (char *)NULL,
			xdr_des_block, (char *)key)) {
		return (-1);
	}
	return (0);
}

/*
 * Keep the handle cached.  This call may be made quite often.
 */
static CLIENT *
getkeyserv_handle()
{
	void *localhandle;
	struct netconfig *nconf;
	struct netconfig *tpconf;
	static CLIENT *clnt;
	struct timeval wait_time;

#define	TOTAL_TIMEOUT	30	/* total timeout talking to keyserver */
#define	TOTAL_TRIES	5	/* Number of tries */

	if (clnt)
		return (clnt);
	if (!(localhandle = setnetconfig()))
		return (NULL);
	tpconf = NULL;
	while (nconf = getnetconfig(localhandle)) {
		if (strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) {
			if (nconf->nc_semantics == NC_TPI_CLTS) {
				clnt = clnt_tp_create(_rpc_gethostname(),
					KEY_PROG, KEY_VERS, nconf);
				if (clnt)
					break;
			} else {
				tpconf = nconf;
			}
		}
	}
	if ((clnt == NULL) && (tpconf))
		/* Now, try the connection-oriented loopback transport */
		clnt = clnt_tp_create(_rpc_gethostname(), KEY_PROG,
				KEY_VERS, tpconf);
	endnetconfig(localhandle);

	if (clnt == NULL)
		return (NULL);

	clnt->cl_auth = authsys_create("", geteuid(), 0, 0, NULL);
	if (clnt->cl_auth == NULL) {
		clnt_destroy(clnt);
		clnt = NULL;
		return (NULL);
	}
	wait_time.tv_sec = TOTAL_TIMEOUT/TOTAL_TRIES;
	wait_time.tv_usec = 0;
	(void) clnt_control(clnt, CLSET_RETRY_TIMEOUT, &wait_time);

	return (clnt);
}

/* returns  0 on failure, 1 on success */
static
key_call(proc, xdr_arg, arg, xdr_rslt, rslt)
	u_long proc;
	bool_t (*xdr_arg)();
	char *arg;
	bool_t (*xdr_rslt)();
	char *rslt;
{
	CLIENT *clnt;
	struct timeval wait_time;

	clnt = getkeyserv_handle();
	if (clnt == NULL)
		return (0);

	wait_time.tv_sec = TOTAL_TIMEOUT;
	wait_time.tv_usec = 0;

	if (CLNT_CALL(clnt, proc, xdr_arg, arg, xdr_rslt, rslt,
		wait_time) == RPC_SUCCESS)
		return (1);
	else
		return (0);
}
