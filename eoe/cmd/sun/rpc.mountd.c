/*
 * Copyright (c) 1987 Sun Microsystems, Inc. 
 */
#include <ctype.h>
#include <pwd.h>
#include <sys/param.h>
#include <rpc/rpc.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/fs/nfs.h>
#include <rpcsvc/mount.h>
#include <rpcsvc/ypclnt.h>
#include <rpc/pmap_clnt.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <exportent.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/capability.h>

#define MAXRMTABLINELEN		(MAXPATHLEN + MAXHOSTNAMELEN + 2)

static char RMTAB[] = "/etc/rmtab";

static void mnt(struct svc_req *, SVCXPRT *);
static void mnt3(struct svc_req *, SVCXPRT *);
static void mntsgi(struct svc_req *, SVCXPRT *);
static void usage(void);
static void freeexports(struct exports *);
static void *exmalloc(size_t);
static char *exstrdup(const char *);
static struct groups **newgroup(char *, struct groups **);
static struct exports **newexport(char *, struct groups *, struct exports **);
static void dostatvfs(struct svc_req *);
static void log_cant_reply(SVCXPRT *);
static int  do_getfh(char *, fhandle_t *);
static int  issock(int);
extern struct hostent *__getverfhostent(struct in_addr, int);
extern int  getfh(char *, fhandle_t *);
extern int  issubdir(char *, char *);

static void mount(struct svc_req *);
static void mount3(struct svc_req *);
static void umount(struct svc_req *);
static void umountall(struct svc_req *);
static void export(struct svc_req *);

/*
 * mountd's version of a "struct mountlist". It is the same except
 * for the added ml_pos field.
 */
struct mountdlist {
/* same as XDR mountlist */
	char *ml_name;
	char *ml_path;
	struct mountdlist *ml_nxt;
/* private to mountd */
	long ml_pos;		/* position of mount entry in RMTAB */
        int  ml_active;         /* stamps mount entry as valid or not */
};

static struct mountdlist *mountlist=NULL;
static int nfs_portmon = 1;
static int rflag = 0;		/* Remove most likely corrupt rmtab entries */

static void rmtab_load(void);
static void rmtab_delete(long);
static void rmtab_valid(long, char);
static long rmtab_insert(char *, char *);
static void check_rmtab(int);

static struct exportlist *getexportlist(void);
static void exportlist(struct svc_req *);

static struct timeval lifetime = { 60, 0 };
static FILE *f = NULL;

extern int __svc_label_agile;

int
main(int argc, char **argv)
{
	SVCXPRT *transp;
	int pid;
	register int i;
	int tcpsock, udpsock, tcpsock_sgi, udpsock_sgi;
	int tcpproto, udpproto;

	while ((i = getopt(argc, argv, "l:nr")) != EOF) {
		switch (i) {
		  case 'l':
			lifetime.tv_sec = atoi(optarg);
			break;
		  case 'n':
			nfs_portmon = 0;
			break;
		  case 'r':
			rflag = 1;
			break;
		  default:
			usage();
		}
	}
	if (optind != argc) {
		usage();
	}
	__svc_label_agile = (sysconf(_SC_IP_SECOPTS) > 0);

	if (issock(0)) {
		/*
		 * Started from inetd -- don't register with portmapper.
		 * Inetd allocates socket descriptors to protocols based
		 * on lexicographic order of protocol name, so the tcp
		 * socket is on descriptor 0, and udp on 1.
		 */
		tcpsock = 0;
		udpsock = 1;
		tcpsock_sgi = 2;
		udpsock_sgi = 3;
		tcpproto = udpproto = 0;
	} else {
#ifndef DEBUG
		/*
		 * Started from shell, background.
		 */
		pid = fork();
		if (pid < 0) {
			perror("mountd: can't fork");
			exit(1);
		}
		if (pid) {
			exit(0);
		}

		/*
		 * Close existing file descriptors, open "/dev/null" as
		 * standard input, output, and error, and detach from
		 * controlling terminal.
		 */
		i = getdtablesize();
		while (--i >= 0)
			(void)close(i);
		(void)open("/dev/null", O_RDONLY);
		(void)open("/dev/null", O_WRONLY);
		(void)dup(1);
		i = open("/dev/tty", O_RDWR);
		if (i >= 0) {
			ioctl(i, TIOCNOTTY, (char *)0);
			(void)close(i);
		}
#endif /* !DEBUG */
		lifetime.tv_sec = 0;	/* command line mountd lives forever */
		pmap_unset(MOUNTPROG, MOUNTVERS);
		pmap_unset(MOUNTPROG, MOUNTVERS3);
		pmap_unset(MOUNTPROG_SGI, MOUNTVERS_SGI_ORIG);
		tcpsock = udpsock = tcpsock_sgi = udpsock_sgi = RPC_ANYSOCK;
		tcpproto = IPPROTO_TCP;
		udpproto = IPPROTO_UDP;
	}

	/* 4.3 openlog */
	openlog("mountd", LOG_PID, LOG_DAEMON);

	/*
	 * Create UDP service
	 */
	if ((transp = svcudp_create(udpsock)) == NULL) {
		syslog(LOG_ERR, "couldn't create UDP transport");
		exit(1);
	}
	if (!svc_register(transp, MOUNTPROG, MOUNTVERS, mnt, udpproto)) {
		syslog(LOG_ERR, "couldn't register mount as a UDP service");
		exit(1);
	}
	if ((transp = svcudp_create(udpsock)) == NULL) {
		syslog(LOG_ERR, "couldn't create UDP transport");
		exit(1);
	}
	if (!svc_register(transp, MOUNTPROG, MOUNTVERS3, mnt3, udpproto)) {
		syslog(LOG_ERR, "couldn't register mount as a UDP service");
		exit(1);
	}
	if ((transp = svcudp_create(udpsock_sgi)) == NULL) {
		syslog(LOG_INFO, "couldn't create SGI's mount UDP transport");
	} else {
		if (!svc_register(transp, MOUNTPROG_SGI, MOUNTVERS_SGI_ORIG, 
		    mntsgi, udpproto)) {
			syslog(LOG_INFO, 
			      "couldn't register SGI's mount as a UDP service");
		}
	}

	/*
	 * Create TCP service
	 */
	if ((transp = svctcp_create(tcpsock, 0, 0)) == NULL) {
		syslog(LOG_ERR, "couldn't create TCP transport");
		exit(1);
	}
	if (!svc_register(transp, MOUNTPROG, MOUNTVERS, mnt, tcpproto)) {
		syslog(LOG_ERR, "couldn't register mount as a TCP service");
		exit(1);
	}
	if ((transp = svctcp_create(tcpsock_sgi, 0, 0)) == NULL) {
		syslog(LOG_INFO, "couldn't create SGI's mount TCP transport");
	} else {
		if (!svc_register(transp, MOUNTPROG_SGI, MOUNTVERS_SGI_ORIG, 
		    mntsgi, tcpproto)) {
			syslog(LOG_INFO, 
			      "couldn't register SGI's mount as a TCP service");
		}
	}

	/*
	 * Initalize the world 
	 */
	_yellowup(1);

	/*
	 * Start serving 
	 */
	rmtab_load();
	for (;;) {
		fd_set readfds;

		readfds = svc_fdset;
		switch (select(FD_SETSIZE, &readfds, 0, 0,
				lifetime.tv_sec ? &lifetime : NULL)) {
		  case -1:
			if (errno == EINTR)
				break;
			syslog(LOG_ERR, "select: %m");
			if (f)
				fclose(f);
			exit(1);
		  case 0:
			if (f)
				fclose(f);
			exit(0);
		  default:
			svc_getreqset(&readfds);
		}
	}
}

/*
 * Determine if a descriptor belongs to a socket or not 
 */
static int
issock(int fd)
{
	struct stat st;

	if (fstat(fd, &st) < 0) {
		return (0);
	}
	/*
	 * SunOS returns S_IFIFO for sockets, while 4.3 returns 0 and does not
	 * even have an S_IFIFO mode.  Since there is confusion about what the
	 * mode is, we check for what it is not instead of what it is. 
	 */
	switch (st.st_mode & S_IFMT) {
	case S_IFCHR:
	case S_IFREG:
	case S_IFLNK:
	case S_IFDIR:
	case S_IFBLK:
		return (0);
	default:
		return (1);
	}
}

/*
 * Server procedure switch routine
 */
static void
mnt3(struct svc_req *rqstp, SVCXPRT *transp)
{
        struct mountdlist *ml;
        struct mountdlist *ml_elem;
        struct mountdlist *mountlist_reply;
        struct mountdlist *last_ml;

#ifdef MNT_TRACE
	syslog(LOG_INFO, "mnt3: %s requests %u",
		inet_ntoa(svc_getcaller(transp)->sin_addr),
		rqstp->rq_proc);
#endif
	switch (rqstp->rq_proc) {
	case NULLPROC:
		errno = 0;
		if (!svc_sendreply(transp, xdr_void, (char *)0))
			log_cant_reply(transp);
		break;
	case MOUNTPROC_MNT:
		mount3(rqstp);
		break;
	case MOUNTPROC_DUMP:
		errno = 0;
		mountlist_reply = NULL;

                /* Build a list of valid mounts */
                for (ml = mountlist; ml != NULL; ml = ml->ml_nxt) {
                        if (ml->ml_active) {
                                /*
                                 * Add this entry to the list sent on reply
                                 */
                                ml_elem = (struct mountdlist *)
                                        exmalloc(sizeof(struct mountdlist));
                                ml_elem->ml_path = (char *)
                                        exmalloc(strlen(ml->ml_path) + 1);
                                (void)strcpy(ml_elem->ml_path, ml->ml_path);
                                ml_elem->ml_name = (char *)
                                        exmalloc(strlen(ml->ml_name) + 1);
                                (void)strcpy(ml_elem->ml_name, ml->ml_name);
                                ml_elem->ml_nxt = NULL;

                                if (mountlist_reply) {
                                        last_ml->ml_nxt = ml_elem;
                                        last_ml = last_ml->ml_nxt;
                                }
                                else
                                        mountlist_reply = last_ml = ml_elem;
                        }
                }

		if (!svc_sendreply(transp, xdr_mountlist, 
				   (char *)&mountlist_reply))
			log_cant_reply(transp);

                /* Free list sent on reply */
                while (mountlist_reply) {
                        ml_elem = mountlist_reply;
                        mountlist_reply = mountlist_reply->ml_nxt;
                        free(ml_elem->ml_path);
                        free(ml_elem->ml_name);
                        free((char *)ml_elem);
                }

		break;
	case MOUNTPROC_UMNT:
		umount(rqstp);
		break;
	case MOUNTPROC_UMNTALL:
		umountall(rqstp);
		break;
	case MOUNTPROC_EXPORT:
	case MOUNTPROC_EXPORTALL:
		export(rqstp);
		break;
	default:
		svcerr_noproc(transp);
		break;
	}
	return;
}

static void
mnt(struct svc_req *rqstp, SVCXPRT *transp)
{
        struct mountdlist *ml;
        struct mountdlist *ml_elem;
        struct mountdlist *mountlist_reply;
        struct mountdlist *last_ml;

#ifdef MNT_TRACE
	syslog(LOG_INFO, "mnt: %s requests %u", 
		inet_ntoa(svc_getcaller(transp)->sin_addr),
		rqstp->rq_proc);
#endif
	switch (rqstp->rq_proc) {
	case NULLPROC:
		errno = 0;
		if (!svc_sendreply(transp, xdr_void, (char *)0))
			log_cant_reply(transp);
		break;
	case MOUNTPROC_MNT:
		mount(rqstp);
		break;
	case MOUNTPROC_DUMP:
		errno = 0;
		mountlist_reply = NULL;

                /* Build a list of valid mounts */
                for (ml = mountlist; ml != NULL; ml = ml->ml_nxt) {
                        if (ml->ml_active) {
                                /*
                                 * Add this entry to the list sent on reply
                                 */
                                ml_elem = (struct mountdlist *)
                                        exmalloc(sizeof(struct mountdlist));
                                ml_elem->ml_path = (char *)
                                        exmalloc(strlen(ml->ml_path) + 1);
                                (void)strcpy(ml_elem->ml_path, ml->ml_path);
                                ml_elem->ml_name = (char *)
                                        exmalloc(strlen(ml->ml_name) + 1);
                                (void)strcpy(ml_elem->ml_name, ml->ml_name);
                                ml_elem->ml_nxt = NULL;

                                if (mountlist_reply) {
                                        last_ml->ml_nxt = ml_elem;
                                        last_ml = last_ml->ml_nxt;
                                }
                                else
                                        mountlist_reply = last_ml = ml_elem;
                        }
                }

		if (!svc_sendreply(transp, xdr_mountlist, 
				   (char *)&mountlist_reply))
			log_cant_reply(transp);

                /* Free list sent on reply */
                while (mountlist_reply) {
                        ml_elem = mountlist_reply;
                        mountlist_reply = mountlist_reply->ml_nxt;
                        free(ml_elem->ml_path);
                        free(ml_elem->ml_name);
                        free((char *)ml_elem);
                }

		break;
	case MOUNTPROC_UMNT:
		umount(rqstp);
		break;
	case MOUNTPROC_UMNTALL:
		umountall(rqstp);
		break;
	case MOUNTPROC_EXPORT:
	case MOUNTPROC_EXPORTALL:
		export(rqstp);
		break;
	default:
		svcerr_noproc(transp);
		break;
	}
	return;
}

/*
 * Server procedure switch routine for SGI's mount program
 */
static void
mntsgi(struct svc_req *rqstp, SVCXPRT *transp)
{
	switch (rqstp->rq_proc) {
	case MOUNTPROC_EXPORTLIST:
		exportlist(rqstp);
		break;
	case MOUNTPROC_STATVFS:
		dostatvfs(rqstp);
		break;
	default:
		mnt(rqstp, transp);
		break;
	}
	return;
}

struct hostent *
getclientsname(SVCXPRT *transp)
{
	struct sockaddr_in actual;

	actual = *svc_getcaller(transp);
	if (nfs_portmon) {
		if (ntohs(actual.sin_port) >= IPPORT_RESERVED) {
			return (NULL);
		}
	}
	/*
	 * Don't use the unix credentials to get the machine name, instead use
	 * the source IP address. 
	 */
	return (__getverfhostent(actual.sin_addr, 1));
}

static void
log_cant_reply(SVCXPRT *transp)
{
	int saverrno;
	struct sockaddr_in actual;
	register struct hostent *hp;
	register char *name;

	saverrno = errno;	/* save error code */
	actual = *svc_getcaller(transp);
	/*
	 * Don't use the unix credentials to get the machine name, instead use
	 * the source IP address. 
	 */
	hp = gethostbyaddr((char *)&actual.sin_addr, sizeof(actual.sin_addr),
			   AF_INET);
	if (hp != NULL)
		name = hp->h_name;
	else
		name = inet_ntoa(actual.sin_addr);

	errno = saverrno;
	if (errno == 0)
		syslog(LOG_ERR, "couldn't send reply to %s", name);
	else
		syslog(LOG_ERR, "couldn't send reply to %s: %m", name);
}

/*
 * Check mount requests, add to mounted list if ok 
 */
static void
mount(struct svc_req *rqstp)
{
	SVCXPRT *transp;
	fhandle_t fh;
	struct fhstatus fhs;
	char *path;
	struct mountdlist *ml;
	struct mountdlist *end;
	char *gr;
	char *grl;
	struct exportent *xent;
	struct exportlist *el;
	char *machine;
	char **aliases;
	struct hostent *client;
	char  *clientname;
	cap_t ocap;
	cap_value_t cap_mac_read = CAP_MAC_READ;

	transp = rqstp->rq_xprt;
	path = NULL;
	if (rqstp->rq_cred.oa_flavor == AUTH_SYS) {
		struct authunix_parms *aup;
		aup = (struct authunix_parms *)rqstp->rq_clntcred;
		clientname = aup->aup_machname;
	} else {
		clientname = "<unknown>";
	}
	
	if (!svc_getargs(transp, xdr_path, &path)) {
		svcerr_decode(transp);
		return;
	}
	if (do_getfh(path, &fh) < 0) {
		if (errno == EINVAL) {
			fhs.fhs_status = EACCES;
		} else {
			fhs.fhs_status = errno;
		}
		syslog(LOG_DEBUG,
			"%s mount request for %s: getfh failed: %m",
			clientname, path);
		goto fail;
	} else
		fhs.fhs_status = 0;
	el = getexportlist();
	if (el == NULL) {
		fhs.fhs_status = EACCES;
		goto fail;
	}
	ocap = cap_acquire(1, &cap_mac_read);
	while (!issubdir(path, el->el_entry.ee_dirname)) {
		el = el->el_next;
		if (el == NULL) {
			cap_surrender(ocap);
			fhs.fhs_status = EACCES;
			goto fail;
		}
	}
	cap_surrender(ocap);
	xent = (struct exportent *) &el->el_entry;

	/* Check access list */

	grl = getexportopt(xent, ACCESS_OPT);
	if (grl == NULL) {
		/*
		 * Skip the address to name lookup if exported to
		 * everybody. All this can do is waste time.
		 */
		client = NULL;
		goto hit;
	}
	client = getclientsname(transp);
	if (client == NULL) {
		clientname = "<unknown>";
	} else {
		clientname = client->h_name;
	}
	while ((gr = strtok(grl, ":")) != NULL) {
		grl = NULL;
		if (strcasecmp(gr, clientname) == 0)
			goto hit;
		if (client == NULL) {
			continue;
		}
		for (aliases = client->h_aliases; *aliases != NULL;
		     aliases++) {
			if (strcasecmp(gr, *aliases) == 0)
				goto hit;
		}
	}
	grl = getexportopt(xent, ACCESS_OPT);
	while ((gr = strtok(grl, ":")) != NULL) {
		grl = NULL;
		if (innetgr(gr, clientname, (char *)NULL, _yp_domain))
			goto hit;
		if (client == NULL) {
			continue;
		}
		for (aliases = client->h_aliases; *aliases != NULL;
		     aliases++) {
			if (innetgr(gr, *aliases, (char *)NULL, _yp_domain))
				goto hit;
		}
	}

 	/* Check root and rw lists */
 
 	grl = getexportopt(xent, ROOT_OPT);
 	if (grl != NULL) {
 		while ((gr = strtok(grl, ":")) != NULL) {
 			grl = NULL;
 			if (strcasecmp(gr, clientname) == 0)
 				goto hit;
 		}
 	}
 	grl = getexportopt(xent, RW_OPT);
 	if (grl != NULL) {
 		while ((gr = strtok(grl, ":")) != NULL) {
 			grl = NULL;
 			if (strcasecmp(gr, clientname) == 0)
 				goto hit;
 		}
 	}
 
	fhs.fhs_status = EACCES;
	goto fail;

hit:
	fhs.fhs_fh = fh;
	machine = NULL;
	for (ml = mountlist; ml != NULL && machine == NULL; ml = ml->ml_nxt) {
		if (strcmp(ml->ml_path, path) == 0) {
			if (strcasecmp(ml->ml_name, clientname) == 0) {
				goto found;
			}
			if (client == NULL) {
				continue;
			}
			for (aliases = client->h_aliases; *aliases != NULL;
			     aliases++) {
				if (strcasecmp(ml->ml_name, *aliases) == 0) {
					goto found;
				}
			}
		}
	}
found:
	errno = 0;
	if (!svc_sendreply(transp, xdr_fhstatus, (char *)&fhs))
		log_cant_reply(transp);
	if (ml == NULL && strcmp(clientname, "<unknown>")) {
		ml = (struct mountdlist *) exmalloc(sizeof(struct mountdlist));
		ml->ml_path = exstrdup(path);
		ml->ml_name = exstrdup(clientname);
#ifdef RMTAB_CHECKING
		/*
		 * Add new entry at the tail of the list, this is required
		 * for the check_rmtab() function to work.
		 */
		ml->ml_nxt = NULL;
		check_rmtab(__LINE__);
		ml->ml_pos = rmtab_insert(clientname, path);
		if (mountlist == NULL) {
			mountlist = ml;
		} else {
			for (end = mountlist; end->ml_nxt != NULL; end = end->ml_nxt);
			end->ml_nxt = ml;
		}
		check_rmtab(__LINE__);
#else /* RMTAB_CHECKING */
		/*
		 * Add new entry at the head of the list
		 */
		ml->ml_nxt = mountlist;
		ml->ml_pos = rmtab_insert(clientname, path);
		mountlist = ml;
#endif /* RMTAB_CHECKING */
	} else if (ml != NULL && !ml->ml_active) {
                /* Validate line in /etc/rmtab */
                rmtab_valid(ml->ml_pos, ml->ml_name[0]);
                ml->ml_active = 1;
        }
	svc_freeargs(transp, xdr_path, &path);
	return;

fail:
	errno = 0;
	if (!svc_sendreply(transp, xdr_fhstatus, (char *)&fhs))
		log_cant_reply(transp);
	svc_freeargs(transp, xdr_path, &path);
}

/*
 * Check mount requests, add to mounted list if ok 
 */
static void
mount3(struct svc_req *rqstp)
{
	SVCXPRT *transp;
	struct mountres3 mountres3;
	char fh3[FHSIZE3];
	char *path;
	struct mountdlist *ml;
	struct mountdlist *end;
	char *gr;
	char *grl;
	struct exportent *xent;
	struct exportlist *el;
	char *machine;
	char **aliases;
	struct hostent *client;
	char *clientname;
	int flavor;
	cap_t ocap;
	cap_value_t cap_mac_read = CAP_MAC_READ;

	transp = rqstp->rq_xprt;
	path = NULL;
	if (rqstp->rq_cred.oa_flavor == AUTH_SYS) {
		struct authunix_parms *aup;
		aup = (struct authunix_parms *)rqstp->rq_clntcred;
		clientname = aup->aup_machname;
	} else {
		clientname = "<unknown>";
	}
	if (!svc_getargs(transp, xdr_path, &path)) {
		svcerr_decode(transp);
		return;
	}
	mountres3.mountres3_u.mountinfo.fhandle.fhandle3_len = 32;
	mountres3.mountres3_u.mountinfo.fhandle.fhandle3_val = fh3;
	if (do_getfh(path, (fhandle_t *) fh3) < 0) {
		if (errno == EINVAL) {
			mountres3.fhs_status = (mountstat3) EACCES;
		} else {
			mountres3.fhs_status = (mountstat3) errno;
		}
		syslog(LOG_DEBUG,
			"%s mount3 request for %s: getfh failed: %m",
			clientname, path);
		goto fail;
	} else
		mountres3.fhs_status = MNT_OK;
	el = getexportlist();
	if (el == NULL) {
		mountres3.fhs_status = (mountstat3) EACCES;
		goto fail;
	}
	ocap = cap_acquire(1, &cap_mac_read);
	while (!issubdir(path, el->el_entry.ee_dirname)) {
		el = el->el_next;
		if (el == NULL) {
			cap_surrender(ocap);
			mountres3.fhs_status = (mountstat3) EACCES;
			goto fail;
		}
	}
	cap_surrender(ocap);
	xent = (struct exportent *) &el->el_entry;

	/* Check access list */

	grl = getexportopt(xent, ACCESS_OPT);
	if (grl == NULL) {
		/*
		 * Skip the address to name lookup if exported to
		 * everybody. All this can do is waste time.
		 */
		client = NULL;
		goto hit;
	}
	client = getclientsname(transp);
	if (client == NULL) {
		clientname = "<unknown>";
	} else {
		clientname = client->h_name;
	}
	while ((gr = strtok(grl, ":")) != NULL) {
		grl = NULL;
		if (strcasecmp(gr, clientname) == 0)
			goto hit;
		if (client == NULL) {
			continue;
		}
		for (aliases = client->h_aliases; *aliases != NULL;
		     aliases++) {
			if (strcasecmp(gr, *aliases) == 0)
				goto hit;
		}
	}
	grl = getexportopt(xent, ACCESS_OPT);
	while ((gr = strtok(grl, ":")) != NULL) {
		grl = NULL;
		if (innetgr(gr, clientname, (char *)NULL, _yp_domain))
			goto hit;
		if (client == NULL) {
			continue;
		}
		for (aliases = client->h_aliases; *aliases != NULL;
		     aliases++) {
			if (innetgr(gr, *aliases, (char *)NULL, _yp_domain))
				goto hit;
		}
	}

 	/* Check root and rw lists */
 
 	grl = getexportopt(xent, ROOT_OPT);
 	if (grl != NULL) {
 		while ((gr = strtok(grl, ":")) != NULL) {
 			grl = NULL;
 			if (strcasecmp(gr, clientname) == 0)
 				goto hit;
 		}
 	}
 	grl = getexportopt(xent, RW_OPT);
 	if (grl != NULL) {
 		while ((gr = strtok(grl, ":")) != NULL) {
 			grl = NULL;
 			if (strcasecmp(gr, clientname) == 0)
 				goto hit;
 		}
 	}
 
	mountres3.fhs_status = (mountstat3) EACCES;
	goto fail;

hit:
	machine = NULL;
	for (ml = mountlist; ml != NULL && machine == NULL; ml = ml->ml_nxt) {
		if (strcmp(ml->ml_path, path) == 0) {
			if (strcasecmp(ml->ml_name, clientname) == 0) {
				goto found;
			}
			if (client == NULL) {
				continue;
			}
			for (aliases = client->h_aliases; *aliases != NULL;
			     aliases++) {
				if (strcasecmp(ml->ml_name, *aliases) == 0) {
					goto found;
				}
			}
		}
	}
found:
	flavor = AUTH_UNIX;
	mountres3.mountres3_u.mountinfo.auth_flavors.auth_flavors_val =
							&flavor;
	mountres3.mountres3_u.mountinfo.auth_flavors.auth_flavors_len =
							1;
	errno = 0;
	if (!svc_sendreply(transp, xdr_mountres3, (char *)&mountres3))
		log_cant_reply(transp);
	if (ml == NULL && strcmp(clientname, "<unknown>")) {
		ml = (struct mountdlist *) exmalloc(sizeof(struct mountdlist));
		ml->ml_path = exstrdup(path);
		ml->ml_name = exstrdup(clientname);
#ifdef  RMTAB_CHECKING
		/*
		 * Add new entry at the tail of the list, this is required
		 * for the check_rmtab() function to work.
		 */
		ml->ml_nxt = NULL;
		check_rmtab(__LINE__);
		ml->ml_pos = rmtab_insert(clientname, path);
		if (mountlist == NULL) {
			mountlist = ml;
		} else {
			for (end = mountlist; end->ml_nxt != NULL; end = end->ml_nxt);
			end->ml_nxt = ml;
		}
		check_rmtab(__LINE__);
#else /* RMTAB_CHECKING */
		/*
		 * Add new entry at the head of the list
		 */
		ml->ml_nxt = mountlist;
		ml->ml_pos = rmtab_insert(clientname, path);
		mountlist = ml;
#endif /* RMTAB_CHECKING */

	} else if (ml != NULL && !ml->ml_active) {
                /* Validate line in /etc/rmtab */
                rmtab_valid(ml->ml_pos, ml->ml_name[0]);
                ml->ml_active = 1;
        }
	svc_freeargs(transp, xdr_path, &path);
	return;

fail:
	errno = 0;
	if (!svc_sendreply(transp, xdr_mountres3, (char *)&mountres3))
		log_cant_reply(transp);
	svc_freeargs(transp, xdr_path, &path);
}

/*
 * Helper to attempt a removal
 * Returns TRUE if we found it.
 */
int
help_remove(char *name, char *path)
{
	struct mountdlist *ml;
	int gotany = 0;

	for (ml = mountlist; ml != NULL; ml = ml->ml_nxt) {
		if ((path == NULL || strcmp(ml->ml_path, path) == 0) &&
		     strcasecmp(ml->ml_name, name) == 0) {
			check_rmtab(__LINE__);
			rmtab_delete(ml->ml_pos);
			ml->ml_active = 0;
			check_rmtab(__LINE__);
			if (path)
			    return (1);
			gotany = 1;
		}
	}
	return (gotany);
}

/*
 * Remove an entry from mounted list 
 */
static void
umount(struct svc_req *rqstp)
{
	char *path;
	char *client;
	struct hostent *hp;
	SVCXPRT *transp;

	transp = rqstp->rq_xprt;
	path = NULL;
	if (!svc_getargs(transp, xdr_path, &path)) {
		svcerr_decode(transp);
		return;
	}
	if (rqstp->rq_cred.oa_flavor == AUTH_SYS) {
		struct authunix_parms *aup;
		aup = (struct authunix_parms *)rqstp->rq_clntcred;
		client = aup->aup_machname;
	} else {
		client = "<unknown>";
	}
	if (!help_remove(client, path)) {
		hp = getclientsname(transp);
		if (hp) {
			(void) help_remove(hp->h_name, path);
		}
	}
	errno = 0;
	if (!svc_sendreply(transp, xdr_void, (char *)NULL))
		log_cant_reply(transp);
	svc_freeargs(transp, xdr_path, &path);
}

/*
 * Remove all entries for one machine from mounted list 
 */
static void
umountall(struct svc_req *rqstp)
{
	char *client;
	struct hostent *hp;
	SVCXPRT *transp;

	transp = rqstp->rq_xprt;
	if (!svc_getargs(transp, xdr_void, NULL)) {
		svcerr_decode(transp);
		return;
	}
	if (rqstp->rq_cred.oa_flavor == AUTH_SYS) {
		struct authunix_parms *aup;
		aup = (struct authunix_parms *)rqstp->rq_clntcred;
		client = aup->aup_machname;
	} else {
		client = "<unknown>";
	}
	if (!help_remove(client, NULL)) {
		hp = getclientsname(transp);
		if (hp) {
			(void) help_remove(hp->h_name, NULL);
		}
	}
	/*
	 * We assume that this call is asynchronous and made via the portmapper
	 * callit routine.  Therefore return control immediately. The error
	 * causes the portmapper to remain silent, as opposed to every machine
	 * on the net blasting the requester with a response. 
	 */
	svcerr_systemerr(transp);
}

/*
 * send current export list 
 */
static void
export(struct svc_req *rqstp)
{
	struct exportlist *el;
	struct exportent *xent;
	struct exports *ex;
	struct exports **tail;
	char *grl;
	char *gr;
	struct groups *groups;
	struct groups **grtail;
	SVCXPRT *transp;

	transp = rqstp->rq_xprt;
	if (!svc_getargs(transp, xdr_void, NULL)) {
		svcerr_decode(transp);
		return;
	}
	ex = NULL;
	tail = &ex;
	for (el = getexportlist(); el != NULL; el = el->el_next) {
		xent = (struct exportent *) &el->el_entry;
		grl = getexportopt(xent, ACCESS_OPT);
		groups = NULL;
		if (grl != NULL) {
			grtail = &groups;
			while ((gr = strtok(grl, ":")) != NULL) {
				grl = NULL;
				grtail = newgroup(gr, grtail);
			}
		}
		tail = newexport(xent->xent_dirname, groups, tail);
	}

	errno = 0;
	if (!svc_sendreply(transp, xdr_exports, (char *)&ex))
		log_cant_reply(transp);
	freeexports(ex);
}

#if 0
static int
str_to_uid(char *name, int *uidp)
{
        struct passwd *pw;
        if (isdigit(*name)) {
                *uidp = atoi(name);
                return 1;
        }
        pw = getpwnam(name);
        if (pw) {
                *uidp = pw->pw_uid;
                return 1;
        }
        return 0;
}
#endif

/*
 * Send export list as dirname/options string pairs.
 */
static void
exportlist(struct svc_req *rqstp)
{
	SVCXPRT *transp;
	struct exportlist *el;

	transp = rqstp->rq_xprt;
	if (!svc_getargs(transp, xdr_void, NULL)) {
		svcerr_decode(transp);
		return;
	}
	el = getexportlist();
	errno = 0;
	if (!svc_sendreply(transp, xdr_exportlist, (char *)&el))
		log_cant_reply(transp);
}

static void
dostatvfs(struct svc_req *rqstp)
{
	SVCXPRT *transp;
	char *path;
	struct statvfs 	lclstatvfs;
	struct mntrpc_statvfs rpcstatvfs;

	transp = rqstp->rq_xprt;
	path = NULL;

	if (!svc_getargs(transp, xdr_path, &path)) {
		svcerr_decode(transp);
		return;
	}
	errno = statvfs(path, &lclstatvfs);
	svc_freeargs(transp, xdr_path, &path);

	if (errno) {
		syslog(LOG_ERR,"rpc.mountd: error %d in statvfs(%s)\n",
			errno, path);
		svcerr_systemerr(transp);
		return;
	}

	rpcstatvfs.f_bsize = lclstatvfs.f_bsize;
	rpcstatvfs.f_frsize = lclstatvfs.f_frsize;
	rpcstatvfs.f_blocks = lclstatvfs.f_blocks;
	rpcstatvfs.f_bavail = lclstatvfs.f_bavail;
	rpcstatvfs.f_files = lclstatvfs.f_files;
	rpcstatvfs.f_ffree = lclstatvfs.f_ffree;
	rpcstatvfs.f_favail = lclstatvfs.f_favail;
	rpcstatvfs.f_fsid = lclstatvfs.f_fsid;
	strncpy(rpcstatvfs.f_basetype, lclstatvfs.f_basetype,
				sizeof rpcstatvfs.f_basetype);
	rpcstatvfs.f_flag = lclstatvfs.f_flag;
	strncpy(rpcstatvfs.f_fstr, lclstatvfs.f_fstr,
				sizeof rpcstatvfs.f_fstr);
	rpcstatvfs.f_namemax = lclstatvfs.f_namemax;

	errno = 0;
	if (!svc_sendreply(transp, xdr_statvfs, (char *)&rpcstatvfs))
		log_cant_reply(transp);
}

#if 0
static void
newgroups(struct exportent *xent, char *optname, struct groups **grp)
{
	char *list, *element;

	list = getexportopt(xent, optname);
	if (list == NULL)
		return;
	while ((element = strtok(list, ":")) != NULL) {
		list = NULL;
		grp = newgroup(element, grp);
	}
}

static void
freegroups(struct groups *gr)
{
	while (gr) {
		struct groups *tmp;

		tmp = gr->g_next;
		free((char *) gr);
		gr = tmp;
	}
}
#endif

static void
freeexports(struct exports *ex)
{
	struct groups *groups, *tmpgroups;
	struct exports *tmpex;

	while (ex) {
		groups = ex->ex_groups;
		while (groups) {
			tmpgroups = groups->g_next;
			free(groups->g_name);
			free((char *)groups);
			groups = tmpgroups;
		}
		tmpex = ex->ex_next;
		free(ex->ex_name);
		free((char *)ex);
		ex = tmpex;
	}
}


static struct groups **
newgroup(char *name, struct groups **tail)
{
	struct groups *new;

	new = (struct groups *) exmalloc(sizeof(*new));
	new->g_name = exstrdup(name);
	new->g_next = NULL;
	*tail = new;
	return (&new->g_next);
}


static struct exports **
newexport(char *name, struct groups *groups, struct exports **tail)
{
	struct exports *new;

	new = (struct exports *) exmalloc(sizeof(*new));
	new->ex_name = exstrdup(name);
	new->ex_groups = groups;
	new->ex_next = NULL;
	*tail = new;
	return (&new->ex_next);
}


static void *
exmalloc(size_t size)
{
	void *ret;

	if ((ret = malloc(size)) == (void *) 0) {
		syslog(LOG_ERR, "Out of memory");
		if (f)
			fclose(f);
		exit(1);
	}
	return (ret);
}

static char *
exstrdup(const char *s)
{
	return strcpy(exmalloc(strlen(s)+1), s);
}


static void
usage(void)
{
	(void)fprintf(stderr, "usage: rpc.mountd [-l lifetime] [-n]\n");
	exit(1);
}

/*
 * Old geth() took a file descriptor. New getfh() takes a pathname.
 * So the the mount daemon can run on both old and new kernels, we try
 * the old version of getfh() if the new one fails.
 */
static int
do_getfh(char *path, fhandle_t *fh)
{
	int fd;
	int res;
	int save;
	cap_t ocap;
	static const cap_value_t cap_mount_read[] = {CAP_MOUNT_MGT,
						     CAP_MAC_READ};

	ocap = cap_acquire (2, cap_mount_read);
	res = getfh(path, fh);
	cap_surrender(ocap);
	if (res < 0 && errno == EBADF) {	
		/*
		 * This kernel does not have the new-style getfh()
		 */
		ocap = cap_acquire (1, &cap_mount_read[1]);
		res = fd = open(path, 0, 0);
		cap_surrender(ocap);
		if (fd >= 0) {
			ocap = cap_acquire (1, &cap_mount_read[0]);
			res = getfh((char *)fd, fh);
			cap_surrender(ocap);
			save = errno;
			(void)close(fd);
			errno = (save == ENOENT) ? EACCES : save;
		} else if (errno == ENOENT) {
			errno = EACCES;
		}
	} else if (errno == ENOENT) {
		errno = EACCES;
	}
	return (res);
}



static void
rmtab_load(void)
{
	char *path;
	char *name;
	char *end;
	char *check_1;
	char *check_2;
	struct mountdlist *ml;
	char line[MAXRMTABLINELEN];

	f = fopen(RMTAB, "r");
	if (f != NULL) {
		while (fgets(line, sizeof(line), f) != NULL) {
			name = line;
			/*
			 * If the name/path delimiter (colon) is found
			 * and we are removing questionable entries,
			 * perform further checking.
			 */
			if (((path = strchr(name, ':')) != NULL) && 
			    (rflag == 1)) {
				check_1 = strchr(path+1, ':');
				check_2 = strchr(path, '#');
			} else {
				check_1 = NULL;
				check_2 = NULL;
			}
			/*
			 * If the host name start with a # it's an entry that
			 * has been inactivated.
			 * If we didn't find the colon it's not a valid entry
			 * If we found a second colon it's not a valid entry
			 * If we found a pound sign anywhere, but the begining
			 * of a line it's not a valid entry
			 */
			if ((*name != '#') && (path != NULL) && 
			    (*(path+1) == '/') &&
			    (check_1 == NULL) && (check_2 == NULL)) {

				/* Null terminate name */
				*path = 0;

				path++;
				end = strchr(path, '\n');
				if (end != NULL) {
					/* Null terminate path */
					*end = 0;
				} else {
					/* skip entry, no newline found */
					continue;
				}

				/*
				 * If this is a duplicate entry skip it.
				 */
				if ((rflag == 1) && 
				    dup_check( path, name)) {
					continue; /* skip duplicate entry */
				}

				/*
				 * Add this entry to the list
				 */
				ml = (struct mountdlist *) 
					exmalloc(sizeof(struct mountdlist));
				ml->ml_path = (char *)
					exmalloc(strlen(path) + 1);
				(void)strcpy(ml->ml_path, path);
				ml->ml_name = (char *)
					exmalloc(strlen(name) + 1);
				(void)strcpy(ml->ml_name, name);
				ml->ml_nxt = mountlist;
				mountlist = ml;
			}
		}
		fclose(f);
		(void)truncate(RMTAB, (off_t)0);
	} 
	f = fopen(RMTAB, "w+");
	if (f != NULL) {
		setvbuf(f, (char *) 0, _IOLBF, MAXRMTABLINELEN + 1);
		for (ml = mountlist; ml != NULL; ml = ml->ml_nxt) {
			ml->ml_pos = rmtab_insert(ml->ml_name, ml->ml_path);
		}
	}
}

int
dup_check( path, name)
	char *path; 
	char *name;
{
	struct mountdlist *ml;

	for (ml = mountlist; ml != NULL; ml = ml->ml_nxt) {

		if ((strncmp( path, ml->ml_path, strlen(path)) == 0) && 
		    (strncmp( name, ml->ml_name, strlen(name)) == 0)) {

			return( 1); /* duplicate entry */
		}
	}
	return( 0); /* no duplicate entry found */
}


static long
rmtab_insert(char *name, char *path)
{
	long pos;

	if (f == NULL || fseek(f, 0L, 2) == -1) {
		return (-1);
	}
	pos = ftell(f);
	if (fprintf(f, "%s:%s\n", name, path) == EOF) {
		fflush(f);
		return (-1);
	}
	fflush(f);
	return (pos);
}

static void
rmtab_delete(long pos)
{
	if (f != NULL && pos != -1 && fseek(f, pos, 0) == 0) {
		fprintf(f, "#");
		fflush(f);
	}
}

static void
rmtab_valid(long pos, char first)
{
        if (f != NULL && pos != -1 && fseek(f, pos, 0) == 0) {
                putc(first, f);
                fflush(f);
        }
}

static struct exportlist *
getexportlist(void)
{
	struct stat sb;
	struct exportlist **tail;
	struct exportlist *el;
	struct exportent *xent;
	static FILE *xtab;
	static time_t xtabmtime = -1;
	static struct exportlist *list;

	if (xtab == NULL) {
		xtab = setexportent();
		if (xtab == NULL)
			return NULL;
		flock(fileno(xtab), LOCK_UN);
	}
	if (fstat(fileno(xtab), &sb) < 0) {
		endexportent(xtab);
		xtab = NULL;
		return NULL;
	}
	if (xtabmtime == sb.st_mtime) {
		/* cached list is still valid */
		return list;
	}
	tail = &list;
	while ((el = *tail) != NULL) {
		*tail = el->el_next;
		free(el->el_entry.ee_dirname);
		if (el->el_entry.ee_options)
			free(el->el_entry.ee_options);
		free((char *) el);
	}
	rewind(xtab);
	flock(fileno(xtab), LOCK_SH);
	while ((xent = getexportent(xtab)) != NULL) {
		el = (struct exportlist *) exmalloc(sizeof *el);
		el->el_entry.ee_dirname = exstrdup(xent->xent_dirname);
		if (xent->xent_options)
			el->el_entry.ee_options = exstrdup(xent->xent_options);
		else
			el->el_entry.ee_options = NULL;
		el->el_next = NULL;
		*tail = el;
		tail = &el->el_next;
	}
	flock(fileno(xtab), LOCK_UN);
	xtabmtime = sb.st_mtime;
	return list;
}


static void
check_rmtab(int from_line)
{
#ifdef RMTAB_CHECKING
	char *path;
	char *name;
	char *end;
	struct mountdlist *ml;
	char line[MAXRMTABLINELEN];
	int  error=0;

	/* Place the file pointer at the beginning of the file */
	if (f == NULL || fseek(f, 0L, 0) == -1) {
		syslog(LOG_INFO, "Seek to beginning of rmtab failed");
		return;
	}

	/* Start with the first active mount */
	ml = mountlist;
	if (ml == NULL) return;

	/* Read each line in rmtab */
	while (fgets(line, sizeof(line), f) != NULL) {

		/* Get the starting positions of name and path */
		name = line;
		path = strchr(name, ':');

		/* Make sure we have a path */
		if (path == NULL) {
			error |= 0x1000;
		}
		
		/* Skip inactive or no path entires */
		if ((*name != '#') && (path != NULL)) {

			/* Null terminate name */
			*path = 0;	

			/* Adjust to the true starting positions of path */
			path++;

			/* Find end of path and terminate */
			end = strchr(path, '\n');
			if (end != NULL) {

				/* Null terminate path */
				*end = 0;

				/*
				 * Verify that the name and path we have 
				 * matches that read from the rmtab file.
				 */
				if (strcmp(ml->ml_name, name) != 0) {
					error |= 0x0030;
				}
				if (strcmp(ml->ml_path, path) != 0) {
					error |= 0x0004;
				}
			} else {
				/* no newline, skip entry */
				error |= 0x0200;
			}

			/* Next active entry */
			ml = ml->ml_nxt;
		}
	}
	if (error) {
		syslog(LOG_INFO, 
		"rmtab detected corrupt of type 0x%x, on call from line %d",
		error, from_line);
	}
#endif /* RMTAB_CHECKING */
}

