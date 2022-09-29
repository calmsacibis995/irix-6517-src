#include <stdio.h>
#include <grp.h>

main()
{
	gid_t g[64];
	int i, j, ngrps;
	struct group *gr;
	
	ngrps = getgroups(64, g);

	for (i = 0; i < ngrps; i++) {
		printf("groups[%d] = %d\n", i, g[i]);
		gr = getgrgid(g[i]);
		if (gr) {
			printf("\tname = %s, passwd = %s, gid = %d\n",
			    gr->gr_name, gr->gr_passwd, gr->gr_gid);
			for (j = 0; gr->gr_mem[j]; j++) {
				printf("\t\tmember[%d]: %s\n", j,
				    gr->gr_mem[j]);
			}
		} else {
			fprintf(stderr, "\tfailed getgrgid(%d)\n", g[i]);
		}
	}
}
