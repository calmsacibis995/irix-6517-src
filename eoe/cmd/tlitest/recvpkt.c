#include <stdio.h>
#include <fcntl.h>
#include <tiuser.h>
#include <string.h>
#include <sys/endian.h>	/* for [hn]to[nh][sl] */
#include <sys/socket.h> /* for AF_INET */
#include <netinet/in.h>	/* for sockaddr_in & INADDR_* */
#include <arpa/inet.h>	/* for inet_ntoa */
#include "tlitest.h"

void
query( struct t_unitdata *ud)
{

	printf("Unitdata Received: address size = %d, opt size = %d, data size = %d\n",
		ud->addr.len, ud->opt.len, ud->udata.len);
	if (ud->addr.len != 0) {
		struct sockaddr_in	*sin;

		if (ud->addr.len < (sizeof *sin - sizeof sin->sin_zero)) {
			fprintf(stderr, "reckpkt: Invalid address length received\n");
			return;
		}
		sin = (struct sockaddr_in *)ud->addr.buf;
		printf("\tFrom: %s:%d (family:%d)\n",
			inet_ntoa(sin->sin_addr),
			ntohs(sin->sin_port),
			ntohs(sin->sin_family));
	}
	if (ud->opt.len != 0) {
		printbuf("\tOptions: ", ud->opt.buf, ud->opt.len);
	}
	if (ud->udata.len != 0) {
		printbuf("\tData: ", ud->udata.buf, ud->udata.len);
	}
}


int
main( int argc, char ** argv)
{
	struct t_bind 		*bind;
	struct t_unitdata 	*ud;
	struct t_uderr 		*uderr;
	struct sockaddr_in	*sin;
	int	fd;

	extern int t_errno;

	if ((fd = t_open("/dev/udp", O_RDWR, NULL)) < 0) {
		t_error("recvpkt: unable to open /dev/udp");
		exit(1);
	}
	if ((bind = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR)) == NULL) {
		t_error("recvpkt: t_alloc of t_bind failed");
		exit(2);
	}
	printf("reckpkt: bind maxlen = %d, len = %d, buf = 0x%08x\n",
		bind->addr.maxlen, bind->addr.len, bind->addr.buf);
	if (bind->addr.maxlen < sizeof *sin) {
		fprintf( stderr, "recvpkt: t_alloc bind buffer too small\n");
		exit(100);
	}
	bind->addr.maxlen = sizeof *sin;
	bind->addr.len    = sizeof *sin;
	bind->qlen = 0;

	sin = (struct sockaddr_in *)bind->addr.buf;
	sin->sin_family      = htons(AF_INET);
	sin->sin_port        = htons(DGRAMTESTPORT);
	sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (t_bind(fd, bind, bind) < 0) {
		t_error("recvpkt: t_bind failed");
		exit(3);
	}
	if ( sin->sin_family      != htons(AF_INET) 
	||   sin->sin_port        != htons(DGRAMTESTPORT)
	||   sin->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
		fprintf(stderr, "recvpkt: t_bind bound wrong address\n");
		exit(4);
	}

	if ((ud = (struct t_unitdata *)t_alloc(fd, T_UNITDATA, T_ALL)) == NULL) 
	{
		t_error("recvpkt: t_alloc of t_unitdata structure failed");
		exit(5);
	}
	printf("recvpkt: unitdata maxlen = %d, len = %d, buf = 0x%08x\n",
		ud->addr.maxlen, ud->addr.len, ud->addr.buf);

	if ((uderr = (struct t_uderr *)t_alloc(fd, T_UDERROR, T_ALL)) == NULL) {
		t_error("recvpkt: t_alloc of t_uderr structure failed");
		exit(6);
	}
	while (1) {
		int	flags;

		if (t_rcvudata(fd, ud, &flags) < 0) {
			if (t_errno == TLOOK) {
				if (t_rcvuderr(fd, uderr) < 0) {
					t_error("recvpkt: t_rcvuderr failed");
					exit(7);
				}
				fprintf(stderr, "recvpkt bad datagram, error = %d\n",
					uderr->error);
				continue;
			}
			t_error("recvpkt: t_rcvudata failed");
			exit(8);
		}
		query(ud);
	}

}
