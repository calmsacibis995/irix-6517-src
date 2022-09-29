#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "dozip.h"

/*
 * dozip -- Gzip compression module
 *
 *   Compresses a file into Gzip (.gz) format
 *
 *   This is a matching counterpart to the unzip module, however it's not
 *   a standlone module.  It uses temp files and uses system() to call gzip.
 *
 *   On success, returns length of data written to output.
 *   On error, returns -1.
 *
 *   Notes:  Temp files may be left around if compression is aborted.
 */

#define GZIP_COMMAND	"gzip"

int dozip(int (*my_getc)(void),
	  void (*my_putc)(int c))
{
    char	gzip_in[256];
    char	gzip_out[256];
    char	gzip_cmd[256];
    FILE       *fp;
    int		c, r;
    int		gz_size;

    tmpnam(gzip_in);
    tmpnam(gzip_out);

    r	= -1;
    fp	= 0;

    /*
     * Write the input data to a temporary file.
     */

    if ((fp = fopen(gzip_in, "w")) == 0)
	goto done;

    while ((c = (*my_getc)()) != EOF)
	putc(c, fp);

    if (fclose(fp) < 0)
	goto done;
    else
	fp = 0;

    /*
     * Compress the temporary file.
     */

    sprintf(gzip_cmd, "%s < %s > %s", GZIP_COMMAND, gzip_in, gzip_out);

    if ((r = system(gzip_cmd)) < 0 || ((r >> 8) & 255) != 0)
	goto done;

    /*
     * Read the compressed file and pass it to output.
     */

    if ((fp = fopen(gzip_out, "r")) == 0)
	goto done;

    gz_size = 0;

    while ((c = getc(fp)) != EOF) {
	(*my_putc)(c);
	gz_size++;
    }

    (void) fclose(fp);
    fp = 0;

    r = 0;

 done:

    if (fp)
	fclose(fp);

    (void) unlink(gzip_in);
    (void) unlink(gzip_out);

    return (r < 0) ? r : gz_size;
}

#ifdef TEST_MAIN
#include <stdio.h>

int my_getc(void) { return getchar(); }
void my_putc(int c) { putchar(c); }

main()
{
    int	r;

    if ((r = dozip(my_getc, my_putc)) < 0) {
	fprintf(stderr, "%d\n", r);
	exit(1);
    }

    exit(0);
}
#endif /* TEST_MAIN */
