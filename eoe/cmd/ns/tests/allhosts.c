#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>


main(int argc, char **argv)
{
	struct hostent *h, h2;
	char buf[4096], b2[4096], b3[4096];
	int i, herr;

	while (h = gethostent()) {
		printf("name = %s, addrtype = %d, length = %d\n",
		    h->h_name, h->h_addrtype, h->h_length);
		for (i = 0; h->h_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, h->h_aliases[i]);
		}
		if (! gethostbyname_r(h->h_name, &h2, b2, sizeof(b2), &herr)) {
			fprintf(stderr, "failed gethostbyname: %s\n",
			    h->h_name);
		}
		for (i = 0; h->h_addr_list[i]; i++) {
			printf("\taddress[%d]: %s\n", i,
			    inet_ntoa(*(struct in_addr *)h->h_addr_list[i]));
			if (! gethostbyaddr_r(h->h_addr_list[i], h->h_length,
			    h->h_addrtype, &h2, b3, sizeof(b3), &herr)) {
				fprintf(stderr, "failed gethostbyaddr: %s\n",
			    inet_ntoa(*(struct in_addr *)h->h_addr_list[i]));
			}
		}
	}
}
