/* point-to-point daemon utility code
 */

#ident "$Revision: 1.48 $"

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <paths.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <values.h>
#include <math.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/stropts.h>
#include <sys/conf.h>
#include <sys/major.h>

#include <netinet/tcp.h>
#include <net/soioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "pputil.h"

#if !defined(PPP) && !defined(SLIP)
#error "must be compiled for either SLIP or PPP"
#endif

#ifdef PPP
extern void proxy_unarp(void);
#endif

#if defined(PPP_IRIX_62) || defined(PPP_IRIX_53)
extern void setservice(char*);
extern void stlock(char*);
extern int conn_trycalls;
extern char lock_pid[SIZEOFPID+2];	/* account for \n and \0 */
extern int Nlocks;
extern char *AbortStr[];
extern int AbortSeen, Aborts;
extern int conn(char*);
#endif

static void stray_ctty(void);

static pid_t kludged;

#define USEC_PER_SEC 1000000

/* Update the nondecreasing clock that is not affected by time changes,
 * as well as the system time-of-day clock.  Use a value that is the
 * number of microseconds since the system was started.
 * Using only times() produces a clock that is too coarse, so combine
 * the kernel's time-of-day clock with times()'s number of ticks since
 * the system started.
 */
time_t					/* return seconds */
get_clk(void)
{
#	define FLOAT_TIME(t) (t.tv_sec*1.0 + t.tv_usec*1.0/USEC_PER_SEC)
	static double sstart_d, prev_clk_d;
	double d, cur_date_d;
	clock_t ticks;			/* HZ ticks since system started. */
	struct tms tms;

	ticks = times(&tms);
	gettimeofday(&cur_date);

	/* Notice changes in the apparent time-of-day when the system was
	 * started, and infer an adjustment in the system clock.
	 * Use that to keep the daemon's nondecreasing clock straight.
	 */
	cur_date_d = FLOAT_TIME(cur_date);
	d = cur_date_d - ticks*1.0/HZ;

	/* From the start of a tick until its end, the appearant system
	 * start time will appear to increase.  At the start of the next
	 * tick, it will decrease by 1.0/HZ seconds.  Consecutive
	 * nondecreasing clock values could jump forward or backwards
	 * by 1.0/HZ seconds as a result if the computed system start
	 * time were accept uncritically.
	 */
	if (fabs(d-sstart_d) > 1.0/HZ) {
		if (d < sstart_d) {
			/* do not let the clock jump backwards */
			sstart_d = MIN(cur_date_d-prev_clk_d+1.0/USEC_PER_SEC,
				       d+1.0/HZ);
		} else {
			sstart_d = d-1.0/HZ;
		}
	}

	prev_clk_d = cur_date_d-sstart_d;
	clk.tv_sec = (long)floor(prev_clk_d);
	clk.tv_usec = (long)((prev_clk_d-clk.tv_sec)*USEC_PER_SEC);
	return clk.tv_sec;
}


void
timevalsub(struct timeval *t1,
	   struct timeval *t2,
	   struct timeval *t3)
{
	register time_t sec, usec;

	sec = t2->tv_sec - t3->tv_sec;
	usec = t2->tv_usec - t3->tv_usec;
	while (usec < 0) {
		sec--;
		usec += USEC_PER_SEC;
	}
	while (usec >= USEC_PER_SEC) {
		sec++;
		usec -= USEC_PER_SEC;
	}
	t1->tv_sec = sec;
	t1->tv_usec = usec;
}


/* Arrange to make stderr point to something useful for the UUCP
 * code, and otherwise start the error logging.
 */
void
kludge_stderr(void)
{
	static char pidstr[16+1+16+1+1];
	int fildes[2];
	pid_t pid;
	int i;
	int serror;
	char *es;


	pid = getpid();
	if (stderrfd < 0
	    || (kludged > 0 && kludged != pid)) {
		kludged = pid;
		(void)sprintf(pidstr,"%0.16s[%d]",pgmname,kludged);
		stderrfd = -1;

		if (0 > pipe(fildes)) {
			stderrpid = -1;
			es = "pipe() failed for log";
		} else {
			stderrpid = fork();
			es = "fork() failed for log";
		}
		if (stderrpid == 0) {
			/* in the child */
			no_signals(SIG_IGN);

			if (0 <= dup2(fildes[0],0)) {
				/* no controlly tty */
				ctty = 0;
				stray_ctty();
				/* stderr is open, ensure it is file ID 2 */
				if (stderrttyfd >= 0
				    && stderrttyfd != 2
				    && 0 <= dup2(stderrttyfd,2))
					stderrttyfd = 2;
				for (i = getdtablehi(); --i > 0; ) {
					if (i != stderrttyfd)
						(void)close(i);
				}
				(void)execlp("logger", "logger",
					     (stderrttyfd == 2) ? "-st" : "-t",
					     pidstr, "-p", "daemon.err", 0);
			}
			syslog(LOG_ERR, "failed to start logger: %m");
			exit(-1);

		} else if (stderrpid > 0) {
			if (stderrttyfd == 2) {
				stderrttyfd = dup(stderrttyfd);
				(void)close(2);
			}
			(void)close(fildes[0]);
			if (0 > dup2(fildes[1], 2)) {
				es = "dup2()";
				stderrpid = -1;
			} else {
				(void)dup2(2,1);
				stderrfd = 2;
			}
			if (fildes[1] != 2)
				(void)close(fildes[1]);
		}

		if (stderrpid < 0) {
			/* fall back to the kludge if the fork fails */
			serror = errno;
			if (!freopen("/dev/log", "a", stderr))
				return;	/* huh? */
			stderrfd = fileno(stderr);
			errno = serror;
			log_errno(es,"");
		}

		setlinebuf(stderr);
	}
}


void
call_system(char *link_name,
	    char *cmd_name,
	    char *cmd_base,
	    int note_exit)
{
#define CMD_NORM "1>/dev/null "
#define CMD_VERBOSE "set -x; 1>&2 "
	int i;
	char *cmd;

	if (debug != 0) {
		cmd = malloc(strlen(cmd_base) + sizeof(CMD_VERBOSE));
		strcpy(cmd, CMD_VERBOSE);
	} else {
		cmd = malloc(strlen(cmd_base) + sizeof(CMD_NORM));
		strcpy(cmd, CMD_NORM);
	}
	strcat(cmd, cmd_base);

	kludge_stderr();
	i = system(cmd);
	if (0 > i) {
		log_errno(cmd_name, cmd_base);
	} else if (note_exit && (i = WEXITSTATUS(i)) != 0) {
		log_complain(link_name,"%s`%s` exit=%d",
			     cmd_name, cmd_base, i);
	}

	free(cmd);
#undef CMD_STDIO
#undef CMD_VERBOSE
}


void
init_rand(void)
{
	extern unsigned sysid(unsigned char id[16]);

	srandom(sysid(0) ^ time(0) ^ gethostid());
	srand48(random());
	srand(random());
}


/* lock the device after answering the phone
 */
void
grab_dev(const char *hello_msg,
	 struct dev *dp)
{
	FILE *lf;
	pid_t pid;
	char *p;
	int i;

	dp->devfd = 0;
	if (!set_tty_modes(dp))		/* set async tty modes */
		cleanup(1);

	p = ttyname(dp->devfd);
	if (!p) {
		log_complain("","unable to discover device name (fd=%d)",
			     dp->devfd);
		cleanup(1);
	}
	i = strlen(p);
	if (i > sizeof(dp->nodename)-sizeof("/dev/")
	    || i < sizeof("/dev/")) {
		log_complain("","bogus device name \"%s\" (fd=%d)",
			     p,dp->devfd);
		cleanup(1);
	}
	(void)strcpy(dp->nodename,p+sizeof("/dev/")-1);
	(void)strcpy(dp->line,p);
	if (!lockname(dp->nodename,dp->lockfile,
		      sizeof(dp->lockfile))) {
		log_complain("","unable to compute lock file name for %s",
			     dp->nodename);
	}

	if (mmlock(dp->nodename)) {	/* Try to lock the tty */
		lf = fopen(dp->lockfile,"r+");
		if (!lf) {
			log_complain("","unable to open lockfile for %s",
				     dp->line);
			cleanup(1);
		}
		if (1 != fscanf(lf, "%ld", &pid)) {
			log_complain("","unable to parse lockfile for %s",
				     dp->line);
			cleanup(1);
		}
		if (pid != getpid()) {
			log_debug(1, "","strange PID %d in lockfile for %s"
				  " (my PPID=%d);"
				  " incoming/outgoing call collision?",
				  pid, dp->line, getppid());
			/* Since things are confused, let the other guy know
			 * and then quit.
			 */
			(void)kill(pid, SIGINT);
			cleanup(1);
		}
		(void)fclose(lf);
		stlock(dp->lockfile);
	}

	if (dp->sync == SYNC_OFF) {
		if (0 > write(dp->devfd, hello_msg, strlen(hello_msg)))
			log_errno("write(hello)","");
		if (0 > tcdrain(dp->devfd) && errno != EINTR)
			log_errno("hello tcdrain","");
		(void)sginap(HZ/5);
	}
}


/* Relay between a socket and a pty for TCP testing.
 * This wants a uucp/Devices entry like:
 *	PPPTCP - - Any TCP login
 * and a uucp/Systems entry like:
 *	tcpgar Never PPPTCP,t Any garnet "" \0lnam\0rnam\0ppp/38400\0\c
 *		assword~2 tcpppp PPP~2
 * PPP needs
 *	map_char_num=0xff
 */
static void
pty_relay(struct dev *dp,
	  int soc,			/* socket being connected to pty */
	  int mpty)			/* master side of pty */
{
	int i, j;
	char buf[2048];
	fd_set rfds;

	/* kludge to keep the PTY open while closing everything else */
	dp->devfd = soc;
	rendnode = mpty;
	stderrfd = -1;
	stderrpid = -1;
	status_poke_fd = -1;
	closefds();
	modfd = -1;
	rendnode = -1;

	no_signals(SIG_IGN);
	(void)signal(SIGPIPE, killed);

	FD_ZERO(&rfds);
	for (;;) {
		FD_SET(mpty, &rfds);
		FD_SET(soc, &rfds);
		i = select(MAX(mpty,soc)+1, &rfds, 0, 0, 0);
		if (i < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			bad_errno("pty_relay() select()","");
		}

		if (FD_ISSET(mpty, &rfds)) {
			i = read(mpty, buf, sizeof(buf));
			if (i <= 0) {
				if (i == 0)
					log_debug(4,"","pty_relay mpty EOF");
				else
					bad_errno("pty_relay read(mpty)", "");
				exit(0);
			}
			j = write(soc, buf, i);
			if (i != j) {
				if (j < 0) {
					if (errno == EBADF || debug != 0)
					    bad_errno("pty_relay write(soc)",
						      "");
					exit(0);
				} else {
					log_complain("",
						     "pty_relay write(soc)=%d"
						     " not %d", i, j);
				}
			}
		}

		if (FD_ISSET(soc, &rfds)) {
			i = read(soc, buf, sizeof(buf));
			if (i <= 0) {
				if (i == 0)
					log_debug(4,"","pty_relay soc EOF");
				else
					bad_errno("pty_relay read(soc)", "");
				exit(0);
			}
			j = write(mpty, buf, i);
			if (i != j) {
				if (j < 0) {
					if (errno == EBADF || debug != 0)
					    bad_errno("pty_relay write(mpty)",
						      "");
					exit(0);
				} else {
					log_complain("",
						     "pty_relay write(mpty)=%d"
						     " not %d", i, j);
				}
			}
		}
	}
}


int					/* 0=failed */
set_tty_modes(struct dev *dp)
{
	char lname[FMNAMESZ+1];
	char *spty_name;
	int soc, spty, mpty = -1;
	int fl;
	int val, val_len;
	FILE *lf;


	/* if we are talking to a socket, then assume this is a test
	 * over TCP and insert a pty in series.
	 * Just muddle through if we fail to make the pty.
	 */
	if (!strcmp(dp->nodename, "-")) {
		soc = dp->devfd;

		val_len = sizeof(val);
		if (0 > getsockopt(soc,SOL_SOCKET,SO_SNDBUF, &val, &val_len)) {
			log_errno("getsockopt(soc, SO_SNDBUF)","");
		} else if (val < 60*1024) {
			val = 60*1024;
			if (0 > setsockopt(soc,SOL_SOCKET,SO_SNDBUF,
					   &val, sizeof(val)))
				log_errno("setsockopt(soc, SO_SNDBUF)","");
		}
		val = 1;
		if (0 > setsockopt(soc, IPPROTO_TCP, TCP_NODELAY,
				   &val, sizeof(val))) {
			log_errno("setsockopt(soc, TCP_NODELAY)","");
			return 0;
		}


		if (0 == (spty_name = _getpty(&mpty,O_RDWR|O_NOCTTY,0600,1))) {
			log_complain("", "_getpty() failed");
			return 0;
		}

		if (0 > (spty = open(spty_name, O_RDWR|O_NOCTTY))) {
			log_errno("spty open() ", spty_name);
			(void)close(mpty);
			return 0;
		}


		/* switch to the pty
		 */
		(void)strcpy(dp->line, spty_name);
		(void)strcpy(dp->nodename,
			     &spty_name[!strncmp(spty_name,"/dev/",5) ? 5:0]);
		if (!lockname(dp->nodename,dp->lockfile,
			      sizeof(dp->lockfile))) {
			log_complain("","unable to make lock file name for %s",
				     dp->nodename);
			return 0;
		}

		lf = fopen(dp->lockfile,"w");
		if (!lf) {
			log_errno("fopen(pty) lock file ", dp->lockfile);
			return 0;
		}
		(void)fprintf(lf, LOCKPID_PAT, getpid());
		stlock(dp->lockfile);
		(void)fclose(lf);

		dp->devfd = spty;
		dp->sync = SYNC_OFF;

	} else {
		if (0 > fstat(dp->devfd, &dp->dev_sbuf)) {
			dp->dev_sbuf.st_ino = 0;
			log_errno("fstat(dev)","");
			return 0;
		}

		/* default line type */
#if defined(SPLI_MAJOR) || defined(WSTY_MAJOR)
		if (dp->sync == SYNC_DEFAULT
		    && (major(dp->dev_sbuf.st_rdev) == SPLI_MAJOR
#ifdef WSTY_MAJOR
			|| major(dp->dev_sbuf.st_rdev) == WSTY_MAJOR
#endif
			))
			dp->sync = SYNC_ON;
#endif
		if (dp->sync == SYNC_DEFAULT) {
			if (0 > ioctl(dp->devfd,I_LOOK,lname)) {
				if (errno != EINVAL) {
					log_errno("ioctl(I_LOOK)","");
					return 0;
				}
				/* assume EINVAL indicates a MUX and that
				 * all MUXes are for sync devices.  sigh.
				 */
				dp->sync = SYNC_ON;
			} else if (strcmp("stty_ld", lname)) {
				log_debug(1,"",
					  "no line discipline STREAMS module"
					  "--assume sync");
				dp->sync = SYNC_ON;
			} else {
				dp->sync = SYNC_OFF;
			}
		}
	}

	if (dp->sync == SYNC_OFF) {
		if (0 > ioctl(dp->devfd, TCGETA, (char*)&dp->devtio)) {
			log_errno("set_tty_modes TCGETA","");
			return 0;
		}

#if defined(PPP_IRIX_62) || defined(PPP_IRIX_53)
#ifdef CNEW_RTSCTS
		dp->devtio.c_cflag &= (CBAUD | CNEW_RTSCTS);
#else
		dp->devtio.c_cflag &= CBAUD;
#endif
#else
#ifdef CNEW_RTSCTS
		dp->devtio.c_cflag &= CNEW_RTSCTS;
#else
		dp->devtio.c_cflag = 0;
#endif
#endif
		dp->devtio.c_cflag |= CS8|CREAD|HUPCL;
		if (dp->xon_xoff)
			dp->devtio.c_iflag = IGNPAR|IGNBRK | IXON|IXOFF;
		else
			dp->devtio.c_iflag = IGNPAR|IGNBRK;
		dp->devtio.c_oflag = 0;
		dp->devtio.c_lflag = 0;
		dp->devtio.c_cc[VMIN] = 0;
		dp->devtio.c_cc[VTIME] = 0;
		if (0 > ioctl(dp->devfd, TCSETAW, (char*)&dp->devtio)) {
			log_errno("TCSETAW","");
			return 0;
		}

		/* after the pty is ready, start using it
		 */
		if (mpty >= 0) {
			dp->pty_pid = fork();
			if (0 > dp->pty_pid) {
				log_errno("pty fork()","");
				return 0;
			}
			if (dp->pty_pid == 0)
				pty_relay(dp, soc, mpty);

			log_debug(1,"","pty pid=%d", dp->pty_pid);
			dp->dev_sbuf.st_ino = 0;
			(void)close(soc);
			(void)close(mpty);
		}

	} else {
		/* Use non-blocking I/O on sync channels, since it is
		 * better to drop output packets than to wait forever for
		 * the ISDN daemon to work.  Blocking forever in putmsg()
		 * causes lots of havoc.
		 */
		fl = fcntl(dp->devfd, F_GETFL, 0);
		if (fl == -1) {
			log_errno("fcntl(F_GETFL)","");
		} else if (0 > fcntl(dp->devfd, F_SETFL, fl | FNDELAY)) {
			log_errno("fcntl(F_SETFL, | FNDELAY)","");
		}
	}

	return 1;
}


/* get a tty ready
 */
int					/* 0=failed */
rdy_tty_dev(struct dev *dp)
{
	char lname[FMNAMESZ+1];
	int i;

	if (!set_tty_modes(dp))
		return 0;

	if (dp->dev_sbuf.st_ino != 0) {
		if (dp->dev_sbuf.st_uid != 0
		    && 0 > chown(dp->line, 0,0))
			log_errno("chown(dev) ", dp->line);
		if (0 > chmod(dp->line, S_IREAD|S_IWRITE))
			log_errno("chmod(dev) ", dp->line);
	}

	/* strip line discipline */
	if (dp->sync == SYNC_OFF) {
		if (0 > ioctl(dp->devfd,I_LOOK,lname)) {
			log_errno("ioctl(I_LOOK) ","");
			strcpy(lname,"???");
		}
		if (0 > ioctl(dp->devfd,I_POP,0))
			log_errno("ioctl(I_POP) ",lname);
		if (0 > ioctl(dp->devfd, TCFLSH, 2))
			log_errno("TCFLSH-2","");
	}

	/* install custom STREAMS modules
	 */
	for (i = 0; i < num_smods; i++) {
		if (0 > ioctl(dp->devfd, I_PUSH, smods[i]))
			bad_errno("ioctl(I_PUSH) ", smods[i]);
		log_debug(6,"","ioctl(I_PUSH,%s) ok", smods[i]);
	}

	return 1;
}


/* set IP parameters
 */
void
set_ip(void)
{
	int soc;
	struct ifreq ifr;

	soc = socket(AF_INET, SOCK_DGRAM, 0);
	if (soc < 0)
		bad_errno("set_ip() socket()","");

	if (netmask.sin_addr.s_addr == 0) {
		if (IN_CLASSA(remhost.sin_addr.s_addr))
			netmask.sin_addr.s_addr = htonl(IN_CLASSA_NET);
		else if (IN_CLASSB(remhost.sin_addr.s_addr))
			netmask.sin_addr.s_addr = htonl(IN_CLASSB_NET);
		else
			netmask.sin_addr.s_addr = htonl(IN_CLASSC_NET);
	}
	(void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_addr = *(struct sockaddr *)&netmask;
#ifdef _HAVE_SIN_LEN
	netmask.sin_len = _SIN_ADDR_SIZE;
#endif
	netmask.sin_family = AF_INET;
	if (0 > ioctl(soc, SIOCSIFNETMASK, (caddr_t)&ifr))
		bad_errno("SIOCSIFNETMASK","");

	if (metric != 0) {
		(void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		ifr.ifr_metric = metric;
		if (0 > ioctl(soc, SIOCSIFMETRIC, (char*)&ifr))
			bad_errno("SIOCSIFMETRIC","");
	}

	(void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_addr = *(struct sockaddr *)&remhost;
	if (0 > ioctl(soc, SIOCSIFDSTADDR, (char*)&ifr))
		bad_errno("SIOCSIFDSTADDR","");

	(void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_addr = *(struct sockaddr *)&lochost;
	if (0 > ioctl(soc, SIOCSIFADDR, (char*)&ifr))
		bad_errno("SIOCSIFADDR","");

	(void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (0 > ioctl(soc, SIOCGIFFLAGS, (char*)&ifr))
		bad_errno("SIOCGIFFLAGS","");
	if (debug > 2) {
		ifr.ifr_flags |= IFF_DEBUG;
	} else {
		ifr.ifr_flags &= ~IFF_DEBUG;
	}
	ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
	(void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (0 > ioctl(soc, SIOCSIFFLAGS, (char*)&ifr))
		bad_errno("SIOCSIFFLAGS","");

	(void)close(soc);
}



/* create directory for rendezvous
 */
static int				/* 0=failed */
rend_mkdir(void)
{
	char rm_cmd[sizeof("/bin/rm -rf ")+RENDPATH_LEN];
	struct stat sbuf;

	for (;;) {
		if (0 > lstat(RENDDIR, &sbuf)) {
			if (errno != ENOENT)
				bad_errno("lstat() ", RENDDIR);
			if (0 > mkdir(RENDDIR, 0755)
			    && errno != EEXIST) {
				log_errno("mkdir() ", RENDDIR);
				return 0;
			}
			continue;
		}

		if (S_ISDIR(sbuf.st_mode)
		    && sbuf.st_uid == 0
		    && (sbuf.st_mode & (S_IWGRP | S_IWOTH)) == 0)
			break;

		log_complain("","stray %s", RENDDIR);
		(void)sprintf(rm_cmd, "/bin/rm -rf %s", RENDDIR);
		(void)system(rm_cmd);
		if (0 <= lstat(RENDDIR, &sbuf)) {
			log_complain("", "failed to `%s`", rm_cmd);
			return 0;
		}
	}

	return 1;
}



/* define another rendezvous
 */
int					/* 0=too many names */
add_rend_name(char *prefix,		/* "" or "ep-" */
	      char *str)		/* hostname, MAC address, etc */
{
	int i;
	struct rend *rp;
	char path[RENDPATH_LEN];

	if (!rend_mkdir())
		return 0;

	(void)sprintf(path, RENDPATH_PAT, prefix, str);
	for (rp = rends, i = 0; i < MAX_RENDPATHS; i++, rp++) {
		/* add the path to the list if it is new
		 */
		if (rp->path[0] == '\0') {
			(void)strcpy(rp->path, path);
			rp->made = 0;
			rp->st.st_uid = -1;
			num_rends++;
			return 1;
		}

		/* quit if the path is already known
		 */
		if (!strcmp(path,rp->path))
			return 1;
	}

	log_complain("","too many rendezvous names with \"%s\"", str);
	return 0;
}


/* mark all rendezvous names removed so that they will not be removed.
 */
void
unmade_rend(void)
{
	int i;
	struct rend *rp;

	for (rp = rends, i = 0; i < MAX_RENDPATHS; i++, rp++) {
		rp->made = 0;
		rp->st.st_uid = -1;
	}
	status_poke_path[0] = '\0';
	status_path[0] = '\0';
}


/* remove one or all rendezvous file names
 */
void
rm_rend(char *prefix,			/* "" or "ep-" */
	char *str)			/* hostname, MAC address, etc */
{
	int i;
	struct rend *rp;
	char path[RENDPATH_LEN];

	if (str)
		(void)sprintf(path, RENDPATH_PAT, prefix, str);
	for (rp = rends, i = 0; i < MAX_RENDPATHS; i++, rp++) {
		if (rp->path[0] == '\0')
			continue;
		if (rp->made && (!str || !strcmp(path, rp->path))) {
			(void)unlink(rp->path);
			rp->made = 0;
			rp->st.st_uid = -1;
			num_rends--;
			if (str) {
				rp->path[0] = '\0';
				return;
			}
			rp->path[0] = '\0';
		}
	}
	if (str) {
		(void)unlink(path);
		return;
	}
	if (status_poke_path[0] != '\0') {
		(void)unlink(status_poke_path);
		status_poke_path[0] = '\0';
	}
	if (status_path[0] != '\0') {
		(void)unlink(status_poke_path);
		status_path[0] = '\0';
	}
}


/* Create the rendezvous FIFO, if it does not already exist.
 *	Must only be called after link_fifo() so that rp->st.st_uid is valid.
 */
static void
make_fifo(int tgt)			/* this name */
{
	struct rend *rp = &rends[tgt];


	if (rp->st.st_uid == 0) {
		if (0 <=  utime(rp->path, 0))
			return;
		if (errno != ENOENT)
			bad_errno("lstat() ", rp->path);
		log_complain("","%s disappeared",rp->path);
	}

	(void)unlink(rp->path);
	rp->st.st_uid = -1;
	if (!rend_mkdir())
		return;
	if (0 > mknod(rp->path, S_IFIFO|S_IRUSR|S_IWUSR,0)) {
		log_errno("mknod() ", rp->path);
		rendnode = -1;
	} else {
		rp->made = 1;
	}
}


/* Check that the rendezvous FIFO exits
 *	Close its file descriptor if it is wrong.
 *	Create its links if ok.
 */
static int				/* return good name index */
link_fifo(int mode)			/* 0=create nothing,
					 * 1=create only if rendnode ok
					 * 2=create all */
{
	static nlink_t complain_num_rends;
	struct rend *rp, *good_rp;
	int i, good, bad;
	char *p1, *p2;
	struct stat sfd;


	/* Check all of the names to see that they are the same inode.
	 * If not, remove the oldest.
	 */
	good_rp = &rends[good = 0];
	for (rp = rends, i = 0; i < MAX_RENDPATHS; i++, rp++) {
		if (rp->path[0] == '\0')
			continue;

		if (0 > lstat(rp->path, &rp->st)) {
			/* bad node if non-existent or cannot stat() it */
			if (errno != ENOENT)
				bad_errno("lstat() ", rp->path);
			if (rp->made)
				log_complain("","%s disappeared",rp->path);
			rp->made = 0;
			rp->st.st_uid = -1;
			continue;
		}

		if (!S_ISFIFO(rp->st.st_mode)
		    || rp->st.st_uid != 0) {
			/* Bad node if not one of our FIFOs.
			 */
			log_complain("","stray %s", rp->path);
			(void)unlink(rp->path);
			rp->made = 0;
			rp->st.st_uid = -1;
			continue;
		}

		/* note the first good name
		 */
		while (good < i
		       && rends[good].st.st_uid != 0)
			good_rp = &rends[++good];

		/* bad if not the same as the other FIFOs
		 */
		if (good != i
		    && (good_rp->st.st_dev != rp->st.st_dev
			|| good_rp->st.st_ino != rp->st.st_ino)) {
			p1 = good_rp->path;
			p2 = rp->path;
			if (good_rp->st.st_mtime >= rp->st.st_mtime) {
				bad = i;
			} else {
				bad = good;
				good_rp = &rends[good = i];
			}
			if (mode != 0)
				log_complain("","conflicting %s and %s;"
					     " unlink %s",
					     p1, p2, rends[bad].path);
			rends[bad].made = 0;
			rends[bad].st.st_uid = -1;
		}
	}

	/* If no good path exists, then start creating with the first one.
	 */
	if (good_rp->st.st_uid != 0) {
		good_rp = &rends[good = 0];
		(void)close(rendnode);
		rendnode = -1;

	} else if (rendnode >= 0) {
		if (0 > fstat(rendnode,&sfd)) {
			log_errno("rendezvous fstat() ","rendnode");
			(void)close(rendnode);
			rendnode = -1;
		} else if (good_rp->st.st_dev != sfd.st_dev
			   || good_rp->st.st_ino != sfd.st_ino) {
			log_complain("","rendezvous %s replaced",
				     good_rp->path);
			(void)close(rendnode);
			rendnode = -1;
		}
	}

	/* if only checking (and so do not have the lock), quit now.
	 */
	if (mode == 0 || (mode == 1 && rendnode < 0))
		return (good_rp->st.st_uid != 0) ? -1 : good;

	/* create the node and the links
	 */
	make_fifo(good);
	for (rp = rends, i = 0; i < MAX_RENDPATHS; i++, rp++) {
		if (rp->path[0] == '\0')
			continue;

		rp->made = 1;
		if (i != good && rp->st.st_uid != 0) {
			(void)unlink(rp->path);
			if (0 > link(good_rp->path,rp->path)) {
				log_errno("link() ", rp->path);
				rendnode = -1;
			}
		}
	}

	/* check for stray links
	 */
	if (0 > stat(good_rp->path, &good_rp->st)) {
		log_errno("stat() ", good_rp->path);
		rendnode = -1;
	}
	if (good_rp->st.st_nlink != num_rends
	    && good_rp->st.st_nlink != complain_num_rends) {
		log_debug(1,"","%s has %d instead of %d links",
			  good_rp->path, good_rp->st.st_nlink, num_rends);
		complain_num_rends = good_rp->st.st_nlink;
	}
	return good;
}


/* create the rendezvous FIFO if necessary
 *	The IP address of the remote host must be known.
 */
int					/* 1=need to pass on FD */
make_rend(int make_nodes)		/* 1=names are known so make nodes */
{
#	define REND_LOCK "pputil"
#	define REND_LOCK2 (LOCKPRE "." REND_LOCK)
	int good, i;
	struct rend *good_rp;


	if (num_rends == 0) {
		log_debug(6,"","peer name and address unknown for rendezvous");
		return 0;
	}

	/* If the FIFO already exists, then just check to see that it has
	 * not been deleted by an overeager /tmp cleaner.
	 */
	if (rendnode >= 0) {
		(void)link_fifo(1);
		if (rendnode >= 0)
			return 0;
	}

	/* We do not have the FIFO open.   Check to see if it exists
	 */
	good = link_fifo(0);

	/* If we are not sure of the right names, do not create any nodes.
	 */
	if (!make_nodes) {
		if (good < 0)
			return 0;
		good_rp = &rends[good];
		/* if this succeeds, then a PPP process is already
		 * in charge of the link.
		 */
		rendnode = open(good_rp->path, O_WRONLY|O_NDELAY, 0);
		return (rendnode >= 0);
	}

	/* Lock to mostly fix race during creation of the pipe.
	 * Try only for a while, and then just give up and hope
	 * for the best.
	 */
	for (i = 0;  mmlock(REND_LOCK) && i < 5*4; i++) {
		log_debug(2,"","waiting to make rendezvous node");
		sginap(HZ/4);
	}

	/* Now that we have the lock, see if some other process
	 * was quicker and created the FIFO
	 */
	good = link_fifo(0);
	if (good < 0)
		good = 0;
	good_rp = &rends[good];

	/* Create a FIFO for this connection.
	 * If FIFO already exists, check to see the connection is
	 * already active.  If so, poke the existing daemon and quit.
	 * Open an existing name if there is one to avoid creating
	 * links that the existing daemon might object to.
	 */
	make_fifo(good);
	rendnode = open(good_rp->path, O_WRONLY|O_NDELAY, 0);
	if (rendnode >= 0) {
		/* No error means some other daemon is in charge.
		 * Give away the line or quit.
		 */
		rmlock(REND_LOCK2);
		return 1;
	}

	if (errno != ENXIO)
		bad_errno("open(O_WRONLY) ",good_rp->path);
	/* ENXIO means we are in charge.
	 *
	 * Re-open the FIFO for reading and writing while keeping it open.
	 */
	i = rendnode;
	rendnode = open(good_rp->path, O_RDWR | O_NDELAY, 0);
	if (rendnode < 0)
		bad_errno("open(O_RDWR) ", good_rp->path);
	(void)close(i);

	rmlock(REND_LOCK2);

	/* check again, to resolve the race getting the lock.
	 * While we were waiting for the lock
	 */
	(void)link_fifo(2);
	if (rendnode < 0) {
		log_complain("", "failed to reliably create rendezvous");
		unmade_rend();
		cleanup(1);
	}

	return 0;
}


/* common error exit */
void
cleanup(int code)
{
	struct dev *dp;
	pid_t pid;
	int st;

	for (dp = dp0; dp != 0; dp = dp->next)
		rel_dev(dp);

	rm_rend(0,0);
	if (rendnode >= 0) {
		(void)close(rendnode);
		rendnode = -1;
	}
	if (modfd >= 0) {
		(void)close(modfd);
		modfd = -1;
	}

	/* delete added route */
	if (del_route != 0)
		call_system("", "del_route: ", del_route, 1);

	/* optional ending script */
	if (end_cmd != 0)
		call_system("", "end_cmd: ", end_cmd, 1);

#ifdef PPP
	/* Delete proxy-ARP table entry. */
	proxy_unarp();
#endif /* PPP */

	/* wait for children */
	if ((pid = add_pid) > 0) {
		add_pid = -1;
		log_debug(7,"","kill pid %d", pid);
		(void)kill(pid, SIGTERM);
		(void)waitpid(pid,&st,0);
	}

	/* tell about any attempted calls */
	if (dp0->acct.attempts
	    && add_pid != 0)
		ck_acct(dp0,1);

	log_debug(1,"","exiting with %d", code);

	stderrfd = -1;
	stderrttyfd = -1;
	closefds();
	if (stderrpid > 0) {
		(void)waitpid(stderrpid,&st,0);
		stderrpid = -1;
	}
	exit(code);
}


/* get rid of signals */
void
no_signals(__sigret_t (*ksig)())
{
	(void)alarm(0);

	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, ksig);
	(void)signal(SIGTERM, ksig);
	(void)signal(SIGPIPE, ksig);
	(void)signal(SIGUSR1, SIG_IGN);
	(void)signal(SIGUSR2, SIG_IGN);
	(void)signal(SIGCHLD, SIG_DFL);
}


/* (probably) killed by resident daemon */
void
killed(int code)

{
	log_debug(1,"","killed by signal %d", code);
	exit(code);
}


/* clear accounting
 *	get_clk() must have been called recently.
 */
void
clr_acct(struct dev *dp)
{
	dp->acct.last_date = cur_date.tv_sec;
	dp->acct.last_secs = clk.tv_sec;
	dp->acct.call_start = 0;
	dp->acct.calls = 0;
	dp->acct.attempts = 0;
	dp->acct.failures = 0;
	dp->acct.answered = 0;
	dp->acct.min_setup = MAXINT;
	dp->acct.max_setup = 0;
	dp->acct.setup = 0;
	if (dp->acct.sconn == 0)
		dp->acct.sconn = clk.tv_sec;
	dp->acct.orig_conn = 0;
	dp->acct.orig_idle = 0;
	dp->acct.ans_conn = 0;
	dp->acct.ans_idle = 0;
}


static char*
hms(int secs)
{
#	define NUM_BUFS 10
	static int n;
	static struct {
		char c[11];		/* hhhh:mm:ss */
	} bufs[NUM_BUFS];
	char *p;

	p = bufs[n].c;
	if (++n >= NUM_BUFS)
		n = 0;
	if (secs < 0)
		secs = 0;
	sprintf(p, "%d:%02d:%02d",
		(secs/(60*60)) % 10000, (secs/60) % 60, secs % 60);
	return p;
}


/* account for phone line use
 */
void
ck_acct(struct dev *dp,
	int force)			/* 1=do it anyway */
{
	static char tbuf[] = "dayweek month dd hh:mm:ss ";
	static char tpat[] = "%a %b %e %T";
	struct tm lt;


	if (!demand_dial && (!force || !camping))
		return;

	get_clk();

	/* Report for the log once a day, after midnight and before 3 am.
	 * This minimizes problems with daylight savings time.
	 */
	if (!force) {
		if (cur_date.tv_sec < dp->acct.next_date)
			return;
		lt = *localtime(&cur_date.tv_sec);
		if (lt.tm_hour >= 3)
			return;
	}

	syslog(LOG_INFO, "%s: %d answered, %d links originated,"
	       " %d calls dialed, %d gave up link",
	       remote,
	       dp->acct.answered,
	       dp->acct.calls, dp->acct.attempts, dp->acct.failures);
	if (dp->acct.calls != 0)
		syslog(LOG_INFO, "%s: %d/%d/%d min/avg/max seconds call setup",
		       remote,
		       dp->acct.min_setup,
		       dp->acct.setup/dp->acct.calls,
		       dp->acct.max_setup);
	(void)cftime(tbuf,tpat,&dp->acct.last_date);
	syslog(LOG_INFO,"%s: calling %s,idle %s; answering %s,idle %s;"
	       " disconncted %s since %s",
	       remote,
	       hms(dp->acct.orig_conn), hms(dp->acct.orig_idle),
	       hms(dp->acct.ans_conn), hms(dp->acct.ans_idle),
	       hms(dp->acct.sconn - dp->acct.last_secs
		   - dp->acct.orig_conn-dp->acct.ans_conn),
	       tbuf);

	/* if not forcing things, reset the counters */
	if (!force) {
		clr_acct(dp);
		lt.tm_sec = 1;
		lt.tm_min = 1;
		lt.tm_hour = 0;
		dp->acct.next_date = mktime(&lt) + 24*60*60;
	}
}



/* get rid of stray controlling TTY
 *	We do not want control characters to give us signals.
 */
static void
stray_ctty(void)
{
	register int i;

	if (ctty <= 0) {
		i = open("/dev/tty", O_RDWR|O_NDELAY);
		if (i >= 0) {
			ioctl(i, TIOCNOTTY, (char *)0);
			(void)close(i);
		}
	}

}


/* stop being interactive
 */
void
no_interact(void)
{
	interact = 0;
	stderrfd = -1;
	stderrttyfd = -1;

	/* If not interactive, avoid controlling tty to avoid stray
	 * signals from the keyboard.
	 */
	ctty = 0;
	stray_ctty();

	(void)setsid();

	(void)chdir("/");
}



/* see if a file descriptor is important
 */
static int				/* 1=save it */
savefd(int fd)
{
	struct dev *dp;

	if (fd < 0)
		return 1;

	if (fd == stderrfd || fd == stderrttyfd
	    || fd == modfd || fd == rendnode
	    || fd == status_poke_fd)
		return 1;

	for (dp = dp0; dp != 0; dp = dp->next) {
		if (fd == dp->devfd || fd == dp->devfd_save)
			return 1;
	}
	return 0;
}



/* garbage collect stray file descriptors from UUCP and friends
 */
void
closefds(void)
{
	extern void _yp_unbind_all(void);
	extern void _res_close(void);
	int i;

	stray_ctty();

	/* Close all stray files and tell YP and DNS.
	 * The UUCP code likes to leave things open.
	 */
	for (i = getdtablehi(); --i > 2; ) {
		if (!savefd(i))
			(void)close(i);
	}
	_yp_unbind_all();		/* tell NIS its FD is dead */
	_res_close();			/* tell resolver its FD is dead */
	closelog();

	i = open(_PATH_DEVNULL, O_RDWR, 0);
	if (i >= 0) {
		if (!savefd(0))
			(void)dup2(i, 0);
		if (!savefd(1))
			(void)dup2(i, 1);
		if (!savefd(2))
			(void)dup2(i, 2);
		if (i > 2)
			(void)close(i);
	}

	openlog(pgmname, LOG_PID | LOG_ODELAY | LOG_NOWAIT, LOG_DAEMON);
}


char *
ascii_str(u_char *s,
	  int len)
{
	static char buf[200];
	char *p;
	u_char *s2, c;

	for (p = buf; len > 0 && p < &buf[sizeof(buf)-1]; len--) {
		c = *s++;
		if (c == '\0') {
			for (s2 = s+1; s2 < &s[len]; s2++) {
				if (*s2 != '\0')
					break;
			}
			if (s2 >= &s[len])
			    break;
		}

		if (c >= ' ' && c < 0x7f && c != '\\') {
			*p++ = c;
			continue;
		}
		*p++ = '\\';
		switch (c) {
		case '\\':
			*p++ = '\\';
			break;
		case '\n':
			*p++= 'n';
			break;
		case '\r':
			*p++= 'r';
			break;
		case '\t':
			*p++ = 't';
			break;
		case '\b':
			*p++ = 'b';
			break;
		default:
			p += sprintf(p,"%o",c);
			break;
		}
	}
	*p = '\0';
	return buf;
}


void
log_errno(char *s1, char *s2)
{
	int serrno = errno;

	kludge_stderr();
	(void)fprintf(stderr, "%s: %s%s: %s\n",
		      remote, s1,s2, strerror(serrno));
	(void)fflush(stderr);
	errno = serrno;
}


void
bad_errno(char *s1, char *s2)
{
	log_errno(s1,s2);
#ifdef DEBUG
	(void)fflush(stderr);
	abort();
#endif
	cleanup(1);
}


void
log_debug(int lvl, char* name, char *p, ...)
{
	va_list args;

	if (debug < lvl)
	    return;

	kludge_stderr();

	va_start(args, p);
	(void)fprintf(stderr, "%s%s: ", remote,name);
	(void)vfprintf(stderr, p, args);
	(void)putc('\n',stderr);
	(void)fflush(stderr);
	va_end(args);
}


void
log_cd(int sw, int lvl, char* name, char *p, ...)
{
	va_list args;

	if (!sw && debug < lvl)
	    return;

	va_start(args, p);
	kludge_stderr();
	(void)fprintf(stderr, "%s%s: ", remote,name);
	(void)vfprintf(stderr, p, args);
	(void)putc('\n',stderr);
	(void)fflush(stderr);
	va_end(args);
}


void
log_complain(char* name, char *p, ...)
{
	va_list args;

	va_start(args, p);
	kludge_stderr();
	(void)fprintf(stderr, "%s%s: ", remote,name);
	(void)vfprintf(stderr, p, args);
	(void)putc('\n',stderr);
	(void)fflush(stderr);
	va_end(args);
}


/* Touch the devices to make them appear active to `w`.
 */
void
act_devs(int weak,			/* 0=TCP/IP active, 1=IP active, 2=? */
	 int touch)			/* 1=set the mtime on the device */
{
	struct dev *dp;
	time_t t0, t, b, d;

	t0 = clk.tv_sec + ((weak >= 1) ? sactime : lactime);
	for (dp = dp0; dp != 0; dp = dp->next) {
		if (dp->line[0] == '\0')
			continue;
		t = t0;
		b = dp->acct.toll_bound;
		if (b > 0
		    && dp->acct.call_start != 0) {
			/* Round up connect-time to close to a multiple
			 * of the boundary.  Do not get too close to
			 * the boundary to avoid paying for an extra
			 * minute.
			 */
			d = b - ((t0 - dp->acct.call_start) % b);
			if (d > 1 && d < b-1)
				t += d;
		}
		if (weak <= 1)
			dp->active = t;
		else if (dp->active < t)
			dp->active = t;

		if (touch
		    && dp->touched+HEARTBEAT < get_clk()) {
			if (0 > utime(dp->line, 0))
				log_errno("touch() ", dp->line);
			dp->touched = clk.tv_sec;
		}
	}
}


/* finish with the device
 */
void
rel_dev(struct dev *dp)
{
	int st;

	if (modfd >= 0
	    && dp->dev_index != -1) {
		if (0 > ioctl(modfd,I_UNLINK,dp->dev_index)) /* unlink MUX */
			log_errno("I_UNLINK","");
		dp->devfd = dp->devfd_save;
	}
	dp->dev_index = -1;
	dp->devfd_save = -1;
	dp->devfd = -1;

	/* restore the owner and permissions of the device
	 */
	if (dp->dev_sbuf.st_ino != 0) {
		if (dp->dev_sbuf.st_uid != 0
		    && 0 > chown(dp->line, dp->dev_sbuf.st_uid,
				 dp->dev_sbuf.st_gid))
			log_errno("chown(restore) ", dp->line);
		if (0 > chmod(dp->line, dp->dev_sbuf.st_mode))
			log_errno("chmod(restore) ", dp->line);
		dp->dev_sbuf.st_ino = 0;
	}

	dp->line[0] = '\0';
	dp->nodename[0] = '\0';
	closefds();

	dologout(dp);
	if (dp->lockfile[0] != '\0') {
		rmlock(dp->lockfile);
		dp->lockfile[0] = '\0';
	}

	/* get rid of rendezvousing process */
	if (dp->rendpid > 0) {
		if (0 > kill(dp->rendpid, SIGINT)
		    && (errno != ESRCH || debug >= 1))
			log_errno("kill(rel_dev rendezvous)","");
		(void)waitpid(dp->rendpid,&st,0);
	}
	dp->rendpid = -1;

	if (dp->pty_pid > 0) {
		if (0 > kill(dp->pty_pid, SIGPIPE)
		    && errno != ESRCH)
			log_errno("kill(rel_dev pty_pid)","");
		(void)waitpid(dp->pty_pid,&st,0);
		dp->pty_pid = -1;
	}
}


/* connect to the other side
 */
int					/* 0=failed, 1=done, 2=rendezvous */
get_conn(struct dev *dp,
	 int rend_ok)			/* 1=try to rendezvous */
{
	char ebuf[40+2+80+1];
	fd_set in;
	int i, tries, delay;
	struct timeval timer;
	time_t start;

	/* Try not to keep the modem busy in case the other end
	 * is trying to call us.
	 */
	conn_trycalls = 1;
	/* reset the locking mechanisms
	 */
	lock_pid[0] = '\0';
	Nlocks = 0;

	get_clk();
	start = clk.tv_sec;
	if (Debug)
		kludge_stderr();

	tries = 0;
	delay = 0;
	for (;;) {
		tries++;
		dp->acct.attempts++;

		/* Use the UUCP dialing code. */
		dp->sync = dp->conf_sync;
		setservice(pgmname);
		dp->acct.call_start = clk.tv_sec;
		dp->devfd = conn(dp->uucp_nam);
		alarm(0);			/* in case UUCP forgets */
		(void)signal(SIGALRM, SIG_IGN);

		if (dp->devfd != FAIL) {
			(void)strncpy(dp->nodename,Dc,sizeof(dp->nodename));
			(void)sprintf(dp->line,"/dev/%s",dp->nodename);
			if (!lockname(dp->nodename,dp->lockfile,
				      sizeof(dp->lockfile))) {
				log_complain("",
					     "unable to make lock file name"
					     " for %s",
					     dp->nodename);
			}
			closefds();
			get_clk();
			dp->acct.sconn = clk.tv_sec;
			i = dp->acct.sconn - start;
			dp->acct.setup += i;
			if (i < dp->acct.min_setup
			    || dp->acct.min_setup == 0)
				dp->acct.min_setup = i;
			if (i > dp->acct.max_setup)
				dp->acct.max_setup = i;
			dp->acct.calls++;
			return 1;
		}

		dp->devfd = -1;
		rel_dev(dp);

		sprintf(ebuf, ((Uerror == SS_CHAT_FAILED
				&& AbortSeen < Aborts)
			       ? "%.40s--%.80s" : "%.40s"),
			UERRORTEXT, AbortStr[AbortSeen]);

		/* Give up if not trying hard and not interested in retrying,
		 */
		if (!camping && !demand_dial) {
			log_complain("","fatal error %s", ebuf);
			dp->acct.failures++;
			return 0;
		}
		/* or if the cause of failure is strange.
		 */
		if (Uerror != SS_DIAL_FAILED
		    && Uerror != SS_LOGIN_FAILED
		    && Uerror != SS_CANT_ACCESS_DEVICE
		    && Uerror != SS_CHAT_FAILED
		    && Uerror != SS_LOCKED_DEVICE) {
			log_complain("","fatal error %s on try #%d",
				     ebuf, tries);
			dp->acct.failures++;
			if (dp->modpause > 0)
				sginap(dp->modpause*HZ);
			return 0;
		}

		/* Random backoff in case the other machine
		 * is trying to call us at the same time.
		 * Quick delay if the modem is busy, to let getty
		 * either answer a call or finish initializing the modem,
		 * but exponentially increasing delay otherwise.
		 */
		timer.tv_sec = ((dp->modwait > 0)
				? dp->modwait
				: DEFAULT_ASYNC_MODWAIT);
		timer.tv_usec = 0;
		if (Uerror != SS_LOCKED_DEVICE) {
			i = rint(++delay * drand48());
			timer.tv_sec <<= i;
		}
		log_debug(demand_dial ? 1 : 0,
			  "","%s on try #%d, back off %d seconds",
			  ebuf,tries,timer.tv_sec);
		if (rendnode >= 0) {
			FD_ZERO(&in);
			FD_SET(rendnode, &in);
			switch (select(rendnode+1, &in,0,0,&timer)) {
			case 0:
				break;
			case 1:
				if (!rend_ok)
					return 2;
				if (rendezvous(1))
					return 2;
				break;
			default:
				if (errno != EINTR && errno != EAGAIN)
					bad_errno("select(rendnode) "
						  "in get_conn()","");
				break;
			}
		} else {
			sginap(timer.tv_sec*HZ);
		}
		if (tries > dp->modtries) {
			dp->acct.failures++;
			if (dp->modpause > 0)
				sginap(dp->modpause*HZ);
			return 0;
		}
	}
}
