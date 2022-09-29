#include <stdio.h>
#include <grp.h>

main(int argc, char **argv)
{
	struct group *g, g2;
	int i;
        struct group *r;
        char buf[4096], b2[4096], b3[4096];

	while (g = getgrent()) {
		printf("entry: name = %s, passwd = %s, gid = %d\n",
		    g->gr_name, g->gr_passwd, g->gr_gid);
		for (i = 0; g->gr_mem[i]; i++) {
			printf("\tmember[%d]: %s\n", i, g->gr_mem[i]);
		}
		if (getgrnam_r(g->gr_name,&g2,  b2, sizeof(b2), &r)) {
			fprintf(stderr, "failed getgrnam: %s\n", g->gr_name);
		}
		if (getgrgid_r(g->gr_gid, &g2, b3, sizeof(b3), &r)) {
			fprintf(stderr, "failed getgrgid: %d\n", g->gr_gid);
		}
	}
}
