#include <stdio.h>
#include <pwd.h>

#include "sub_pws.h"

void
sub_getpwent()
{
	struct passwd pw;
	char buf[4096];
	pthread_t	th, th2, th3;

	th = pthread_self();

	while (getpwent_r(&pw, buf, sizeof(buf))) {
		printf("Thread %d :name = %s, passwd = %s, uid = %d, gid = %d\n",
		    th, pw.pw_name, pw.pw_passwd, pw.pw_uid, pw.pw_gid);
		printf("age = %s, comment = %s, gecos = %s\n",
		    pw.pw_age, pw.pw_comment, pw.pw_gecos);
		printf("dir = %s, shell = %s\n",
		    pw.pw_dir, pw.pw_shell);
		pthread_create(&th2,
				NULL,
				(void *)sub_getpwbyname,
				(void *)pw.pw_name);
		pthread_create(&th3,
				NULL,
				(void *)sub_getpwbyuid,
				(void *)pw.pw_uid);

		pthread_join(th2, NULL);
		pthread_join(th3, NULL);
	}
}

void
sub_getpwbyname(const char *name)
{
	struct passwd pw, *p;
	char buf[4096];

	if (getpwnam_r(name, (struct passwd *)&pw, buf,
	    sizeof(buf), &p) != 0) {
		fprintf(stderr, "failed getpwnam_r: %s\n", name);
	}
}

void
sub_getpwbyuid(long uid)
{
	struct passwd pw, *p;
	char buf[4096];

	if (getpwuid_r(uid, (struct passwd *)&pw, buf,
	    sizeof(buf), &p) != 0) {
		fprintf(stderr, "failed getpwuid_r: %d\n", uid);
	}
}
