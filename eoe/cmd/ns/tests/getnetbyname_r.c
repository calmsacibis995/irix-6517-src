#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>


main(int argc, char **argv)
{
	struct netent n;
	struct in_addr a;
	int i;
	char buf[4096];

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (! getnetbyname_r(*argv, &n, buf, sizeof(buf))) {
			fprintf(stderr, "getnetbyname_r failed for [%s]\n", *argv);
		} else {
			a = inet_makeaddr(n.n_net, 0);
			printf("name = %s, addrtype = %d, net = %s\n",
			    n.n_name, n.n_addrtype, inet_ntoa(a));
			for (i = 0; n.n_aliases[i]; i++) {
				printf("\talias[%d]: %s\n", i,
				    n.n_aliases[i]);
			}
		}
	}
}
