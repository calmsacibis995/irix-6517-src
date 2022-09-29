#ifndef lint
static char sccsid[] = 	"@(#)sm_statd.c	1.1 88/04/20 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

	/* sm_statd.c consists of routines used for the intermediate
	 * statd implementation(3.2 rpc.statd);
	 * it creates an entry in "current" directory for each site that it monitors;
	 * after crash and recovery, it moves all entries in "current" 
	 * to "backup" directory, and notifies the corresponding statd of its recovery.
	 */

#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/file.h>
#include <dirent.h>
#include <limits.h>
#include <rpc/rpc.h>
#include <rpcsvc/sm_inter.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include <cap_net.h>
#include <paths.h>

#include "sm_statd.h"

#define MAXPGSIZE 8192
#define SM_INIT_ALARM 15
extern int debug;
extern int Verbose;
extern int errno;
extern char STATE[], CURRENT[], BACKUP[];
extern char *strcpy(), *strcat();
int LOCAL_STATE;


extern int ha;

int ha_mode = 0;

char HA_STATE[PATH_MAX], HA_CURRENT[PATH_MAX], HA_BACKUP[PATH_MAX];
char HA2_STATE[PATH_MAX], HA2_CURRENT[PATH_MAX], HA2_BACKUP[PATH_MAX];

struct name_entry {
	char *name;
	int count;
	struct name_entry *prev;
	struct name_entry *nxt;
};
typedef struct name_entry name_entry;

name_entry *find_name();
name_entry *insert_name();
void delete_name(name_entry **, char *);
name_entry *record_q;
name_entry *recovery_q;

char hostname[MAXNAMLEN];


/*
 * called when statd first comes up; it searches the status dir. to gather
 * all entries to notify its own failure
 */
void
statd_init(char *current, char *backup, char *state)
{
	int cc, fd;
	char buf[MAXPGSIZE];
	struct dirent *dirp;
	DIR 	*dp;
	char from[MAXNAMLEN], to[MAXNAMLEN], path[MAXNAMLEN];
	struct hostent *hp;
	struct tms tm_dummy;
	clock_t start_time, end_time;
	float delta;

	if (debug)
		printf("enter statd_init\n");
	if (gethostname(hostname, MAXNAMLEN) == -1) {
		syslog(LOG_ERR, "unable to get host name: %s", strerror(errno));
		exit(1);
	}
	/* canonicalize hostname */
	if (hp = gethostbyname(hostname)) {
		(void) strncpy(hostname, hp->h_name, sizeof(hostname) -1);
	} else {
		syslog(LOG_WARNING, "unable to get canonical host name: %s",
			hstrerror(h_errno));
	}

	if ((fd = open(state, O_RDWR|O_CREAT, 0644)) < 0) {
		syslog(LOG_ERR, "state file open: %m");
		exit(1);
	}
	cc = read(fd, buf, 10);
	if (cc == 0) {
		if (debug >= 2)
			printf("empty file\n");
		LOCAL_STATE = 0;
	} else if (cc < 0) {
		syslog(LOG_ERR, "state file read: %m");
		exit(1);
	}
	sscanf(buf, "%d", &LOCAL_STATE);
	LOCAL_STATE = ((LOCAL_STATE%2) == 0) ? LOCAL_STATE+1 : LOCAL_STATE+2;
	if (lseek(fd, 0, SEEK_SET)) {
		syslog(LOG_ERR, "state file lseek: %m");
		exit(1);
	}

	if(ha)
		LOCAL_STATE = 1;		/* Force to always be odd */

	cc = sprintf(buf, "%d", LOCAL_STATE);
	(void) write(fd, buf, cc);
	if (fsync(fd) == -1) {
		syslog(LOG_ERR, "state file fsync: %m");
		exit(1);
	}
	(void) close(fd);
	if (debug)
		printf("local state = %d\n", LOCAL_STATE);
	
	if ((mkdir(current, 00777)) == -1) {
		if (errno != EEXIST) {
			syslog(LOG_ERR, "mkdir %s: %m", CURRENT);
			exit(1);
		}
	}
	if ((mkdir(backup, 00777)) == -1) {
		if (errno != EEXIST) {
			syslog(LOG_ERR, "mkdir %s: %m", BACKUP);
			exit(1);
		}
	}

	/* get all entries in CURRENT into BACKUP */
	if ((dp = opendir(current)) == NULL) {
		syslog(LOG_ERR, "opendir: %m");
		exit(1);
	}
	for (dirp = readdir(dp); dirp != NULL; dirp = readdir(dp)) {
/*
		printf("d_name = %s\n", dirp->d_name);
*/
		if (strcmp(dirp->d_name, ".") != 0  &&
		strcmp(dirp->d_name, "..") != 0) {
		/* rename all entries from current to backup */
			(void) strcpy(from , current);
			(void) strcpy(to, backup);
			(void) strcat(from, "/");
			(void) strcat(to, "/");
			(void) strcat(from, dirp->d_name);
			(void) strcat(to, dirp->d_name);
			if (rename(from, to) == -1) {
				syslog(LOG_ERR, "rename: %m");
				exit(1);
			}
			if (debug >= 2)
				printf("rename: %s to %s\n", from ,to);
		}
	}
	closedir(dp);

	/* get all entries in backup into recovery_q */
	if ((dp = opendir(backup)) == NULL) {
		syslog(LOG_ERR, "opendir backup: %m");
		exit(1);
	}

	/* For FailSafe systems only, start a timer. This will be used to 
	 * determine how long a recovery takes.
	 */
	if (ha) {
                start_time = times( &tm_dummy );
        }

	for (dirp = readdir(dp); dirp != NULL; dirp = readdir(dp)) {
		if (strcmp(dirp->d_name, ".") != 0  &&
		strcmp(dirp->d_name, "..") != 0) {
		/* get all entries from backup to recovery_q */
			if (statd_call_statd(dirp->d_name) != 0) {
				syslog(LOG_ERR, "cannot talk to statd at %s",
					dirp->d_name);
				(void) insert_name(&recovery_q, dirp->d_name);
			}
			else { /* remove from backup directory */
				(void) strcpy(path, backup);
				(void) strcat(path, "/");
				(void) strcat(path, dirp->d_name);
				if (debug >= 2)
					printf("remove monitor entry %s\n", path);
				if (unlink(path) == -1) {
					syslog(LOG_ERR, "unlink %s: %m", path);
					exit(1);
				}
			}
		}
	}
	/* For FailSafe systems only, calculate how long
	 * the transfer took. 
	 */
	if (ha) {
		end_time=times( &tm_dummy );
		delta = (float)(end_time - start_time)/sysconf(_SC_CLK_TCK);
		syslog(LOG_INFO,"recovery time = %.2f seconds for dir %s",
			delta, backup);
        }

	closedir(dp);

	/* notify statd */
	if (recovery_q != NULL)
		(void) alarm(1);
}

xdr_notify(xdrs, ntfp)
	XDR *xdrs;
	stat_chge *ntfp;
{
	if (!xdr_string(xdrs, &ntfp->name, MAXNAMLEN+1)) {
		return(FALSE);
	}
	if (!xdr_int(xdrs, &ntfp->state)) {
		return(FALSE);
	}
	return(TRUE);
}

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
			: sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif

static char *rtmsg_buf = NULL;
static char *rtmsglim = NULL;

static int
addr_match(char *sp, int addrs, struct sockaddr_in *key)
{
	register struct sockaddr *sa;
	u_char family = AF_UNSPEC;
	u_char *cp, *cplim;
	int i, j;
	struct sockaddr_in *sin;

	if (addrs == 0) {
		return(0);
	}
	for (i = 1; i; i <<= 1) {
		if (i & addrs) {
			sa = (struct sockaddr *)sp;
			if (i == RTA_IFA) {
				if (sa->sa_family == AF_INET) {
					sin = (struct sockaddr_in *)sa;
					if (sin->sin_addr.s_addr == key->sin_addr.s_addr) {
						return(1);
					}
				}
			}
			ADVANCE(sp, sa);
		}
	}
	return(0);
}

static int
addr_is_local(struct sockaddr_in *addr)
{
	size_t needed;
	int mib[6];
	char *next;
	register struct rt_msghdr *rtm;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;

	if (!rtmsg_buf) {
		/*
		 * get the list of interfaces and the addresses assigned to them
		 * first, find out how big a buffer will be needed, then get the
		 * list
		 */
		mib[0] = CTL_NET;
		mib[1] = PF_ROUTE;
		mib[2] = 0;			/* protocol */
		mib[3] = 0;			/* wildcard address family */
		mib[4] = NET_RT_IFLIST;
		mib[5] = 0;			/* no flags */
		if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
			syslog(LOG_ERR, "error getting interface list buffer size: %s",
				strerror(errno));
			exit(1);
		}
		if ((rtmsg_buf = malloc(needed)) == NULL) {
			syslog(LOG_ERR, "unable to allocate interface list buffer: %s",
				strerror(errno));
			exit(1);
		}
		if (sysctl(mib, 6, rtmsg_buf, &needed, NULL, 0) < 0) {
			syslog(LOG_ERR, "error getting interface list: %s",
				strerror(errno));
			exit(1);
		}
		rtmsglim = rtmsg_buf + needed;
	}
	/*
	 * go through all of the messages in the buffer, processing only
	 * the interface information and new address messages
	 */
	for (next = rtmsg_buf; next < rtmsglim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		switch (rtm->rtm_type) {
			case RTM_IFINFO:
				ifm = (struct if_msghdr *)rtm;
				if (addr_match((char *)(ifm + 1), ifm->ifm_addrs, addr)) {
					return(1);
				}
				break;
			case RTM_NEWADDR:
				ifam = (struct ifa_msghdr *)rtm;
				if (addr_match((char *)(ifam + 1), ifam->ifam_addrs, addr)) {
					return(1);
				}
				break;
		}
	}
	return(0);
}

statd_call_statd(char *name)
{
	int needfree = 0;
	char *local = hostname;
	struct in_addr localaddr;
	char *remote;
	char *token;
	stat_chge ntf;
	int err;
	struct sockaddr_in server_addr;
	struct in_addr *get_addr_cache();
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct timeval  tottimeout;
	register CLIENT *client;
	int socket = RPC_ANYSOCK;
	int status = 0;

	/*
	 * Names will be either <remote name> or <remote name>:<IP address>.
	 * <remote name> is the name of the remote system to which the crash
	 * notification will be sent.  If present, <IP address> is the
	 * address to be used to determine what host name to send to the
	 * remote in the crash notification.
	 */
	remote = name;
	/*
	 * Look for the ':' separator.  If it is present, replace it with
	 * a NULL to convert the supplied name to two strings.  The second
	 * string should be the IP address.
	 */
	if (token = strchr(name, ':')) {
		/*
		 * put a NULL in to separate the two parts of the name
		 * we'll reset this before returning
		 * increment the token pointer to point at the IP address
		 * string
		 */
		*token = '\0';
		token++;
		/*
		 * Convert the IP address string to an in_addr and look up the
		 * host name.
		 */
		if (inet_aton(token, &localaddr)) {
			hp = gethostbyaddr(&localaddr, sizeof(localaddr), AF_INET);
			if (hp) {
				/*
				 * Use the official name corresponding to the IP
				 * address.
				 */
				local = strdup(hp->h_name);
				needfree = 1;
			} else {
				if (debug)
					herror(token);
				if (Verbose) {
					syslog(LOG_INFO, "IP address lookup error: %s",
						hstrerror(token));
				}
				/*
				 * There is no host name for the address.  Just use
				 * hostname.
				 */
				local = hostname;
			}
		} else {
			/*
			 * The IP address string was not valid.  Log a message
			 * and use hostname.
			 */
			syslog(LOG_ERR, "Invalid IP address string %s, remote %s", token,
				remote);
			local = hostname;
		}
		/*
		 * reset the token pointer to point to the spot where the :
		 * was so we can put it back before returning
		 */
		token--;
	} else {
		/*
		 * No IP address is present in the name, so just use hostname.
		 */
		local = hostname;
	}
	ntf.name = local;
	ntf.state = LOCAL_STATE;
	if (debug)
		printf("statd_call_statd at %s, local name %s\n", remote, local);
	if (Verbose) {
		syslog(LOG_INFO, "notifying %s, local name %s", remote, local);
	}
	/*
	 * get the host address
	 * if this fails, return 0 so that the recovery queue entry will be
	 * tossed
	 */
	if ((hp = gethostbyname(remote)) == NULL) {
		if (debug)
			herror(remote);
		if (Verbose) {
			syslog(LOG_INFO, "%s", hstrerror(remote));
		}
		/*
		 * put the : back in the name
		 */
		if (token) {
			*token = ':';
		}
		status = 0;
		goto out;
	}
	bcopy(hp->h_addr, (caddr_t)&server_addr.sin_addr, hp->h_length);      
	server_addr.sin_family = AF_INET;
	server_addr.sin_port =  0;
	/*
	 * do not send crash notification messages to addresses which
	 * are on this system
	 */
	if (addr_is_local(&server_addr)) {
		status = 0;
		if (ha) {
			/*
			 * must do notification on FailSafe as the system may
			 * not have actually crashed
			 */
			if (Verbose) {
				syslog(LOG_INFO, "performing local notification");
			}
			sm_notify(&ntf, NULL);
		} else if (Verbose) {
			syslog(LOG_INFO, "skipping local name %s", remote);
		}
		if (token) {
			*token = ':';
		}
		goto out;
	}

	if (Verbose) {
		syslog(LOG_INFO, "notifying %s, local name %s", remote, local);
	}

	/*
	 * create the client handle
	 * if this fails from a timeout, return -1, otherwise, return 0
	 */
	if ((client = clnttcp_create(&server_addr, SM_PROG, SM_VERS,  &socket, 0,
		0)) == NULL) { 
		if (debug)
			clnt_pcreateerror("clnttcp_create");
		/*
		 * put the : back in the name
		 */
		if (token) {
			*token = ':';
		}
		switch (rpc_createerr.cf_stat) {
			case RPC_TIMEDOUT:
				status = -1;
				goto out;
			case RPC_PROGNOTREGISTERED:
			default:
				status = 0;
				goto out;
		}
	}
	/*
	 * do the SM_NOTIFY call but don't wait for the reply
	 */
	tottimeout.tv_usec = 500000;
	tottimeout.tv_sec = 0; 
	clnt_stat = clnt_call(client, SM_NOTIFY, xdr_notify, &ntf, xdr_void, NULL,
		tottimeout);
	/*
	 * put the : back in the name
	 */
	if (token) {
		*token = ':';
	}
	switch (clnt_stat) {
		case RPC_SUCCESS:
			status = 0;
			break;
		case RPC_TIMEDOUT:
		default:
			status = -1;
	}
	(void) close(socket);
	clnt_destroy(client);
out:
	if (needfree) {
		free(local);
	}
	return(status);
}

void
sm_try()
{
	name_entry *nl, *next;

	if (debug >= 2)
		printf("enter sm_try: recovery_q = %s\n", recovery_q->name);
	next = recovery_q;
	while ((nl = next) != NULL) {
		next = next->nxt;
		if (statd_call_statd(nl->name) == 0) {
			/* remove entry from recovery_q */ 
			delete_name(&recovery_q, nl->name);
		}
	}
	if (recovery_q != NULL)
		(void) alarm(SM_INIT_ALARM);
}

char *
xmalloc(len)
	unsigned len;
{
	char *new;

	if ((new = malloc(len)) == 0) {
		syslog(LOG_ERR, "xmalloc failed");
		return(NULL);
	}
	else {
		bzero(new, len);
		return(new);
	}
}

/*
 * the following two routines are very similar to
 * insert_mon and delete_mon in sm_proc.c, except the structture
 * is different
 */
name_entry *
insert_name(namepp, name)
	name_entry **namepp;
	char *name;
{
	name_entry *new;

	new = (name_entry *) xmalloc(sizeof(struct name_entry)); 
	new->name = xmalloc(strlen(name) + 1);
	(void) strcpy(new->name, name);
	new->nxt = *namepp;
	if (new->nxt != NULL)
		new->nxt->prev = new;
	*namepp = new; 
	return(new);
}

void
delete_name(namepp, name)
	name_entry **namepp;
	char *name;
{
	name_entry *nl;

	nl = *namepp;
	while (nl != NULL) {
		if (strcasecmp(nl->name, name) == 0) {/*found */
			if (nl->prev != NULL)
				nl->prev->nxt = nl->nxt;
			else 
				*namepp = nl->nxt;
			if (nl->nxt != NULL)
				nl->nxt->prev = nl->prev;
			free(nl->name);
			free(nl);
			return;
		}
		nl = nl->nxt;
	}
	return;
}
