
#include <stdio.h>
#include <sys/types.h>
#include <sys/tiuser.h>	/* for struct netbuf */
#include <netconfig.h>
#include <netdir.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>


/*
 * Match config entry for protocol requested.
 */
extern struct netconfig *
_s_match(
	register int			family,
	register int			type,
	register int			proto,
	void				**nethandle);

/*
 * This routine is the main routine it resolves host/service/xprt triples
 * into a bunch of netbufs that should connect you to that particular
 * service. RPC uses it to contact the binder service (rpcbind)
 */
extern int
netdir_getbyname(
	struct netconfig	*tp,	/* The network config entry	*/
	struct nd_hostserv	*serv,	/* the service, host pair	*/
	struct nd_addrlist	**addrs);/* the answer			*/

extern void netdir_perror( char	*s);

int
main( int argc, char ** argv )
{

	struct netconfig *   nc;
	void *               nh;
	int                  nderr;
	int                  nbufs;
	int                  i;
	struct nd_hostserv   ndhs;
	struct nd_addrlist * ndal;
	struct netbuf *      nbuf;

	if (argc != 3) {
		fprintf(stderr, "usage: %s hostname servicename\n", argv[0]);
		exit(1);
	}

	/* use libsocket's convenient lookup routine to get a netconfig */
	nc = _s_match( AF_INET, SOCK_STREAM, 0, &nh);
	if (nc == NULL) {
		fprintf(stderr, "%s: _s_match returned null\n", argv[0]);
		exit(2);
	}

	/* setup the nd_hostserv struct */
	ndhs.h_host = argv[1];	/* hostname */
	ndhs.h_serv = argv[2];	/* service name */

	nderr = netdir_getbyname( nc, &ndhs, &ndal );
	if (nderr) {
		fprintf(stderr, "%s: netdir_getbyname returned %d\n",
			argv[0], nderr);
		netdir_perror( argv[0]);
		exit(3);
	}
	/* examine the results */
	nbufs = ndal->n_cnt;
	nbuf  = ndal->n_addrs;

	printf( "%s: netdir_getbyname found %d answers:\n", argv[0], nbufs);
	for (i = 0; i < nbufs; ++i ) {
		struct sockaddr_in *sin = (struct sockaddr_in *)nbuf->buf;
		printf("length: %d,\tfamily: %d,\tport: %d,\taddress: %s\n",
			nbuf->len, sin->sin_family, sin->sin_port,
			inet_ntoa(sin->sin_addr));
	}

	exit(0);
}
