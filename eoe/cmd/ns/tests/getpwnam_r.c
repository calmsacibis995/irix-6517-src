#include <stdio.h>
#include <pwd.h>

main(int argc, char **argv)
{
	struct xpasswd xpw;
	struct passwd *p;
	char buf[4096];

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (getpwnam_r(*argv, (struct passwd *)&xpw, buf,
		    sizeof(buf), &p) != 0) {
			fprintf(stderr, "1. getpwnam_r failed for [%s]\n",
				*argv);
		} else {
			if (p) {
				printf("name = %s, passwd = %s, uid = %d, gid = %d\n",
				    p->pw_name, p->pw_passwd, p->pw_uid, p->pw_gid);
				printf("age = %s, comment = %s, gecos = %s\n",
				    p->pw_age, p->pw_comment, p->pw_gecos);
				printf("dir = %s, shell = %s\n",
				    p->pw_dir, p->pw_shell);
			} else {
				fprintf(stderr, "getpwnam_r failed for [%s] \n",
				*argv);
			}
		}
	}
}
