#include <stdio.h>
#include <netdb.h>


main(int argc, char **argv)
{
	struct rpcent *r;
	int i;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		r = getrpcbynumber(strtol(*argv, 0, 10));
		if (! r) {
			fprintf(stderr, "getrpcbynumber failed\n");
		} else {
			printf("name = %s, number = %d\n",
			    r->r_name, r->r_number);
			for (i = 0; r->r_aliases[i]; i++) {
				printf("\talias[%d]: %s\n", i, r->r_aliases[i]);
			}
		}
	}
}
