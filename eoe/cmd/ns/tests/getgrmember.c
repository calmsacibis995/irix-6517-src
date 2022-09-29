#include <stdio.h>
#include <grp.h>
#include <pwd.h>

main(int argc, char **argv)
{
	struct group *g;
	struct passwd *p;
	gid_t groups[32];
	int i, count;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		p = getpwnam(*argv);
		if (! p) {
			fprintf(stderr, "getpwnam failed\n");
		} else {
			groups[0] = p->pw_gid;
			if ((count = getgrmember(*argv, groups, 32, 1)) == -1) {
				fprintf(stderr, "getgrmember failed\n");
			} else {
				for (i = 0; i < count; i++) {
					printf("group[%d]: %d\n", i, groups[i]);
				}
			}
		}
	}
}
