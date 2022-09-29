#if 0
static char sccsid[] = "@(#)udp.c	1.4 91/03/08 NFSSRC4.1 Copyr 1990 Sun Micro";
#endif

	/*
	 * Copyright (c) 1988 by Sun Microsystems, Inc.
	 */

/*
 * this file consists of routines to support call_udp();
 * client handles are cached in a hash table;
 * clntudp_create is only called if (site, prog#, vers#) cannot
 * be found in the hash table;
 * a cached entry is destroyed, when remote site crashes
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <bstring.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/signal.h>
#include "prot_lock.h"

extern int debug;
extern int HASH_SIZE;

static int mysock = RPC_ANYSOCK;

#define MAX_HASHSIZE 100

static struct cache {
	char *host;
	int prognum;
	int versnum;
	int proto;
	int sock;
	CLIENT *client;
	struct cache *nxt;
} *table[MAX_HASHSIZE];


hash(name)
	unsigned char *name;
{
	int len;
	int i, c;

	c = 0;
	len = strlen(name);
	for (i = 0; i< len; i++) {
		c = c +(int) name[i];
	}
	c = c %HASH_SIZE;
	return (c);
}

/*
 * find_hash returns the cached entry;
 * it returns NULL if not found;
 */
static struct cache *
find_hash(char *host, int prognum, int versnum, int proto)
{
	struct cache *cp;

	cp = table[hash(host)];
	while ( cp != NULL) {
		if (strcasecmp(cp->host, host) == 0 &&
		     cp->prognum == prognum && cp->versnum == versnum &&
		     cp->proto == proto) {
			/* found */
			return (cp);
		}
		cp = cp->nxt;
	}
	return (NULL);
}

static struct cache *
add_hash(char *host, int prognum, int versnum, int proto, CLIENT *client)
{
	struct cache *cp;
	int h;

	if ((cp = xmalloc(sizeof(*cp))) == NULL ) {
		return (NULL);
	}
	if ((cp->host = xmalloc(strlen(host)+1)) == NULL ) {
		free(cp);
		return (NULL);
	}
	(void) strcpy(cp->host, host);
	cp->prognum = prognum;
	cp->versnum = versnum;
	cp->proto = proto;
	cp->client = client;
	h = hash(host);
	cp->nxt = table[h];
	table[h] = cp;
	return (cp);
}

void
delete_hash(char *host)
{
	struct cache *cp;
	struct cache *cp_prev = NULL;
	struct cache *next;
	int h;
	int found = 0;

	/*
	 * if there is more than one entry with same host name;
	 * delete has to be recurrsively called
	 */

	h = hash(host);
	next = table[h];
	while ((cp = next) != NULL) {
		next = cp->nxt;
		if (strcasecmp(cp->host, host) == 0) {
			if (cp_prev == NULL) {
				table[h] = cp->nxt;
			} else {
				cp_prev->nxt = cp->nxt;
			}
			if (debug)
				printf("delete hash entry (%x), %s \n", cp, host);
			if (cp->client)
				clnt_destroy(cp->client);
			free(cp->host);
			free(cp);
			found = 1;
		}
		else {
			cp_prev = cp;
		}
	}
	if (debug && !found)
		printf("delete_hash: %s not in table\n", host);
}

int
flush_client_cache()
{
	int i;
	struct cache *cp;
	struct cache *next;
	struct cache *prev;

	if (debug)
		printf("flushing UDP entries from client handle cache\n");
	for ( i = 0; i < HASH_SIZE; i++ ) {
		for ( prev = NULL, cp = table[i]; cp; ) {
			if ( cp->proto == IPPROTO_UDP ) {
				if ( !prev ) {
					table[i] = cp->nxt;
				} else {
					prev->nxt = cp->nxt;
				}
				next = cp->nxt;
				if (debug)
					printf("delete hash entry (%x), %s \n", cp, cp->host);
				if (cp->client)
					clnt_destroy(cp->client);
				free(cp->host);
				free(cp);
				cp = next;
			} else {
				prev = cp;
				cp = cp->nxt;
			}
		}
	}
}

int
call_udp(char *host, int prognum, int versnum, int procnum,
	xdrproc_t inproc, void *in, xdrproc_t outproc, void *out,
	int valid_in, int tot)
{
	struct sockaddr_in server_addr;
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct timeval timeout, tottimeout;
	struct cache *cp;
	int sigmask = sigmask(SIGHUP);
	int osigmask;

	if (debug > 2)
		printf("call_udp: [%s, %d, %d]\n", host, prognum, versnum);
	/* Get a long lived socket to talk to the status monitor with */
	if (mysock == RPC_ANYSOCK) {
		int	dontblock = 1;
		mysock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (mysock < 0) {
			syslog(LOG_ERR,"cannot send because socket could not be created.");
			return (-1);
		}
		/* attempt to bind to prileged port */
		(void)bindresvport(mysock, (struct sockaddr_in *)0);
		/* the sockets rpc controls are non-blocking */
		(void)ioctl(mysock, FIONBIO, (char *) &dontblock);
	}
	osigmask = sigblock(sigmask);
	if ((cp = find_hash(host, prognum, versnum, IPPROTO_UDP)) == NULL) {
		CLIENT *tmpcp;


		if (debug > 2)
			printf("find_hash: is new\n");

		if ((hp = gethostbyname(host)) == NULL) {
			if (debug > 2)
				herror(host);
			(void)sigsetmask(osigmask);
			return ((int) RPC_UNKNOWNHOST);
		}
		timeout.tv_usec = 0;
		timeout.tv_sec = 15;
		bcopy(hp->h_addr, &server_addr.sin_addr, hp->h_length);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port =  0;
		if ((tmpcp = clntudp_create(&server_addr, prognum,
			versnum, timeout, &mysock)) == NULL) {
			(void)sigsetmask(osigmask);
			return ((int) rpc_createerr.cf_stat);
		}
		if ((cp = add_hash(host, prognum, 
				versnum, IPPROTO_UDP, tmpcp)) == NULL) {
			if (debug)
				fprintf(stderr,
				"call_udp: cannot send due to out of cache\n");
			clnt_destroy(tmpcp);
			(void)sigsetmask(osigmask);
			return (-1);
		}

	} else {
		if (valid_in == 0) { /* cannot use cache */
			if (debug > 2)
				printf("udp: ignoring %x, creating handle\n",
					cp);
			if ((hp = gethostbyname(host)) == NULL) {
				/* 
				 * don't delete from hash in case of
				 * temporary NIS/name server problem.
				 */
				(void)sigsetmask(osigmask);
				return ((int) RPC_UNKNOWNHOST);
			}
			/* get rid of previous client struct */
			if (cp->client != NULL) clnt_destroy(cp->client);
			timeout.tv_usec = 0;
			timeout.tv_sec = 15;
			bcopy(hp->h_addr, &server_addr.sin_addr, hp->h_length);
			server_addr.sin_family = AF_INET;
			server_addr.sin_port =  0;
			if ((cp->client = clntudp_create(&server_addr, prognum,
				versnum, timeout, &mysock)) == NULL) {
				delete_hash(host);
				(void)sigsetmask(osigmask);
				return ((int) rpc_createerr.cf_stat);
			}
		}
	}

	tottimeout.tv_sec = tot;
	tottimeout.tv_usec = 0;
	clnt_stat = clnt_call(cp->client, procnum, inproc, in,
	    outproc, out, tottimeout);
	(void)sigsetmask(osigmask);
	if (debug > 2)
		printf("call_udp: clnt_stat=%d (%s)\n",
			(int) clnt_stat, clnt_sperrno(clnt_stat));
	return ((int) clnt_stat);
}


int
call_tcp(char *host, int prognum, int versnum, int procnum,
	xdrproc_t inproc, void *in, xdrproc_t outproc, void *out,
	int valid_in, int tot)
{
        struct sockaddr_in server_addr;
        enum clnt_stat clnt_stat;
	struct hostent *hp;
        struct timeval  tottimeout;
	struct cache *cp;
        int socket = RPC_ANYSOCK;

	if (debug > 2)
		printf("call_tcp: [%s, %d, %d]\n", host, prognum, versnum);
	if ((cp = find_hash(host, prognum, versnum, IPPROTO_TCP)) == NULL) {
		CLIENT *tmpcp;


		if (debug > 2)
			printf("find_hash: is new\n");

		if ((hp = gethostbyname(host)) == NULL) {
			if (debug > 2)
				herror(host);
			return ((int) RPC_UNKNOWNHOST);
		}
		bcopy(hp->h_addr, &server_addr.sin_addr, hp->h_length);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port =  0;

		if ((tmpcp = clnttcp_create(&server_addr, prognum,
		    versnum,  &socket, 0, 0)) == NULL) { 
			if (debug)
				clnt_pcreateerror("clnttcp_create");
			return ((int) rpc_createerr.cf_stat);  
		}
		if ((cp = add_hash(host, prognum, 
				versnum, IPPROTO_TCP, tmpcp)) == NULL) {
			if (debug)
				fprintf(stderr,
				"call_tcp: cannot send due to out of cache\n");
			clnt_destroy(tmpcp);
			return (-1);
		}
	} else {
		if (valid_in == 0) { /* cannot use cache */
			if (debug > 2)
				printf("tcp: ignoring %x, creating handle\n",
					cp);
			if ((hp = gethostbyname(host)) == NULL) {
				/* 
				 * don't delete from hash in case of
				 * temporary NIS/name server problem.
				 */
				return ((int) RPC_UNKNOWNHOST);
			}
			bcopy(hp->h_addr, &server_addr.sin_addr, hp->h_length);
			server_addr.sin_family = AF_INET;
			server_addr.sin_port =  0;
			/* get rid of previous client struct */
			if (cp->client != NULL) clnt_destroy(cp->client);
			if ((cp->client = clnttcp_create(&server_addr, prognum,
			    versnum, &socket, 0, 0)) == NULL) { 
				if (debug)
					clnt_pcreateerror("clnttcp_create");
				delete_hash(host);
				return ((int) rpc_createerr.cf_stat);  
			}
		}
	}
 
	tottimeout.tv_sec = tot; 
	tottimeout.tv_usec = 0;
again:
        clnt_stat = clnt_call(cp->client, procnum, inproc, in,
            outproc, out, tottimeout);
	if (clnt_stat != RPC_SUCCESS)  {
		if( clnt_stat == RPC_TIMEDOUT) {
			if (tot != 0) {
				if(debug)
					printf("call_tcp: timeout, retry\n");
				goto again;
			}
			/* if tot == 0, no reply is expected */
		} else {
			if(debug) {
				printf("call_tcp: clnt_call failed: %s\n",
					clnt_sperrno(clnt_stat));
			}
		}
	}
        return (int) clnt_stat;
}
