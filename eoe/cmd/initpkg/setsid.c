/*
 * setsid command args ...
 *
 * Does setsid(2) system call and exec's command args ...
 * Causes resulting command to run as process group leader.
 * Useful in /etc/inittab, for :wait: and :bootwait:
 * (which unlike other entries, init doesn't make group leaders)
 * which want to interact on the console with the user,
 * and have ^C (stty intr) work.
 *
 * For example, use this by changing the inittab line:
 *	prei:2:bootwait:/etc/mrinitrc            </dev/console >/dev/console 2>&1
 * to the line:
 *	prei:2:bootwait:setsid /dev/console /etc/mrinitrc
 *
 * Silicon Graphics
 * Paul Jackson
 * March 11, 1994
 */

#include <unistd.h>
#include <fcntl.h>
#include <termio.h>

main (int argc, char *const *argv)
{
	char *msg;
	int ercfd;
	int i, retry=0;
	pid_t tpgrp;
	char *ttyd = argv[1];
	char cons[8];

	/* dup stderr for error message if exec fails */
	ercfd = dup(2);
	fcntl (ercfd, F_SETFD, FD_CLOEXEC);

retryit:
	close(0);
	close(1);
	close(2);

	setsid();

	open(ttyd, O_RDWR);
	open(ttyd, O_RDWR);
	open(ttyd, O_RDWR);

	if(!retry && ioctl(1, TIOCGPGRP, &tpgrp) == -1) {
		/* didn't get a controlling tty; try console device matching
		 * nvram console, or keyboard SIGINT and some scripts will fail. */
		if(sgikopt("console", cons, sizeof(cons)) == 0) {
			ttyd = (*cons == 'd') ? "/dev/ttyd1" : "/dev/tport";
			retry++;
			goto retryit;
		}

	}

	argv += 2;
	argc -= 2;
	execv (*argv, argv);

	msg = "setsid execv failed:";
	write (ercfd, msg, strlen(msg));
	for (i=0; i<argc; i++) {
		write (ercfd, " ", 1);
		write (ercfd, argv[i], strlen(argv[i]));
	}
	write (ercfd, "\n", 1);

	exit (1);
}
