#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <string.h>
#include <assert.h>
#include "defaults.h"
#include "parameters.h"

char *OptTokens[] = {
	"file",
	"dir",
	"processes",
	"filesize",
	"access",
	"pausetime",
	"recsize",
	"repeat",
	"montime",
	"locktimeo",
	NULL
};

struct parameters OptDefaults = {
	DEFAULT_FILE,
	DOT,
	DEFAULT_FILE_SIZE,
	DEFAULT_PROCESSES,
	DEFAULT_ACCESS,
	DEFAULT_PAUSE,
	{DEFAULT_RECSIZE, DEFAULT_RECSIZE},
	DEFAULT_REPEAT,
	DEFAULT_MONTIME,
	DEFAULT_LOCKTIMEO,
	DEFAULT_SIGNUM,
	DEFAULT_KILLTIME
};

extern char *Progname;
extern int Verbose;
extern int errno;

int
valid_parameters( struct parameters *paramp )
{
	assert( valid_addresses( (caddr_t)paramp, sizeof( struct parameters ) ) );
	assert( valid_addresses( (caddr_t)paramp->p_file, 1 ) );
	assert( valid_addresses( (caddr_t)paramp->p_dir, 1 ) );
}

void
error( pid_t pid, char *func, char *syscall, char *msg )
{
	int status = errno;

	if ( pid >= 0 ) {
		fprintf( stderr, "%s[%d] %s: ", Progname, pid, func );
	} else {
		fprintf( stderr, "%s %s: ", Progname, func );
	}
	if ( msg ) {
		fprintf( stderr, "%s: ", msg );
	}
	if ( syscall ) {
		if ( status ) {
			perror( syscall );
		} else {
			fprintf( stderr, "%s failed\n", syscall );
		}
	} else {
		fprintf( stderr, "\n" );
	}
	fflush( stderr );
}

enum access_method
str_to_access( char *str )
{
	enum access_method method;

	assert( valid_addresses( str, 1 ) );
	if ( strcmp( str, "random" ) == 0 ) {
		method = RANDOM;
	} else if ( strcmp( str, "sequential" ) == 0 ) {
		method = SEQUENTIAL;
	} else {
		method = BADMETHOD;
	}
	return( method );
}

int
get_parameters( char *paramstr, struct parameters *paramp )
{
	int status = 0;
	char *token;
	char *optval;

	assert( valid_addresses( (caddr_t)paramstr, 1 ) );
	assert( valid_addresses( (caddr_t)paramp, sizeof( struct parameters ) ) );
	while ( paramstr && *paramstr ) {
		switch( getsubopt( &paramstr, OptTokens, &optval ) ) {
			case FILENAME:
				paramp->p_file = optval;
				break;
			case DIRNAME:
				paramp->p_dir = optval;
				break;
			case PROCESSES:
				paramp->p_processes = atol( optval );
				break;
			case FILESIZE:
				paramp->p_filesize = atol( optval );
				break;
			case ACCESS:
				if ( (paramp->p_access = str_to_access( optval )) ==
					BADMETHOD ) {
						fprintf( stderr,
							"%s: access method %s is not legal\n",
							Progname, optval );
						fflush( stderr );
						exit( ERROREXIT );
				}
				break;
			case PAUSETIME:
				paramp->p_pausetime = atol( optval );
				break;
			case RECSIZE:
				if ( token = strtok( optval, "-" ) ) {
					paramp->p_recsize.r_low = atol( token );
					if ( token = strtok( NULL, "" ) ) {
						paramp->p_recsize.r_high = atol( token );
					} else {
						paramp->p_recsize.r_high = paramp->p_recsize.r_low;
					}
				} else {
					paramp->p_recsize.r_high = paramp->p_recsize.r_low =
						atol( optval );
				}
				break;
			case REPEAT:
				paramp->p_repeat = atol( optval );
				break;
			case MONTIME:
				paramp->p_montime = atol( optval );
				break;
			case LOCKTIMEO:
				paramp->p_locktimeout = atol( optval );
				break;
			default:
				fprintf( stderr, "%s: unknown parameter %s\n", Progname,
					optval );
				status = ERROREXIT;
		}
	}
	return( status );
}
