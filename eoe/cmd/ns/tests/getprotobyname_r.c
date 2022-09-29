#include <stdio.h>
#include <netdb.h>


main(int argc, char **argv)
{
	struct protoent p;
	int i;
	char buf[4096];

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (! getprotobyname_r(*argv, &p, buf, sizeof(buf))) {
			fprintf(stderr, "getprotobyname_r failed for [%s]\n",
				*argv);
		} else {
			printf("name = %s, number = %d\n",
			    p.p_name, p.p_proto);
			for (i = 0; p.p_aliases[i]; i++) {
				printf("\talias[%d]: %s\n", i,
				    p.p_aliases[i]);
			}
		}
	}
}
