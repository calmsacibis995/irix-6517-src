/* $Id: udgping.c,v 1.1 1996/10/11 20:26:58 sca Exp $ */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <stdio.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
  
char	buf[90];
void
usage(void)
{
	fprintf(stderr, "usage: udgping -p port [-C count] [-n] [-c]\n");
	exit(1);
}

void
error(char *s)
{
	perror(s);
	exit(1);
}

main(int argc, char **argv)
{
	int	s, r, sunlen;
	struct	sockaddr_un sun, sun2;
	char 	*port = "/tmp/9999";
	int	on = 1;
	int 	i = 0;
	extern	int optind;
	extern	char *optarg;
	int	c;
	int	conn = 0;
	int	nonblock = 0;
	int	count = 0x7fffffff;

	while ((c = getopt(argc, argv, "p:cnC:")) != EOF) {
		switch (c) {
		case 'c':
			conn = 1;
			break;
		case 'n':
			nonblock = 1;
			break;
		case 'p':
			port = optarg;
			break;
		case 'C':
			count = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc) {
		usage();
	}
	memset((char *)&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	memcpy(sun.sun_path, port, strlen(port));

	s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s < 0)
		error("socket");
	if (conn) {
		r = connect(s, &sun, (size_t)(sizeof(sun.sun_family) +
			strlen(port) + 1));
		if (r < 0)
			error("connect");
	}

	sunlen = sizeof(sun2);
	if (nonblock)
		ioctl(s, FIONBIO, &on);
	while (count--) {
		sprintf(buf, "line %d\n",i++);
		if (conn) {
			r = send(s, buf, strlen(buf), 0);
		} else {
			r = sendto(s, buf, strlen(buf), 0, &sun, 
				(size_t)(sizeof(sun.sun_family) + 
					strlen(port) + 1));
		}
		if (r < 0 && errno != ENOENT)
			error("send");
		if (conn) {
			r = recv(s, buf, (size_t)sizeof(buf), 0);
		} else {
			r = recvfrom(s, buf, (size_t)sizeof(buf), 0, &sun2, &sunlen);
		}
		if (r < 0 && errno != EWOULDBLOCK)
			error("recv");
		if (errno == EWOULDBLOCK)
			continue;
		write(1, buf, r);
	}
	exit(0);
	/* NOTREACHED */
}
