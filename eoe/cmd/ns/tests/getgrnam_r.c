#include <stdio.h>
#include <grp.h>

main(int argc, char **argv)
{
	struct group g, *r;
	char buf[4096];
	int i;

	memset(buf, 0xf0, sizeof(buf));
	memset(&g, 0xf0, sizeof(g));

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (getgrnam_r(*argv, &g, buf, sizeof(buf), &r)) {
			fprintf(stderr, "getgrnam_r failed for [%s]\n", *argv);
		} else if (r) {
			printf("name = %s, passwd = %s, gid = %d\n",
			    g.gr_name, g.gr_passwd, g.gr_gid);
			for (i = 0; g.gr_mem[i]; i++) {
				printf("\tmember[%d]: %s\n", i, g.gr_mem[i]);
			}
		} else {
			fprintf(stderr, "getgrnam_r: not found: %s\n", *argv);
		}
	}
}
