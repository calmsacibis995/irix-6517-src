#ifndef _UNZIP_H
#define _UNZIP_H

/*
 * unzip -- Compact standalone Gzip decompression module
 *
 *   Decompresses a file in Gzip (.gz) format
 *
 *   On success, returns length of data written to output.
 *   On error, returns a negative error code.
 *
 *   Typical tmp buffer size requirements:
 *	0x20000 for a 120 KB uncompressed file.
 *	0x80000 for a 3 MB uncompressed file.
 */

int unzip(int (*my_getc)(void),
	  void (*my_putc)(int c),
	  char *tmp, int tmpsize);

char *unzip_errmsg(int errcode);

#define UNZIP_ERROR_NONE	0
#define UNZIP_ERROR_MAGIC      -1
#define UNZIP_ERROR_FORMAT     -2
#define UNZIP_ERROR_MEMORY     -3
#define UNZIP_ERROR_CRC	       -4
#define UNZIP_ERROR_LENGTH     -5

#endif /* _UNZIP_H */
