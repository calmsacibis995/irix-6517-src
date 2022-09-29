/*
 *	@(#)	yp_host.c	initial release
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>

#define INSERT	1
#define CHANGE	2
#define DELETE	3

extern char	*optarg;
extern int	optind, opterr;
extern char	*malloc();
extern char	*strrchr();
extern char	*getpass();
extern char	*inet_ntoa();

void usage(void);

main(argc, argv)
int	argc;
char	**argv;
{
	int	c, fun, hostid;
	char	*host = 0, *net = 0, *net_mask = 0, *new_name = 0;
	char	*alias = 0, *paswd = 0, *tmp;
	int	err, s;
	struct in_addr	inaddr;
	struct ifreq	netmask;

	while( (c = getopt(argc, argv, "rcdh:n:m:a:w:")) != -1 ) {
		switch (c) {
		case 'r':
			fun = INSERT;
			break;
		case 'c':
			fun = CHANGE;
			break;
		case 'd':
			fun = DELETE;
			break;
		case 'h':
			host = malloc(strlen(optarg) + 1);
			strcpy(host, optarg);
			break;
		case 'n':
			net = malloc(strlen(optarg) + 1);
			strcpy(net, optarg);
			break;
		case 'm':
			net_mask = malloc(strlen(optarg) + 1);
			strcpy(net_mask, optarg);
			break;
		case 'a':
			alias = malloc(strlen(optarg) + 1);
			strcpy(alias, optarg);
			break;
		case 'w':
			new_name = malloc(strlen(optarg) + 1);
			strcpy(new_name , optarg);
			break;
		case '?':
			usage();
		}
	}

	if ( host == 0 || fun == 0 )
		usage();

	if ( fun == CHANGE && new_name == 0 )
		usage();

	if ( fun == INSERT && net == 0 ) {
		err = -1;
		s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		strcpy(netmask.ifr_name, "ec0");
		if ( (err = ioctl(s, SIOCGIFNETMASK, &netmask)) < 0 ) {
			strcpy(netmask.ifr_name, "enp0");
			if ( (err = ioctl(s, SIOCGIFNETMASK, &netmask)) < 0 ) {
				strcpy(netmask.ifr_name, "et0");
				err = ioctl(s, SIOCGIFNETMASK, &netmask);
			}
		}

		hostid = gethostid();
		tmp = inet_ntoa(hostid);
		net = malloc(strlen(tmp) + 1);
		strcpy(net, tmp);
		if ( err < 0 ) {
			tmp = strrchr(net, '.');
			*tmp = 0;
			net_mask = 0;
		}
		else {
			hostid = ((struct sockaddr_in *)&netmask.ifr_addr)->
				sin_addr.s_addr;
			tmp = inet_ntoa(hostid);
			net_mask = malloc(strlen(tmp) + 1);
			strcpy(net_mask, tmp);
		}
	}

	if ( fun != INSERT )
		paswd = getpass("Enter root password of ypmaster:");

	switch (fun) {
	case INSERT:
		err = registerinethost(host, net, net_mask, &inaddr, alias);
		if ( err )
			printf("Host register error, %d\n", err);
		else
			printf("New ip addr = %s\n", inet_ntoa(inaddr));
		break;
	case CHANGE:
		err = renamehost(host, new_name, alias, paswd);
		if ( err )
			printf("Host entry change error, %d\n", err);
		break;
	case DELETE:
		err = unregisterhost(host, paswd);
		if ( err )
			printf("Host unregister error, %d\n", err);
		break;
	}

	exit(err);
}


void
usage(void)
{
	printf("yp_host -r -h host [-n net [-m mask]] [-a aliases]\n");
	printf("        -c -h host -w newname [-a aliases]\n");
	printf("        -d -h host\n");
	exit(0);
}
