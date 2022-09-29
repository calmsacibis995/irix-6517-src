#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>


main(int argc, char **argv)
{
	struct hostent h;
	char buf[4096];
	struct in_addr addr;
	int herr;
	int i;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (! inet_aton(*argv, &addr)) {
			fprintf(stderr, "failed to convert address: %s\n",
			    *argv);
		}
		if (! gethostbyaddr_r(&addr, sizeof(addr), AF_INET, &h, buf,
		    sizeof(buf), &herr)) {
			fprintf(stderr, "gethostbyaddr failed: %s\n",
			    hstrerror(herr));
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
