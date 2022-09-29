#ifndef lint
static char sccsid[] = 	"@(#)ypxfr.c	1.2 88/05/19 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/*
* This is a user command which gets a NIS data base from some running
* server, and gets it to the local site by using the normal NIS client
* enumeration functions.  The map is copied to a temp name, then the real
* map is removed and the temp map is moved to the real name.  ypxfr then
* sends a "YPPROC_CLEAR" message to the local server to insure that he will
* not hold a removed map open, so serving an obsolete version.  
* 
* ypxfr [-h <host>] [-d <domainname>] 
*		     [-s <domainname>] [-f] [-c] [-C tid prot ipadd port] map
* 
* where host may be either a name or an internet address of form ww.xx.yy.zz
* 
* If the host is ommitted, ypxfr will attempt to discover the master by 
* using normal NIS services.  If it can't get the record, it will use
* the address of the callback, if specified. If the host is specified 
* as an internet address, no NIS services need to be locally available.  
* 
* If the domain is not specified, the default domain of the local machine
* is used.
* 
* If the -f flag is used, the transfer will be done even if the master's
* copy is not newer than the local copy.
*
* The -c flag suppresses the YPPROC_CLEAR request to the local ypserv.  It
* may be used if ypserv isn't currently running to suppress the error message.
* 
* The -C flag is used to pass callback information to ypxfr when it is
* activated by ypserv.  The callback information is used to send a
* yppushresp_xfr message with transaction id "tid" to a yppush process
* speaking a transient protocol number "prot".  The yppush program is
* running on the node with IP address "ipadd", and is listening (UDP) on
* "port".  
*
* The -s option is used to specify a source domain which may be
* different from the destination domain, for transfer of maps
* that are identical in different domains (e.g. services.byname)
*  
*/
#include <dbm.h>
#undef NULL
#define DATUM

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <syslog.h>
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypv1_prot.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>

#include "ypdefs.h"
USE_YP_PREFIX
USE_YP_MASTER_NAME
USE_YP_SECURE
USE_YP_LAST_MODIFIED
USE_YPDBPATH
USE_DBM

# define PARANOID 1		/* make sure maps have the right # entries */

#define UDPINTER_TRY 10			/* Seconds between tries for udp */
#define UDPTIMEOUT UDPINTER_TRY*4	/* Total timeout for udp */
#define CALLINTER_TRY 10		/* Seconds between callback tries */
#define CALLTIMEOUT CALLINTER_TRY*6	/* Total timeout for callback */


struct timeval udp_intertry = {
    UDPINTER_TRY,
    0
};
struct timeval udp_timeout = {
    UDPTIMEOUT,
    0
};
struct timeval tcp_timeout = {
    180,	/* timeout for map enumeration (could be long) */
    0
};


int	debug = FALSE;

char *domain = NULL;
char *source = NULL;
char *map = NULL;
char *master = NULL;		/* The name of the xfer peer as specified as a
				 *   -h option, or from querying the NIS */
struct in_addr master_addr;	/* Addr of above */
struct dom_binding master_server;/* To talk to above */
unsigned int master_prog_vers;	/* YPVERS or YPOLDVERS */
char *master_name = NULL;	/* Map's master as contained in the map */
unsigned *master_version = NULL; /* Order number as contained in the map */
char *master_ascii_version;	/* ASCII order number as contained in the map */
bool fake_master_version = FALSE; /* TRUE only if there's no order number in
				   * the map, and the user specified -f */
bool force = FALSE;		/* TRUE iff user specified -f flag */
bool logging = FALSE;		/* TRUE iff no tty, but log file exists */
bool interactive = TRUE;	/* TRUE iff no tty */
bool send_clear = TRUE;		/* FALSE iff user specified -c flag */
bool callback = FALSE;		/* TRUE iff -C flag set.  tid, proto, ipadd,
				 * and port will be set to point to the
				 * command line args. */
bool secure_map = FALSE;	/* TRUE if there is yp_secure in either the
				 * local or the master version of the map */

static bool run_secure = FALSE; /* on SunOS, we'd use issecure() */

char *tid;
char *proto;
char *ipadd;
char *port;
int entry_count;		/* counts entries in the map */
char logfile[] = "/var/yp/ypxfr.log";
char err_usage[] =
"Usage:\n\
ypxfr [-f] [-h <master>] [-d <domainname>]\n\
      [-s <source domainname>] [-S] [-c] [-C tid prot ipadd port] map\n\n\
where\n\
	-f forces transfer even if the master's copy is not newer.\n\
	-h <master> may be either a name or an Internet address\n\
	    of form ww.xx.yy.zz\n\
	-s <domainname> specifies the source domain.\n\
	-S ypxfr will check that ypserv is using \"priviledged\" ports.\n\
	-c inhibits sending a \"Clear map\" message to the local ypserv.\n\
	-C is used by ypserv to pass callback information.\n";
char err_bad_args[] =
	"%s argument is bad.\n";
char err_cant_get_kname[] =
	"Can't get %s back from system call.\n";
char err_null_kname[] =
	"%s hasn't been set on this machine.\n";
char err_bad_hostname[] = "hostname";
char err_bad_mapname[] = "mapname";
char err_bad_domainname[] = "domainname";
char yptempname_prefix[] = "ypxfr_map.";
char ypbkupname_prefix[] = "ypxfr_bkup.";

void get_command_line_args();
bool get_master_addr();
bool bind_to_server();
bool ping_server();
bool  get_private_recs();
bool get_order();
bool get_v1order();
bool get_v2order();
bool get_master_name();
bool get_v1master_name();
bool get_v2master_name();
void find_map_master();
bool move_map();
unsigned get_local_version();
void mkfilename();
void mk_tmpname();
bool rename_map();
bool check_map_existence();
bool get_map();
bool add_private_entries();
bool new_mapfiles();
void del_mapfiles();
void set_output();
bool send_ypclear();
void xfr_exit();
void send_callback();
int ypall_callback();
int map_yperr_to_pusherr();
void logprintf(char *,...);

extern void yp_make_dbname();
extern u_long inet_addr();

/*
 * This is the mainline for the ypxfr process.
 */

void
main(argc, argv)
	int argc;
	char **argv;
	
{
	static char default_domain_name[YPMAXDOMAIN];
	static unsigned big = 0xffffffff;
	int status;
	
	set_output();

	get_command_line_args(argc, argv);

	if (!domain) {
		
		if (!getdomainname(default_domain_name, YPMAXDOMAIN) ) {
			domain = default_domain_name;
		} else {
			logprintf(err_cant_get_kname, err_bad_domainname);
			xfr_exit(YPPUSH_RSRC);
		}

		if (strlen(domain) == 0) {
			logprintf(err_null_kname, err_bad_domainname);
			xfr_exit(YPPUSH_RSRC);
		}
	}
	if (!source)
		source = domain;
	
	if (!master) {
		find_map_master();
	} else {
		if (!get_master_addr() ) {
			xfr_exit(YPPUSH_MADDR);
		} 
	}

	if (!bind_to_server(master, master_addr, &master_server,
	    &master_prog_vers, &status) ) {
		xfr_exit(status);
	}

	if (!get_private_recs(&status) ) {
		xfr_exit(status);
	}
	
	if (!master_version) {

		if (force) {
			master_version = &big;
			fake_master_version = TRUE;
		} else {
			logprintf(
    "Can't get order number for map %s from server at %s: use the -f flag.\n",
			  map, master);
			xfr_exit(YPPUSH_FORCE);
		}
	}
	
	if (!move_map(&status) ) {
		xfr_exit(status);
	}

	if (send_clear && !send_ypclear(&status) ) {
		xfr_exit(status);
	}

	if (interactive || logging) {
		logprintf("Transferred map %s from %s (%d entries).\n",
		    map, master, entry_count);
	}

	xfr_exit(YPPUSH_SUCC);
	/*NOTREACHED*/
}

/*
 * This decides whether we're being run interactively or not, and, if not,
 * whether we're supposed to be logging or not.  If we are logging, it sets
 * up stderr to point to the log file, and sets the "logging"
 * variable.  If there's no logging, the output goes to syslog.
 * Logging output differs from interactive output in the presence of a
 * timestamp, present only in the log file.  stderr is reset, too, because it
 * it's used by various library functions, including clnt_perror.
 */
void
set_output()
{
	if (!isatty(1)) {
		interactive = FALSE;
		if (access(logfile, W_OK)) {
			(void) freopen("/dev/null", "w", stderr);
			openlog("ypxfr", LOG_PID, LOG_DAEMON);
		} else {
			(void) freopen(logfile, "a", stderr);
			logging = TRUE;
		}
	}
}

/*
 * This constructs a logging record.
 */
/*VARARGS1*/
void
logprintf(char *format,...)
{
	struct timeval t;
	va_list ap;

	if (interactive || logging) {
		if (logging) {
			(void) gettimeofday(&t, NULL);
			fseek(stderr,0,2);
			(void) fprintf(stderr, "%19.19s: ", ctime(&t.tv_sec));
		}
		va_start(ap, format);
		(void) vfprintf(stderr, format, ap);
		va_end(ap);
		fflush(stderr);
	} else {
		va_start(ap, format);
		(void) vsyslog(LOG_ERR, format, ap);
		va_end(ap);
	}
}

/*
 * This does the command line argument processing.
 */
void
get_command_line_args(argc, argv)
	int argc;
	char **argv;
	
{
	argv++;

	if (argc < 2) {
		logprintf(err_usage);
		xfr_exit(YPPUSH_BADARGS);
	}
	
	while (--argc) {

		if ( (*argv)[0] == '-') {

			switch ((*argv)[1]) {

			case 'f': {
				force = TRUE;
				argv++;
				break;
			}

			case 'D': {
				debug = TRUE;
				argv++;
				break;
			}

			case 'S': {
				run_secure = TRUE;
				argv++;
				break;
			}

			case 'c': {
				send_clear = FALSE;
				argv++;
				break;
			}

			case 'h': {

				if (argc > 1) {
 					argv++;
					argc--;
					master = *argv;
					argv++;

					if (strlen(master) > 256) {
						logprintf( err_bad_args,
						    	  err_bad_hostname);
						xfr_exit(YPPUSH_BADARGS);
					}
					
				} else {
					logprintf(err_usage);
					xfr_exit(YPPUSH_BADARGS);
				}
				
				break;
			}
				
			case 'd':
				if (argc > 1) {
					argv++;
					argc--;
					domain = *argv;
					argv++;

					if (strlen(domain) > YPMAXDOMAIN) {
						logprintf( err_bad_args,
						    	  err_bad_domainname);
						xfr_exit(YPPUSH_BADARGS);
					}
					
				} else {
					logprintf(err_usage);
					xfr_exit(YPPUSH_BADARGS);
				}				
				break;

			case 's':
				if (argc > 1) {
					argv++;
					argc--;
					source = *argv;
					argv++;

					if (strlen(source) > YPMAXDOMAIN) {
						logprintf( err_bad_args,
						    	  err_bad_domainname);
						xfr_exit(YPPUSH_BADARGS);
					}
					
				} else {
					logprintf(err_usage);
					xfr_exit(YPPUSH_BADARGS);
				}				
				break;

			case 'C': {

				if (argc > 5) {
					callback = TRUE;
					argv++;
					tid = *argv++;
					proto = *argv++;
					ipadd = *argv++;
					port = *argv++;
					argc -= 4;
				} else {
					logprintf(err_usage);
					xfr_exit(YPPUSH_BADARGS);
				}
				
				break;
			}

			default: {
				logprintf(err_usage);
				xfr_exit(YPPUSH_BADARGS);
			}
			
			}
			
		} else {

			if (!map) {
				map = *argv;
				argv++;
			
				if (strlen(map) > YPMAXMAP) {
					logprintf( err_bad_args,
				   		  err_bad_mapname);
					xfr_exit(YPPUSH_BADARGS);
				}
				
			} else {
				logprintf(err_usage);
				xfr_exit(YPPUSH_BADARGS);
			}
		}
	}

	if (!map) {
		logprintf(err_usage);
		xfr_exit(YPPUSH_BADARGS);
	}
}

/*
 * This checks to see if the master name is an ASCII internet address, in
 * which case it's translated to an internet address here, or is a host
 * name.  In the second case, the standard library routine gethostbyname(3n)
 * (which uses the NIS services) is called to do the translation.  
 */
bool
get_master_addr()
{
	struct in_addr tempaddr;
	struct hostent *h;
	bool error = FALSE;

	if (master==NULL) {
		/*
		 * if we were unable to get the master name, use the
		 * address of the person who called us.
		 */
	    if (callback) {
	       master_addr.s_addr = inet_addr(ipadd);
	       master = ipadd;
	       return (TRUE);
	    }
	    return (FALSE);
	}

	if (isdigit(*master) ) {
		tempaddr.s_addr = inet_addr(master);

		if ((int) tempaddr.s_addr != -1) {
			master_addr = tempaddr;
		} else {
			error = TRUE;
		}

	} else {

		if (h = gethostbyname(master) ) {
			(void) bcopy(h->h_addr, (char *) &master_addr,
			    h->h_length);
		} else {
			error = TRUE;
		}
	}
	
	if (error) {
		logprintf(
		   "Can't translate master name %s to an address.\n", master);
		return (FALSE);
	} else {
		return (TRUE);
	}
}

/*
 * This sets up a udp connection to speak the correct program and version
 * to a NIS server.  vers is set to one of YPVERS or YPOLDVERS to reflect which
 * language the server speaks.
 */
bool
bind_to_server(host, host_addr, pdomb, vers, status)
	char *host;
	struct in_addr host_addr;
	struct dom_binding *pdomb;
	unsigned int *vers;
	int *status;
{
	if (ping_server(host, host_addr, pdomb, YPVERS, status)) {
		*vers = YPVERS;
		return (TRUE);
	} else if (*status == YPPUSH_YPERR) {
		return (FALSE);
	} else {
		if (ping_server(host, host_addr, pdomb, YPOLDVERS, status)) {
			*vers = YPOLDVERS;
			return (TRUE);
		} else {
			return (FALSE);
		}
	}
}

/*
 * This sets up a UDP channel to a server which is assumed to speak an input
 * version of YPPROG.  The channel is tested by pinging the server.  In all
 * error cases except "Program Version Number Mismatch", the error is
 * reported, and in all error cases, the client handle is destroyed and the
 * socket associated with the channel is closed.
 */
bool
ping_server(host, host_addr, pdomb, vers, status)
	char *host;
	struct in_addr host_addr;
	struct dom_binding *pdomb;
	unsigned int vers;
	int *status;
{
	enum clnt_stat rpc_stat;
	
	pdomb->dom_server_addr.sin_addr = host_addr;
	pdomb->dom_server_addr.sin_family = AF_INET;
	pdomb->dom_server_addr.sin_port = htons((u_short) 0);
	pdomb->dom_server_port = htons((u_short) 0);
	pdomb->dom_socket = RPC_ANYSOCK;

	if (pdomb->dom_client = clntudp_create(&(pdomb->dom_server_addr),
	    YPPROG, vers, udp_intertry, &(pdomb->dom_socket )) ) {
		    
		/*
		 * when running secure, we should only accept data
		 * from a server which is on a reserved port.
		 */
		if (run_secure &&
		    (pdomb->dom_server_addr.sin_family != AF_INET ||
		    pdomb->dom_server_addr.sin_port >= IPPORT_RESERVED) ) {
			clnt_destroy(pdomb->dom_client);
			close(pdomb->dom_socket);
			(void) logprintf("bind_to_server: server %s is not using a privileged port\n", host);
			*status = YPPUSH_YPERR;
			return (FALSE);
		}

		rpc_stat = clnt_call(pdomb->dom_client, YPBINDPROC_NULL,
		    xdr_void, 0, xdr_void, 0, udp_timeout);

		if (rpc_stat == RPC_SUCCESS) {
			return (TRUE);
		} else {
			clnt_destroy(pdomb->dom_client);
			close(pdomb->dom_socket);
			
			if (rpc_stat != RPC_PROGVERSMISMATCH) {
				logprintf(
				    "bind_to_server %s: RPC call error%s\n",
				    host, clnt_sperror(pdomb->dom_client, ""));
			}
			
			*status = YPPUSH_RPC;
			return (FALSE);
		}
	} else {
		logprintf("bind_to_server %s: clntudp_create error%s",
			host, clnt_spcreateerror(""));
		*status = YPPUSH_RPC;
		return (FALSE);
	}
}

/*
 * This gets values for the YP_LAST_MODIFIED and YP_MASTER_NAME keys from the
 * master server's version of the map.  Values are held in static variables
 * here.  In the success cases, global pointer variables are set to point at
 * the local statics.  
 */
bool
get_private_recs(pushstat)
	int *pushstat;
{
	static char anumber[20];
	static unsigned number;
	static char name[YPMAXPEER + 1];
	int status;

	status = 0;
	
	if (get_order(anumber, &number, &status) ) {
		master_version = &number;
		master_ascii_version = anumber;
		if (debug) fprintf(stderr,"ypxfr: Master Version is %s\n",master_ascii_version);
	} else {

		if (status != 0) {
			*pushstat = status;
			if (debug) fprintf(stderr,"ypxfr: Couldn't get map's master version number, status was %d\n",status);
			return (FALSE);
		}
	}

	if (get_master_name(name, &status) ) {
		master_name = name;
		if (debug) fprintf(stderr,"ypxfr: Maps master is '%s'\n",master_name);
	} else {

		if (status != 0) {
			*pushstat = status;
			if (debug) fprintf(stderr,"ypxfr: Couldn't get map's master name, status was %d\n",status);
			return (FALSE);
		}
		master_name = master;
	}

	return (TRUE);
}

/*
 * This gets the map's order number from the master server
 */
bool
get_order(an, n, pushstat)
	char *an;
	unsigned *n;
	int *pushstat;
{
	if (master_prog_vers == YPVERS) {
		return (get_v2order(an, n, pushstat) );
	} else {
		return (get_v1order(an, n, pushstat) );
	}
}

bool
get_v1order(an, n, pushstat)
	char *an;
	unsigned *n;
	int *pushstat;
{
	struct yprequest req;
	struct ypresponse resp;
	bool retval;

	retval = FALSE;
	req.yp_reqtype = YPMATCH_REQTYPE;
	req.ypmatch_req_domain = source;
	req.ypmatch_req_map = map;
	req.ypmatch_req_keyptr = yp_last_modified;
	req.ypmatch_req_keysize = yp_last_modified_sz;
	
	resp.ypmatch_resp_valptr = NULL;
	resp.ypmatch_resp_valsize = 0;

	if(clnt_call(master_server.dom_client, YPOLDPROC_MATCH, _xdr_yprequest,
	    &req, _xdr_ypresponse, &resp, udp_timeout) == RPC_SUCCESS) {

		if (resp.ypmatch_resp_status == YP_TRUE) {
			bcopy(resp.ypmatch_resp_valptr, an,
			    resp.ypmatch_resp_valsize);
			an[resp.ypmatch_resp_valsize] = '\0';
			*n = atoi(an);
			retval = TRUE;
		} else if (resp.ypmatch_resp_status != YP_NOKEY) {
			*pushstat = map_yperr_to_pusherr(
			    resp.ypmatch_resp_status);

			if (interactive) {
				logprintf(
    "(info) Can't get order number from ypserv at %s.  Reason: %s.\n",
				    master, yperr_string(ypprot_err(
				    (unsigned) resp.ypmatch_resp_status)) );
			}
		}

		CLNT_FREERES(master_server.dom_client, _xdr_ypresponse, &resp);
	} else {
		*pushstat = YPPUSH_RPC;
		logprintf("get_v1order: RPC call to %s failed%s\n", master,
			    clnt_sperror(master_server.dom_client, ""));
	}

	return(retval);
}

bool
get_v2order(an, n, pushstat)
	char *an;
	unsigned *n;
	int *pushstat;
{
	struct ypreq_nokey req;
	struct ypresp_order resp;
	int retval;

	req.domain = source;
	req.map = map;
	
	/*
	 * Get the map''s order number, null-terminate it and store it,
	 * and convert it to binary and store it again.
	 */
	retval = FALSE;
	
	if((enum clnt_stat) clnt_call(master_server.dom_client,
	    YPPROC_ORDER, xdr_ypreq_nokey, &req, xdr_ypresp_order, &resp,
	    udp_timeout) == RPC_SUCCESS) {

		if (resp.status == YP_TRUE) {
			sprintf(an, "%d", resp.ordernum);
			*n = resp.ordernum;
			retval = TRUE;
		} else if (resp.status != YP_BADDB) {
			*pushstat = ypprot_err(resp.status);
			
			if (interactive) {
				logprintf(
    "(info) Can't get order number from ypserv at %s.  Reason: %s.\n",
				    master, yperr_string(
				    ypprot_err(resp.status)) );
			}
		}

		CLNT_FREERES(master_server.dom_client, xdr_ypresp_order,
		    &resp);
	} else {
		*pushstat = YPPUSH_RPC;
		logprintf("get_v2order: RPC call to %s failed%s\n", master,
			    clnt_sperror(master_server.dom_client, ""));
	}

	return (retval);
}

/*
 * This gets the map's master name from the master server
 */
bool
get_master_name(name, pushstat)
	char *name;
	int *pushstat;
{
	if (master_prog_vers == YPVERS) {
		return (get_v2master_name(name, pushstat));
	} else {
		return (get_v1master_name(name, pushstat));
	}
}

bool
get_v1master_name(name, pushstat)
	char *name;
	int *pushstat;
{
	struct yprequest req;
	struct ypresponse resp;
	bool retval;

	retval = FALSE;
	req.yp_reqtype = YPMATCH_REQTYPE;
	req.ypmatch_req_domain = source;
	req.ypmatch_req_map = map;
	req.ypmatch_req_keyptr = yp_master_name;
	req.ypmatch_req_keysize = yp_master_name_sz;
	
	resp.ypmatch_resp_valptr = NULL;
	resp.ypmatch_resp_valsize = 0;

	if(clnt_call(master_server.dom_client, YPOLDPROC_MATCH, _xdr_yprequest,
	    &req, _xdr_ypresponse, &resp, udp_timeout) == RPC_SUCCESS) {

		if (resp.ypmatch_resp_status == YP_TRUE) {
			bcopy(resp.ypmatch_resp_valptr, name,
			    resp.ypmatch_resp_valsize);
			name[resp.ypmatch_resp_valsize] = '\0';
			retval = TRUE;
		} else if (resp.ypmatch_resp_status != YP_NOKEY) {
			*pushstat = map_yperr_to_pusherr(
			    resp.ypmatch_resp_status);

			logprintf(
	"(info) Can't get master name from ypserv at %s. Reason: %s.\n",
				    master, 
				    yperr_string(ypprot_err(
				    (unsigned) resp.ypmatch_resp_status)) );
		}
		
		CLNT_FREERES(master_server.dom_client, _xdr_ypresponse, &resp);
	} else {
		*pushstat = YPPUSH_RPC;
		logprintf("get_v1master_name: RPC call to %s failed%s\n",master,
			    clnt_sperror(master_server.dom_client, ""));
	}

	return(retval);
}

bool
get_v2master_name(name, pushstat)
	char *name;
	int *pushstat;
{
	struct ypreq_nokey req;
	struct ypresp_master resp;
	int retval;

	req.domain = source;
	req.map = map;
	resp.master = NULL;
	retval = FALSE;
	
	if((enum clnt_stat) clnt_call(master_server.dom_client,
	    YPPROC_MASTER, xdr_ypreq_nokey, &req, xdr_ypresp_master, &resp,
	    udp_timeout) == RPC_SUCCESS) {

		if (resp.status == YP_TRUE) {
			strcpy(name, resp.master);
			retval = TRUE;
		} else if (resp.status != YP_BADDB) {
			*pushstat = ypprot_err(resp.status);

			if (interactive) {
				logprintf(
"(info) Can't get master name from ypserv at %s. Reason: %s.\n",
				    master, yperr_string(
				    ypprot_err(resp.status)) );
			}
		}
	
		CLNT_FREERES(master_server.dom_client, xdr_ypresp_master,
		    &resp);
	} else {
		*pushstat = YPPUSH_RPC;
		logprintf("get_v2master_name: RPC call to %s failed%s\n",master,
			    clnt_sperror(master_server.dom_client, ""));
	}

	return (retval);
}

/*
 * This tries to get the master name for the named map, from any
 * server's version, using the vanilla NIS client interface.  If we get a
 * name back, the global "master" gets pointed to it.
 */
void
find_map_master()
{
	int err;
		
	if (err = yp_master(source, map, &master)) {
		logprintf("Can't get master of %s. Reason: %s.\n", map,
		    yperr_string(err));
	}
	if (!get_master_addr() ) {
		xfr_exit(YPPUSH_MADDR);
	} 
	yp_unbind(source);
}

/*
 * This does the work of transferrring the map.
 */
bool
move_map(pushstat)
	int *pushstat;
{
	unsigned local_version;
	char map_name[MAXNAMLEN + 1];
	char tmp_name[MAXNAMLEN + 1];
	char bkup_name[MAXNAMLEN + 1];
	char an[11];
	unsigned n;
	datum key;
	datum val;

	mkfilename(map_name);
	
	if (!force) {
		local_version = get_local_version(map_name);
		if (debug) fprintf(stderr,"ypxfr: Local version of map '%s' is %d\n",map_name,local_version);

		if (local_version >= *master_version) {
			if (interactive || logging)
			logprintf(
			    "Map %s at %s is not more recent than local.\n",
			    map, master);
			*pushstat = YPPUSH_AGE;
			return (FALSE);
		}
	}
	 
	mk_tmpname(yptempname_prefix, tmp_name);
	
	if (!new_mapfiles(tmp_name) ) {
		logprintf("Can't create temp map %s.\n", tmp_name);
		*pushstat = YPPUSH_FILE;
		return (FALSE);
	}

	if (dbminit(tmp_name) < 0) {
		logprintf("Can't dbm init temp map %s.\n", tmp_name);
		del_mapfiles(tmp_name);
		*pushstat = YPPUSH_DBM;
		return (FALSE);
	}

	if (!get_map(tmp_name, pushstat) ) {
		del_mapfiles(tmp_name);
		return (FALSE);
	}

	if (!add_private_entries(tmp_name) ) {
		del_mapfiles(tmp_name);
		*pushstat = YPPUSH_DBM;
		return (FALSE);
	}

	/*
	 * Decide whether the map just transferred is a secure map.
	 * If we already know the local version was secure, we do not
	 * need to check this version.
	 */
	if (!secure_map) {
		key.dptr = yp_secure;
		key.dsize = yp_secure_sz;
		val = fetch(key);
		if (val.dptr != NULL) {
			secure_map = TRUE;
		}
	}

	dbmclose();

	if (!get_order(an, &n, pushstat)) {
		return(FALSE);
	}
	if (n != *master_version) {
		logprintf("Version skew at %s while transferring map %s.\n",
		    	  master, map);
		del_mapfiles(tmp_name);
		*pushstat = YPPUSH_SKEW;
		return (FALSE);
	}

# ifdef PARANOID
	if (!count_mismatch(tmp_name,entry_count)) {
		del_mapfiles(tmp_name);
		*pushstat = YPPUSH_DBM;
		return(FALSE);
	}
# endif /* PARANOID */

	if (!check_map_existence(map_name) ) {

		if (!rename_map(tmp_name, map_name) ) {
			del_mapfiles(tmp_name);
			logprintf("Rename error:  couldn't mv %s to %s.\n",
			    	  tmp_name, map_name);
			*pushstat = YPPUSH_FILE;
			return (FALSE);
		}
		
	} else {
		mk_tmpname(ypbkupname_prefix, bkup_name);
	
		if (!rename_map(map_name, bkup_name) ) {
			(void) rename_map(bkup_name, map_name);
			logprintf(
			  "Rename error:  check that old %s is still intact.\n",
			    map_name);
			del_mapfiles(tmp_name);
			*pushstat = YPPUSH_FILE;
			return (FALSE);
		}

		if (rename_map(tmp_name, map_name) ) {
			del_mapfiles(bkup_name);
		} else {
			del_mapfiles(tmp_name);
			(void) rename_map(bkup_name, map_name);
				logprintf(
			  "Rename error:  check that old %s is still intact.\n",
			    map_name);
			*pushstat = YPPUSH_FILE;
			return (FALSE);
		}
	}

	return (TRUE);
}

/*
 * This tries to get the order number out of the local version of the map.
 * If the attempt fails for any version, the function will return "0"
 */
unsigned
get_local_version(name)
	char *name;
{
	datum key;
	datum val;
	datum secureval;
	char number[11];
	
	if (!check_map_existence(name) ) {
		return (0);
	}
	if (debug)
	    fprintf(stderr,"ypxfr: Map does exist, checking version now.\n");

	if (dbminit(name) < 0) {
		return (0);
	}
		
	key.dptr = yp_last_modified;
	key.dsize = yp_last_modified_sz;
	val = fetch(key);
	if (!val.dptr) {	/* Check to see if dptr is NULL */
		return (0);
	}
	if (val.dsize == 0 || val.dsize > 10) {
		return (0);
	}
	/* Now save this value while we have it available */
	(void) bcopy(val.dptr, number, val.dsize);
	number[val.dsize] = '\0';

	if (debug) fprintf(stderr,"ypxfr:[a] Last Modified '%s'\n",number);

	/* Now check to see if it is 'secure' */
	/* while we are here, decide if the local map is secure */
	key.dptr = yp_secure;
	key.dsize = yp_secure_sz;
	secureval = fetch(key);
	if (secureval.dptr != NULL) {
		secure_map = TRUE;
	}
	/* finish up */
	dbmclose();
	/* check to see that number didn't get stomped on */
	if (debug)
		fprintf(stderr,"ypxfr:[b] Last Modified '%s'\n",number);

	return ((unsigned) atoi(number) );
}

/*
 * This constructs a file name for a map, minus its dbm_dir or dbm_pag extensions
 */
void
mkfilename(ppath)
	char *ppath;
{
	char xname[MAXNAMLEN];

	if ( (strlen(domain) + strlen(map) + strlen(ypdbpath) + 3) 
	    > (MAXNAMLEN + 1) ) {
		logprintf("Map name string too long.\n");
	}

	(void) strcpy(ppath, ypdbpath);
	(void) strcat(ppath, "/");
	(void) strcat(ppath, domain);
	(void) strcat(ppath, "/");
	/*
	 * Translate logical map name into (shorter) disk file name
	 */
	(void) yp_make_dbname(map, xname, sizeof xname);
	(void) strcat(ppath, xname);
}

/*
 * This returns a temporary name for a map transfer minus its dbm_dir or
 * dbm_pag extensions.
 */
void
mk_tmpname(prefix, xfr_name)
	char *prefix;
	char *xfr_name;
{
	char xfr_anumber[10];
	long xfr_number;

	if (!xfr_name) {
		return;
	}

	xfr_number = getpid();
	(void) sprintf(xfr_anumber, "%d", xfr_number);
	
	(void) strcpy(xfr_name, ypdbpath);
	(void) strcat(xfr_name, "/");
	(void) strcat(xfr_name, domain);
	(void) strcat(xfr_name, "/");
	(void) strcat(xfr_name, prefix);
	(void) strcat(xfr_name, map);
	(void) strcat(xfr_name, ".");
	(void) strcat(xfr_name, xfr_anumber);
	(void) mktemp(xfr_name);
}

/*
 * This deletes the .pag and .dir files which implement a map.
 *
 * Note:  No error checking is done here for a garbage input file name or for
 * failed unlink operations.
 */
void
del_mapfiles(basename)
	char *basename;
{
	char dbfilename[MAXNAMLEN + 1];

	if (!basename) {
		return;
	}
	
	strcpy(dbfilename, basename);
	strcat(dbfilename, dbm_pag);
	unlink(dbfilename);
	strcpy(dbfilename, basename);
	strcat(dbfilename, dbm_dir);
	unlink(dbfilename);
}

/*
 * This checks to see if the source map files exist, then renames them to the
 * target names.  This is a boolean function.  The file names from.pag and
 * from.dir will be changed to to.pag and to.dir in the success case.
 *
 * Note:  If the second of the two renames fails, yprename_map will try to
 * un-rename the first pair, and leave the world in the state it was on entry.
 * This might fail, too, though...
 */
bool
rename_map(from, to)
	char *from;
	char *to;
{
	char fromfile[MAXNAMLEN + 1];
	char tofile[MAXNAMLEN + 1];
	char savefile[MAXNAMLEN + 1];

	if (!from || !to) {
		return (FALSE);
	}
	
	if (!check_map_existence(from) ) {
		return (FALSE);
	}
	
	(void) strcpy(fromfile, from);
	(void) strcat(fromfile, dbm_pag);
	(void) strcpy(tofile, to);
	(void) strcat(tofile, dbm_pag);
	
	if (rename(fromfile, tofile) ) {
		logprintf("Can't mv %s to %s: %s\n",
			fromfile, tofile, strerror(errno));
		return (FALSE);
	}
	
	(void) strcpy(savefile, tofile);
	(void) strcpy(fromfile, from);
	(void) strcat(fromfile, dbm_dir);
	(void) strcpy(tofile, to);
	(void) strcat(tofile, dbm_dir);
	
	if (rename(fromfile, tofile) ) {
		logprintf("Can't mv %s to %s: %s\n",
			fromfile, tofile, strerror(errno));

		(void) strcpy(fromfile, from);
		(void) strcat(fromfile, dbm_pag);
		(void) strcpy(tofile, to);
		(void) strcat(tofile, dbm_pag);
		
		if (rename(tofile, fromfile) ) {
			logprintf("Can't recover from rename failure.\n");
		}
		
		return (FALSE);
	}
	
	if (!secure_map) {
		chmod(savefile, 0644);
		chmod(tofile, 0644);
	}

	return (TRUE);
}

/*
 * This performs an existence check on the dbm data base files <pname>.pag and
 * <pname>.dir.
 */
bool
check_map_existence(pname)
	char *pname;
{
	char dbfile[MAXNAMLEN + 1];
	struct stat filestat;
	int len;

	if (debug) fprintf(stderr,"ypxfr: Checking the existence of '%s'\n",pname);
	if (!pname || ((len = strlen(pname)) == 0) ||
	    (len + 5) > (MAXNAMLEN + 1) ) {
		return (FALSE);
	}
		
	errno = 0;
	(void) strcpy(dbfile, pname);
	(void) strcat(dbfile, dbm_dir);

	if (debug) fprintf(stderr,"ypxfr: First file is '%s'\n",dbfile);
	if (stat(dbfile, &filestat) != -1) {
		(void) strcpy(dbfile, pname);
		(void) strcat(dbfile, dbm_pag);

		if (stat(dbfile, &filestat) != -1) {
			return (TRUE);
		} else {

			if (errno != ENOENT) {
				logprintf("stat error on map file %s: %s\n",
				    	  dbfile, strerror(errno));
			}

			return (FALSE);
		}

	} else {

		if (errno != ENOENT) {
			logprintf("stat error on map file %s: %s\n",
				    	  dbfile, strerror(errno));
		}

		return (FALSE);
	}
}

/*
 * This creates <pname>.dir and <pname>.pag
 */
bool
new_mapfiles(pname)
	char *pname;
{
	char dbfile[MAXNAMLEN + 1];
	int f;
	int len;

	if (!pname || ((len = strlen(pname)) == 0) ||
	    (len + 5) > (MAXNAMLEN + 1) ) {
		return (FALSE);
	}
		
	errno = 0;
	(void) strcpy(dbfile, pname);
	(void) strcat(dbfile, dbm_dir);

	if ((f = open(dbfile, (O_WRONLY | O_CREAT | O_TRUNC), 0600)) >= 0) {
		(void) close(f);
		(void) strcpy(dbfile, pname);
		(void) strcat(dbfile, dbm_pag);

		if ((f = open(dbfile, (O_WRONLY | O_CREAT | O_TRUNC),
		    0600)) >= 0) {
			(void) close(f);
			return (TRUE);
		} else {
			return (FALSE);
		}

	} else {
		return (FALSE);
	}
}

count_callback(status) 
	int status;
{
	if (status != YP_TRUE) {
		
		if (status != YP_NOMORE) {
			logprintf(
			    "Error from ypserv on %s (ypall_callback) = %s.\n",
			    master, yperr_string(ypprot_err(status)));
		}
		
		return(TRUE);
	}

	entry_count++;
	return(FALSE);
}

/*
 * This counts the entries in the dbm file after the transfer to
 * make sure that the dbm file was built correctly.  
 * Returns TRUE if everything is OK, FALSE if they mismatch.
 */
count_mismatch(pname,oldcount)
	char *pname;
	int oldcount;
{
	datum key;
# ifdef REALLY_PARANOID
	datum value;
	struct dom_binding domb;
	enum clnt_stat s;
	struct ypreq_nokey allreq;
	struct ypall_callback cbinfo;
# endif

	entry_count = 0;
	if (dbminit(pname) < 0) {
	    logprintf("count_mismatch: can't init dbm map %s.\n", pname);
	    return(FALSE);
	}
	for (key = firstkey(); key.dptr != NULL; key = nextkey(key))
	    entry_count++;
	dbmclose();

	if (oldcount != entry_count) {
	    logprintf("*** Count mismatch in dbm file %s: old=%d, new=%d ***\n",
			map, oldcount, entry_count);
	    return(FALSE);
	}

# ifdef REALLY_PARANOID
	domb.dom_server_addr.sin_addr = master_addr;
	domb.dom_server_addr.sin_family = AF_INET;
	domb.dom_server_addr.sin_port = htons((u_short) 0);
	domb.dom_server_port = htons((u_short) 0);
	domb.dom_socket = RPC_ANYSOCK;

	if ((domb.dom_client = clnttcp_create(&(domb.dom_server_addr),
	    YPPROG, master_prog_vers, &(domb.dom_socket), 0, 0)) ==
	    (CLIENT *) NULL) {
		logprintf("%s", clnt_spcreateerror(
		    "count_mismatch: TCP channel create failure"));
		return(FALSE);
	}

	if (master_prog_vers == YPVERS) {
		int tmpstat;

		allreq.domain = source;
		allreq.map = map;
		cbinfo.foreach = count_callback;
		tmpstat = 0;
		cbinfo.data = (char *) &tmpstat;
	
		entry_count = 0;
		s = clnt_call(domb.dom_client, YPPROC_ALL, xdr_ypreq_nokey,
		    &allreq, xdr_ypall, &cbinfo, tcp_timeout);

		if (tmpstat == 0) {
			if (s == RPC_SUCCESS) {
			} else {
				logprintf("%s\n", clnt_sperror(domb.dom_client,
			  "count_mismatch: RPC clnt_call (TCP) failure"));
		    		return(FALSE);
			}
			
		} else {
		    return(FALSE);
		}
		
	} else {
	    logprintf("Wrong version number!\n");
	    return(FALSE);
	}
    clnt_destroy(domb.dom_client);
    close(domb.dom_socket);
    entry_count += 2;			/* add in YP_entries */
    if (oldcount != entry_count) {
	logprintf(
	"*** Count mismatch after enumerate %s: old=%d, new=%d ***\n",
		    map, oldcount, entry_count);
	return(FALSE);
    }
# endif /* REALLY_PARANOID */

    return(TRUE);
}

/*
 * This sets up a TCP connection to the master server, and either gets
 * ypall_callback to do all the work of writing it to the local dbm file
 * (if the ypserv is current version), or does it itself for an old ypserv.  
 */
bool
get_map(pname, pushstat)
	char *pname;
	int *pushstat;
{
	struct dom_binding domb;
	enum clnt_stat s;
	struct ypreq_nokey allreq;
	struct ypall_callback cbinfo;
	struct yprequest oldreq;
	struct ypresponse resp;
	bool retval = FALSE;
	int tmpstat;
	
	domb.dom_server_addr.sin_addr = master_addr;
	domb.dom_server_addr.sin_family = AF_INET;
	domb.dom_server_addr.sin_port = htons((u_short) 0);
	domb.dom_server_port = htons((u_short) 0);
	domb.dom_socket = RPC_ANYSOCK;

	if ((domb.dom_client = clnttcp_create(&(domb.dom_server_addr),
	    YPPROG, master_prog_vers, &(domb.dom_socket), 0, 0)) ==
	    (CLIENT *) NULL) {
		logprintf("%s", clnt_spcreateerror(
			"get_map: TCP channel create failure"));
		*pushstat = YPPUSH_RPC;
		return(FALSE);
	}

	entry_count = 0;
	if (master_prog_vers == YPVERS) {
		allreq.domain = source;
		allreq.map = map;
		cbinfo.foreach = ypall_callback;
		tmpstat = 0;
		cbinfo.data = (char *) &tmpstat;
	
		s = clnt_call(domb.dom_client, YPPROC_ALL, xdr_ypreq_nokey,
		    &allreq, xdr_ypall, &cbinfo, tcp_timeout);

		if (tmpstat == 0) {
			
			if (s == RPC_SUCCESS) {
				retval = TRUE;
			} else {
				logprintf("%s\n", clnt_sperror(domb.dom_client,
			  "get_map/all: RPC clnt_call (TCP) failure"));
				*pushstat = YPPUSH_RPC;
			}
			
		} else {
			*pushstat = tmpstat;
		}
		
	} else {
		datum inkey, inval;

		oldreq.yp_reqtype = YPFIRST_REQTYPE;
		oldreq.ypfirst_req_domain = source;
		oldreq.ypfirst_req_map = map;
	
		resp.ypfirst_resp_keyptr = NULL;
		resp.ypfirst_resp_keysize = 0;
		resp.ypfirst_resp_valptr = NULL;
		resp.ypfirst_resp_valsize = 0;
	
		if((s = clnt_call(domb.dom_client, YPOLDPROC_FIRST,
		    _xdr_yprequest, &oldreq, _xdr_ypresponse,
		    &resp, tcp_timeout)) != RPC_SUCCESS) {
			logprintf("%s\n", clnt_sperror(domb.dom_client,
			    "get_map/first: RPC clnt_call (TCP) failure"));
			*pushstat = YPPUSH_RPC;
			goto cleanup;
		}

		if (resp.ypfirst_resp_status != YP_TRUE) {
			logprintf(
			    "Error from ypserv on %s (get first) = %s.\n",
			    master, yperr_string(ypprot_err(
			    resp.ypfirst_resp_status)));
			*pushstat = YPPUSH_RPC;
			goto cleanup;
		}

		inkey = resp.ypfirst_resp_keydat;
		inval = resp.ypfirst_resp_valdat;
	
		/*
		 * Continue to get the next entries in the map as long as
		 * there are no errors, and there are entries remaining.  
		 */
		oldreq.yp_reqtype = YPNEXT_REQTYPE;
		oldreq.ypnext_req_domain = source;
		oldreq.ypnext_req_map = map;

		while (TRUE) {
			if (strncmp(inkey.dptr,yp_prefix,yp_prefix_sz)) {
				if (store(inkey, inval) < 0) {
					logprintf(
				    "Can't do dbm store into temp map %s.\n",
				    	    pname);
					*pushstat = YPPUSH_DBM;
					goto cleanup;
				}
				entry_count++;
			}
			CLNT_FREERES(domb.dom_client, _xdr_ypresponse, &resp);

			oldreq.ypnext_req_keydat = inkey;
			resp.ypnext_resp_keydat.dptr = NULL;
			resp.ypnext_resp_valdat.dptr = NULL;
			resp.ypnext_resp_keydat.dsize = 0;
			resp.ypnext_resp_valdat.dsize = 0;
	
			if ((s = clnt_call(domb.dom_client, YPOLDPROC_NEXT,
		    	    _xdr_yprequest, &oldreq, _xdr_ypresponse,
		    	    &resp, tcp_timeout)) != RPC_SUCCESS) {
				logprintf("%s\n", clnt_sperror(domb.dom_client,
				 "get_map/next: RPC clnt_call (TCP) failure"));
				*pushstat = YPPUSH_RPC;
				break;
			}

			if (resp.ypnext_resp_status != YP_TRUE) {

				if (resp.ypnext_resp_status == YP_NOMORE) {
					retval = TRUE;
				} else {
					logprintf(
				  "Error from ypserv on %s (get next) = %d.\n",
			    		    master, 
					    yperr_string(ypprot_err(
					    resp.ypnext_resp_status)));
					*pushstat = YPPUSH_RPC;
				}
				
				break;
			}

			inkey = resp.ypnext_resp_keydat;
			inval = resp.ypnext_resp_valdat;
		}
	}
cleanup:
	clnt_destroy(domb.dom_client);
	close(domb.dom_socket);
	return (retval);
}

/*
 * This sticks each key-value pair into the current map.  It returns FALSE as
 * long as it wants to keep getting called back, and TRUE on error conditions
 * and "No more k-v pairs".
 */
int
ypall_callback(status, key, kl, val, vl, pushstat)
	int status;
	char *key;
	int kl;
	char *val;
	int vl;
	int *pushstat;
{
	datum keydat;
	datum valdat;
	datum test;

	if (status != YP_TRUE) {
		
		if (status != YP_NOMORE) {
			logprintf(
			    "Error from ypserv on %s (ypall_callback) = %s.\n",
			    master, yperr_string(ypprot_err(status)));
			*pushstat = map_yperr_to_pusherr(status);
		}
		
		return(TRUE);
	}

	keydat.dptr = key;
	keydat.dsize = kl;
	valdat.dptr = val;
	valdat.dsize = vl;
	entry_count++;

# ifdef PARANOID
	test = fetch(keydat);
	if (test.dptr!=NULL) {
		logprintf("Duplicate key %s in map %s\n",key,map);
		*pushstat  = YPPUSH_DBM;
		return(TRUE);
	}
# endif /* PARANOID */
	if (store(keydat, valdat) < 0) {
		logprintf("Can't do dbm store into temp map %s.\n",map);
		*pushstat  = YPPUSH_DBM;
		return(TRUE);
	}
# ifdef PARANOID
	test = fetch(keydat);
	if (test.dptr==NULL) {
		logprintf("Key %s was not inserted into dbm file %s\n",
			  key,map);
		*pushstat  = YPPUSH_DBM;
		return(TRUE);
	}
# endif /* PARANOID */
	return(FALSE);
}

/*
 * This maps a YP_xxxx error code into a YPPUSH_xxxx error code
 */
int
map_yperr_to_pusherr(yperr)
	int yperr;
{
	int reason;

	switch (yperr) {
	
 	case YP_NOMORE:
		reason = YPPUSH_SUCC;
		break;

 	case YP_NOMAP:
		reason = YPPUSH_NOMAP;
		break;

 	case YP_NODOM:
		reason = YPPUSH_NODOM;
		break;

 	case YP_NOKEY:
		reason = YPPUSH_YPERR;
		break;

 	case YP_BADARGS:
		reason = YPPUSH_BADARGS;
		break;

 	case YP_BADDB:
		reason = YPPUSH_YPERR;
		break;

	default:
		reason = YPPUSH_XFRERR;
		break;
	}
	
  	return(reason);
}

/*
 * This writes the last-modified and master entries into the new dbm file
 */
bool
add_private_entries(pname)
	char *pname;
{
	datum key;
	datum val;

	if (!fake_master_version) {
		key.dptr = yp_last_modified;
		key.dsize = yp_last_modified_sz;
		val.dptr = master_ascii_version;
		val.dsize = strlen(master_ascii_version);

		if (store(key, val) < 0) {
			logprintf("Can't do dbm store into temp map %s.\n",
		    	    	  pname);
			return (FALSE);
		}
		entry_count++;
	}
	
	if (master_name) {
		key.dptr = yp_master_name;
		key.dsize = yp_master_name_sz;
		val.dptr = master_name;
		val.dsize = strlen(master_name);
		if (store(key, val) < 0) {
			logprintf("Can't do dbm store into temp map %s.\n",
		    	    	  pname);
			return (FALSE);
		}
		entry_count++;
	}
	
	return (TRUE);
}
	
	
/*
 * This sends a YPPROC_CLEAR message to the local ypserv process.  If the
 * local ypserv is a v.1 protocol guy, we'll just say we succeeded.  Such a
 * situation is outlandish - why are they running the old ypserv at the same
 * place they are running ypxfr?  And who are they, anyway?
 */
bool
send_ypclear(pushstat)
	int *pushstat;
{
	struct sockaddr_in myaddr;
	struct dom_binding domb;
	char local_host_name[MAXHOSTNAMELEN];
	unsigned int progvers;
	int status;

	get_myaddress(&myaddr);

	if (gethostname(local_host_name, sizeof(local_host_name))) {
		logprintf("Can't get local machine name.\n");
		*pushstat = YPPUSH_RSRC;
		return (FALSE);
	}

	if (!bind_to_server(local_host_name, myaddr.sin_addr, &domb,
	    &progvers, &status) ) {
		*pushstat = YPPUSH_CLEAR;
		return (FALSE);
	}

	if (progvers == YPOLDVERS)
		return (TRUE);

	if((enum clnt_stat) clnt_call(domb.dom_client,
	    YPPROC_CLEAR, xdr_void, 0, xdr_void, 0,
	    udp_timeout) != RPC_SUCCESS) {
		logprintf(
		"Can't send ypclear message to ypserv on the local machine.\n");
		xfr_exit(YPPUSH_CLEAR);
	}

	return (TRUE);
}

/*
 * This decides if send_callback has to get called, and does the process exit.
 */
void
xfr_exit(status)
	int status;
{
	if (callback) {
		send_callback(&status);
	}
	if (status == YPPUSH_SUCC) {
		exit(0);
	} else {
		if (interactive || logging) {
			logprintf("ypxfr: error %d\n", status);
		}
		exit(1);
	}
}

/*
 * This sets up a UDP connection to the yppush process which contacted our
 * parent ypserv, and sends him a status on the requested transfer.
 *
 * N.B. keep in sync with ypserv/ypserv_proc.c's version.
 */
void
send_callback(status)
	int *status;
{
	struct yppushresp_xfr resp;
	struct dom_binding domb;

	resp.transid = (unsigned long) atoi(tid);
	resp.status = (unsigned long) *status;

	domb.dom_server_addr.sin_addr.s_addr = inet_addr(ipadd);
	domb.dom_server_addr.sin_family = AF_INET;
	domb.dom_server_addr.sin_port = (unsigned short) htons(atoi(port));
	domb.dom_server_port = domb.dom_server_addr.sin_port;
	domb.dom_socket = RPC_ANYSOCK;

	udp_intertry.tv_sec = CALLINTER_TRY;
	udp_timeout.tv_sec = CALLTIMEOUT;

	if ((domb.dom_client = clntudp_create(&(domb.dom_server_addr),
	    (unsigned long) htons(atoi(proto)), YPPUSHVERS, 
	    udp_intertry, &(domb.dom_socket) ) ) == NULL) {
		*status = YPPUSH_RPC;
		return;
	}	
	
	if((enum clnt_stat) clnt_call(domb.dom_client,
	    YPPUSHPROC_XFRRESP, xdr_yppushresp_xfr, &resp, xdr_void, 0, 
	    udp_timeout) != RPC_SUCCESS) {
		*status = YPPUSH_RPC;
		return;
	} 
}
