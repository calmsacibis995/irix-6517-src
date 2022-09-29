
#include <stdio.h>
#include <ulocks.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>


#define	SEMACOUNT	1
#define	NUSERS		4

usptr_t *make_arena(char *, size_t, int);

void on_ckpt() {;}


main()
{
	char *aname = "/var/tmp/arenaJACK";
	usptr_t *arena;
	usema_t *sema;
	pid_t cpid;
	pid_t ctest_pid = getppid();
	int fd, stat, avail;
	FILE *fp;
	signal(SIGCKPT, on_ckpt);
	if ((arena = make_arena(aname, 65536, NUSERS)) == NULL) {
		perror("make_arena");
		return (-1);
	}
	if ((sema = usnewpollsema(arena, SEMACOUNT)) == NULL) {
		perror("usnewpollsema");
		return (-1);
	}
	fp = fopen("INFO", "w");
	printf("printing info on %s\n", aname);
	if ((cpid = fork()) == 0) {
		if ((fd = usopenpollsema(sema, S_IRWXU)) == -1) {
			perror("child: usopenpollsema");
			return (-1);
		}

		if ((avail = uspsema(sema)) == -1) {
			perror("child:uspsema");
			exit(-1);
		}
		printf("child:sema avail=%d\n", avail);

		if (usdumpsema(sema, fp, aname) == -1) {
			perror("child: usdumpsema");
			return (-1);
		}
		pause();

		exit (0);
	}
	if (cpid < 0) {
		perror("fork");
		return (-1);
	}
	if ((fd = usopenpollsema(sema, S_IRWXU)) < 0) {
		perror("usopenpollsema");
		return (-1);
	}
	pause();

	if ((avail = uspsema(sema)) == -1) {
		perror("uspsema");
		return(-1);
	}
	printf("sema avail=%d\n", avail);

	if (usdumpsema(sema, fp, aname) == -1) {
		perror("usdumpsema");
		return (-1);
	}
	wait(&stat);
	fwrite("\n", 2, 1, fp);	
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
