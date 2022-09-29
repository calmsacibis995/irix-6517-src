#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>


main(int argc, char **argv)
{
	struct hostent *h;
	struct in_addr addr;
	int i;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (! inet_aton(*argv, &addr)) {
			fprintf(stderr, "failed to convert address: %s\n",
			    *argv);
		}
		h = gethostbyaddr(&addr, sizeof(addr), AF_INET);
		if (! h) {
			fprintf(stderr, "gethostbyaddr failed: %s\n",
			    hstrerror(h_errno));
		} else {
			printf("name = %s, addrtype = %d, length = %d\n",
			    h->h_name, h->h_addrtype, h->h_length);
			for (i = 0; h->h_aliases[i]; i++) {
				printf("\talias[%d]: %s\n", i, h->h_aliases[i]);
			}
			for (i = 0; h->h_addr_list[i]; i++) {
				printf("\taddress[%d]: %s\n", i,
				    inet_ntoa(*(struct in_addr *)h->h_addr_list[i]));
			}
		}
	}
}
