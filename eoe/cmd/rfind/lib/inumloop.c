#include <stdlib.h>
#include <sys/types.h>
#include "fsdump.h"

static char buf[128];			/* for testing - read inums into here */

/*
 * Development debugging routine - lets me interactively
 * display contents of inodes, by typing in one inode
 * number after the "inum:" prompt.  Use '^D' to terminate
 * loop and return.
 */

void inumloop () {
	while (printf ("inum: "), gets (buf)) {
		int ino = atoi (buf);
		inod_t *p;
		if (PINO(&mh,ino)->i_mode == 0) {
			printf ("ino %d not valid\n", ino);
			continue;
		}
		p = PINO(&mh,ino);
		printf (
		    "%4d md %06o nxlnk %4d u %3u g %3u sz %6lld at %9d ct %9d mt %9d gen %9u dev %9d\n",
		    ino, p->i_mode, p->i_xnlink, p->i_uid, p->i_gid, p->i_size, p->i_atime, p->i_ctime, p->i_mtime, p->i_gen, p->i_rdev);
		pwd(ino);
	}
}
