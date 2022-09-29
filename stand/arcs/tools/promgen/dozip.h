#ifndef _DOZIP_H
#define _DOZIP_H

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

int dozip(int (*my_getc)(void),
	  void (*my_putc)(int c));

#endif /* _DOZIP_H */
