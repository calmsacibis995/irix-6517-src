#include <stdio.h>
#include <grp.h>

main(int argc, char **argv)
{
	struct group *g;
	int i;

	while (g = getgrent()) {
		printf("name = %s, passwd = %s, gid = %d\n",
		    g->gr_name, g->gr_passwd, g->gr_gid);
		for (i = 0; g->gr_mem[i]; i++) {
			printf("\tmember[%d]: %s\n", i, g->gr_mem[i]);
		}
	}
}
