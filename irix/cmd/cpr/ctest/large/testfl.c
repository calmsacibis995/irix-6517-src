
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

void on_ckpt() {;}

main(int argc, char **argv)
{
	pid_t ctest_pid = getppid();
	int rc, fd, delay = atol(argv[1]);
	struct stat sbuf;
	flock_t fl;	
	signal(SIGCKPT, on_ckpt);

        if ((fd = open("LFILE", O_RDONLY|O_CREAT, S_IRWXU)) == -1) {
		perror("open");
		exit(1);
	}

       	fl.l_type = F_RDLCK;
        fl.l_whence = 1;
        fl.l_start = 0;
        fl.l_len = 0;
        if (fcntl(fd, F_SETLK, &fl)) {
                perror("fcntl: F_SETLK");
                exit(1);
        }
	
	pause();

	printf("\n\ttestfl: l_type=%d l_start=%d l_len=%d l_sysid=%d l_pid=%d\n",
		fl.l_type, fl.l_start, fl.l_len, fl.l_sysid, fl.l_pid); 
	if (fl.l_type == 1 && fl.l_start == 0 && fl.l_len == 0 &&
		fl.l_sysid == 0 && fl.l_pid == 1)
		kill(ctest_pid, SIGUSR1);
	else
	{
		printf("\n\ttestfl: ERROR: flock settings changed!\n");
		kill(ctest_pid, SIGUSR2);
	}
	close(fd);
	kill(ctest_pid, SIGUSR1);


	/* the following is for another test */

/*	fd = open("LFILE", O_CREAT|O_RDWR, S_IRWXU);	
	
	if (fstat(fd, &sbuf)) {
		perror("fstat");
		exit(1);
	}
	printf("st_mode=%x\n", sbuf.st_mode);

	if ((rc = write(fd, "stock is down, USA--\n\0", 22)) == -1) {
		perror("write");
		exit (1);
	}
	close(fd);*/
}
