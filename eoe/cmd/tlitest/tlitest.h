#include <ctype.h>
#include <tiuser.h>
#define DGRAMTESTPORT 12345
#define STREAMTESTPORT 12345

#define MYLINEMAX 64
#define MYMAXLEN (MYLINEMAX * 4)

int 
min( int a, int b)
{
	return ((a < b) ? a : b);
}

void
printbuf( char * hdr, char * buf, int buflen)
{
	char prnbuf[120];
	static const char hexchars[] = "0123456789ABCDEF";

	buflen = min( buflen, MYMAXLEN);
	while (buflen > 0) {
		char * cp;
		int linelen;

		cp = prnbuf;
		linelen = min( buflen, MYLINEMAX );
		buflen -= linelen;
		while ( linelen-- > 0 && (cp - prnbuf) < MYLINEMAX ) {
			int byte;

			byte = *buf++;
			if (isprint(byte))
				*cp++ = byte;
			else {
				*cp++ = '[';
				*cp++ = hexchars[ (byte >> 4) & 0xf ];
				*cp++ = hexchars[ byte & 0xf ];
				*cp++ = ']';
			}
		}
		*cp   = 0;
		printf("%s%s\n", hdr, prnbuf);
	}
}

void
print_tlook( int event)
{
	char ** erp;
	static char *terrs[] = {
		"connection indication received ",
		"connect confirmation received  ",
		"normal data received",
		"expedited data received",
		"disconnect received",
		"fatal error occurred",
		"data gram error indication",
		"orderly release indication",
		0};

	event &= T_EVENTS;
	for (erp = terrs; event; erp++) {
		if (event & 1)
			fprintf(stderr, "%s\n", *erp);
		event >>= 1;
	}
}
