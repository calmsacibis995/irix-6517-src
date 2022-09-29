#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>



main(int argc, char **argv)
{
	struct netent *n, *n2, retn, retn2;
	struct in_addr a;
	int i;
	char buf[4096],b2[4096], b3[4096];

	while (n = getnetent()) {
		a = inet_makeaddr(n->n_net, 0);
		printf("name = %s, addrtype = %d, net = %s\n",
		    n->n_name, n->n_addrtype, inet_ntoa(a));
		for (i = 0; n->n_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, n->n_aliases[i]);
		}
		if (! getnetbyname_r(n->n_name, &retn, b2, sizeof(b2))) {
			fprintf(stderr, "failed getnetbyname_r: %s\n", n->n_name);
		}
		if ( ! getnetbyaddr_r(n->n_net, AF_INET, &retn2, b3, sizeof(b3))) {
			fprintf(stderr, "failed getnetbyaddr_r: %s\n",
			    inet_ntoa(a));
		}
	}
}
