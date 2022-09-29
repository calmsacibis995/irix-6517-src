#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>


main(int argc, char **argv)
{
	struct hostent *h;
	int i;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		h = gethostbyname(*argv);
		if (! h) {
			fprintf(stderr, "gethostbyname failed for [%s]: %s\n",
			    *argv, hstrerror(h_errno));
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
