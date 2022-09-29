#include <stdio.h>
#include <netdb.h>


main(int argc, char **argv)
{
	struct servent *s, *s2, rets;
	int i;
	char buf[4086], b2[4096];

	while (s = getservent()) {
		printf("name = %s, port = %d, protocol = %s\n",
		    s->s_name, s->s_port, s->s_proto);
		for (i = 0; s->s_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, s->s_aliases[i]);
		}
		if (! getservbyname_r(s->s_name, s->s_proto, &rets, buf, sizeof(buf))) {
			fprintf(stderr, "failed getservbyname: %s\n",
			    s->s_name);
		}
		if (! getservbyport_r(s->s_port, s->s_proto, &rets, b2, sizeof(b2))) {
			fprintf(stderr, "failed getservbyport: %d\n",
			    s->s_port);
		}
	}
}
