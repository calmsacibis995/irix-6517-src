/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#ifdef __STDC__
	#pragma weak yp_bind = _yp_bind
	#pragma weak yp_unbind = _yp_unbind
	#pragma weak yp_get_default_domain = _yp_get_default_domain
#endif
#include "synonyms.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>		/* prototype for getpid()/sleep() */
#include <bstring.h>		/* prototype for bzero() */
#include <sys/capability.h>

#include "yp_prot.h"
#include "ypclnt.h"

enum bind_status {
	BS_BAGIT,
	BS_RETRY,
	BS_OK
};

static char *_default_domain(void);
static bool check_pmap_up(struct sockaddr_in *, int *);
static bool check_binder_up(struct sockaddr_in *, int *);
static enum bind_status talk2_pmap(struct sockaddr_in *, int, int *);
static enum bind_status talk2_binder(struct sockaddr_in *, char **,
		struct ypbind_resp *, int *);
static enum bind_status get_binder_port(int, int *);
static bool check_binding(char *, struct dom_binding **);
static void newborn(void);
static struct dom_binding *load_dom_binding(struct ypbind_resp *, char *, int*);

/*
 * Time parameters when talking to the ypbind and pmap processes
 */

#define YPSLEEPTIME 5			/* Time to sleep between tries */
unsigned int _ypsleeptime = YPSLEEPTIME;

#define YPBIND_TIMEOUT 30		/* Total seconds for timeout */
#define YPBIND_INTER_TRY 30		/* Seconds between tries */

static const struct timeval bind_intertry = {
	YPBIND_INTER_TRY,		/* Seconds */
	0				/* Microseconds */
	};
static const struct timeval bind_timeout = {
	YPBIND_TIMEOUT,			/* Seconds */
	0				/* Microseconds */
	};

/*
 * Time parameters when talking to the ypserv process
 */

#ifdef  DEBUG
#define YPTIMEOUT 120			/* Total seconds for timeout */
#define YPINTER_TRY 60			/* Seconds between tries */
#else
#define YPTIMEOUT 20			/* Total seconds for timeout */
#define YPINTER_TRY 5			/* Seconds between tries */
#endif

static const struct timeval ypserv_intertry = {
	YPINTER_TRY,			/* Seconds */
	0				/* Microseconds */
	};
struct timeval _ypserv_timeout = {
	YPTIMEOUT,			/* Seconds */
	0				/* Microseconds */
	};

/* List of bound domains */
static struct dom_binding *bound_domains _INITBSS;

#define	CHECKPERIOD	YPINTER_TRY	/* seconds between recheck */
#define	RECHECKPERIOD	YPBIND_TIMEOUT	/* seconds between auto-recheck */

static time_t rechecktime _INITBSSS;	/* auto-recheck time, for _yellowup */

int	_yp_disabled _INITBSS;
int	_yp_is_bound _INITBSS;
char	_yp_domain[256] _INITBSSS;
int	_yp_domainsize = sizeof _yp_domain;

void
_yp_disable()
{
	_yp_disabled = 1;
}

void
_yp_enable()
{
	_yp_disabled = 0;
}

/* move out of function scope so we get a global symbol for use with data cording */
static pid_t mypid _INITBSS; /* cached pid to detect startup and forks */

int
_yellowup(recheck)
	int recheck;
{
	int firsttime;
	pid_t pid;

	if (_yp_disabled) {
		_yp_is_bound = 0;
		return 0;
	}
	pid = getpid();
	firsttime = (pid != mypid);
	/*
	 * Use the previous answer this time if we have recently checked.
	 * If the caller thinks things should be checked (e.g. set*ent()),
	 * and if it has been at least a timeout since the last check,
	 * then check again.
	 */
	if (!firsttime
	    && time(0) < rechecktime
	    && (!recheck || time(0)+(RECHECKPERIOD-CHECKPERIOD) < rechecktime))
		return _yp_is_bound;
	if (firsttime) {
		mypid = pid;
		newborn();	/* Is this the first sighting of this pid? */
	}
	if (_yp_domain[0] == '\0'
	    && (getdomainname(_yp_domain, _yp_domainsize) < 0
		|| _yp_domain[0] == '\0')) {
		_yp_is_bound = 0;
		return 0;
	}
	_yp_is_bound = (yp_bind(_yp_domain) == 0);
	/*
	 * If the binding worked, then do not check again for long time,
	 * but if not, do not check again until there is hope things
	 * have changed.
	 */
	rechecktime = time(0) + (_yp_is_bound ? CHECKPERIOD : RECHECKPERIOD);
	return _yp_is_bound;
}

/*
 * binder_port holds what we believe to be the local port for ypbind.  It is
 * set only by talk2_pmap.  It is cleared (set to 0) by:
 *	1. talk2_pmap: always upon entry.
 *	2. check_binder_up if:
 *		- It can't create a socket to speak to the binder.
 *		- If it fails to bind to the port.
 *	3. talk2_binder if there are RPC errors when trying to use the port.
 */
static unsigned long binder_port _INITBSS;	/* Initialize to "no port" */

/*
 * Attempts to locate a Network Information Services 
 * server that serves a passed domain.  If
 * one is found, an entry is created on the static list of domain-server pairs
 * pointed to by cell bound_domains, a udp path to the server is created and
 * the function returns 0.  Otherwise, the function returns a defined errorcode
 * YPERR_xxxx.
 */
int
_yp_dobind(domain, binding)
	const char *domain;
	struct dom_binding **binding;	/* if result == 0, ptr to dom_binding */
{
	struct dom_binding *pdomb;	/* Ptr to new domain binding */
	struct sockaddr_in ypbinder;	/* To talk with ypbinder */
	char *pdomain;			/* For xdr interface */
	struct ypbind_resp ypbind_resp; /* Response from local ypbinder */
	int tries;			/* Number of times we've tried with
					 *  the current protocol */
	int status;
	enum bind_status loopctl;
	bool bound;
	bool oldport = FALSE;

	if ( (domain == NULL) ||(strlen(domain) == 0) ) {
		return (YPERR_BADARGS);
	}

	newborn();

	if (check_binding((char *)domain, binding) )
		return (0);		/* We are bound */

	pdomain = (char *)domain;
	
	/*
	 * Try to get the binder's port
	 */
	for (bound = FALSE; !bound; ) {

		if (binder_port) {
			oldport = TRUE;
		} else {
			oldport = FALSE;
			
			/*
			 * Get the binder's port.  We'll loop as long as
			 * get_binder_port returns BS_RETRY.  
			 */
			for (loopctl = BS_RETRY; loopctl != BS_OK; ) {
		 
	 			switch (loopctl =
				    get_binder_port(YPBINDVERS, &status) ) {
				case BS_BAGIT:
					return (status);
				case BS_OK:
					break;
				}
			}
		}
		
		/*
		 * See whether ypbind is up.  If no, bag it if it's a
		 * resource error, or if we are using a port we just got
		 * from the port mapper.  Otherwise loop around to try to
		 * get a valid port.  
		 */
		if (!check_binder_up(&ypbinder, &status)) {
			
			if (status == YPERR_RESRC) {
				return (status);
			}

			if (!oldport && status == YPERR_YPBIND) {
				return (status);
			}

			continue;
		}
		
		/*
		 * At this point, we think we know how to talk to the
		 * binder, and the binder is apparently alive.  Until we
		 * succeed in binding the domain, or we know we can't ever
		 * bind the domain, we will try forever.  This loops when
		 * talk2_binder returns BS_RETRY, and terminates when
		 * talk2_binder returns BS_BAGIT, or BS_OK.  If binder_port
		 * gets cleared, we will not execute this loop again, but
		 * will go to the top of the enclosing loop to try to get
		 * the binder's port again.  It is never the case that both
		 * talk2_binder returns  BS_OK and that it clears the
		 * binder_port.  
		 */
		for (loopctl = BS_RETRY, tries = 1;
		    binder_port && (loopctl != BS_OK); tries++) {
		
			switch (loopctl = talk2_binder(&ypbinder,
						       &pdomain, &ypbind_resp, &status) ) {
			case BS_BAGIT:
				return (status);
			case BS_OK:
				bound = TRUE;
			}
		}
	}

	if ( (pdomb = load_dom_binding(&ypbind_resp, (char *)domain, &status) ) ==
	    (struct dom_binding *) NULL) {
		return (status);
	}

	*binding = pdomb;			/* Return ptr to the binding
						 *   entry */
	return (0);				/* This is the go path */
}

/*
 * This is a "wrapper" function for _yp_dobind for vanilla user-level
 * functions which neither know nor care about struct dom_bindings.
 */
int
yp_bind(domain)
	const char *domain;
{
	 
	struct dom_binding *binding;
	
	return (_yp_dobind(domain, &binding) );
}

/*
 * Attempts to find a dom_binding in the list at bound_domains having the
 * domain name field equal to the passed domain name, and removes it if found.
 * The domain-server binding will not exist after the call to this function.
 * All resources associated with the binding will be freed.
 */
void
yp_unbind (domain)
	const char *domain;
{
	struct dom_binding *pdomb;
	struct dom_binding *ptrail;
	

	if ( (domain == NULL) ||(strlen(domain) == 0) ) {
		return;
	}
	
	for (pdomb = bound_domains; pdomb != NULL;
	    ptrail = pdomb, pdomb = pdomb->dom_pnext) {
		
		if (strcmp(domain, pdomb->dom_domain) == 0) {
			clnt_destroy(pdomb->dom_client);
			(void) close(pdomb->dom_socket);

			if (pdomb == bound_domains) {
				bound_domains = pdomb->dom_pnext;
			} else {
				ptrail->dom_pnext = pdomb->dom_pnext;
			}

			free((char *) pdomb);
			rechecktime = time(0) + CHECKPERIOD;
			break;	
		}
		
	}
	if (bound_domains == NULL)
		_yp_is_bound = 0;
}

void
_yp_unbind_all (void)
{
	struct dom_binding *pdomb, *next;
	
	for (pdomb = bound_domains; pdomb != NULL; pdomb = next) {
		next = pdomb->dom_pnext;
		clnt_destroy(pdomb->dom_client);
		(void) close(pdomb->dom_socket);
		free(pdomb);
	}
	bound_domains = NULL;
	rechecktime = time(0) + CHECKPERIOD;
	_yp_is_bound = 0;
}

static char *default_domain _INITBSS;

static char *
_default_domain(void)
{
	char temp[256];

	if (default_domain)
		return (default_domain);
	if (getdomainname(temp, sizeof(temp)))
		return (0);
	if (strlen(temp) > 0) {
		default_domain = (char *)malloc(strlen(temp)+1);
		if (default_domain == 0)
			return (0);
		strcpy(default_domain, temp);
		return (default_domain);
	}
	return (0);
}

/*
 * This is a wrapper for the system call getdomainname which returns a
 * ypclnt.h error code in the failure case.  It also checks to see that
 * the domain name is non-null, knowing that the null string is going to
 * get rejected elsewhere in the NIS client package.
 */
int
yp_get_default_domain(domain)
	char **domain;
{

	if ((*domain = _default_domain()) != 0)
		return (0);
	return (YPERR_YPERR);
}

/*
 * Check to see if this is a new process incarnation which has
 * inherited bindings from a parent, and unbind the world if so.
 */
static pid_t newpid _INITBSS;

static void
newborn(void)
{
	pid_t testpid;

	if ((testpid = getpid() ) != newpid) {
		newpid = testpid;

		_yp_unbind_all();
	}
}

/*
 * This checks that the socket for a domain which has already been bound
 * hasn't been closed or changed under us.  If it has, unbind the domain
 * without closing the socket, which may be in use by some higher level
 * code.  This returns TRUE and points the binding parameter at the found
 * dom_binding if the binding is found and the socket looks OK, and FALSE
 * otherwise.  
 */
static bool
check_binding(domain, binding)
	char *domain;
	struct dom_binding **binding;
{
	struct dom_binding *pdomb;
	struct sockaddr_in local_name;
	int local_name_len = sizeof(struct sockaddr_in);
	bool rebind = FALSE;

	for (pdomb = bound_domains; pdomb != NULL; pdomb = pdomb->dom_pnext) {
		
		if (strcmp(domain, pdomb->dom_domain) == 0) {

			if (getsockname(pdomb->dom_socket,
			    (struct sockaddr *) &local_name,
			    &local_name_len) == 0) {

				if (local_name.sin_family != AF_INET) {
					rebind = TRUE;
				}

				if (pdomb->dom_local_port !=
				    local_name.sin_port) {
					rebind = TRUE;
				}
				
			} else {
				rebind = TRUE;
			}

			if (rebind) {
				pdomb->dom_socket = -1;
				CLNT_CONTROL( pdomb->dom_client, CLSET_FD_NCLOSE, NULL);
				yp_unbind(domain);
				pdomb = (struct dom_binding *) NULL;
				break;
			} else {
				*binding = pdomb;
				return (TRUE);
			}
		}
	}

	return (FALSE);
}

/*
 * This checks whether the portmapper is up.  If the connect fails, the
 * portmapper is dead.  As a side effect, the pmapper sockaddr_in is
 * initialized. The connection is not used for anything else, so it is
 * immediately closed.
 */
static bool
check_pmap_up(pmapper, err)
	struct sockaddr_in *pmapper;
	int *err;
{
	int sokt;

	pmapper->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	pmapper->sin_family = AF_INET;
	pmapper->sin_port = htons(PMAPPORT);
	bzero(pmapper->sin_zero, 8);
	sokt =  socket(AF_INET, SOCK_STREAM, 0); 
	
	if (sokt == -1) {
		*err = YPERR_RESRC;
		return (FALSE);
	}

	if (connect(sokt, (struct sockaddr *) pmapper,
	    sizeof(struct sockaddr_in)) < 0) {
		(void) close(sokt);
		*err = YPERR_PMAP;
		return (FALSE);
	}
	
	(void) close(sokt);
	return (TRUE);
}

/*
 * This checks whether ypbind is up.  If the bind succeeds, ypbind is dead,
 * because the binder port returned from portmap should already be in use.
 * There are two side effects.  The ypbind sockaddr_in is initialized. If
 * the function returns FALSE, the global binder_port will be set to 0.  
 */
static bool
check_binder_up(ypbinder, err)
	struct sockaddr_in *ypbinder;
	int *err;
{
	int sokt;
	int status;
	int cap_enabled = (sysconf (_SC_CAP) > 0);
	cap_t ocap;
	cap_value_t capv = CAP_PRIV_PORT;

	if (binder_port == 0) {
		return (FALSE);
	}
	
	ypbinder->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ypbinder->sin_family = AF_INET;
	ypbinder->sin_port = (unsigned short)htons(binder_port);
	bzero(ypbinder->sin_zero, 8);
	sokt =  socket(AF_INET, SOCK_DGRAM, 0); /* Throw-away socket */
	
	if (sokt == -1) {
		binder_port = 0;
		*err = YPERR_RESRC;
		return (FALSE);
	}

	setoserror(0);
	ocap = cap_acquire (1, &capv);
	status = bind(sokt, (struct sockaddr *) ypbinder,
	    sizeof(struct sockaddr_in));
	cap_surrender (ocap);
	(void) close(sokt);
	    
	if (status == -1) {
		if (errno == EADDRINUSE) {
			/*
			 * since we cannot grab the port, ypbind must be
			 * up and healthy.
			 */
			return (TRUE);
		}
		if ((errno == EACCES) &&
		    (binder_port < IPPORT_RESERVED) &&
		    (cap_enabled || geteuid() != 0)) {
			/*
			 * The port is privileged.  This must be "secure"
			 * unix where have to use NIS.
			 */
			return (TRUE);
		}
	}

	binder_port = 0;
	*err = YPERR_YPBIND;
	return (FALSE); 
}

/*
 * This asks the portmapper for addressing info for ypbind speaking a passed
 * program version number.  If it gets that info, the port number is stashed
 * in binder_port, but binder_port will be set to 0 when talk2_pmap returns
 * anything except BS_OK.  If the RPC call to the portmapper failed, the
 * current process will be put to sleep for _ypsleeptime seconds before
 * this function returns.
 */
static enum bind_status
talk2_pmap(pmapper, vers, err)
	struct sockaddr_in *pmapper;
	int vers;
	int *err;
{
	int sokt;
	CLIENT *client;
	struct pmap portmap;
	enum clnt_stat clnt_stat;

	binder_port = 0;
	portmap.pm_prog = YPBINDPROG;
	portmap.pm_vers = (unsigned long)vers;
	portmap.pm_prot = IPPROTO_UDP;
	portmap.pm_port = 0;		/* Don't care */

	sokt = RPC_ANYSOCK;

	if ((client = clntudp_bufcreate(pmapper, PMAPPROG, PMAPVERS,
	    bind_intertry, &sokt, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE))  == NULL) {
		*err = YPERR_RPC;
		return (BS_BAGIT);
	}

	clnt_stat = (enum clnt_stat) clnt_call(client, PMAPPROC_GETPORT,
	    (xdrproc_t) xdr_pmap, &portmap, (xdrproc_t) xdr_u_long, &binder_port, bind_timeout);
	clnt_destroy(client);
	/* sockt closed by clnt_destroy */
	
	if (clnt_stat  == RPC_SUCCESS) {

		if (binder_port != 0) {
			return (BS_OK);
		} else {
			*err = YPERR_YPBIND;
			return (BS_BAGIT);
		}

	} else {
		(void) sleep(_ypsleeptime);
		*err = YPERR_RPC;
		return (BS_RETRY);
	}
}

/*
 * This talks to the local ypbind process, and asks for a binding for the
 * passed domain. In the
 * success case, the ypbind response will be returned as it was loaded by
 * ypbind - that is, containing a valid binding.  If the RPC call to ypbind
 * failed, the current process will be put to sleep for _ypsleeptime seconds
 * before this function returns.  
 */
static enum bind_status
talk2_binder(ypbinder, ppdomain, ypbind_resp, err)
	struct sockaddr_in *ypbinder;
	char **ppdomain;
	struct ypbind_resp *ypbind_resp;
	int *err;
{
	int sokt;
	CLIENT *client;
	enum clnt_stat clnt_stat;

	sokt = RPC_ANYSOCK;

	if ((client = clntudp_bufcreate(ypbinder, YPBINDPROG, YPBINDVERS,
	    bind_intertry, &sokt, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE))  == NULL) {
		*err = YPERR_RPC;
		return (BS_BAGIT);
	}

	clnt_stat = (enum clnt_stat)clnt_call(client, YPBINDPROC_DOMAIN,
	    (xdrproc_t) xdr_ypdomain_wrap_string, ppdomain, (xdrproc_t) xdr_ypbind_resp,
	    ypbind_resp, bind_timeout);
	clnt_destroy(client);
	/* sockt closed by clnt_destroy */
			    
	if (clnt_stat == RPC_SUCCESS) {
			
		if (ypbind_resp->ypbind_status == YPBIND_SUCC_VAL) {
			/* Binding successfully returned from ypbind */
			return (BS_OK);
		} else {
			*err = YPERR_DOMAIN;
			if (ypbind_resp->ypbind_respbody.ypbind_error
			    == YPBIND_ERR_NOSERV) {
				return (BS_BAGIT);
			}
			return (BS_RETRY);
		}
			
	} else {

		if (clnt_stat == RPC_PROGVERSMISMATCH) {
			*err = YPERR_YPBIND;
			return (BS_BAGIT);
		} else {
			(void) sleep(_ypsleeptime);
			binder_port = 0;
		}
		
		*err = YPERR_RPC;
		return (BS_RETRY);
	}
}

/*
 * This handles all the conversation with the portmapper to find the port
 * ypbind is listening on.  If binder_port is already non-zero, this returns
 * BS_OK immediately without changing anything.
 */
static enum bind_status
get_binder_port(vers, err)
	int vers;			/* !ypbind! program version number */
	int *err;
{
	struct sockaddr_in pmapper;

	if (binder_port) {
		return (BS_OK);
	}

	if (!check_pmap_up(&pmapper, err) ) {
		return (BS_BAGIT);
	}
	
	return (talk2_pmap(&pmapper, vers, err) );
}

/*
 * This allocates some memory for a domain binding, initialize it, and
 * returns a pointer to it.  Based on the program version we ended up
 * talking to ypbind with, fill out an opvector of appropriate protocol
 * modules.  
 */
static struct dom_binding *
load_dom_binding(ypbind_resp, domain, err)
	struct ypbind_resp *ypbind_resp;
	char *domain;
	int *err;
{
	struct dom_binding *pdomb;
	struct sockaddr_in dummy;	/* To get a port bound to socket */
	struct sockaddr_in local_name;
	int local_name_len = sizeof(struct sockaddr_in);

	pdomb = (struct dom_binding *) NULL;
	 
	if ((pdomb = (struct dom_binding *) malloc(sizeof(struct dom_binding)))
		== NULL) {
		(void) fprintf(stderr, "load_dom_binding:  malloc failure.\n");
		*err = YPERR_RESRC;
		return (struct dom_binding *) (NULL);
	}

	pdomb->dom_server_addr.sin_addr =
	    ypbind_resp->ypbind_respbody.ypbind_bindinfo.ypbind_binding_addr;
	pdomb->dom_server_addr.sin_family = AF_INET;
	pdomb->dom_server_addr.sin_port =
	    ypbind_resp->ypbind_respbody.ypbind_bindinfo.ypbind_binding_port;
	bzero(pdomb->dom_server_addr.sin_zero, 8);
	pdomb->dom_socket = RPC_ANYSOCK;
	
	/*
	 * Open up a udp path to the server, which will remain active globally.
	 */
	if ((pdomb->dom_client = clntudp_bufcreate(&(pdomb->dom_server_addr),
						   YPPROG, YPVERS,
	    ypserv_intertry, &(pdomb->dom_socket), YPMSGSZ, YPMSGSZ))
	    == NULL) {
		free((char *) pdomb);
		*err = YPERR_RPC;
		return (struct dom_binding *) (NULL);
	}

	/*
	 * Bind the socket to an anonymous address so a port gets allocated for
	 * the socket, that we can check for later.
	 */
	(void) fcntl(pdomb->dom_socket, F_SETFD, FD_CLOEXEC);
	bzero((char *)&dummy, sizeof (dummy));
	dummy.sin_family = AF_INET;
	(void)bind(pdomb->dom_socket, (struct sockaddr *)&dummy, sizeof(dummy));
	
	/*
	 * Remember the bound port number
	 */
	if (getsockname(pdomb->dom_socket, (struct sockaddr *) &local_name,
	    &local_name_len) == 0) {
		pdomb->dom_local_port = local_name.sin_port;
	} else {
		free((char *) pdomb);
		*err = YPERR_YPERR;
		return (struct dom_binding *) (NULL);
	}

	(void) strcpy(pdomb->dom_domain, domain);/* Remember the domain name */
	pdomb->dom_pnext = bound_domains;	/* Link this to the list as */
	bound_domains = pdomb;			/* ... the head entry */
	return (pdomb);
}
