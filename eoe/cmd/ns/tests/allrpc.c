#include <stdio.h>
#include <netdb.h>


main(int argc, char **argv)
{
	struct rpcent *r, r2;
	char buf[4096], b2[4096];
	int i;

	while (r = getrpcent()) {
		printf("name = %s, number = %d\n", r->r_name, r->r_number);
		for (i = 0; r->r_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, r->r_aliases[i]);
		}
		if (! getrpcbyname_r(r->r_name, &r2, buf, sizeof(buf))) {
			fprintf(stderr, "failed getrpcbyname: %s\n", r->r_name);
		}
		if (! getrpcbynumber_r(r->r_number, &r2, b2, sizeof(b2))) {
			fprintf(stderr, "failed getrpcbynumber: %d\n",
			    r->r_number);
		}
	}
}
