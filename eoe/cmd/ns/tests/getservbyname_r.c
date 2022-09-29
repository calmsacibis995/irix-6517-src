#include <stdio.h>
#include <netdb.h>


main(int argc, char **argv)
{
	struct servent s, *sp;
	char buf[4096];
	int i;

	for (argc--, argv++; argc > 0; argc -= 2, argv += 2) {
		if (argc >= 2) {
			sp = getservbyname_r(argv[0], argv[1], &s, buf,
			    sizeof(buf));
		} else {
			sp = getservbyname_r(argv[0], "udp", &s, buf,
			    sizeof(buf));
		}
		if (! sp) {
			fprintf(stderr, "getservbyname_r failed for [%s]\n",
				argv[0]);
		} else {
			printf("name = %s, port = %d, protocol = %s\n",
			    s.s_name, s.s_port, s.s_proto);
			for (i = 0; s.s_aliases[i]; i++) {
				printf("\talias[%d]: %s\n", i, s.s_aliases[i]);
			}
		}
	}
}
