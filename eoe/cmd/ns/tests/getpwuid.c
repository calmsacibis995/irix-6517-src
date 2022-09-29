#include <stdio.h>
#include <pwd.h>

main(int argc, char **argv)
{
	struct passwd *pw;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		pw = getpwuid(atoi(*argv));
		if (! pw) {
			fprintf(stderr, "getpwuid failed\n");
		} else {
			printf("name = %s, passwd = %s, uid = %d, gid = %d\n",
			    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid);
			printf("age = %s, comment = %s, gecos = %s\n",
			    pw->pw_age, pw->pw_comment, pw->pw_gecos);
			printf("dir = %s, shell = %s\n",
			    pw->pw_dir, pw->pw_shell);
		}
	}
}
