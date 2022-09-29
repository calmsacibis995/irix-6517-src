/* basic functionality needed to call inflate() and unlzw() from gzip-1.2.4 */

#include <stdio.h>
#include <malloc.h>
#include <setjmp.h>

#include "gzip.h"
#include "lzw.h"

/* global buffers */

unsigned char  *inbuf;
unsigned char  *outbuf;

/* used by unlzw() */
unsigned short *d_buf;
unsigned char  *tab_suffix;
unsigned short *tab_prefix;

long bytes_out;			/* number of output bytes */
unsigned insize;		/* valid bytes in inbuf */
unsigned inptr;			/* idx of next byte to be processed in inbuf */
unsigned outcnt;		/* bytes in output buffer */
unsigned outbufsiz;		/* size of output buffer */
int exit_code = OK;		/* program exit code */
int maxbits = BITS;		/* max bits per code for LZW */

static jmp_buf packjmp;

int
ungzip(char *src, int srccnt, char *dest, int destcnt)
{
	int method;
	uch flags;
	int ret;
	extern void *td, *tl;
	extern huft_free(void *);

	/* Clear output buffer */
	outbuf = (unsigned char *)dest;
	outbufsiz = destcnt;
	outcnt = bytes_out = 0L;
	tl = td = NULL;	/* dynamic memory used by inflate */

	/* initialize input buffer */
	inbuf = (unsigned char *)src;
	insize = srccnt;
	inptr = 0;

	if (ret=setjmp(packjmp)) {
		if (ret == 2)	/* error */
			return -1;
		outbuf[outcnt-1] = '\0';
		if (tl) huft_free(tl);
		if (td) huft_free(td);
		return outcnt;
	}

	/* skip magic number */
	get_byte();
	get_byte();

	method = (int)get_byte();
	flags = (uch)get_byte();
	if (method != DEFLATED ||
	    (flags & ENCRYPTED) != 0 ||
	    (flags & CONTINUATION) != 0 ||
	    (flags & RESERVED) != 0) {
		fprintf(stderr, "bad gzip format\n");
		return -1;
	}
	/* skip timestamp */
	get_byte();
	get_byte();
	get_byte();
	get_byte();
	/* skip extra flags and OS type */
	get_byte();
	get_byte();

	if ((flags & EXTRA_FIELD) != 0) {
		unsigned len = (unsigned)get_byte();
		len |= ((unsigned)get_byte())<<8;
		while (len--) (void)get_byte();
	}
	if ((flags & ORIG_NAME) != 0) {
		/* Discard the old name */
		while (get_byte() != 0) /* null */ ;
	}
	if ((flags & COMMENT) != 0) {
		/* Discard file comment if any */
		while (get_byte() != 0) /* null */ ;
	}

	if (inflate() != 0)
		return -1;

	longjmp(packjmp, 1);
	/*NOTREACHED*/
}


int
uncompress(char *src, int srccnt, char *dest, int destcnt)
{
	int ret;

	if (d_buf == NULL) {
	    d_buf = malloc(DIST_BUFSIZE * sizeof(short));
	    tab_suffix = malloc(WSIZE * sizeof(char));
	    tab_prefix = malloc((1L<<BITS) * sizeof(short));
	}

	/* Clear output buffer */
	outbuf = (unsigned char *)dest;
	outbufsiz = destcnt;
	outcnt = bytes_out = 0L;

	/* initialize input buffer */
	inbuf = (unsigned char *)src;
	insize = srccnt;
	inptr = 0;

	if (ret=setjmp(packjmp)) {
		if (ret == 2)	/* error */
			return -1;
		outbuf[outcnt-1] = '\0';
		return outcnt;
	}

	/* skip magic number */
	get_byte();
	get_byte();

	if (unlzw(0,0) != OK)
		return -1;

	longjmp(packjmp, 1);
	/*NOTREACHED*/
}

/*
 * ======== from gzip/util.c ========
 */

/*
 * Fill the input buffer. This is called only when the buffer is empty.
 */
/*ARGSUSED*/
int fill_inbuf(
	int eof_ok)          /* set if EOF acceptable as a result */
{
	longjmp(packjmp, 1);
	/*NOTREACHED*/
}


/*
 * Error handlers.
 */
/*ARGSUSED*/
void error(char *m)
{
	longjmp(packjmp, 2);
}


void flush_outbuf(void)
{
	if (outcnt == 0) return;
	longjmp(packjmp, 1);
}

/*
 * used by unlzw()
 */
/*ARGSUSED*/
void write_buf(int fd, voidp buf, unsigned cnt)
{
	outcnt = cnt;
	longjmp(packjmp, 1);
}
