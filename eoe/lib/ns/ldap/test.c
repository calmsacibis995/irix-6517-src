#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>


#include "lber.h"
#include "ldap.h"

/* including the "internal" defs is legit and nec. since this test routine has 
 * a-priori knowledge of libldap internal workings.
 * hodges@stanford.edu 5-Feb-96
 */
#include "ldap-int.h"

#if !defined( PCNFS ) && !defined( WINSOCK ) && !defined( MACOS )
#define MOD_USE_BVALS
#endif /* !PCNFS && !WINSOCK && !MACOS */

#ifdef NEEDPROTOS
static void handle_result( LDAP *ld, LDAPMessage *lm );
static void print_ldap_result( LDAP *ld, LDAPMessage *lm, char *s );
static void print_search_entry( LDAP *ld, LDAPMessage *res );
static void free_list( char **list );
#else
static void handle_result();
static void print_ldap_result();
static void print_search_entry();
static void free_list();
#endif /* NEEDPROTOS */

#define NOCACHEERRMSG	"don't compile with -DNO_CACHE if you desire local caching"

char *dnsuffix;

#ifndef WINSOCK
static char *
getline( char *line, int len, FILE *fp, char *prompt )
{
	printf(prompt);

	if ( fgets( line, len, fp ) == NULL )
		return( NULL );

	line[ strlen( line ) - 1 ] = '\0';

	return( line );
}
#endif /* WINSOCK */

static char **
get_list( char *prompt )
{
	static char	buf[256];
	int		num;
	char		**result;

	num = 0;
	result = (char **) 0;
	while ( 1 ) {
		getline( buf, sizeof(buf), stdin, prompt );

		if ( *buf == '\0' )
			break;

		if ( result == (char **) 0 )
			result = (char **) malloc( sizeof(char *) );
		else
			result = (char **) realloc( result,
			    sizeof(char *) * (num + 1) );

		result[num++] = (char *) strdup( buf );
	}
	if ( result == (char **) 0 )
		return( NULL );
	result = (char **) realloc( result, sizeof(char *) * (num + 1) );
	result[num] = NULL;

	return( result );
}


static void
free_list( char **list )
{
	int	i;

	if ( list != NULL ) {
		for ( i = 0; list[ i ] != NULL; ++i ) {
			free( list[ i ] );
		}
		free( (char *)list );
	}
}


#ifdef MOD_USE_BVALS
static int
file_read( char *path, struct berval *bv )
{
	FILE		*fp;
	long		rlen;
	int		eof;

	if (( fp = fopen( path, "r" )) == NULL ) {
	    	perror( path );
		return( -1 );
	}

	if ( fseek( fp, 0L, SEEK_END ) != 0 ) {
		perror( path );
		fclose( fp );
		return( -1 );
	}

	bv->bv_len = ftell( fp );

	if (( bv->bv_val = (char *)malloc( bv->bv_len )) == NULL ) {
		perror( "malloc" );
		fclose( fp );
		return( -1 );
	}

	if ( fseek( fp, 0L, SEEK_SET ) != 0 ) {
		perror( path );
		fclose( fp );
		return( -1 );
	}

	rlen = fread( bv->bv_val, 1, bv->bv_len, fp );
	eof = feof( fp );
	fclose( fp );

	if ( rlen != bv->bv_len ) {
		perror( path );
		free( bv->bv_val );
		return( -1 );
	}

	return( bv->bv_len );
}
#endif /* MOD_USE_BVALS */


static LDAPMod **
get_modlist( char *prompt1, char *prompt2, char *prompt3 )
{
	static char	buf[256];
	int		num;
	LDAPMod		tmp;
	LDAPMod		**result;
#ifdef MOD_USE_BVALS
	struct berval	**bvals;
#endif /* MOD_USE_BVALS */

	num = 0;
	result = NULL;
	while ( 1 ) {
		if ( prompt1 ) {
			getline( buf, sizeof(buf), stdin, prompt1 );
			tmp.mod_op = atoi( buf );

			if ( tmp.mod_op == -1 || buf[0] == '\0' )
				break;
		}

		getline( buf, sizeof(buf), stdin, prompt2 );
		if ( buf[0] == '\0' )
			break;
		tmp.mod_type = strdup( buf );

		tmp.mod_values = get_list( prompt3 );
#ifdef MOD_USE_BVALS
		if ( tmp.mod_values != NULL ) {
			int	i;

			for ( i = 0; tmp.mod_values[i] != NULL; ++i )
				;
			bvals = (struct berval **)calloc( i + 1,
			    sizeof( struct berval *));
			for ( i = 0; tmp.mod_values[i] != NULL; ++i ) {
				bvals[i] = (struct berval *)malloc(
				    sizeof( struct berval ));
				if ( strncmp( tmp.mod_values[i], "{FILE}",
				    6 ) == 0 ) {
					if ( file_read( tmp.mod_values[i] + 6,
					    bvals[i] ) < 0 ) {
						return( NULL );
					}
				} else {
					bvals[i]->bv_val = tmp.mod_values[i];
					bvals[i]->bv_len =
					    strlen( tmp.mod_values[i] );
				}
			}
			tmp.mod_bvalues = bvals;
			tmp.mod_op |= LDAP_MOD_BVALUES;
		}
#endif /* MOD_USE_BVALS */

		if ( result == NULL )
			result = (LDAPMod **) malloc( sizeof(LDAPMod *) );
		else
			result = (LDAPMod **) realloc( result,
			    sizeof(LDAPMod *) * (num + 1) );

		result[num] = (LDAPMod *) malloc( sizeof(LDAPMod) );
		*(result[num]) = tmp;	/* struct copy */
		num++;
	}
	if ( result == NULL )
		return( NULL );
	result = (LDAPMod **) realloc( result, sizeof(LDAPMod *) * (num + 1) );
	result[num] = NULL;

	return( result );
}


#ifdef LDAP_REFERRALS
int
bind_prompt( LDAP *ld, char **dnp, char **passwdp, int *authmethodp,
	int freeit )
{
	static char	dn[256], passwd[256];

	if ( !freeit ) {
#ifdef KERBEROS
		getline( dn, sizeof(dn), stdin,
		    "re-bind method (0->simple, 1->krbv41, 2->krbv42, 3->krbv41&2)? " );
		if (( *authmethodp = atoi( dn )) == 3 ) {
			*authmethodp = LDAP_AUTH_KRBV4;
		} else {
			*authmethodp |= 0x80;
		}
#else /* KERBEROS */
		*authmethodp = LDAP_AUTH_SIMPLE;
#endif /* KERBEROS */

		getline( dn, sizeof(dn), stdin, "re-bind dn? " );
		strcat( dn, dnsuffix );
		*dnp = dn;

		if ( *authmethodp == LDAP_AUTH_SIMPLE && dn[0] != '\0' ) {
			getline( passwd, sizeof(passwd), stdin,
			    "re-bind password? " );
		} else {
			passwd[0] = '\0';
		}
		*passwdp = passwd;
	}

	return( LDAP_SUCCESS );
}
#endif /* LDAP_REFERRALS */


int
#ifdef WINSOCK
ldapmain(
#else /* WINSOCK */
main(
#endif /* WINSOCK */
	int argc, char **argv )
{
	LDAP		*ld;
	int		i, c, port, cldapflg, errflg, method, id, msgtype;
	char		line[256], command1, command2, command3;
	char		passwd[64], dn[256], rdn[64], attr[64], value[256];
	char		filter[256], *host, **types;
	char		**exdn;
	char		*usage = "usage: %s [-u] [-h host] [-d level] [-s dnsuffix] [-p port] [-t file] [-T file]\n";
	int		bound, all, scope, attrsonly;
	LDAPMessage	*res;
	LDAPMod		**mods, **attrs;
	struct timeval	timeout;
	char		*copyfname = NULL;
	int		copyoptions = 0;
	LDAPURLDesc	*ludp;

	extern char	*optarg;
	extern int	optind;

#ifdef MACOS
	if (( argv = get_list( "cmd line arg?" )) == NULL ) {
		exit( 1 );
	}
	for ( argc = 0; argv[ argc ] != NULL; ++argc ) {
		;
	}
#endif /* MACOS */

	host = NULL;
	port = LDAP_PORT;
	dnsuffix = "";
	cldapflg = errflg = 0;

	while (( c = getopt( argc, argv, "uh:d:s:p:t:T:" )) != -1 ) {
		switch( c ) {
		case 'u':
#ifdef CLDAP
			cldapflg++;
#else /* CLDAP */
			printf( "Compile with -DCLDAP for UDP support\n" );
#endif /* CLDAP */
			break;

		case 'd':
#ifdef LDAP_DEBUG
			ldap_debug = atoi( optarg );
			if ( ldap_debug & LDAP_DEBUG_PACKETS ) {
				lber_debug = ldap_debug;
			}
#else
			printf( "Compile with -DLDAP_DEBUG for debugging\n" );
#endif
			break;

		case 'h':
			host = optarg;
			break;

		case 's':
			dnsuffix = optarg;
			break;

		case 'p':
			port = atoi( optarg );
			break;

#if !defined(MACOS) && !defined(DOS)
		case 't':	/* copy ber's to given file */
			copyfname = strdup( optarg );
			copyoptions = LBER_TO_FILE;
			break;

		case 'T':	/* only output ber's to given file */
			copyfname = strdup( optarg );
			copyoptions = (LBER_TO_FILE | LBER_TO_FILE_ONLY);
			break;
#endif

		default:
		    ++errflg;
		}
	}

	if ( host == NULL && optind == argc - 1 ) {
		host = argv[ optind ];
		++optind;
	}

	if ( errflg || optind < argc - 1 ) {
		fprintf( stderr, usage, argv[ 0 ] );
		exit( 1 );
	}
	
	printf( "%sldap_open( %s, %d )\n", cldapflg ? "c" : "",
		host == NULL ? "(null)" : host, port );

	if ( cldapflg ) {
#ifdef CLDAP
		ld = cldap_open( host, port );
#endif /* CLDAP */
	} else {
		ld = ldap_open( host, port );
	}

	if ( ld == NULL ) {
		perror( "ldap_open" );
		exit(1);
	}

#if !defined(MACOS) && !defined(DOS)
	if ( copyfname != NULL ) {
		if ( (ld->ld_sb.sb_fd = open( copyfname, O_WRONLY | O_CREAT,
		    0600 ))  == -1 ) {
			perror( copyfname );
			exit ( 1 );
		}
		ld->ld_sb.sb_options = copyoptions;
	}
#endif

	bound = 0;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	(void) memset( line, '\0', sizeof(line) );
	while ( getline( line, sizeof(line), stdin, "\ncommand? " ) != NULL ) {
		command1 = line[0];
		command2 = line[1];
		command3 = line[2];

		switch ( command1 ) {
		case 'a':	/* add or abandon */
			switch ( command2 ) {
			case 'd':	/* add */
				getline( dn, sizeof(dn), stdin, "dn? " );
				strcat( dn, dnsuffix );
				if ( (attrs = get_modlist( NULL, "attr? ",
				    "value? " )) == NULL )
					break;
				if ( (id = ldap_add( ld, dn, attrs )) == -1 )
					ldap_perror( ld, "ldap_add" );
				else
					printf( "Add initiated with id %d\n",
					    id );
				break;

			case 'b':	/* abandon */
				getline( line, sizeof(line), stdin, "msgid? " );
				id = atoi( line );
				if ( ldap_abandon( ld, id ) != 0 )
					ldap_perror( ld, "ldap_abandon" );
				else
					printf( "Abandon successful\n" );
				break;
			default:
				printf( "Possibilities: [ad]d, [ab]ort\n" );
			}
			break;

		case 'b':	/* asynch bind */
#ifdef KERBEROS
			getline( line, sizeof(line), stdin,
			    "method (0->simple, 1->krbv41, 2->krbv42)? " );
			method = atoi( line ) | 0x80;
#else /* KERBEROS */
			method = LDAP_AUTH_SIMPLE;
#endif /* KERBEROS */
			getline( dn, sizeof(dn), stdin, "dn? " );
			strcat( dn, dnsuffix );

			if ( method == LDAP_AUTH_SIMPLE && dn[0] != '\0' )
				getline( passwd, sizeof(passwd), stdin,
				    "password? " );
			else
				passwd[0] = '\0';

			if ( ldap_bind( ld, dn, passwd, method ) == -1 ) {
				fprintf( stderr, "ldap_bind failed\n" );
				ldap_perror( ld, "ldap_bind" );
			} else {
				printf( "Bind initiated\n" );
				bound = 1;
			}
			break;

		case 'B':	/* synch bind */
#ifdef KERBEROS
			getline( line, sizeof(line), stdin,
			    "method 0->simple 1->krbv41 2->krbv42 3->krb? " );
			method = atoi( line );
			if ( method == 3 )
				method = LDAP_AUTH_KRBV4;
			else
				method = method | 0x80;
#else /* KERBEROS */
			method = LDAP_AUTH_SIMPLE;
#endif /* KERBEROS */
			getline( dn, sizeof(dn), stdin, "dn? " );
			strcat( dn, dnsuffix );

			if ( dn[0] != '\0' )
				getline( passwd, sizeof(passwd), stdin,
				    "password? " );
			else
				passwd[0] = '\0';

			if ( ldap_bind_s( ld, dn, passwd, method ) !=
			    LDAP_SUCCESS ) {
				fprintf( stderr, "ldap_bind_s failed\n" );
				ldap_perror( ld, "ldap_bind_s" );
			} else {
				printf( "Bind successful\n" );
				bound = 1;
			}
			break;

		case 'c':	/* compare */
			getline( dn, sizeof(dn), stdin, "dn? " );
			strcat( dn, dnsuffix );
			getline( attr, sizeof(attr), stdin, "attr? " );
			getline( value, sizeof(value), stdin, "value? " );

			if ( (id = ldap_compare( ld, dn, attr, value )) == -1 )
				ldap_perror( ld, "ldap_compare" );
			else
				printf( "Compare initiated with id %d\n", id );
			break;

		case 'd':	/* turn on debugging */
#ifdef LDAP_DEBUG
			getline( line, sizeof(line), stdin, "debug level? " );
			ldap_debug = atoi( line );
			if ( ldap_debug & LDAP_DEBUG_PACKETS ) {
				lber_debug = ldap_debug;
			}
#else
			printf( "Compile with -DLDAP_DEBUG for debugging\n" );
#endif
			break;

		case 'E':	/* explode a dn */
			getline( line, sizeof(line), stdin, "dn? " );
			exdn = ldap_explode_dn( line, 0 );
			for ( i = 0; exdn != NULL && exdn[i] != NULL; i++ ) {
				printf( "\t%s\n", exdn[i] );
			}
			break;

		case 'g':	/* set next msgid */
			getline( line, sizeof(line), stdin, "msgid? " );
			ld->ld_msgid = atoi( line );
			break;

		case 'v':	/* set version number */
			getline( line, sizeof(line), stdin, "version? " );
			ld->ld_version = atoi( line );
			break;

		case 'm':	/* modify or modifyrdn */
			if ( strncmp( line, "modify", 4 ) == 0 ) {
				getline( dn, sizeof(dn), stdin, "dn? " );
				strcat( dn, dnsuffix );
				if ( (mods = get_modlist(
				    "mod (0=>add, 1=>delete, 2=>replace -1=>done)? ",
				    "attribute type? ", "attribute value? " ))
				    == NULL )
					break;
				if ( (id = ldap_modify( ld, dn, mods )) == -1 )
					ldap_perror( ld, "ldap_modify" );
				else
					printf( "Modify initiated with id %d\n",
					    id );
			} else if ( strncmp( line, "modrdn", 4 ) == 0 ) {
				getline( dn, sizeof(dn), stdin, "dn? " );
				strcat( dn, dnsuffix );
				getline( rdn, sizeof(rdn), stdin, "newrdn? " );
				if ( (id = ldap_modrdn( ld, dn, rdn )) == -1 )
					ldap_perror( ld, "ldap_modrdn" );
				else
					printf( "Modrdn initiated with id %d\n",
					    id );
			} else {
				printf( "Possibilities: [modi]fy, [modr]dn\n" );
			}
			break;

		case 'q':	/* quit */
#ifdef CLDAP
			if ( cldapflg )
				cldap_close( ld );
#endif /* CLDAP */
#ifdef LDAP_REFERRALS
			if ( !cldapflg )
#else /* LDAP_REFERRALS */
			if ( !cldapflg && bound )
#endif /* LDAP_REFERRALS */
				ldap_unbind( ld );
			exit( 0 );
			break;

		case 'r':	/* result or remove */
			switch ( command3 ) {
			case 's':	/* result */
				getline( line, sizeof(line), stdin,
				    "msgid (-1=>any)? " );
				if ( line[0] == '\0' )
					id = -1;
				else
					id = atoi( line );
				getline( line, sizeof(line), stdin,
				    "all (0=>any, 1=>all)? " );
				if ( line[0] == '\0' )
					all = 1;
				else
					all = atoi( line );
				if (( msgtype = ldap_result( ld, id, all,
				    &timeout, &res )) < 1 ) {
					ldap_perror( ld, "ldap_result" );
					break;
				}
				printf( "\nresult: msgtype %d msgid %d\n",
				    msgtype, res->lm_msgid );
				handle_result( ld, res );
				res = NULLMSG;
				break;

			case 'm':	/* remove */
				getline( dn, sizeof(dn), stdin, "dn? " );
				strcat( dn, dnsuffix );
				if ( (id = ldap_delete( ld, dn )) == -1 )
					ldap_perror( ld, "ldap_delete" );
				else
					printf( "Remove initiated with id %d\n",
					    id );
				break;

			default:
				printf( "Possibilities: [rem]ove, [res]ult\n" );
				break;
			}
			break;

		case 's':	/* search */
			getline( dn, sizeof(dn), stdin, "searchbase? " );
			strcat( dn, dnsuffix );
			getline( line, sizeof(line), stdin,
			    "scope (0=Base, 1=One Level, 2=Subtree)? " );
			scope = atoi( line );
			getline( filter, sizeof(filter), stdin,
			    "search filter (e.g. sn=jones)? " );
			types = get_list( "attrs to return? " );
			getline( line, sizeof(line), stdin,
			    "attrsonly (0=attrs&values, 1=attrs only)? " );
			attrsonly = atoi( line );

			if ( cldapflg ) {
#ifdef CLDAP
			    getline( line, sizeof(line), stdin,
				"Requestor DN (for logging)? " );
			    if ( cldap_search_s( ld, dn, scope, filter, types,
				    attrsonly, &res, line ) != 0 ) {
				ldap_perror( ld, "cldap_search_s" );
			    } else {
				printf( "\nresult: msgid %d\n",
				    res->lm_msgid );
				handle_result( ld, res );
				res = NULLMSG;
			    }
#endif /* CLDAP */
			} else {
			    if (( id = ldap_search( ld, dn, scope, filter,
				    types, attrsonly  )) == -1 ) {
				ldap_perror( ld, "ldap_search" );
			    } else {
				printf( "Search initiated with id %d\n", id );
			    }
			}
			free_list( types );
			break;

		case 't':	/* set timeout value */
			getline( line, sizeof(line), stdin, "timeout? " );
			timeout.tv_sec = atoi( line );
			break;

		case 'U':	/* set ufn search prefix */
			getline( line, sizeof(line), stdin, "ufn prefix? " );
			ldap_ufn_setprefix( ld, line );
			break;

		case 'u':	/* user friendly search w/optional timeout */
			getline( dn, sizeof(dn), stdin, "ufn? " );
			strcat( dn, dnsuffix );
			types = get_list( "attrs to return? " );
			getline( line, sizeof(line), stdin,
			    "attrsonly (0=attrs&values, 1=attrs only)? " );
			attrsonly = atoi( line );

			if ( command2 == 't' ) {
				id = ldap_ufn_search_c( ld, dn, types,
				    attrsonly, &res, ldap_ufn_timeout,
				    &timeout );
			} else {
				id = ldap_ufn_search_s( ld, dn, types,
				    attrsonly, &res );
			}
			if ( res == NULL )
				ldap_perror( ld, "ldap_ufn_search" );
			else {
				printf( "\nresult: err %d\n", id );
				handle_result( ld, res );
				res = NULLMSG;
			}
			free_list( types );
			break;

		case 'l':	/* URL search */
			getline( line, sizeof(line), stdin,
			    "attrsonly (0=attrs&values, 1=attrs only)? " );
			attrsonly = atoi( line );
			getline( line, sizeof(line), stdin, "LDAP URL? " );
			if (( id = ldap_url_search( ld, line, attrsonly  ))
				== -1 ) {
			    ldap_perror( ld, "ldap_url_search" );
			} else {
			    printf( "URL search initiated with id %d\n", id );
			}
			break;

		case 'p':	/* parse LDAP URL */
			getline( line, sizeof(line), stdin, "LDAP URL? " );
			if (( i = ldap_url_parse( line, &ludp )) != 0 ) {
			    fprintf( stderr, "ldap_url_parse: error %d\n", i );
			} else {
			    printf( "\t  host: " );
			    if ( ludp->lud_host == NULL ) {
				printf( "DEFAULT\n" );
			    } else {
				printf( "<%s>\n", ludp->lud_host );
			    }
			    printf( "\t  port: " );
			    if ( ludp->lud_port == 0 ) {
				printf( "DEFAULT\n" );
			    } else {
				printf( "%d\n", ludp->lud_port );
			    }
			    printf( "\t    dn: <%s>\n", ludp->lud_dn );
			    printf( "\t attrs:" );
			    if ( ludp->lud_attrs == NULL ) {
				printf( " ALL" );
			    } else {
				for ( i = 0; ludp->lud_attrs[ i ] != NULL; ++i ) {
				    printf( " <%s>", ludp->lud_attrs[ i ] );
				}
			    }
			    printf( "\n\t scope: %s\n", ludp->lud_scope == LDAP_SCOPE_ONELEVEL ?
				"ONE" : ludp->lud_scope == LDAP_SCOPE_BASE ? "BASE" :
				ludp->lud_scope == LDAP_SCOPE_SUBTREE ? "SUB" : "**invalid**" );
			    printf( "\tfilter: <%s>\n", ludp->lud_filter );
			    ldap_free_urldesc( ludp );
			}
			    break;

		case 'n':	/* set dn suffix, for convenience */
			getline( line, sizeof(line), stdin, "DN suffix? " );
			strcpy( dnsuffix, line );
			break;

		case 'e':	/* enable cache */
#ifdef NO_CACHE
			printf( NOCACHEERRMSG );
#else /* NO_CACHE */
			getline( line, sizeof(line), stdin, "Cache timeout (secs)? " );
			i = atoi( line );
			getline( line, sizeof(line), stdin, "Maximum memory to use (bytes)? " );
			if ( ldap_enable_cache( ld, i, atoi( line )) == 0 ) {
				printf( "local cache is on\n" ); 
			} else {
				printf( "ldap_enable_cache failed\n" ); 
			}
#endif /* NO_CACHE */
			break;

		case 'x':	/* uncache entry */
#ifdef NO_CACHE
			printf( NOCACHEERRMSG );
#else /* NO_CACHE */
			getline( line, sizeof(line), stdin, "DN? " );
			ldap_uncache_entry( ld, line );
#endif /* NO_CACHE */
			break;

		case 'X':	/* uncache request */
#ifdef NO_CACHE
			printf( NOCACHEERRMSG );
#else /* NO_CACHE */
			getline( line, sizeof(line), stdin, "request msgid? " );
			ldap_uncache_request( ld, atoi( line ));
#endif /* NO_CACHE */
			break;

		case 'o':	/* set ldap options */
			getline( line, sizeof(line), stdin, "alias deref (0=never, 1=searching, 2=finding, 3=always)?" );
			ld->ld_deref = atoi( line );
			getline( line, sizeof(line), stdin, "timelimit?" );
			ld->ld_timelimit = atoi( line );
			getline( line, sizeof(line), stdin, "sizelimit?" );
			ld->ld_sizelimit = atoi( line );

			ld->ld_options = 0;

#ifdef STR_TRANSLATION
			getline( line, sizeof(line), stdin,
				"Automatic translation of T.61 strings (0=no, 1=yes)?" );
			if ( atoi( line ) == 0 ) {
				ld->ld_lberoptions &= ~LBER_TRANSLATE_STRINGS;
			} else {
				ld->ld_lberoptions |= LBER_TRANSLATE_STRINGS;
#ifdef LDAP_CHARSET_8859
				getline( line, sizeof(line), stdin,
					"Translate to/from ISO-8859 (0=no, 1=yes?" );
				if ( atoi( line ) != 0 ) {
					ldap_set_string_translators( ld,
					    ldap_8859_to_t61,
					    ldap_t61_to_8859 );
				}
#endif /* LDAP_CHARSET_8859 */
			}
#endif /* STR_TRANSLATION */

#ifdef LDAP_DNS
			getline( line, sizeof(line), stdin,
				"Use DN & DNS to determine where to send requests (0=no, 1=yes)?" );
			if ( atoi( line ) != 0 ) {
				ld->ld_options |= LDAP_OPT_DNS;
			}
#endif /* LDAP_DNS */

#ifdef LDAP_REFERRALS
			getline( line, sizeof(line), stdin,
				"Recognize and chase referrals (0=no, 1=yes)?" );
			if ( atoi( line ) != 0 ) {
				ld->ld_options |= LDAP_OPT_REFERRALS;
				getline( line, sizeof(line), stdin,
					"Prompt for bind credentials when chasing referrals (0=no, 1=yes)?" );
				if ( atoi( line ) != 0 ) {
					ldap_set_rebind_proc( ld, bind_prompt );
				}
			}
#endif /* LDAP_REFERRALS */
			break;

		case 'O':	/* set cache options */
#ifdef NO_CACHE
			printf( NOCACHEERRMSG );
#else /* NO_CACHE */
			getline( line, sizeof(line), stdin, "cache errors (0=smart, 1=never, 2=always)?" );
			switch( atoi( line )) {
			case 0:
				ldap_set_cache_options( ld, 0 );
				break;
			case 1:
				ldap_set_cache_options( ld,
					LDAP_CACHE_OPT_CACHENOERRS );
				break;
			case 2:
				ldap_set_cache_options( ld,
					LDAP_CACHE_OPT_CACHEALLERRS );
				break;
			default:
				printf( "not a valid cache option\n" );
			}
#endif /* NO_CACHE */
			break;

		case '?':	/* help */
    printf( "Commands: [ad]d         [ab]andon         [b]ind\n" );
    printf( "          [B]ind async  [c]ompare         [l]URL search\n" );
    printf( "          [modi]fy      [modr]dn          [rem]ove\n" );
    printf( "          [res]ult      [s]earch          [q]uit/unbind\n\n" );
    printf( "          [u]fn search  [ut]fn search with timeout\n" );
    printf( "          [d]ebug       [e]nable cache    set ms[g]id\n" );
    printf( "          d[n]suffix    [t]imeout         [v]ersion\n" );
    printf( "          [U]fn prefix  [x]uncache entry  [X]uncache request\n" );
    printf( "          [?]help       [o]ptions         [O]cache options\n" );
    printf( "          [E]xplode dn  [p]arse LDAP URL\n" );
			break;

		default:
			printf( "Invalid command.  Type ? for help.\n" );
			break;
		}

		(void) memset( line, '\0', sizeof(line) );
	}

	return( 0 );
}

static void
handle_result( LDAP *ld, LDAPMessage *lm )
{
	switch ( lm->lm_msgtype ) {
	case LDAP_RES_COMPARE:
		printf( "Compare result\n" );
		print_ldap_result( ld, lm, "compare" );
		break;

	case LDAP_RES_SEARCH_RESULT:
		printf( "Search result\n" );
		print_ldap_result( ld, lm, "search" );
		break;

	case LDAP_RES_SEARCH_ENTRY:
		printf( "Search entry\n" );
		print_search_entry( ld, lm );
		break;

	case LDAP_RES_ADD:
		printf( "Add result\n" );
		print_ldap_result( ld, lm, "add" );
		break;

	case LDAP_RES_DELETE:
		printf( "Delete result\n" );
		print_ldap_result( ld, lm, "delete" );
		break;

	case LDAP_RES_MODRDN:
		printf( "ModRDN result\n" );
		print_ldap_result( ld, lm, "modrdn" );
		break;

	case LDAP_RES_BIND:
		printf( "Bind result\n" );
		print_ldap_result( ld, lm, "bind" );
		break;

	default:
		printf( "Unknown result type 0x%x\n", lm->lm_msgtype );
		print_ldap_result( ld, lm, "unknown" );
	}
}

static void
print_ldap_result( LDAP *ld, LDAPMessage *lm, char *s )
{
	ldap_result2error( ld, lm, 1 );
	ldap_perror( ld, s );
/*
	if ( ld->ld_error != NULL && *ld->ld_error != '\0' )
		fprintf( stderr, "Additional info: %s\n", ld->ld_error );
	if ( NAME_ERROR( ld->ld_errno ) && ld->ld_matched != NULL )
		fprintf( stderr, "Matched DN: %s\n", ld->ld_matched );
*/
}

static void
print_search_entry( LDAP *ld, LDAPMessage *res )
{
	BerElement	*ber;
	char		*a, *dn, *ufn;
	struct berval	**vals;
	int		i;
	LDAPMessage	*e;

	for ( e = ldap_first_entry( ld, res ); e != NULLMSG;
	    e = ldap_next_entry( ld, e ) ) {
		if ( e->lm_msgtype == LDAP_RES_SEARCH_RESULT )
			break;

		dn = ldap_get_dn( ld, e );
		printf( "\tDN: %s\n", dn );

		ufn = ldap_dn2ufn( dn );
		printf( "\tUFN: %s\n", ufn );
#ifdef WINSOCK
		ldap_memfree( dn );
		ldap_memfree( ufn );
#else /* WINSOCK */
		free( dn );
		free( ufn );
#endif /* WINSOCK */

		for ( a = ldap_first_attribute( ld, e, &ber ); a != NULL;
		    a = ldap_next_attribute( ld, e, ber ) ) {
			printf( "\t\tATTR: %s\n", a );
			if ( (vals = ldap_get_values_len( ld, e, a ))
			    == NULL ) {
				printf( "\t\t\t(no values)\n" );
			} else {
				for ( i = 0; vals[i] != NULL; i++ ) {
					int	j, nonascii;

					nonascii = 0;
					for ( j = 0; j < vals[i]->bv_len; j++ )
						if ( !isascii( vals[i]->bv_val[j] ) ) {
							nonascii = 1;
							break;
						}

					if ( nonascii ) {
						printf( "\t\t\tlength (%ld) (not ascii)\n", vals[i]->bv_len );
#ifdef BPRINT_NONASCII
						lber_bprint( vals[i]->bv_val,
						    vals[i]->bv_len );
#endif /* BPRINT_NONASCII */
						continue;
					}
					printf( "\t\t\tlength (%ld) %s\n",
					    vals[i]->bv_len, vals[i]->bv_val );
				}
				ber_bvecfree( vals );
			}
		}
	}

	if ( res->lm_msgtype == LDAP_RES_SEARCH_RESULT
	    || res->lm_chain != NULLMSG )
		print_ldap_result( ld, res, "search" );
}


#ifdef WINSOCK
void
ldap_perror( LDAP *ld, char *s )
{
	char	*errs;

	if ( ld == NULL ) {
		perror( s );
		return;
	}

	errs = ldap_err2string( ld->ld_errno );
	printf( "%s: %s\n", s, errs == NULL ? "unknown error" : errs );
	if ( ld->ld_error != NULL && *ld->ld_error != '\0' ) {
		printf( "%s: additional info: %s\n", s, ld->ld_error );
	}
}
#endif /* WINSOCK */
