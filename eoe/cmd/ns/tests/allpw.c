#include <stdio.h>
#include <pwd.h>

main(int argc, char **argv)
{
	struct xpasswd xpw;
	struct passwd *pw, *p;
	char buf[4096], b2[4096];

	while (pw = getpwent()) {
		printf("name = %s, passwd = %s, uid = %d, gid = %d\n",
		    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid);
		printf("age = %s, comment = %s, gecos = %s\n",
		    pw->pw_age, pw->pw_comment, pw->pw_gecos);
		printf("dir = %s, shell = %s\n",
		    pw->pw_dir, pw->pw_shell);
		if (getpwnam_r(pw->pw_name, (struct passwd *)&xpw, buf,
		    sizeof(buf), &p) != 0) {
			fprintf(stderr, "failed getpwnam_r: %s\n", pw->pw_name);
		}
		if (getpwuid_r(pw->pw_uid, (struct passwd *)&xpw, b2,
		    sizeof(b2), &p) != 0) {
			fprintf(stderr, "failed getpwuid_r: %d\n", pw->pw_uid);
		}
	}
}
