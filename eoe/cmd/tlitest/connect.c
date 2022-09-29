#include <stdio.h>
#include <fcntl.h>
#include <tiuser.h>
#include <string.h>
#include <sys/endian.h>	/* for [hn]to[nh][sl] */
#include <sys/socket.h> /* for AF_INET */
#include <netinet/in.h>	/* for sockaddr_in & INADDR_* */
#include <arpa/inet.h>	/* for inet_ntoa */
#include "tlitest.h"

#define DISCONNECT -1

extern int t_errno;

void
do_service( int fd)
{
	int  segmentno;
	int  nbytes;
	int  flags = 0;
	char buf[1024];

	for (segmentno = 0; ++segmentno < 100;) {

		sprintf(buf, 
	"%d: 0123456789ABCDEFGHIJKLMNOPQRSWTUVWXYZabcdefghijklmnopqrstuvwxyz",
			segmentno);

		flags = 0;
		nbytes = t_snd(fd, buf, 64, &flags);
		if (nbytes < 0) {
			if (t_errno == TLOOK) {
				int  terr;

				if (T_ORDREL != (terr = t_look(fd))) {
					fprintf(stderr, "connect: t_look returned %d\n",
						terr);
					t_error("connect: t_rcv failed");
					exit(6);
				}
				fprintf(stderr, "T_ORDREL received \n");
				break;
			}
			t_error("connect: t_snd failed");
			exit(9);
		} else if (nbytes != 64) {
			fprintf(stderr, "connect: t_send returned %d\n", nbytes);
		}
		sleep(1);
	}
	if (t_sndrel(fd) < 0) {
		t_error("connect: t_sndrel failed");
		exit(10);
	}
	if (t_rcvrel(fd) < 0) {
		t_error("connect: t_rcvrel failed");
		exit(11);
	}
}



int
main( int argc, char ** argv)
{
	struct t_bind 		*bind;
	struct t_call 	*sndcall;
	struct sockaddr_in	*sin;
	int	fd;

	if ((fd = t_open("/dev/tcp", O_RDWR, NULL)) < 0) {
		t_error("connect: unable to open /dev/tcp");
		exit(1);
	}

	if ((bind = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR)) == NULL) {
		t_error("connect: t_alloc of t_bind failed");
		exit(2);
	}
	printf("connect: bind maxlen = %d, len = %d, buf = 0x%08x\n",
		bind->addr.maxlen, bind->addr.len, bind->addr.buf);
	if (bind->addr.maxlen < sizeof *sin) {
		fprintf( stderr, "connect: t_alloc bind buffer too small\n");
		exit(100);
	}
	bind->addr.maxlen = sizeof *sin;
	bind->addr.len    = sizeof *sin;
	bind->qlen = 0;

#ifdef oldway
	if (t_bind(fd, NULL, bind) < 0) {
		t_error("connect: t_bind failed");
		exit(2);
	}
	sin = (struct sockaddr_in *)bind->addr.buf;
	printf("connect: Bound to: %s:%d (family:%d)\n",
		inet_ntoa(sin->sin_addr),
		ntohs(sin->sin_port),
		ntohs(sin->sin_family));
#else
	if (t_bind(fd, NULL, NULL) < 0) {
		t_error("connect: t_bind failed");
		exit(2);
	}
#endif

	if ((sndcall = (struct t_call *)t_alloc(fd, T_CALL, T_ADDR)) == NULL) {
		t_error("connect: t_alloc of t_call structure failed");
		exit(3);
	}
	printf("connect: sndcall maxlen = %d, len = %d, buf = 0x%08x\n",
		sndcall->addr.maxlen, sndcall->addr.len, sndcall->addr.buf);

	sndcall->addr.len    = sizeof *sin;
	sin = (struct sockaddr_in *)sndcall->addr.buf;
	sin->sin_family      = htons(AF_INET);
	sin->sin_port        = htons(STREAMTESTPORT);
	sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (t_connect(fd, sndcall, NULL) < 0) {
		if (t_errno == TLOOK) {
			int  terr;

			if (-1 != (terr = t_look(fd))) {
				fprintf(stderr, 
				"connect: t_connect returned T_LOOK (0x%04x)\n",
					terr);
				print_tlook(terr);
				exit(5);
			} 
		}
		t_error("connect: t_connect failed");
		exit(5);
	}

	/* Data transfer phase */
	do_service(fd);
	return 0;
}
