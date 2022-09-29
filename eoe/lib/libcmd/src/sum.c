/*
 * sum.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.3 $"

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <std.h>
#include <sum.h>
#define	MSW( l )	( ((l) >> 16) & 0x0000ffffL )
#define	LSW( l )	( (l) & 0x0000ffffL )

/***	sumpro -- prolog
 *
 */
void
sumpro(register struct suminfo *sip)
{
    sip->si_sum = sip->si_nbytes = 0L;
    return;
}

/***	sumupd -- update
 *
 */
void
sumupd(struct suminfo *sip, register char *buf, register int cnt)
{
    register long sum;

    if ( cnt <= 0 )
	return;
    sip->si_nbytes += cnt;
    sum = sip->si_sum;
    while ( cnt-- > 0 )
	sum += *buf++ & 0x00ff;
    sip->si_sum = sum;
    return;
}

/***	sumepi -- epilog
 *
 */
void
sumepi(register struct suminfo *sip)
{
    register long sum;

    sum = sip->si_sum;
    sum = LSW( sum ) + MSW( sum );
    sip->si_sum = (ushort) (LSW( sum ) + MSW( sum ));
    return;
}

/***	sumout -- output
 *
 */
void
sumout(FILE *fp, struct suminfo *sip)
{
#ifdef	M_V7
#define	FMT	"%u\t%D"
#else
#define	FMT	"%u\t%ld"
#endif
    fprintf(
	fp, FMT,
	(unsigned) sip->si_sum,
	(sip->si_nbytes + MULBSIZE - 1) / MULBSIZE
    );
#undef	FMT
    return;
}

#ifdef	BLACKBOX
char	*Pgm		= "tstsum";
char	Enoopen[]	= "cannot open %s\n";
char	Ebadread[]	= "read error on %s\n";

int
main( argc, argv )
int argc;
char **argv;
{
    char *pn;
    FILE *fp;
    int cnt;
    struct suminfo si;
    char buf[BUFSIZ];

    --argc; ++argv;
    for ( ; argc > 0; --argc, ++argv ) {
	pn = *argv;
	if ( (fp = fopen( pn, "r" )) == NULL ) {
	    error( Enoopen, pn );
	    continue;
	}
	sumpro( &si );
	while ( (cnt = fread( buf, sizeof(char), BUFSIZ, fp )) != 0 && cnt != EOF )
	    sumupd( &si, buf, cnt );
	if ( cnt == EOF && ferror( fp ) )
	    error( Ebadread, pn );
	sumepi( &si );
	sumout( stdout, &si );
	printf( "\t%s\n", pn );
	fclose( fp );
    }
    exit( 0 );
}

error( fmt, a1, a2, a3, a4, a5 )
char *fmt;
{
    fprintf( stderr, "%s: ", Pgm );
    fprintf( stderr, fmt, a1, a2, a3, a4, a5 );
    return;
}
#endif
