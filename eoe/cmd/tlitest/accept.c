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
do_service( int conn_fd)
{
	int  nbytes;
	int  flags = 0;
	char buf[1024];

	while (-1 != (nbytes = t_rcv( conn_fd, buf, sizeof buf, &flags))) {
		printbuf("Data:", buf, nbytes);
	}
	if (t_errno == TLOOK) {
		int  terr;

		if (T_ORDREL != (terr = t_look(conn_fd))) {
			fprintf(stderr, "accept: t_look returned %d\n", terr);
			t_error("accept: t_rcv failed");
			exit(23);
		}
		if (t_rcvrel(conn_fd) < 0) {
			t_error("accept: t_rcvrel failed");
			exit(24);
		}
		if (t_sndrel(conn_fd) < 0) {
			t_error("accept: t_sndrel failed");
			exit(25);
		}
		exit(0);
	}
	t_error("accept: t_rcv failed");
	exit(27);
}

void
run_server( int listen_fd, int conn_fd)
{
	int pid;

	pid = fork();
	
	if (pid < 0) {
		perror("fork failed");
		exit(20);
	}

	if (pid > 0) {	/* parent */
		if (t_close(conn_fd) < 0) {
			t_error("accept: t_close failed for parent");
			exit(21);
		}
		return;
	}

	/* child */
	if (t_close(listen_fd) < 0) {
		t_error("accept: t_close failed for child");
		exit(22);
	}
	do_service(conn_fd);
}

int
accept_call( int listen_fd, struct t_call *call)
{
	int conn_fd;

	if ((conn_fd = t_open("/dev/tcp", O_RDWR, NULL)) < 0) {
		t_error("accept: unable to open /dev/tcp for accept");
		exit(7);
	}
	if (t_bind(conn_fd, NULL, NULL) < 0) {
		t_error("accept: t_bind failed for conn_fd");
		exit(8);
	}
	if (t_accept(listen_fd, conn_fd, call) < 0) {
		if (t_errno == TLOOK) {	/* must be a disconnect? */
			int  terr;

			if (T_ORDREL != (terr = t_look(conn_fd))) {
				fprintf(stderr, 
					"accept: t_look returned %d\n",
					terr);
				t_error("accept: t_accept failed(85)");
				exit(85);
			}
			t_error("accept: t_accept failed(9)");
			if (t_rcvdis(listen_fd, NULL) < 0) {
				t_error("accept: t_rcvdis failed");
				exit(9);
			}
			if (t_close(conn_fd) < 0) {
				t_error("accept: t_close failed for conn_fd");
				exit(10);
			}
			return DISCONNECT;
		}
		t_error("accept: t_accept failed(11)");
		exit(11);
	}
	return conn_fd;
}

int
main( int argc, char ** argv)
{
	struct t_bind 		*bind;
	struct t_call 	*call;
	struct sockaddr_in	*sin;
	int	listen_fd;

	if ((listen_fd = t_open("/dev/tcp", O_RDWR, NULL)) < 0) {
		t_error("accept: unable to open /dev/tcp");
		exit(1);
	}

	if ((bind = (struct t_bind *)t_alloc(listen_fd, T_BIND, T_ADDR)) == NULL) {
		t_error("accept: t_alloc of t_bind failed");
		exit(2);
	}
	printf("accept: bind maxlen = %d, len = %d, buf = 0x%08x\n",
		bind->addr.maxlen, bind->addr.len, bind->addr.buf);
	if (bind->addr.maxlen < sizeof *sin) {
		fprintf( stderr, "accept: t_alloc bind buffer too small\n");
		exit(100);
	}
	bind->addr.maxlen = sizeof *sin;
	bind->addr.len    = sizeof *sin;
	bind->qlen = 1;

	sin = (struct sockaddr_in *)bind->addr.buf;
	sin->sin_family      = htons(AF_INET);
	sin->sin_port        = htons(STREAMTESTPORT);
	sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (t_bind(listen_fd, bind, bind) < 0) {
		t_error("accept: t_bind failed");
		exit(3);
	}
	if ( sin->sin_family      != htons(AF_INET) 
	||   sin->sin_port        != htons(STREAMTESTPORT)
	||   sin->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
		fprintf(stderr, "accept: t_bind bound wrong address\n");
		exit(4);
	}

	if ((call = (struct t_call *)t_alloc(listen_fd, T_CALL, T_ALL)) == NULL) {
		t_error("accept: t_alloc of t_call structure failed");
		exit(5);
	}
	printf("accept: call maxlen = %d, len = %d, buf = 0x%08x\n",
		call->addr.maxlen, call->addr.len, call->addr.buf);

	while (1) {
		int conn_fd;

		if (t_listen(listen_fd, call) < 0) {
			t_error("accept: t_listen failed");
			exit(6);
		}
		printf("accept: t_listen returned\n");
		if ((conn_fd = accept_call(listen_fd, call)) != DISCONNECT) {
			run_server(listen_fd, conn_fd);
		}
	}

}
