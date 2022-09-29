#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/fs/efs_ino.h>

#ifndef EFS_MAX_INLINE
#define EFS_MAX_INLINE 96
#endif
#define BSIZE (EFS_MAX_INLINE*2)
char buffer[BSIZE];
int die_now;

#define puterr(s)	put_err(argv[0], s, *argptr)

static void jump_ship(void);
static void put_err(char *x, char *op, char *filename);

int
main(int argc, char **argv)
{
	int sz;
	int args;
	struct stat stbuf;
	int ret = 0;
	char **argptr = &argv[1];
	extern int errno;

	sighold(SIGHUP);
	sigset(SIGINT, jump_ship);
	sigset(SIGALRM, jump_ship);
	sigset(SIGTERM, jump_ship);

	for (args = argc; --args > 0; argptr++) {

		sz = readlink(*argptr, buffer,  BSIZE);
		if (sz < 0) {
			puterr("readlink");
			ret++;
			continue;
		}

		if (sz > EFS_MAX_INLINE) {
			continue;
		}

		if (lstat(*argptr, &stbuf)) {
			puterr("lstat");
			ret++;
			continue;
		}

		if (die_now)
			exit(ret);

		if (unlink(*argptr)) {
			puterr("unlink");
			ret++;
			continue;
		}

		buffer[sz] = 0;
		if (symlink(buffer, *argptr)) {
			puterr("symlink");
			ret++;
			continue;
		}

		(void)lchown(*argptr, stbuf.st_uid, stbuf.st_gid);
	}

	exit(ret);
	/* NOTREACHED */
}

static void
put_err(char *x, char *op, char *filename)
{
	char errbuf[128];
	char *s = errbuf;

	(void)strcpy(s, x);

	while (*s != 0)
		s++;

	s += sprintf(s, ": ");

	(void)strcpy(s, op);

	while (*s != 0)
		s++;

	(void)sprintf(s, " %s", filename);
	perror(errbuf);
}

static void
jump_ship(void)
{
	die_now = 1;
}
