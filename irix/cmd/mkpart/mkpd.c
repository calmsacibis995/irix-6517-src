
#ident "irix/cmd/mkpd.c: $Revision: 1.17 $"

#define SN0 1

#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/conf.h>
#include <fcntl.h>

#include <sys/syssgi.h>
#include <syslog.h>
#include <sys/errno.h>

#include <sys/SN/router.h>
#include <sys/SN/SN0/sn0drv.h>

#include "mkpart.h"
#include "mkpd.h"
#include "mkprou.h"

extern int errno ;
static int debug ;
static int lock ;

#define DPRINT(l,x)	if (debug >= l) printf x

#define SLEEP_AND_CONTINUE { sleep(MKPD_SLEEP_TIME) ; continue ; }

static void 	mkpd_prreq_ipaddr(mkpd_packet_t *, char *, int, int *) ;
static void 	mkpd_prreq_roucfg(mkpd_packet_t *, char *, int, int *) ;
static void 	mkpd_prreq_lock(mkpd_packet_t *, char *, int, int *) ;
static void 	mkpd_prreq_unlock(mkpd_packet_t *, char *, int, int *) ;

static int 	mkpd_lsock_init(void) ;
static void 	pr_loc_req(pfdi_t *, int) ;
static int 	get_data_from_remote(char, pfdi_t *, int, char *, int, int) ;
static void 	mkpd_close_part_raw(pfdi_t *pfdi, fd_set *fds) ;
static int	is_mkpd_running(void) ;

/*
 * Call back routines for remote requests to daemon.
 */
struct req_handler {
	unsigned char 	reqid ;
	void		(*funcp)(mkpd_packet_t *, char *, int, int *) ;
} reqh[] = {
	OPCODE_GET_IPADDR, mkpd_prreq_ipaddr,
	OPCODE_GET_ROUCFG, mkpd_prreq_roucfg,
	OPCODE_LOCK_PART, mkpd_prreq_lock,
	OPCODE_UNLOCK_PART, mkpd_prreq_unlock,
	0, NULL
} ;

void
init_timeval(struct timeval *t)
{
	t->tv_sec  = MKPD_SLEEP_TIME ;
	t->tv_usec = 0 ;
}

/* 
 * pfdi - partition file descriptor info
 * pfdi support
 */
int
pfdiLookup(pfdi_t *pfdi, partid_t p)
{
	int i ;

	for (i=0; i<pfdiCnt(pfdi); i++)
		if (pfdiPartId(pfdi, i) == p)
			return i ;
	return -1 ;
}

int
pfdiDelete(pfdi_t *pfdi)
{
	int 	i ;

        for (i=0; i<pfdiCnt(pfdi); i++)
		if (pfdiPartPi(pfdi, i))
			free(pfdiPartPi(pfdi, i)) ;
	return 1 ;
}

/*
 * Add a partition to pfdi. Copy partinfo obtained from syssgi.
 */
int
pfdiAdd(pfdi_t *pfdi, part_info_t *pi)
{
	int newi ;

	newi = pfdiCnt(pfdi) ;
	if ((pfdiPartPi(pfdi, newi) = (part_info_t *)
			malloc(sizeof(part_info_t))) == NULL) {
		return -1 ;
	}

	bcopy(pi, pfdiPartPi(pfdi, newi), sizeof(part_info_t)) ;
	pfdiPartFd(pfdi, newi) 	= -1 ;
	pfdiPartSts(pfdi, newi) = PFD_CLSE ;
	pfdiCntIncr(pfdi) ;

	return newi ;
}

void
pfdiDump(pfdi_t *pfdi)
{
	int i ;

	for (i=0; i<pfdiCnt(pfdi);i++)
		printf("p = %d, fd = %d\n", 
			pfdiPartId(pfdi,i), pfdiPartFd(pfdi,i)) ;
}

void
pfdiFree(pfdi_t *pfdi)
{
	pfdiDelete(pfdi) ;
	if (pfdi)
		free(pfdi) ;
}

pfdi_t *
pfdiInit(void)
{
	pfdi_t *pf ;

	pf = (pfdi_t *)malloc(sizeof(pfdi_t)) ;
	if (pf == NULL)
		goto init_done  ;
	bzero(pf, sizeof(pfdi_t)) ;
	pfdiSize(pf) = MAX_PARTITIONS ;
init_done:
	return pf ;
}

/*
 * Build a request packet.
 */
void
mkpd_make_req(	char 		ptype,
		char 		op,
		mkpd_cnt_t 	cnt,
		mkpd_packet_t 	*packet,
		int 		flag)
{
	packet->version = VERSION ;
	packet->ptype 	= ptype ;
	packet->op 	= op ;
	packet->cnt 	= cnt ;
	packet->partid 	= get_my_partid() ;
	packet->flag 	= flag ;
}

/*
 * First send the count of data that is going.
 * Then send the actual data.
 */
void 
mkpd_send_resp_data(int fd, char *c, mkpd_cnt_t cnt)
{
	mkpd_packet_t	packet ;
	char		data[PACKET_DATA_LEN] ;

	bzero(&packet, sizeof(packet)) ;
	bzero(data, sizeof(data)) ;

	mkpd_make_req(PACKET_TYPE_COUNT, 0, cnt, &packet, 0) ;

	write(fd, &packet, PACKET_REQ_LEN) ;
	write(fd, c, cnt) ;
	DPRINT(1, ("send_resp_data: %x %d\n", *c, cnt)) ;
}

/* Send err response. This is a 0 in the 1st byte with data len = 1 */

void 
mkpd_send_resp_err(int fd)
{
	char errbuf[16] ;

	*errbuf = 0 ;
	mkpd_send_resp_data(fd, errbuf, 1) ;
	DPRINT(1,("send_resp_err\n")) ;
}

/*
 * Call back routine definitions.
 */

/*
 * Get local IP address into dbuf.
 *
 * Returns:
 *
 * 	count of bytes in cnt.
 * 	cnt = 1 and *dbuf = 0 if error.
 */
/* ARGSUSED */
static void
mkpd_prreq_ipaddr(mkpd_packet_t *pkt, char *dbuf, int dbuflen, mkpd_cnt_t *cnt)
{
	char 	name[MKPD_MAX_HOST_NAME_LEN] ;
	struct 	hostent *he ;
	char 	*c = "" ;
	char 	*prefix = MKPD_XPNET_NAME_PREFIX ;

	if ((dbuflen <= 0) || (dbuf == NULL))
		return ;

	if (dbuflen < HOSTADDR_LEN) {
		*cnt = 1 ;
		*dbuf = 0 ;
		return ;
	}

	strcpy(name, prefix) ;

	if (gethostname(name, MKPD_MAX_HOST_NAME_LEN) < 0)
		return ;

	he = gethostbyname(name) ;
	if (he == NULL)
        	return ;

	c = *he->h_addr_list ;
	if (!c)
		return ;

	bcopy(c, dbuf, HOSTADDR_LEN) ;
	*cnt = HOSTADDR_LEN ;
	DPRINT(1,("ipaddr: %x.%x.%x.%x\n", c[0], c[1], c[2], c[3])) ;
}

/*
 * Get the router config map for the local partition.
 */

/* ARGSUSED */

static void
mkpd_prreq_roucfg(mkpd_packet_t *pkt, char *data, int len, mkpd_cnt_t *cnt)
{
	part_rou_map_t 	*pmap = (part_rou_map_t *)data ;

	part_rou_map_init(pmap) ;
	*cnt = create_my_rou_map(get_my_partid(), pmap, len) ;

	if (*cnt < 0) {	/* err resp protocol */
		*cnt = 1 ;
		pmap->cnt = 0 ;
	}

	DPRINT(1,("roucfg: %d\n", cnt)) ;
}

/* ARGSUSED */

static void
mkpd_prreq_lock(mkpd_packet_t *pkt, char *dbuf, int dbuflen, mkpd_cnt_t *cnt)
{
	if (lock < 0) {
		lock = pkt->partid ;
		*dbuf = 1 ;	/* Operation succeeded */
	} else
		*dbuf = 0 ;

	*cnt = 1 ;
}

/* ARGSUSED */

static void
mkpd_prreq_unlock(mkpd_packet_t *pkt, char *dbuf, int dbuflen, mkpd_cnt_t *cnt)
{
	if ((pkt->flag) || (lock == pkt->partid)) {
		lock = -1 ;
		*dbuf = 1 ;	/* Operation succeeded */
	} else
		*dbuf = 0 ;

	*cnt = 1 ;
}

/*
 * Choose request call back based on request id.
 */
__psunsigned_t
mkpd_get_funcp(unsigned char reqid)
{
	struct req_handler *r ;

	r = &reqh[0] ;

	while (r->reqid) {
		if (r->reqid == reqid)
	    		return ((__psunsigned_t)r->funcp) ;
		r++ ;
	}
	return NULL ;
}

/*
 * mkpd_update_part_map
 *
 * 	Scan partition map returned by syssgi.
 * 	Add new partition info to pfdi.
 * 
 * Return
 *	
 * 	-1 if mem alloc fails.
 */

int
mkpd_update_part_map(pfdi_t *pfdi)
{
	int 		pcnt ;
	part_info_t	*tmp_pi ;
	int		npi = MAX_PARTITIONS ;
	int		i ;
	int		rv = 1 ;

        tmp_pi = (part_info_t *)malloc(npi * sizeof(part_info_t)) ;
	if (tmp_pi == NULL)
		return -1 ;
        bzero(tmp_pi, npi * sizeof(part_info_t)) ;

	pcnt = syssgi(SGI_PART_OPERATIONS,
		 SYSSGI_PARTOP_PARTMAP, npi, tmp_pi) ;
	if (pcnt < 0) {
		rv = -1 ;
		goto updpm_done ;
	}

	if (debug)
		fprintf(stderr, "np=%d ", pcnt) ;

	/* For all partitions found */
	for (i=0; i<pcnt; i++) {
		/* Is it already taken care of? */
		if (pfdiLookup(pfdi, (tmp_pi+i)->pi_partid) >= 0)
			continue ;
		/* Add it to list */
		if (pfdiAdd(pfdi, (tmp_pi + i)) < 0) {
			rv = -1 ;
                	goto updpm_done ;
        	}
	}

updpm_done:
    	free(tmp_pi) ;
	return rv ;
}

/*
 * mkpd_open_part_raw
 *
 * 	Open raw files of all active partitions.
 * 	Build an fd_set ready for select on read.
 */

void
mkpd_open_part_raw(partid_t myp, pfdi_t *pfdi, fd_set *fds)
{
	char 	name[MKPD_MAX_XRAW_FNAME_LEN] ;
	int 	i ;
	int	fd ;
	char 	errname[64] ;

	for (i=0; i<pfdiCnt(pfdi); i++) {
		/* Skip local or open partition */
		if (pfdiPartId(pfdi,i) == myp)
			continue ;

		if (PART_OPEN(pfdi, i)) {
			FD_SET(pfdiPartFd(pfdi,i), fds) ;
			continue ;
		}

		/* get path name of partition admin special file */

		if (get_raw_path(name, pfdiPartId(pfdi, i)) < 0)
			continue ;

		fd = open(name, O_RDWR|O_NDELAY) ;
		if (fd < 0) {
			if (debug) {
				sprintf(errname, "Open %s", name) ;
				perror(errname) ;
			}
			mkpd_close_part_raw(pfdi, fds) ;
			/* XXX retry if it says resource temp unavailable */
			continue ;
		} else {
			DPRINT(1,("%s: fd = %d\n", name, fd)) ;
		}

		/* Setup connection to partition i */

		pfdiPartFd(pfdi,i) = fd ;
		PART_SET_OPEN(pfdi,i) ;
		FD_SET(fd, fds) ;
	}
}

/*
 * mkpd_close_part_raw
 *
 * 	close all raw partitions as an error occured in one
 *	of them.
 */
static void
mkpd_close_part_raw(pfdi_t *pfdi, fd_set *fds)
{
	int		i ;
	int		fd ;

        for (i=0; i<pfdiCnt(pfdi); i++) {
                if (PART_OPEN(pfdi, i)) {
			fd = pfdiPartFd(pfdi,i) ;
			pfdiPartFd(pfdi, i) = -1 ;
			close(fd) ;
			PART_SET_CLOSE(pfdi,i) ;
			FD_CLR(fd, fds) ;
		}
	}
}

/* 
 * process_request
 *
 * 	process a request on a file descriptor.
 */
void
process_request(	pfdi_t 	*pfdi, 
			int 	lsock, 
			fd_set 	*fds, 
			int 	maxfds)
{
	int 		i ;
	void 		(*fp)() ;
	mkpd_cnt_t	cnt ;
	mkpd_packet_t	packet ;
	/* Receive the wanted data in this buffer */
	char 		*data ;
	int		datalen ;
	int		opcode ;

	bzero(&packet, sizeof(packet)) ;

	for (i=0; i<maxfds; i++) {
		if (FD_ISSET(i, fds)) {

			/* Process request from local socket */

			if (i == lsock) {
				pr_loc_req(pfdi, lsock) ;
				continue ;
			}

			/* Process req from remote daemon */

			/* Read packet of type opcode */

			if ((opcode = mkpd_read_req(	i, 
							&packet, 
							PACKET_TYPE_OPCODE
							)) < 0) {
				continue ;
			}

			/* allocate buffer of len given in packet */

			datalen = packet.cnt ;
			data = (char *)malloc(datalen) ;
			if (data == NULL) {
                                mkpd_send_resp_err(i) ;
				continue ;
			}
			bzero(data, datalen) ;

			/* Execute the callback for the reqd op */

			if ((fp = (void (*)())mkpd_get_funcp(opcode))) {
				(*fp)(&packet, data, datalen, &cnt) ;
        			mkpd_send_resp_data(i, data, cnt) ;
			} else { 	/* Illegal opcode */
				mkpd_send_resp_err(i) ;
			}

			if (data)
				free(data) ;
		}
	}
}

int
mkpd_select(fd_set *fds)
{
	struct timeval	t ;
	int		sel, maxfds ;

	maxfds = getdtablehi() ;
	init_timeval(&t) ;
	sel = select(maxfds, fds, NULL, NULL, &t) ;
	return sel ;
}

/*
 * mkpart_daemon
 *
 * Main loop. Serves requests received from any
 * active partitions. Keeps looking out for new
 * partitions coming up.
 * 
 * Returns: -1 if any error, like mem alloc or
 * 	    file open. Does not return otherwise.
 */

int
mkpart_daemon(int lsock, partid_t myp)
{
	fd_set		rfds ;
	pfdi_t		*pfdi ;
	int 		sel, maxfds ;
	int		one = 1 ;
	int 		cnt = 5 ;

	if ((pfdi = pfdiInit()) == NULL)
  		return -1 ;

	do {
		/*
 	 	 * Read partition map all over again.
	 	 * If new partitions have come up, they will
	 	 * be serviced from now on.
	 	 */
		if (mkpd_update_part_map(pfdi) < 0)
			SLEEP_AND_CONTINUE
		/*
	 	 * Open all the partition raw files.
	 	 */
		FD_ZERO(&rfds) ;

		mkpd_open_part_raw(myp, pfdi, &rfds) ;

		FD_SET(lsock, &rfds) ;

		/*
		 * Check all of them for any requests. If one
		 * is available, process the request.
		 */

        	maxfds 	= getdtablehi() ;
		sel 	= mkpd_select(&rfds) ;

		if (sel > 0) {
	    		process_request(pfdi, lsock, &rfds, maxfds) ;
		} else if (sel == 0) { 		/* timed out */
			DPRINT(1,(". ")) ;
			fflush(stdout) ;
		} else {
			mkpd_close_part_raw(pfdi, &rfds) ;
		}

		/* Some testing debug code. */

		if ((!cnt--) && (debug == 2)){
			cnt = 5 ;
			mkpd_close_part_raw(pfdi, &rfds) ;
		}
    	} while (one) ;

/* NOTREACHED */

	pfdiFree(pfdi) ;

	return -1 ;
}

void
main(int argc, char **argv)
{
	partid_t		mypid ;
	int			lsock ;
	int			one = 1 ;

	lock = -1 ;

	if (argc > 1)
		debug = argv[1][0] - '0' ;

	if ((lsock = mkpd_lsock_init()) < 0)
		exit(0) ;

	if (!debug) {
        	ignore_signal(SIGINT, 	SA_RESTART) ;
        	ignore_signal(SIGTERM, 	SA_RESTART) ;
        	ignore_signal(SIGPIPE, 	SA_RESTART) ;
		ignore_signal(SIGHUP, 	SA_RESTART) ;
	}

        openlog("mkpd", LOG_PID , LOG_DAEMON);
	if (debug)
		logmsg("started ...\n") ;

	if (0 > _daemonize(debug? (_DF_NOCHDIR|_DF_NOFORK):0,
			   	debug? STDOUT_FILENO:-1,
			   	debug? STDERR_FILENO:-1,
			   	lsock)) {
		logmsg("_daemonize failed\n") ;
		exit(1) ;
	}

	while (one) {
		/*
 		 * If there are no partitions up yet, try later
		 */
        	if ((mypid = get_my_partid()) == INVALID_PARTID)
	    		SLEEP_AND_CONTINUE

		/*
		 * Try to service mkpart requests. If we fail due
		 * to any reason, wait and retry the whole thing again.
		 */
		if (mkpart_daemon(lsock, mypid) < 0)
	    		SLEEP_AND_CONTINUE
	}
}

static int
mkpd_lsock_init(void)
{
	int 			lsock ;
	struct sockaddr_un	sname ;

	lsock = socket(PF_UNIX, SOCK_STREAM, 0) ;
	if (lsock < 0) {
		if (debug >= 1)
			perror("socket") ;
		goto all_done ;
	}

	sname.sun_family = AF_UNIX ;
	strcpy(sname.sun_path, MKPD_SOCKET_NAME) ;

	if (bind(lsock, (void *)&sname, sizeof(sname)) < 0) {
		if (!is_mkpd_running()) {
			unlink(sname.sun_path) ;
        		if (bind(lsock, (void *)&sname, sizeof(sname)) < 0) {
				logmsg("Unable to use sockets, exiting.\n") ;
				if (debug >= 1)
					perror("bind") ;
				lsock = -1 ;
				goto all_done ;
			}
		} else {
			logmsg("mkpd is already running, exiting.\n") ;
			if (debug >= 1)
				printf("mkpd is already running, exiting.\n") ;
			lsock = -1 ;
			goto all_done ;
		}
	}

	chmod(sname.sun_path, 00700) ;

	if (listen(lsock, MKPD_MAX_SOCKET)) {
		if (debug >= 1)
			perror("listen") ;
		lsock = -1 ;
		goto all_done ;
	}

all_done:
	DPRINT(1,("socket: %d\n", lsock)) ;
	return lsock ;
}

void
lock_local(int fd)
{
        char    status_buf[16] ;

        if (lock < 0) {
                lock = get_my_partid() ;
                *status_buf = 1 ;
        } else
                *status_buf = 0 ;

        mkpd_send_resp_data(fd, status_buf, 1) ;
}

void
unlock_local(int fd, int flag)
{
	char 	status_buf[16] ;

	/* If flag, unlock by force. */

	if ((flag) || (lock == get_my_partid())) {
		lock = -1 ;
		*status_buf = 1 ;
	} else
		*status_buf = 0 ;

        mkpd_send_resp_data(fd, status_buf, 1) ;
}

/*
 * pr_loc_req
 *
 * 	Process a request by the local mkpart command.
 * 	Use sockets for IPC.
 *
 */

static void
pr_loc_req(pfdi_t *pfdi, int lsock)
{
	mkpd_cnt_t 	pcnt, dcnt ;
	int		newfd ;
	mkpd_reqbuf_t	reqbuf ;
	int		indx ;
	char 		*buf ;
	int		buflen ;

	/* accept socket connection */

	if ((newfd = accept(lsock, 0, 0)) < 0)
                return ;

	/* read a request buf */

        pcnt = read(newfd, &reqbuf, sizeof(mkpd_reqbuf_t)) ;
        if (pcnt != sizeof(mkpd_reqbuf_t)) {
		if (pcnt >= 0)	/* we received something */
			mkpd_send_resp_err(newfd) ;
		goto close_ret ;
	}

	if (reqbuf.version != VERSION) {
		mkpd_send_resp_err(newfd) ;
		goto close_ret ;
	}

	DPRINT(1,("pr_loc_req: op = %d p = %d\n", reqbuf.op, reqbuf.part)) ;

	/* Handle special lock processing for local partition. */

	if (reqbuf.part == get_my_partid()) { 
		if (reqbuf.op == OPCODE_UNLOCK_PART)
			unlock_local(newfd, reqbuf.flag) ;
		else if (reqbuf.op == OPCODE_LOCK_PART)
			lock_local(newfd) ;
		else
		 	mkpd_send_resp_err(newfd) ;
		goto close_ret ;

	}

	/* Is the required partition open? */

        indx = pfdiLookup(pfdi, reqbuf.part) ;
	if ((indx < 0) || (!PART_OPEN(pfdi, indx))) {
                mkpd_send_resp_err(newfd) ;
                goto close_ret ;
	}

	/* Alloc memory for request */

	buflen  = reqbuf.cnt ;
	buf 	= (char *)malloc(buflen) ;
	if (buf == NULL)
		return ;	/* XXX */

        dcnt = get_data_from_remote(	reqbuf.op, pfdi, 
					indx, buf, buflen, reqbuf.flag) ;
	if (dcnt < 0)
                mkpd_send_resp_err(newfd) ;
	else
        	mkpd_send_resp_data(newfd, buf, dcnt) ;

	if (buf)
		free(buf) ;
close_ret:
	close(newfd) ;
}

/*
 * get_data_from_remote
 *
 * 	When the daemon receives a request from the local mkpart
 * 	it sends a request out to a remote daemon and gets data
 * 	from it.
 *
 * Returns:
 *
 *	data from remote in databuf, count of valid bytes of data.
 *	-1 if something is wrong.
 */
static int
get_data_from_remote(	char 		op	,
			pfdi_t 		*pfdi	,
			int 		indx	,
			char 		*databuf,
			int 		dbuflen ,
			int		flag)
{
	mkpd_packet_t	packet ;
	mkpd_cnt_t	cnt = -1, cnt1 ;
	int		fd ;
	char 		*tmpbuf ;

	if ((databuf == NULL) || (dbuflen <= 0))
		return -1 ;

	if (PART_OPEN(pfdi, indx))
        	fd = pfdiPartFd(pfdi, indx) ;
	else
		return -1 ;

	/* Send the opcode given by mkpart to the remote daemon. */

	mkpd_make_req(PACKET_TYPE_OPCODE, op, dbuflen, &packet, flag) ;
        if (write(fd, &packet, PACKET_REQ_LEN) != PACKET_REQ_LEN)
		return -1 ;

	/* Find out how much data he has to give. */

	if ((cnt = mkpd_read_req(fd, &packet, PACKET_TYPE_COUNT)) < 0) {
		/* XXX */
		return -1 ;
	}

	/* Alloc a new buffer, if he has lots to give. */

        if (cnt > dbuflen) {
                tmpbuf = (char *)malloc(cnt) ;
		if (tmpbuf == NULL)
			return -1 ;	/* XXX */
        }
        else 	/* Use the supplied buf, it looks enough. */
                tmpbuf = databuf ;

        if ((cnt1 = read(fd, tmpbuf, cnt)) != cnt) {
		if (cnt < 0)		/* total read failure */
                	return -1 ;
		else			/* got less than expected */
			cnt = cnt1 ;
        }

	/* If we got more, truncate data to dbuflen */

        if (cnt > dbuflen) {
                bcopy(tmpbuf, databuf, dbuflen) ;
		cnt = dbuflen ;
                free(tmpbuf) ;
        }

	DPRINT(1,("get_data_from_remote: cnt = %d\n", cnt)) ;
	return cnt ;
}


#include <sys/procfs.h>
#include <dirent.h>
#include <paths.h>
#include <sys/mac.h>

static int
is_mkpd_running(void)
{
	DIR 		*dirp ;
	struct dirent 	*dentp ;
	int		pdlen ;
	char 		pname[100] ;
	struct prpsinfo info ;
	int		num_mkpd = 0 ;

        if ((dirp = opendir(_PATH_PROCFSPI)) == NULL)
	/* Assume things are OK. Let it fail elsewhere. */
                return(0);

        (void) sprintf(pname, "%s%c", _PATH_PROCFSPI, '/');
        pdlen = strlen(pname);

        while (dentp = readdir(dirp)) {
                int     procfd;         /* fd for /proc/pinfo/nnnnn */
                char   *plblstring;     /* process label string */

                if (strcmp(dentp->d_name, ".") == 0 ||
                    strcmp(dentp->d_name, "..") == 0 ||
		    (dentp->d_name[0] == '.'))
                                 continue;

                plblstring = NULL;
                (void) strcpy(pname + pdlen, dentp->d_name);

                if ((procfd = open(pname, O_RDONLY)) == -1) {
                        mac_free(plblstring);
                        continue;
                }

                if (ioctl(procfd, PIOCPSINFO, (char *) &info) == -1) {
                        (void) close(procfd);
			continue ;
		}

                (void) close(procfd);

		if (strstr(info.pr_fname, "mkpd"))
			num_mkpd ++ ;
	}

        (void) closedir(dirp);

	if (num_mkpd > 1)
		return 1 ;
	else
		return 0 ;
}





