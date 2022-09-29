#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>


main(int argc, char **argv)
{
	struct netent *n;
	struct in_addr a;
	long net;
	int i;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (inet_aton(*argv, &a)) {
			net = inet_netof(a);
			n = getnetbyaddr(net, AF_INET);
			if (! n) {
				fprintf(stderr, "getnetbyaddr failed for [%s]\n", 					*argv);
			} else {
				a = inet_makeaddr(n->n_net, 0);
				printf("name = %s, addrtype = %d, net = %s\n",
				    n->n_name, n->n_addrtype, inet_ntoa(a));
				for (i = 0; n->n_aliases[i]; i++) {
					printf("\talias[%d]: %s\n", i,
					    n->n_aliases[i]);
				}
			}
		}
	}
}
