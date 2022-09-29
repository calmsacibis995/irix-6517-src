#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#if defined(_SUNOS) && defined(SVR4)
#include <sys/t_lock.h>
#endif/* _SUNOS */
#include <sys/flock.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <rpc/rpc.h>
#include <rpcsvc/sm_inter.h>
#include <sys/param.h>
#include <string.h>

struct stat_chge {
	char *name;
	int state;
};
typedef struct stat_chge stat_chge;

#define SM_NOTIFY 6

char *typetostr[] = {
	"null",
	"F_RDLCK",
	"F_WRLCK",
	"F_UNLCK",
};

enum option_type {REMOTE, LOCAL, NONE};
enum sysid_format {SYSID_HOSTNAME, SYSID_IPADDRESS, SYSID_NUMERIC, SYSID_NONE};

struct flock Test_fl = {
	(short)F_WRLCK,							/* l_type */
	(short)0,								/* l_whence */
	(off_t)0,								/* l_start */
	(off_t)0,								/* l_len */
	(long)0,								/* l_sysid */
	(pid_t)0,								/* l_pid */
	{(long)0, (long)0, (long)0, (long)0}	/* l_pad[4] */
};

void
usage(char *progname)
{
	fprintf(stderr, "usage: %s [-l sysid,pid] [-r client] file|server\n",
		progname);
}

int
is_numeric_string(char *token)
{
	int state = 0;

	for ( ; *token; token++) {
		switch (state) {
			case 0:
				if (*token == '0') {
					state = 2;
				} else if (isdigit(*token) || (*token == '+') ||
					(*token == '-')) {
						state = 1;
				} else {
					return(0);
				}
				break;
			case 1:
				if (!isdigit(*token)) {
					return(0);
				}
				break;
			case 2:
				if (isdigit(*token) || (*token == 'x') || (*token == 'X')) {
					state = 1;
				} else {
					return(0);
				}
				break;
			case 3:
				return(0);
			default:
				fprintf(stderr, "invalid state %d in is_numeric_string\n",
					state);
				assert((state > 0) && (state < 4));
		}
	}
	assert((state == 1) || (state == 2));
	return(1);
}

int
is_ip_address(char *token)
{
	int state = 0;
	int dots = 0;

	for ( ; *token; token++) {
		switch (state) {
			case 0:
				if (isdigit(*token)) {
					state = 1;
				} else {
					return(0);
				}
				break;
			case 1:
				if (isdigit(*token)) {
					state = 2;
				} else if (*token == '.') {
					state = 4;
					dots++;
				} else {
					return(0);
				}
				break;
			case 2:
				if (isdigit(*token)) {
					state = 3;
				} else if (*token == '.') {
					state = 4;
					dots++;
				} else {
					return(0);
				}
				break;
			case 3:
				if (*token == '.') {
					state = 4;
					dots++;
				} else {
					return(0);
				}
				break;
			case 4:
				if (isdigit(*token)) {
					state = 2;
				} else {
					return(0);
				}
				break;
			case 5:
				return(0);
			default:
				fprintf(stderr, "invalid state %d in is_ip_address\n", state);
				assert((state > 0) && (state < 4));
		}
		if (dots > 3) {
			return(0);
		}
	}
	return(1);
}

enum sysid_format
get_sysid_format(char *token)
{
	enum sysid_format format = SYSID_HOSTNAME;

	if (is_numeric_string(token)) {
		format = SYSID_NUMERIC;
	} else if (is_ip_address(token)) {
		format = SYSID_IPADDRESS;
	}
	return(format);
}

xdr_notify(xdrs, ntfp)
	XDR *xdrs;
	stat_chge *ntfp;
{
	if (!xdr_string(xdrs, &ntfp->name, MAXHOSTNAMELEN+1)) {
		return(FALSE);
	}
	if (!xdr_int(xdrs, &ntfp->state)) {
		return(FALSE);
	}
	return(TRUE);
}

int
main(int argc, char **argv)
{
	struct in_addr ipaddr;
	char *token;
	char errbuf[256];
	stat_chge ntf;
	enum clnt_stat clnt_stat;
	int pid;
	long sysid;
	int error;
	char *progname = *argv;
	char *file = NULL;
	char *host = NULL;
	int fd;
	struct flock fl;
	struct flock temp_fl;
	struct hostent *hp;
	enum option_type option = NONE;
	char *client_name;
	int opt;

	while ((opt = getopt(argc, argv, "l:r:")) != EOF) {
		switch (opt) {
			case 'l':
				if (option != NONE) {
					fprintf(stderr, "%s: only one of -l or -r may be used\n",
						progname);
					exit(-1);
				}
				option = LOCAL;
				token = strtok(optarg, ", ");
				if (!token) {
					usage(progname);
					exit(-1);
				}
				switch (get_sysid_format(token)) {
					case SYSID_HOSTNAME:
						/*
						 * get the IP address of the host and use that as
						 * the sysid
						 */
						hp = gethostbyname(token);
						if (!hp) {
#if defined(_SUNOS)
							fprintf(stderr, "Host %s unknown\n", token);
#else /* _SUNOS */
							herror(token);
#endif /* _SUNOS */
							exit(-1);
						}
						switch (hp->h_addrtype) {
							case AF_INET:
								sysid = ((struct in_addr *)hp->h_addr)->s_addr;
								break;
							default:
								fprintf(stderr,
									"%s: invalid address type %d for host %s\n",
									progname, hp->h_addrtype, token);
								exit(-1);
						}
						break;
					case SYSID_IPADDRESS:
						sysid = inet_addr(token);
						break;
					case SYSID_NUMERIC:
						sysid = strtol(token, NULL, 0);
						break;
					default:
						fprintf(stderr, "%s: sysid must be a host name, "
							"IP address, or number\n", progname);
						exit(-1);
				}
				token = strtok(NULL, " ");
				if (token) {
					pid = IGN_PID;
				} else {
					pid = atoi(token);
				}
				break;
			case 'r':
				if (option != NONE) {
					fprintf(stderr, "%s: only one of -l or -r may be used\n",
						progname);
					exit(-1);
				}
				client_name = optarg;
				option = REMOTE;
				break;
			default:
				usage(progname);
				exit(-1);
		}
	}
	if (optind >= argc) {
		usage(progname);
		exit(-1);
	}
	switch (option) {
		case LOCAL:
			/*
			 * release locks locally for the specified file, sysid, and
			 * process id
			 */
			file = argv[optind];
			fd = open(file, O_RDWR);
			if (fd == -1) {
				error = errno;
				perror(file);
			} else {
				bzero(&fl, sizeof(fl));
				fl.l_sysid = sysid;
				fl.l_type = F_UNLCK;
				fl.l_pid = pid;
				if (fcntl(fd, F_RSETLK, &fl) == -1) {
					error = errno;
					perror("Unlock");
				} else {
					fl.l_type = F_WRLCK;
					fl.l_pid = 0;
					if (fcntl(fd, F_RGETLK, &fl) == -1) {
						error = errno;
						perror("Verification");
					} else {
						if (fl.l_type != F_UNLCK) {
							error = EBUSY;
							fprintf(stderr, "%s locked: %s %ld %ld %ld %ld\n",
								file, typetostr[fl.l_type], fl.l_start,
								fl.l_len, fl.l_sysid, (long)fl.l_pid);
						}
					}
				}
				close(fd);
			}
			break;
		case REMOTE:
			/*
			 * call the remote statd to release locks for the specified
			 * client name
			 */
			ntf.name = client_name;
			ntf.state = 0;
			clnt_stat = callrpc(argv[optind], SM_PROG, SM_VERS, SM_NOTIFY,
				xdr_notify, (void *)&ntf, xdr_void, NULL);
			if (clnt_stat != RPC_SUCCESS) {
				fprintf(stderr, "SM_NOTIFY %s for %s: %s\n", argv[optind],
					client_name, clnt_sperrno(clnt_stat));
			}
			break;
		default:
			/*
			 * release everything
			 */
			file = argv[optind];
			fd = open(file, O_RDWR);
			if (fd == -1) {
				error = errno;
				perror(file);
			} else {
				/*
				 * loop until we get an error, a lock fails to be released,
				 * or no more locks are held for the file
				 */
				for ( fl = Test_fl;
					!fcntl(fd, F_RGETLK, &fl) && (fl.l_type != F_UNLCK);
					fl = Test_fl ) {
						/*
						 * fl contains the description of the lock to be
						 * released, change its type to F_UNLCK and do
						 * F_RSETLK
						 */
						fl.l_type = F_UNLCK;
						if (fcntl(fd, F_RSETLK, &fl)) {
							error = errno;
							ipaddr.s_addr = fl.l_sysid;
							sprintf(errbuf, "%s F_RSETLK %lld %lld %s %d",
								file, fl.l_start, fl.l_len,
								fl.l_sysid ? inet_ntoa(ipaddr) : "0",
								fl.l_pid);
							perror(errbuf);
							break;
						} else {
							/*
							 * verify that the lock has been released
							 * there should be no identical lock held
							 */
							temp_fl = fl;
							if (fcntl(fd, F_RGETLK, &temp_fl)) {
								error = errno;
								ipaddr.s_addr = fl.l_sysid;
								sprintf(errbuf, "%s F_RGETLK %lld %lld %s %d",
									file, fl.l_start, fl.l_len,
									fl.l_sysid ? inet_ntoa(ipaddr) : "0",
									fl.l_pid);
								perror(errbuf);
								break;
							} else if (bcmp(&fl, &temp_fl) == 0) {
								ipaddr.s_addr = fl.l_sysid;
								fprintf(stderr,
									"%s: lock %s %lld %lld %s %d still held\n",
									file, typetostr[fl.l_type], fl.l_start,
									fl.l_len,
									fl.l_sysid ? inet_ntoa(ipaddr) : "0",
									fl.l_pid);
								error = -1;
								break;
							}
						}
				}
				close(fd);
			}
	}
	return(error);
}
