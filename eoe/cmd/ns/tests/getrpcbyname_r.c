#include <stdio.h>
#include <netdb.h>


main(int argc, char **argv)
{
	struct rpcent r;
	char buf[4096];
	int i;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (! getrpcbyname_r(*argv, &r, buf, sizeof(buf))) {
			fprintf(stderr, "getrpcbyname_r failed for [%s]\n",
				*argv);
		} else {
			printf("name = %s, number = %d\n",
			    r.r_name, r.r_number);
			for (i = 0; r.r_aliases[i]; i++) {
				printf("\talias[%d]: %s\n", i, r.r_aliases[i]);
			}
		}
	}
}
