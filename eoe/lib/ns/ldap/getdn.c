/*
 *  Copyright (c) 1994 Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  getdn.c
 */

#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef MACOS
#include <stdlib.h>
#include "macos.h"
#else /* MACOS */
#if defined( DOS ) || defined( _WIN32 )
#include <malloc.h>
#include "msdos.h"
#else /* DOS */
#include <sys/types.h>
#include <sys/socket.h>
#endif /* DOS */
#endif /* MACOS */

#include "lber.h"
#include "ldap.h"

char *
ldap_get_dn( LDAP *ld, LDAPMessage *entry )
{
	char		*dn;
	BerElement	tmp;

	Debug( LDAP_DEBUG_TRACE, "ldap_get_dn\n", 0, 0, 0 );

	if ( entry == NULL ) {
		ld->ld_errno = LDAP_PARAM_ERROR;
		return( NULL );
	}

	tmp = *entry->lm_ber;	/* struct copy */
	if ( ber_scanf( &tmp, "{a", &dn ) == LBER_ERROR ) {
		ld->ld_errno = LDAP_DECODING_ERROR;
		return( NULL );
	}

	return( dn );
}

char *
ldap_dn2ufn( char *dn )
{
	char	*p, *ufn, *r;
	int	state;

	Debug( LDAP_DEBUG_TRACE, "ldap_dn2ufn\n", 0, 0, 0 );

	if ( ldap_is_dns_dn( dn ) || ( p = strchr( dn, '=' )) == NULL )
		return( strdup( dn ));

	ufn = strdup( ++p );

#define INQUOTE		1
#define OUTQUOTE	2
	state = OUTQUOTE;
	for ( p = ufn, r = ufn; *p; p++ ) {
		switch ( *p ) {
		case '\\':
			if ( *++p == '\0' )
				p--;
			else {
				*r++ = '\\';
				*r++ = *p;
			}
			break;
		case '"':
			if ( state == INQUOTE )
				state = OUTQUOTE;
			else
				state = INQUOTE;
			*r++ = *p;
			break;
		case ';':
		case ',':
			if ( state == OUTQUOTE )
				*r++ = ',';
			else
				*r++ = *p;
			break;
		case '=':
			if ( state == INQUOTE )
				*r++ = *p;
			else {
				char	*rsave = r;

				*r-- = '\0';
				while ( !isspace( *r ) && *r != ';'
				    && *r != ',' && r > ufn )
					r--;
				r++;

				if ( strcasecmp( r, "c" )
				    && strcasecmp( r, "o" )
				    && strcasecmp( r, "ou" )
				    && strcasecmp( r, "st" )
				    && strcasecmp( r, "l" )
				    && strcasecmp( r, "cn" ) ) {
					r = rsave;
					*r++ = '=';
				}
			}
			break;
		default:
			*r++ = *p;
			break;
		}
	}
	*r = '\0';

	return( ufn );
}

char **
ldap_explode_dns( char *dn )
{
	int	ncomps, maxcomps;
	char	*s;
	char	**rdns;

	if ( (rdns = (char **) malloc( 8 * sizeof(char *) )) == NULL ) {
		return( NULL );
	}

	maxcomps = 8;
	ncomps = 0;
	for ( s = strtok( dn, "@." ); s != NULL; s = strtok( NULL, "@." ) ) {
		if ( ncomps == maxcomps ) {
			maxcomps *= 2;
			if ( (rdns = (char **) realloc( rdns, maxcomps *
			    sizeof(char *) )) == NULL ) {
				return( NULL );
			}
		}
		rdns[ncomps++] = strdup( s );
	}
	rdns[ncomps] = NULL;

	return( rdns );
}

char **
ldap_explode_dn( char *dn, int notypes )
{
	char	*p, *q, *rdnstart, **rdns = NULL;
	int	state, count = 0, endquote, len;

	Debug( LDAP_DEBUG_TRACE, "ldap_explode_dn\n", 0, 0, 0 );

	if ( ldap_is_dns_dn( dn ) ) {
		return( ldap_explode_dns( dn ) );
	}

	rdnstart = dn;
	p = dn-1;
	state = OUTQUOTE;

	do {

		++p;
		switch ( *p ) {
		case '\\':
			if ( *++p == '\0' )
				p--;
			break;
		case '"':
			if ( state == INQUOTE )
				state = OUTQUOTE;
			else
				state = INQUOTE;
			break;
		case ';':
		case ',':
		case '\0':
			if ( state == OUTQUOTE ) {
				++count;
				if ( rdns == NULL ) {
					if (( rdns = (char **)malloc( 8
						 * sizeof( char *))) == NULL )
						return( NULL );
				} else if ( count >= 8 ) {
					if (( rdns = (char **)realloc( rdns,
						(count+1) * sizeof( char *)))
						== NULL )
						return( NULL );
				}
				rdns[ count ] = NULL;
				endquote = 0;
				if ( notypes ) {
					for ( q = rdnstart;
					    q < p && *q != '='; ++q ) {
						;
					}
					if ( q < p ) {
						rdnstart = ++q;
					}
					if ( *rdnstart == '"' ) {
						++rdnstart;
					}
					
					if ( *(p-1) == '"' ) {
						endquote = 1;
						--p;
					}
				}

				len = p - rdnstart;
				if (( rdns[ count-1 ] = (char *)calloc( 1,
				    len + 1 )) != NULL ) {
				    	SAFEMEMCPY( rdns[ count-1 ], rdnstart,
					    len );
					rdns[ count-1 ][ len ] = '\0';
				}

				/*
				 *  Don't forget to increment 'p' back to where
				 *  it should be.  If we don't, then we will
				 *  never get past an "end quote."
				 */
				if ( endquote == 1 )
					p++;

				rdnstart = *p ? p + 1 : p;
				while ( isspace( *rdnstart ))
					++rdnstart;
			}
			break;
		}
	} while ( *p );

	return( rdns );
}


int
ldap_is_dns_dn( char *dn )
{
	return( dn[ 0 ] != '\0' && strchr( dn, '=' ) == NULL &&
	    strchr( dn, ',' ) == NULL );
}


#if defined( ultrix ) || defined( NeXT )

char *strdup( char *s )
{
	char	*p;

	if ( (p = (char *) malloc( strlen( s ) + 1 )) == NULL )
		return( NULL );

	strcpy( p, s );

	return( p );
}

#endif /* ultrix */
