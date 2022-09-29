#include <stdio.h>
#include <pthread.h>
#include <grp.h>

#include "sub_groups.h"

void
sub_getgrent()
{
	struct group g, g2;
	int i;
        struct group *r;
        char buf[4096];
	pthread_t	th1, th2;

        pthread_t       th;
        th = pthread_self();

	while (getgrent_r(&g, buf, sizeof(buf))) {
		printf("Thread %d :entry: name = %s, passwd = %s, gid = %d\n",
		    th, g.gr_name, g.gr_passwd, g.gr_gid);
		for (i = 0; g.gr_mem[i]; i++) {
			printf("\tmember[%d]: %s\n", i, g.gr_mem[i]);
		}

		pthread_create(&th1,
				NULL,
				(void*)sub_getgrnam_r,
				(void *)g.gr_name);
		pthread_create(&th2,
				NULL,
				(void *)sub_getgrgid_r,
				(void *)g.gr_gid);

		/* Join the threads */
		pthread_join(th1, NULL);	
		pthread_join(th2, NULL);	
	}
}

void
sub_getgrnam_r(char *name)
{
	struct group g2;
        char buf[4096];
	char grname[56];
        struct group *r;
	strcpy(grname, name);

	if (getgrnam_r(grname,&g2,  buf, sizeof(buf), &r)) {
			fprintf(stderr, "failed getgrnam: %s\n", grname);
	}
	else if(r) {
		printf("======getgrnam_r()===== : name = %s, passwd = %s, gid = %d\n",
		    g2.gr_name, g2.gr_passwd, g2.gr_gid);
	}
	else {
		fprintf(stderr, "failed getgrnam: %s\n", grname);
	}
}
void 
sub_getgrgid_r(int gid)
{
	struct group g2;
        char buf[4096];
	int	groupid;
        struct group *r;
 	groupid = gid;	

	if (getgrgid_r(groupid,&g2,  buf, sizeof(buf), &r)) {
		fprintf(stderr, "failed getgrgid_r: %d\n", groupid);
	}
	else if(r) {
		printf("======getgrgid_r()===== : gid = %d, name = %s, passwd = %s\n",
		    g2.gr_gid, g2.gr_name, g2.gr_passwd);
	}
	else {
		fprintf(stderr, "failed getgrgid_r: %d\n", groupid);
	}
}
