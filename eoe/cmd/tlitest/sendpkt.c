#include <stdio.h>
#include <fcntl.h>
#include <tiuser.h>
#include <string.h>
#include <unistd.h>	/* for sleep() */
#include <sys/endian.h>	/* for [hn]to[nh][sl] */
#include <sys/socket.h> /* for AF_INET */
#include <netinet/in.h>	/* for sockaddr_in & INADDR_* */
#include <arpa/inet.h>	/* for inet_ntoa */
#include "tlitest.h"


int
main( int argc, char ** argv)
{
	struct t_bind 		*bind;
	struct t_unitdata 	*ud;
	struct t_uderr 		*uderr;
	struct sockaddr_in	*sin;
	int	fd;
	int	pktno = 0;

	extern int t_errno;

	if ((fd = t_open("/dev/udp", O_RDWR, NULL)) < 0) {
		t_error("sendpkt: unable to open /dev/udp");
		exit(1);
	}
	if ((bind = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR)) == NULL) {
		t_error("sendpkt: t_alloc of t_bind failed");
		exit(2);
	}
	printf("sendpkt: bind maxlen = %d, len = %d, buf = 0x%08x\n",
		bind->addr.maxlen, bind->addr.len, bind->addr.buf);
	if (bind->addr.maxlen < sizeof *sin) {
		fprintf(stderr, "sendpkt: t_alloc bind buffer too small\n");
		exit(100);
	}
	bind->addr.maxlen = sizeof *sin;
	bind->addr.len    = sizeof *sin;
	bind->qlen = 0;

	sin = (struct sockaddr_in *)bind->addr.buf;
	sin->sin_family      = htons(AF_INET);
	sin->sin_port        = htons(0);
	sin->sin_addr.s_addr = htonl(INADDR_ANY);

	if (t_bind(fd, bind, bind) < 0) {
		t_error("sendpkt: t_bind failed");
		exit(3);
	}
	/* Don't care what address we were bound to */

	if ((ud = (struct t_unitdata *)t_alloc(fd, T_UNITDATA, T_ALL)) == NULL) 
	{
		t_error("sendpkt: t_alloc of t_unitdata structure failed");
		exit(5);
	}
	printf("sendpkt: unitdata maxlen = %d, len = %d, buf = 0x%08x\n",
		ud->addr.maxlen, ud->addr.len, ud->addr.buf);
	if (ud->addr.maxlen < sizeof *sin) {
		fprintf( stderr, "sendpkt: t_alloc unitdata buffer too small\n");
		exit(100);
	}
	ud->addr.len = sizeof *sin;

	sin = (struct sockaddr_in *)ud->addr.buf;
	sin->sin_family      = htons(AF_INET);
	sin->sin_port        = htons(DGRAMTESTPORT);
	sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	/* setup data */
	ud->opt.len = 0;	/* no options */

	if ((uderr = (struct t_uderr *)t_alloc(fd, T_UDERROR, T_ALL)) == NULL) {
		t_error("sendpkt: t_alloc of t_uderr structure failed");
		exit(6);
	}

	for (pktno = 0; ++pktno < 100;) {
		int	flags;
		static char udbuf[100];

		sprintf(udbuf, 
	"%d: 0123456789ABCDEFGHIJKLMNOPQRSWTUVWXYZabcdefghijklmnopqrstuvwxyz",
			pktno);
		ud->udata.buf = udbuf;
		ud->udata.len = 64;

		if (t_sndudata(fd, ud, 0) < 0) {
			t_error("sendpkt: t_sndudata failed");
			exit(9);
		}
		sleep(1);
	}

}
