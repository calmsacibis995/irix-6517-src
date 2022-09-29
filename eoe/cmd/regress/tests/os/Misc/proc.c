/*
 * This is a unit test for mmap() ~ /proc bug.
 * Typically, this  bug allows any readable file writable when used with dbx.
 *
 * In this program, a parent process, P, with root privileges creates
 * a tmp file, T, with read permission for others.  Then P forks C,
 * which on fork will have root privileges. C then becomes a 'normal'
 * process through set[u,g]id. By default "guest" account is used as
 * a normal user. Then, C mmaps T at address A (for which as normal user it
 * has only read permission). C opens itself through /proc and writes() 
 * at address A: If there is a bug, then this write has to fail.
 * 
 * After forking, P waits for C, cleans up the tmpfile, and returns C's exit
 * status.
 */
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/wait.h>


int 	Verbose = 0;
char 	normalunam[32] = "guest";
char	*msg = "This message should not be in this file\n";
int 	accessthroproc(void *addr);
char	*tfile;

#define Vprintf if (Verbose) printf
 
static
void
die(char * format, ...)
{
	va_list ap;
	char string[1000];

	strcpy(string, sys_errlist[errno]);
	strcat(string, ": ");
	va_start(ap, format);
	strcat(string, format);
	strcat(string, "\n");
	vfprintf(stderr, string, ap);
	va_end(ap);
	if (tfile) unlink(tfile);

	exit(1);
}

main(int argc, char **argv)
{
        int             fd, winfo;
	pid_t		cpid;	
 
	if ((argc > 2)  || ((argc == 2)  && (strcmp(argv[1], "-v") != 0))) {
		printf("Usage: %s [-v]\n", argv[0]);
		exit(1);
	}
	if (argc == 2) /* we know due to above strcmp, that argv[2] == "-v" */
		Verbose = 1;

	if (getuid() != 0) {
		printf("%s:  Must be root to run this test.\n", argv[0]);
		exit(1);
	}

	Vprintf("Creating tmpfile\n");
	if ((tfile = tmpnam(NULL)) == NULL)
		die("cannot get name for tmp file");
        if ((fd = open(tfile, O_CREAT, 0644)) < 0)
		die("open %s", tfile);
	close(fd);
	Vprintf("Tmpfile (%s) created, forking\n", tfile);

	if ((cpid = fork())  == (pid_t) -1)
		die("fork");
	
	if (cpid) {
		/* parent */
		Vprintf("Parent: waiting for %d\n", cpid);
		if (waitpid(cpid, &winfo, 0) != cpid)
			die("Wait for child %d", cpid);

		Vprintf("Parent: Wait returned 0x%x\n", winfo);
		
		if (WIFEXITED(winfo) == 0)
			die("Child %d did not exit gracefully (wstat = 0x%x)", cpid, winfo);
		unlink(tfile);
		exit(WEXITSTATUS(winfo));
	}
	else
		child();
	/* NOT REACHED */
}



child()
{
	struct passwd 	*normalpwent;
        caddr_t         addr;
	int		fd, error;

	Vprintf("Attempting to get pwent for %s\n", normalunam);
	if ((normalpwent = getpwnam(normalunam)) == NULL)
		die("getpwname %s", normalunam);

	Vprintf("Setting uid/gid to be %d/%d\n",
		normalpwent->pw_uid, normalpwent->pw_gid);
	if (setgid(normalpwent->pw_gid) != 0)
		die("setgid %d", normalpwent->pw_gid);

	if (setuid(normalpwent->pw_uid) != 0)
		die("setuid %d", normalpwent->pw_uid);

	/*
 	 * Check if set[g,u]id worked properly: open the tmp file for
	 * writing. If setid's worked, then call should fail with EACCES.
	 */
	Vprintf("Tesing morphing\n");
        if ((fd = open(tfile, O_RDWR)) >= 0)
		die("open %s, RDWR", tfile);
	if (errno != EACCES)
		die("Expected error to be %d, got %d", EACCES, errno);

	Vprintf("Successfully morphed into %s\n", normalunam);

	/*
	 * On with the show; open file rdonly and mmap in.
	 */
	if ((fd = open(tfile, O_RDONLY)) < 0)
		die("open %s, RDONLY", tfile);

        if ((addr = mmap((caddr_t) 0, 4096, PROT_READ,
		MAP_SHARED|MAP_AUTOGROW, fd, 0)) == (void *) -1)
			die("mmap %s");
	Vprintf("Successfully Mmaped\n");
 
	error = accessthroproc(addr);
        close(fd);
	if (Verbose) {
		char 	cmd[256];

		printf("Contents of file are (should be nothing) :\n");
		printf("Begin File %s\n", tfile);
		sprintf(cmd, "cat %s", tfile);
		system(cmd);
		printf("End File %s\n", tfile);
	}
        exit(error);
}


int
accessthroproc(void *addr)
{
	int 	fd;
	char 	pname[32];

	sprintf(pname, "/proc/%05d", getpid());
	if ((fd = open(pname, O_RDWR|O_SYNC)) < 0)
		die("open %s", pname);
	
	if (lseek(fd, (off_t) addr, SEEK_SET) < 0)
		die("lseek to 0x%x in %s", addr, pname);

	if (write(fd, msg, strlen(msg)) != strlen(msg)) {
		printf("Write to mmaped obj thro /proc failed (Correctly)\n");
		printf("Expected error to be %d, got %d (OK to be different)\n", EPERM, errno);
		close(fd);
		return(0);
	} else {
		printf("Write to mmaped obj thro /proc succeeded!!\n");
		printf("Possible security bug\n");
		close(fd);
		return(-1);
	}
}
