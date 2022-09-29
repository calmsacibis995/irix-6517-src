#ident	"lib/libsc/cmd/menu_cmd.c:  $Revision: 1.34 $"

/*
 *
 * The menu mechanics are in lib/menu.c, this file defines the contents
 * and implementation of the menu.
 *
 */

#include "setjmp.h"
#include <menu.h>
#include <parser.h>
#include <libsc.h>
#include <ctype.h>
#include <libsc.h>
#include <libsc_internal.h>

extern jmp_buf restart_buf;

/*
 * Find the installation tape
 *
 */
int
sa_get_response( char *rbuf )
{
	int c, i, j;

	i = 0;
	/*
	** duplicate the line discipline to add escape.
	*/
	p_curson ();
	do {
		c = getchar();
		switch( c ) {
			case '\n':
			case '\r':
				putchar( '\n' );
				rbuf[i] = '\000';
				return( 0 );
			case '\004':
			case '\033':
				longjmp( restart_buf, 1);
				/* should not get here */
				break;
			case '\025':
				for (j=0;j<i;j++) {
					rbuf[j] = '\000';
					putchar( '\b' );
				        putchar( ' ' );
				        putchar( '\b' );
				}
				i = 0;
				break;
			case '\b':
			case 0x7f: /* DEL */
				if (i>0) {
				    putchar( '\b' );
				    putchar( ' ' );
				    putchar( '\b' );
				    i = i -1;
				}
				rbuf[i] = '\000';
				break;
			default:
				rbuf[i++] = (char)c;
				rbuf[i] = '\000';
				putchar( c );
				break;
		}
	} while (i<LINESIZE-1);
	rbuf[LINESIZE-1] = '\000';
	return( 0 );
}

int
token (char *str, char *first_token)
{
	int number_of_token = 0;

	for (;;) {
		while (isspace (*str)) 
			str++;
		
		if (*str == '\000')
			return (number_of_token);
			
		while (*str && !isspace (*str)) {
			if (number_of_token == 0)
				*first_token++ = *str;
			str++;
		}   /* while */
		if (!number_of_token)
			*first_token = '\000';
		number_of_token++;
	}   /* while */
	/*NOTREACHED*/
	return 0;
}   /* token */
