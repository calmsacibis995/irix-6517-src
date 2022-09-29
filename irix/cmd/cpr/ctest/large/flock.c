/* I can't quite figure out what this test is supposed to be doing. There
is a pause() halfway down the code, but it is preceded by a delayed alarm
signal, indicating that the pause is to be broken by SIGALRM. There is no
other indication of where the checkpoint is meant to occur.

The program basically creates a bunch of files, sets up different read and
write locks on them, does this pause/alarm thing, and then deletes the
files. A lot of the code looks like it was patched on at different times
for quick-fix testing. Possible purpose of the test: to check whether file
locks are preserved across a checkpoint. If this is true, it doesn't quite
do it right. Also, too many things have been done that aren't in accord
with that purpose (the delayed alarm?). I made the test compatible with
the scaffolding, but didn't do much more. */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>


#define	NFILES		15


void
on_checkpoint()
{;}

main(int argc, char **argv)
{

	flock_t fl;
	int fd[NFILES], i, delay = atol(argv[1]);
	char path[32];
	pid_t ctest_pid = getppid();

	signal(SIGCKPT, on_checkpoint);

	if (delay == 0)
		delay = 20;

	for (i = 0; i < NFILES; i++) {
		sprintf(path, "LFILE%d", i);
		fd[i] = open(path, O_CREAT|O_RDWR, S_ISGID|S_IRWXU);	
		write(fd[i], "stock is up, world\n\0", 22);
	}


	for (i = 0; i < NFILES; i=i+2) { 
		fl.l_type = F_RDLCK;
		fl.l_whence = 0;
		fl.l_start = 0;
		fl.l_len = 11;
		if (fcntl(fd[i], F_SETLK, &fl)) {
			perror("fcntl: F_SETLK");
			exit(1);
		}
	
		fl.l_type = F_WRLCK;
		fl.l_whence = 0;
		fl.l_start = 11;
		fl.l_len = 11;
		if (fcntl(fd[i], F_SETLK, &fl)) {
			perror("fcntl: F_SETLK");
			exit(1);
		}
	}

	for (i = 1; i < NFILES; i=i+2) { 
		fl.l_type = F_RDLCK;
		fl.l_whence = 0;
		fl.l_start = 0;
		fl.l_len = 11;
		if (fcntl(fd[i], F_SETLK, &fl)) {
			perror("fcntl: F_SETLK");
			exit(1);
		}
	
		fl.l_type = F_WRLCK;
		fl.l_whence = 0;
		fl.l_start = 11;
		fl.l_len = 11;
		if (fcntl(fd[i], F_SETLK, &fl)) {
			perror("fcntl: F_SETLK");
			exit(1);
		}
		fl.l_type = F_WRLCK;
		fl.l_whence = 0;
		fl.l_start = 5;
		fl.l_len = 10;
		if (fcntl(fd[i], F_SETLK, &fl)) {
			perror("fcntl: F_SETLK");
			exit(1);
		}
	}

	/*	alarm(delay);*/
	pause();

	for (i = 0; i < NFILES; i++) {
		close(fd[i]);

		sprintf(path, "LFILE%d", i);
		unlink(path);	
	}
	kill (ctest_pid, SIGUSR1);
	printf("\t*** flock: Done ***\n");
}
