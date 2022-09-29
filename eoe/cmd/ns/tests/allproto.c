#include <stdio.h>
#include <netdb.h>


main(int argc, char **argv)
{
	struct protoent *p, *p2, retp;
	int i;
	char buf[4096], b2[4096], b3[4096];

	while (p = getprotoent()) {
		printf("name = %s, number = %d\n", p->p_name, p->p_proto);
		for (i = 0; p->p_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, p->p_aliases[i]);
		}
		if (! getprotobyname_r(p->p_name, &retp, b2, sizeof(b2))) {
			fprintf(stderr, "failed getprotobyname: %s\n",
			    p->p_name);
		}
		if (! getprotobynumber_r(p->p_proto, &retp, b3, sizeof(b3))) {
			fprintf(stderr, "failed getprotobynumber: %d\n",
			    p->p_proto);
		}
	}
}
