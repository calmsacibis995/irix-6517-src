#include <stdio.h>
#include <netdb.h>


main(int argc, char **argv)
{
	struct servent *s;
	int i;

	while (s = getservent()) {
		printf("name = %s, port = %d, protocol = %s\n",
		    s->s_name, s->s_port, s->s_proto);
		for (i = 0; s->s_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, s->s_aliases[i]);
		}
	}
}
