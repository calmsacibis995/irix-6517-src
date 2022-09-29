
#include <stdio.h>
#include <ulocks.h>
#include <errno.h>
#include "us.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define	SEMACOUNT	1
#define	NUSERS		8

usptr_t *make_arena(char *, size_t, int);

void on_ckpt() {;}

main()
{
	pid_t ctest_pid = getppid();
	char *aname = "/var/tmp/arenaJACK0\0";
	usptr_t *arena;
	usema_t *sema;
	ushdr_t *usp;
	int fd, avail, count;
	FILE *fp;
	struct stat buf;

	if ((arena = make_arena(aname, 65536, NUSERS)) == NULL) {
		perror("make_arena");
		return (-1);
	}
	usp = (ushdr_t *)arena;
	printf("spdev=%x initpid=%d fd=%d\n", usp->u_spdev, 
		usp->u_lastinit, usp->u_tidmap->t_sfd);

	if (fstat(usp->u_tidmap->t_sfd, &buf) >= 0)
		printf("dev=%x mode=%x type=%s\n", buf.st_rdev, buf.st_mode,
			buf.st_fstype);

#ifdef NOTYET
	if ((sema = usnewsema(arena, SEMACOUNT)) == NULL) {
		perror("usnewsema");
		return (-1);
	}
	fp = fopen("INFO", "w");
	printf("Arena file %s: sema addr=%x\n", aname, sema);

	count = ustestsema(sema);
	printf("remaining sema count=%d\n", count);

	if ((avail = uspsema(sema)) == -1) {
		perror("uspsema");
		return(-1);
	}
	printf("sema avail=%d\n", avail);

	if (usdumpsema(sema, fp, aname) == -1) {
		perror("usdumpsema");
		return (-1);
	}
#endif

	pause();
	kill(ctest_pid, SIGUSR1);
}

usptr_t *
make_arena(char *name, size_t size, int nprocs)
{
	int rc;

	if ((rc = usconfig(CONF_INITUSERS, nprocs)) < 0) {
		printf("usconfig(#users): rc=%d (%s)\n", rc, strerror(errno));
		return (0);
	}	
	if ((rc = usconfig(CONF_INITSIZE, size)) < 0) {
		printf("usconfig(size): rc=%d (%s)\n", rc, strerror(errno));
		return (0);
	}	
	return (usinit(name));
}
