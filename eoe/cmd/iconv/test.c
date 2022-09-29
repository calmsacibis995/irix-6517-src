/*
 * test.c
 *  A small test for checking mbwc iconv functionality.
 */

#include <stdio.h>
#include <nl_types.h>
#include <locale.h>
#include <langinfo.h>

char    ** __mbwc_gettable();

main( int argc, char ** argv )
{
    char	** pp_s = __mbwc_gettable();

    if ( ! pp_s ) {
	perror( "Error initializing" );
    }
    
    if ( ! setlocale(LC_ALL, "") ) {
        perror( "Error setting locale" );
    } else {
	printf( "%s\n", nl_langinfo( CODESET ) );
    }

    if ( pp_s ) while ( * pp_s ) {
	fprintf( stderr, "'%s' - '%s' ", pp_s[ 0 ], pp_s[ 1 ] );
	
	if ( __mbwc_setmbstate( pp_s[ 0 ] ) ) {
	    perror( "Error" );
	} else {
	    fprintf( stderr, "\n" );
	}
	pp_s += 2;
    }
}
