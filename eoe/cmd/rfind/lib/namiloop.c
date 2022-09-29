#include <stdlib.h>
#include <sys/types.h>
#include "fsdump.h"

typedef enum { quiet = 0, showerrors = 1 } t_eflag;
extern t_eflag ErrorFlag;
static char buf[128];			/* for testing - read names into here */
inod_t *namip(char *);
inod_t *xstat (char *, t_eflag);

/*
 * Development debugging routine - lets me interactively
 * display contents of inodes, by typing in one full
 * pathname after the "name:" prompt.  Use '^D' to terminate
 * loop and return.
 */

/* ARGSUSED */
void namiloop (int nitotal) {
	while (printf ("name: "), gets (buf)) {
		ino64_t ino;
		inod_t *p;
		if ((p = xstat (buf, ErrorFlag = quiet)) == NULL) {
			printf ("name not found\n");
			continue;
		}
		ino = p->i_xino;
		if (p->i_mode == 0) {
			printf ("ino %lld zero mode\n", ino);
			continue;
		}
		printf ("%4lld md %06o nxlnk %4d u %3u g %3u sz %6lld at %9d ct %9d mt %9d gen %9u dev %9d\n",
		    ino, p->i_mode, p->i_xnlink, p->i_uid, p->i_gid, p->i_size, p->i_atime, p->i_ctime, p->i_mtime, p->i_gen, p->i_rdev);
		pwd(ino);
	}
}
