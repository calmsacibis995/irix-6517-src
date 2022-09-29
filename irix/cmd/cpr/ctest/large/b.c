#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/syssgi.h>
#include <signal.h>

void on_ckpt() {;}

main()
{
	pid_t ctest_pid = getppid();
	unsigned x;
	int fd1, fd2, fd3;
	int pfd1[2], pfd2[2], pfd3, pfd4, pfd5, pfd6;
	char buf[100];
	char *addr1, *addr2, *addr3;
	signal(SIGCKPT, on_ckpt);
	syssgi(SGI_SPIPE, 1);

	fd1 = open("./foo", O_RDWR|O_CREAT, 0777);
	printf("fd1 %d\n", fd1);
	unlink("./foo");
	write(fd1, "foo\n", 4);
	write(fd1, "bar\n", 4);
	lseek(fd1, 4, 0);

	unlink("./bar");
	fd2 = open("./bar", O_RDWR|O_CREAT, 0444);
	printf("fd2 %d\n", fd2);
	write(fd2, "file bar\n", 9);
	lseek(fd2, 5, 0);
	/*
	 * Do some mmap testing.  mmap in a file with "shared".
	 * mmap in a unlinked file.
	 */
	unlink("mapfile1");
	fd3 = open("mapfile1", O_RDWR|O_CREAT, 0444);
	lseek(fd3, getpagesize()-1, 0);
	write(fd3, buf, 1);
	addr1 = mmap(0, getpagesize(), PROT_WRITE, MAP_SHARED, fd3, 0);
	close(fd3);

	strcpy(addr1, "This is a string to go in the file!\n");

	unlink("mapfile2");
	fd3 = open("mapfile2", O_RDWR|O_CREAT, 0444);
	unlink("mapfile2");
	lseek(fd3, getpagesize()-1, 0);
	write(fd3, buf, 1);
	addr2 = mmap(0, getpagesize(), PROT_WRITE, MAP_SHARED, fd3, 0);
	close(fd3);

	strcpy(addr2, "This is a string to go in the 2nd file!\n");

	unlink("mapfile3");
	fd3 = open("mapfile3", O_RDWR|O_CREAT, 0444);
	lseek(fd3, getpagesize()-1, 0);
	write(fd3, buf, 1);
	addr3 = mmap(0, getpagesize(), PROT_WRITE, MAP_SHARED, fd3, 0);
	close(fd3);

	strcpy(addr3, "This is a string to go in the 3rd file!\n");
	/*
	 * pipe testing
	 */
 	if (pipe(pfd1) < 0) {
                printf("pipe call failed!\n");
                exit(1);
        }
 	if (pipe(pfd2) < 0) {
                printf("pipe call failed!\n");
                exit(1);
        }
	pfd3 = dup(pfd1[0]);
	(void)close(pfd1[0]);
	pfd4 = dup(pfd1[1]);
	pfd5 = dup(pfd2[0]);
	pfd6 = dup(pfd2[1]);

	write(pfd4, "Data in 1st pipe", strlen("Data in 1st pipe")+1);
	write(pfd6, "Data in 2nd pipe", strlen("Data in 2nd pipe")+1);
        printf("Before sleeping...\n");
#ifdef NOTDEF
	(void)setregs();
#endif
        pause();
#ifdef NOTDEF
	if (x = checkregs())
		printf("register failure! %x\n", x);
	else
		printf("register success!\n");
#endif
        printf("I AM BACK!\n");
	read(fd1, buf, 4);
	write(1, buf, 4);
	lseek(fd1, 0, 0);
	write(fd1, "log\njam\n", 8);

	lseek(fd1, 0, 0);
	read(fd1, buf, 8);
	write(1, buf, 8);

	write(fd2, "open\n", 5);
	lseek(fd2, 0,0);
	read(fd2, buf, 10);
	write(1, buf, 10);


	printf("%s", addr1);
	printf("%s", addr2);
	printf("%s", addr3);

	read(pfd3, buf, sizeof(buf));
	printf("pipe 1: %s\n", buf);
	read(pfd5, buf, sizeof(buf));
	printf("pipe 2: %s\n", buf);
	kil(ctest_pid, SIGUSR1);
}

