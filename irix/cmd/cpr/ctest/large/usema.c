
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
	int fd, stat, avail, count;
	FILE *fp;
	signal(SIGCKPT, on_ckpt);
	signal(SIGCLD, SIG_IGN);

	if ((arena = make_arena(aname, 65536, NUSERS)) == NULL) {
		perror("make_arena");
		return (-1);
	}
	if ((sema = usnewsema(arena, SEMACOUNT)) == NULL) {
		perror("usnewsema");
		return (-1);
	}
	fp = fopen("INFO", "w");
	printf("Arena file %s: sema addr=%x\n", aname, sema);
	if ((cpid = fork()) == 0) {

		sleep(2);

		/*
		 * blocked here
		 */
		if ((avail = uspsema(sema)) == -1) {
			perror("child:uspsema");
			exit(-1);
		}
		if (usdumpsema(sema, fp, aname) == -1) {
			perror("child: usdumpsema");
			return (-1);
		}
		printf("child %d:sema avail=%d\n", getpid(), avail);

		exit (0);
	}
	if (cpid < 0) {
		perror("fork");
		return (-1);
	}
	count = ustestsema(sema);
	printf("Testing: remaining sema count=%d\n", count);
	if ((avail = uspsema(sema)) == -1) {
		perror("uspsema");
		return(-1);
	}
	printf("sema avail=%d\n", avail);

	pause();
	/*
	 * unblock the child
	 */
	if ((avail = usvsema(sema)) == -1) {
		perror("usvsema");
		return(-1);
	}
	printf("sema avail=%d\n", avail);

	wait(&stat);
	printf("sleep done\n");

	if (usdumpsema(sema, fp, aname) == -1) {
		perror("usdumpsema");
		return (-1);
	}

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
