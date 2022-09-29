#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>


main(int argc, char **argv)
{
	struct hostent h;
	int i, herr;
	char buf[4096];

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (! gethostbyname_r(*argv, &h, buf, sizeof(buf), &herr)) {
			fprintf(stderr, "gethostbyname failed for [%s]: %s\n",
			    *argv, hstrerror(herr));
		} else {
			printf("name = %s, addrtype = %d, length = %d\n",
			    h.h_name, h.h_addrtype, h.h_length);
			for (i = 0; h.h_aliases[i]; i++) {
				printf("\talias[%d]: %s\n", i, h.h_aliases[i]);
			}
			for (i = 0; h.h_addr_list[i]; i++) {
				printf("\taddress[%d]: %s\n", i,
				    inet_ntoa(*(struct in_addr *)h.h_addr_list[i]));
			}
		}
	}
}
