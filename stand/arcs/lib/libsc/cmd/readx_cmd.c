#include <arcs/errno.h>
#include <arcs/io.h>
#include <libsc.h>

extern char filebuf[2048];
extern int filelength(int fid);
readx(argc, argv)
int argc;
char **argv;
{
    LONG cnt, i;
    ULONG fid, total = 0;
    LARGE off;

    if (argc < 4)
	return -1;

    argv++;
    if (Open((CHAR *)*argv, OpenReadOnly, &fid) != ESUCCESS) {
	printf("\nError Opening File %s", *argv);
	return 0;
    }

    /* seek offset */
    argv++;
    off.hi = 0;
    atob(*argv, (int *)&off.lo);
    Seek(fid, &off, SeekAbsolute);

    argv++;
    atob(*argv, (int *)&cnt);
    if (cnt > sizeof(filebuf)) {
	printf("\n Count Too Large");
    }
    else {
      if (Read(fid, (void *)filebuf, cnt, &total) != ESUCCESS) {
	    printf("\nError Reading File %s\n", *argv);
        }
	for (i =0; i < total; i++)
	    putchar(filebuf[i]);

    }

    Close(fid);
    printf("\nNumber of Characters Read = %d\n", total);
    return 0;
}
