/*
 *  Copyright (c) 1995 Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  request.c - sending of ldap requests; handling of referrals
 */

#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1995 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif

#include <stdio.h>
#include <string.h>
#ifdef MACOS
#include <stdlib.h>
#include <time.h>
#include "macos.h"
#else /* MACOS */
#if defined( DOS ) || defined( _WIN32 )
#include "msdos.h"
#include <time.h>
#include <stdlib.h>
#ifdef PCNFS
#include <tklib.h>
#include <tk_errno.h>
#include <bios.h>
#endif /* PCNFS */
#ifdef NCSA
#include "externs.h"
#endif /* NCSA */
#else /* DOS */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#ifdef _AIX
#include <sys/select.h>
#endif /* _AIX */
#include "portable.h"
#endif /* DOS */
#endif /* MACOS */
#ifdef VMS
#include "ucx_select.h"
#endif
#include "lber.h"
#include "ldap.h"
#include "ldap-int.h"

#ifdef USE_SYSCONF
#include <unistd.h>
#endif /* USE_SYSCONF */


#if defined( LDAP_REFERRALS ) || defined( LDAP_DNS )
#ifdef NEEDPROTOS
static LDAPConn *find_connection( LDAP *ld, LDAPServer *srv, int any );
static void use_connection( LDAP *ld, LDAPConn *lc );
static void free_servers( LDAPServer *srvlist );
#else /* NEEDPROTOS */
static LDAPConn *find_connection();
static void use_connection();
static void free_servers();
#endif /* NEEDPROTOS */
#endif /* LDAP_REFERRALS || LDAP_DNS */


#ifdef LDAP_DNS
#ifdef NEEDPROTOS
static LDAPServer *dn2servers( LDAP *ld, char *dn );
#else /* NEEDPROTOS */
static LDAPServer *dn2servers();
#endif /* NEEDPROTOS */
#endif /* LDAP_DNS */

#ifdef LDAP_REFERRALS
#ifdef NEEDPROTOS
static BerElement *re_encode_request( LDAP *ld, BerElement *origber,
    int msgid, char **dnp );
#else /* NEEDPROTOS */
static BerElement *re_encode_request();
#endif /* NEEDPROTOS */
#endif /* LDAP_REFERRALS */


BerElement *
alloc_ber_with_options( LDAP *ld )
{
	BerElement	*ber;

    	if (( ber = ber_alloc_t( ld->ld_lberoptions )) == NULLBER ) {
		ld->ld_errno = LDAP_NO_MEMORY;
#ifdef STR_TRANSLATION
	} else {
		set_ber_options( ld, ber );
#endif /* STR_TRANSLATION */
	}

	return( ber );
}


void
set_ber_options( LDAP *ld, BerElement *ber )
{
	ber->ber_options = ld->ld_lberoptions;
#ifdef STR_TRANSLATION
	if (( ld->ld_lberoptions & LBER_TRANSLATE_STRINGS ) != 0 ) {
		ber_set_string_translators( ber,
		    ld->ld_lber_encode_translate_proc,
		    ld->ld_lber_decode_translate_proc );
	}
#endif /* STR_TRANSLATION */
}


int
send_initial_request( LDAP *ld, unsigned long msgtype, char *dn,
	BerElement *ber )
{
#if defined( LDAP_REFERRALS ) || defined( LDAP_DNS )
	LDAPServer	*servers;
#endif /* LDAP_REFERRALS || LDAP_DNS */

	Debug( LDAP_DEBUG_TRACE, "send_initial_request\n", 0, 0, 0 );

#if !defined( LDAP_REFERRALS ) && !defined( LDAP_DNS )
	if ( ber_flush( &ld->ld_sb, ber, 1 ) != 0 ) {
		ld->ld_errno = LDAP_SERVER_DOWN;
		return( -1 );
	}

	ld->ld_errno = LDAP_SUCCESS;
	return( ld->ld_msgid );
#else /* !LDAP_REFERRALS && !LDAP_DNS */

#ifdef LDAP_DNS
	if (( ld->ld_options & LDAP_OPT_DNS ) != 0 && ldap_is_dns_dn( dn )) {
		if (( servers = dn2servers( ld, dn )) == NULL ) {
			ber_free( ber, 1 );
			return( -1 );
		}

#ifdef LDAP_DEBUG
		if ( ldap_debug & LDAP_DEBUG_TRACE ) {
			LDAPServer	*srv;

			for ( srv = servers; srv != NULL;
			    srv = srv->lsrv_next ) {
				fprintf( stderr,
				    "LDAP server %s:  dn %s, port %d\n",
				    srv->lsrv_host, ( srv->lsrv_dn == NULL ) ?
				    "(default)" : srv->lsrv_dn,
				    srv->lsrv_port );
			}
		}
#endif /* LDAP_DEBUG */
	} else {
#endif /* LDAP_DNS */
		/*
		 * use of DNS is turned off or this is an X.500 DN...
		 * use our default connection
		 */
		servers = NULL;
#ifdef LDAP_DNS
	}	
#endif /* LDAP_DNS */

	return( send_server_request( ld, ber, ld->ld_msgid, NULL, servers,
	    NULL, 0 ));
#endif /* !LDAP_REFERRALS && !LDAP_DNS */
}



#if defined( LDAP_REFERRALS ) || defined( LDAP_DNS )
int
send_server_request( LDAP *ld, BerElement *ber, int msgid, LDAPRequest
	*parentreq, LDAPServer *srvlist, LDAPConn *lc, int bind )
{
	LDAPRequest	*lr;

	Debug( LDAP_DEBUG_TRACE, "send_server_request\n", 0, 0, 0 );

	ld->ld_errno = LDAP_SUCCESS;	/* optimistic */

	if ( lc == NULL ) {
		if ( srvlist == NULL ) {
			lc = ld->ld_defconn;
		} else {
			if (( lc = find_connection( ld, srvlist, 1 )) ==
			    NULL ) {
				lc = new_connection( ld, &srvlist, 0, 1, bind );
			}
			free_servers( srvlist );
		}
	}

	if ( lc == NULL || lc->lconn_status != LDAP_CONNST_CONNECTED ) {
		ber_free( ber, 1 );
		if ( ld->ld_errno == LDAP_SUCCESS ) {
			ld->ld_errno = LDAP_SERVER_DOWN;
		}
		return( -1 );
	}

	use_connection( ld, lc );
	if (( lr = (LDAPRequest *)calloc( 1, sizeof( LDAPRequest ))) ==
	    NULL ) {
		ld->ld_errno = LDAP_NO_MEMORY;
		free_connection( ld, lc, 0, 0 );
		ber_free( ber, 1 );
		return( -1 );
	} 
	lr->lr_msgid = msgid;
	lr->lr_status = LDAP_REQST_INPROGRESS;
	lr->lr_res_errno = LDAP_SUCCESS;	/* optimistic */
	lr->lr_ber = ber;
	lr->lr_conn = lc;
	if ( parentreq != NULL ) {	/* sub-request */
		++parentreq->lr_outrefcnt;
		lr->lr_origid = parentreq->lr_origid;
		lr->lr_parentcnt = parentreq->lr_parentcnt + 1;
		lr->lr_parent = parentreq;
		lr->lr_refnext = parentreq->lr_refnext;
		parentreq->lr_refnext = lr;
	} else {			/* original request */
		lr->lr_origid = lr->lr_msgid;
	}

	if (( lr->lr_next = ld->ld_requests ) != NULL ) {
		lr->lr_next->lr_prev = lr;
	}
	ld->ld_requests = lr;
	lr->lr_prev = NULL;

	if ( ber_flush( lc->lconn_sb, ber, 0 ) != 0 ) {
#ifdef notyet
		extern int	errno;

		if ( errno == EWOULDBLOCK ) {
			/* need to continue write later */
			lr->lr_status = LDAP_REQST_WRITING;
			mark_select_write( ld, lc->lconn_sb );
		} else {
#else /* notyet */
			ld->ld_errno = LDAP_SERVER_DOWN;
			free_request( ld, lr );
			free_connection( ld, lc, 0, 0 );
			return( -1 );
#endif /* notyet */
#ifdef notyet
		}
#endif /* notyet */
	} else {
		if ( parentreq == NULL ) {
			ber->ber_end = ber->ber_ptr;
			ber->ber_ptr = ber->ber_buf;
		}

		/* sent -- waiting for a response */
		mark_select_read( ld, lc->lconn_sb );
	}

	ld->ld_errno = LDAP_SUCCESS;
	return( msgid );
}


LDAPConn *
new_connection( LDAP *ld, LDAPServer **srvlistp, int use_ldsb,
	int connect, int bind )
{
	LDAPConn	*lc;
	LDAPServer	*prevsrv, *srv;
	Sockbuf		*sb;
#ifdef UNS
	int		rc;
#endif

	/*
	 * make a new LDAP server connection
	 * XXX open connection synchronously for now
	 */
	if (( lc = (LDAPConn *)calloc( 1, sizeof( LDAPConn ))) == NULL ||
	    ( !use_ldsb && ( sb = (Sockbuf *)calloc( 1, sizeof( Sockbuf )))
	    == NULL )) {
		if ( lc != NULL ) {
			free( (char *)lc );
		}
		ld->ld_errno = LDAP_NO_MEMORY;
		return( NULL );
	}

	lc->lconn_sb = ( use_ldsb ) ? &ld->ld_sb : sb;

	if ( connect ) {
		prevsrv = NULL;

		for ( srv = *srvlistp; srv != NULL; srv = srv->lsrv_next ) {
			if (( rc =  open_ldap_connection( ld, lc->lconn_sb,
			    srv->lsrv_host, srv->lsrv_port,
			    &lc->lconn_krbinstance, 1 )) != -1 ) {
				break;
			}
			prevsrv = srv;
		}

		if ( srv == NULL ) {
		    if ( !use_ldsb ) {
			free( (char *)lc->lconn_sb );
		    }
		    free( (char *)lc );
		    ld->ld_errno = LDAP_SERVER_DOWN;
		    return( NULL );
		}

		if ( prevsrv == NULL ) {
		    *srvlistp = srv->lsrv_next;
		} else {
		    prevsrv->lsrv_next = srv->lsrv_next;
		}
		lc->lconn_server = srv;
	}

#ifdef UNS
	if (rc == -2) 
		lc->lconn_status = LDAP_CONNST_CONNECTING;
	else
#endif
		lc->lconn_status = LDAP_CONNST_CONNECTED;

	lc->lconn_next = ld->ld_conns;
	ld->ld_conns = lc;

	/*
	 * XXX for now, we always do a synchronous bind.  This will have
	 * to change in the long run...
	 */
	if ( bind ) {
		int		err, freepasswd, authmethod;
		char		*binddn, *passwd;
		LDAPConn	*savedefconn;

		freepasswd = err = 0;

		if ( ld->ld_rebindproc == NULL ) {
			binddn = passwd = "";
			authmethod = LDAP_AUTH_SIMPLE;
		} else {
			if (( err = (*ld->ld_rebindproc)( ld, &binddn, &passwd,
			    &authmethod, 0 )) == LDAP_SUCCESS ) {
				freepasswd = 1;
			} else {
				ld->ld_errno = err;
				err = -1;
			}
		}


		if ( err == 0 ) {
			savedefconn = ld->ld_defconn;
			ld->ld_defconn = lc;
			++lc->lconn_refcnt;	/* avoid premature free */

			if ( ldap_bind_s( ld, binddn, passwd, authmethod ) !=
			    LDAP_SUCCESS ) {
				err = -1;
			}
			--lc->lconn_refcnt;
			ld->ld_defconn = savedefconn;
		}

		if ( freepasswd ) {
			(*ld->ld_rebindproc)( ld, &binddn, &passwd,
				&authmethod, 1 );
		}

		if ( err != 0 ) {
			free_connection( ld, lc, 1, 0 );
			lc = NULL;
		}
	}

	return( lc );
}


static LDAPConn *
find_connection( LDAP *ld, LDAPServer *srv, int any )
/*
 * return an existing connection (if any) to the server srv
 * if "any" is non-zero, check for any server in the "srv" chain
 */
{
	LDAPConn	*lc;
	LDAPServer	*ls;

	for ( lc = ld->ld_conns; lc != NULL; lc = lc->lconn_next ) {
		for ( ls = srv; ls != NULL; ls = ls->lsrv_next ) {
			if ( lc->lconn_server->lsrv_host != NULL &&
			    ls->lsrv_host != NULL && strcasecmp(
			    ls->lsrv_host, lc->lconn_server->lsrv_host ) == 0
			    && ls->lsrv_port == lc->lconn_server->lsrv_port ) {
				return( lc );
			}
			if ( !any ) {
				break;
			}
		}
	}

	return( NULL );
}



static void
use_connection( LDAP *ld, LDAPConn *lc )
{
	++lc->lconn_refcnt;
	lc->lconn_lastused = time( 0 );
}


void
free_connection( LDAP *ld, LDAPConn *lc, int force, int unbind )
{
	LDAPConn	*tmplc, *prevlc;

	Debug( LDAP_DEBUG_TRACE, "free_connection\n", 0, 0, 0 );

	if ( force || --lc->lconn_refcnt <= 0 ) {
		if ( lc->lconn_status == LDAP_CONNST_CONNECTED ) {
			mark_select_clear( ld, lc->lconn_sb );
			if ( unbind ) {
				send_unbind( ld, lc->lconn_sb );
			}
			close_connection( lc->lconn_sb );
			if ( lc->lconn_sb->sb_ber.ber_buf != NULL ) {
				free( lc->lconn_sb->sb_ber.ber_buf );
			}
		}
		prevlc = NULL;
		for ( tmplc = ld->ld_conns; tmplc != NULL;
		    tmplc = tmplc->lconn_next ) {
			if ( tmplc == lc ) {
				if ( prevlc == NULL ) {
				    ld->ld_conns = tmplc->lconn_next;
				} else {
				    prevlc->lconn_next = tmplc->lconn_next;
				}
				break;
			}
		}
		free_servers( lc->lconn_server );
		if ( lc->lconn_krbinstance != NULL ) {
			free( lc->lconn_krbinstance );
		}
		if ( lc->lconn_sb != &ld->ld_sb ) {
			free( (char *)lc->lconn_sb );
		}
		free( lc );
		Debug( LDAP_DEBUG_TRACE, "free_connection: actually freed\n",
		    0, 0, 0 );
	} else {
		lc->lconn_lastused = time( 0 );
		Debug( LDAP_DEBUG_TRACE, "free_connection: refcnt %d\n",
		    lc->lconn_refcnt, 0, 0 );
	}
}


#ifdef LDAP_DEBUG
void
dump_connection( LDAP *ld, LDAPConn *lconns, int all )
{
	LDAPConn	*lc;

	fprintf( stderr, "** Connection%s:\n", all ? "s" : "" );
	for ( lc = lconns; lc != NULL; lc = lc->lconn_next ) {
		if ( lc->lconn_server != NULL ) {
			fprintf( stderr, "* host: %s  port: %d%s\n",
			    ( lc->lconn_server->lsrv_host == NULL ) ? "(null)"
			    : lc->lconn_server->lsrv_host,
			    lc->lconn_server->lsrv_port, ( lc->lconn_sb ==
			    &ld->ld_sb ) ? "  (default)" : "" );
		}
		fprintf( stderr, "  refcnt: %d  status: %s\n", lc->lconn_refcnt,
		    ( lc->lconn_status == LDAP_CONNST_NEEDSOCKET ) ?
		    "NeedSocket" : ( lc->lconn_status ==
		    LDAP_CONNST_CONNECTING ) ? "Connecting" : "Connected" );
		fprintf( stderr, "  last used: %s\n",
		    ctime( &lc->lconn_lastused ));
		if ( !all ) {
			break;
		}
	}
}


void
dump_requests_and_responses( LDAP *ld )
{
	LDAPRequest	*lr;
	LDAPMessage	*lm, *l;

	fprintf( stderr, "** Outstanding Requests:\n" );
	if (( lr = ld->ld_requests ) == NULL ) {
		fprintf( stderr, "   Empty\n" );
	}
	for ( ; lr != NULL; lr = lr->lr_next ) {
	    fprintf( stderr, " * msgid %d,  origid %d, status %s\n",
		lr->lr_msgid, lr->lr_origid, ( lr->lr_status ==
		LDAP_REQST_INPROGRESS ) ? "InProgress" :
		( lr->lr_status == LDAP_REQST_CHASINGREFS ) ? "ChasingRefs" :
		( lr->lr_status == LDAP_REQST_NOTCONNECTED ) ? "NotConnected" :
		"Writing" );
	    fprintf( stderr, "   outstanding referrals %d, parent count %d\n",
		    lr->lr_outrefcnt, lr->lr_parentcnt );
	}

	fprintf( stderr, "** Response Queue:\n" );
	if (( lm = ld->ld_responses ) == NULLMSG ) {
		fprintf( stderr, "   Empty\n" );
	}
	for ( ; lm != NULLMSG; lm = lm->lm_next ) {
		fprintf( stderr, " * msgid %d,  type %d\n",
		    lm->lm_msgid, lm->lm_msgtype );
		if (( l = lm->lm_chain ) != NULL ) {
			fprintf( stderr, "   chained responses:\n" );
			for ( ; l != NULLMSG; l = l->lm_chain ) {
				fprintf( stderr,
				    "  * msgid %d,  type %d\n",
				    l->lm_msgid, l->lm_msgtype );
			}
		}
	}
}
#endif /* LDAP_DEBUG */


void
free_request( LDAP *ld, LDAPRequest *lr )
{
	LDAPRequest	*tmplr, *nextlr;

	Debug( LDAP_DEBUG_TRACE, "free_request (origid %d, msgid %d)\n",
		lr->lr_origid, lr->lr_msgid, 0 );

	if ( lr->lr_parent != NULL ) {
		--lr->lr_parent->lr_outrefcnt;
	} else {
		/* free all referrals (child requests) */
		for ( tmplr = lr->lr_refnext; tmplr != NULL; tmplr = nextlr ) {
			nextlr = tmplr->lr_refnext;
			free_request( ld, tmplr );
		}
	}

	if ( lr->lr_prev == NULL ) {
		ld->ld_requests = lr->lr_next;
	} else {
		lr->lr_prev->lr_next = lr->lr_next;
	}

	if ( lr->lr_next != NULL ) {
		lr->lr_next->lr_prev = lr->lr_prev;
	}

	if ( lr->lr_ber != NULL ) {
		ber_free( lr->lr_ber, 1 );
	}

	if ( lr->lr_res_error != NULL ) {
		free( lr->lr_res_error );
	}

	if ( lr->lr_res_matched != NULL ) {
		free( lr->lr_res_matched );
	}

	free( lr );
}


static void
free_servers( LDAPServer *srvlist )
{
    LDAPServer	*nextsrv;

    while ( srvlist != NULL ) {
	nextsrv = srvlist->lsrv_next;
	if ( srvlist->lsrv_dn != NULL ) {
		free( srvlist->lsrv_dn );
	}
	if ( srvlist->lsrv_host != NULL ) {
		free( srvlist->lsrv_host );
	}
	free( srvlist );
	srvlist = nextsrv;
    }
}
#endif /* LDAP_REFERRALS || LDAP_DNS */


#ifdef LDAP_REFERRALS
/*
 * XXX merging of errors in this routine needs to be improved
 */
int
chase_referrals( LDAP *ld, LDAPRequest *lr, char **errstrp, int *hadrefp )
{
	int		rc, count, len, newdn;
#ifdef LDAP_DNS
	int		ldapref;
#endif /* LDAP_DNS */
	char		*p, *ports, *ref, *tmpref, *refdn, *unfollowed;
	LDAPRequest	*origreq;
	LDAPServer	*srv;
	BerElement	*ber;

	Debug( LDAP_DEBUG_TRACE, "chase_referrals\n", 0, 0, 0 );

	ld->ld_errno = LDAP_SUCCESS;	/* optimistic */
	*hadrefp = 0;

	if ( *errstrp == NULL ) {
		return( 0 );
	}

	len = strlen( *errstrp );
	for ( p = *errstrp; len >= LDAP_REF_STR_LEN; ++p, --len ) {
		if (( *p == 'R' || *p == 'r' ) && strncasecmp( p,
		    LDAP_REF_STR, LDAP_REF_STR_LEN ) == 0 ) {
			*p = '\0';
			p += LDAP_REF_STR_LEN;
			break;
		}
	}

	if ( len < LDAP_REF_STR_LEN ) {
		return( 0 );
	}

	if ( lr->lr_parentcnt >= ld->ld_refhoplimit ) {
		Debug( LDAP_DEBUG_ANY,
		    "more than %d referral hops (dropping)\n",
		    ld->ld_refhoplimit, 0, 0 );
		    /* XXX report as error in ld->ld_errno? */
		    return( 0 );
	}

	/* find original request */
	for ( origreq = lr; origreq->lr_parent != NULL;
	     origreq = origreq->lr_parent ) {
		;
	}

	unfollowed = NULL;
	rc = count = 0;

	/* parse out & follow referrals */
	for ( ref = p; rc == 0 && ref != NULL; ref = p ) {
#ifdef LDAP_DNS
		ldapref = 0;
#endif /* LDAP_DNS */

		if (( p = strchr( ref, '\n' )) != NULL ) {
			*p++ = '\0';
		} else {
			p = NULL;
		}

		len = strlen( ref );
		if ( len > LDAP_LDAP_REF_STR_LEN && strncasecmp( ref,
		    LDAP_LDAP_REF_STR, LDAP_LDAP_REF_STR_LEN ) == 0 ) {
			Debug( LDAP_DEBUG_TRACE,
			    "chasing LDAP referral: <%s>\n", ref, 0, 0 );
#ifdef LDAP_DNS
			ldapref = 1;
#endif /* LDAP_DNS */
			tmpref = ref + LDAP_LDAP_REF_STR_LEN;
#ifdef LDAP_DNS
		} else if ( len > LDAP_DX_REF_STR_LEN && strncasecmp( ref,
		    LDAP_DX_REF_STR, LDAP_DX_REF_STR_LEN ) == 0 ) {
			Debug( LDAP_DEBUG_TRACE,
			    "chasing DX referral: <%s>\n", ref, 0, 0 );
			tmpref = ref + LDAP_DX_REF_STR_LEN;
#endif /* LDAP_DNS */
		} else {
			Debug( LDAP_DEBUG_TRACE,
			    "ignoring unknown referral <%s>\n", ref, 0, 0 );
			rc = append_referral( ld, &unfollowed, ref );
			*hadrefp = 1;
			continue;
		}

		*hadrefp = 1;
		if (( refdn = strchr( tmpref, '/' )) != NULL ) {
			*refdn++ = '\0';
			newdn = 1;
		} else {
			newdn = 0;
		}

		if (( ber = re_encode_request( ld, origreq->lr_ber,
		    ++ld->ld_msgid, &refdn )) == NULL ) {
			return( -1 );
		}

#ifdef LDAP_DNS
		if ( ldapref ) {
#endif /* LDAP_DNS */
			if (( srv = (LDAPServer *)calloc( 1,
			    sizeof( LDAPServer ))) == NULL ) {
				ber_free( ber, 1 );
				ld->ld_errno = LDAP_NO_MEMORY;
				return( -1 );
			}

			if (( srv->lsrv_host = strdup( tmpref )) == NULL ) {
				free( (char *)srv );
				ber_free( ber, 1 );
				ld->ld_errno = LDAP_NO_MEMORY;
				return( -1 );
			}

			if (( ports = strchr( srv->lsrv_host, ':' )) != NULL ) {
				*ports++ = '\0';
				srv->lsrv_port = atoi( ports );
			} else {
				srv->lsrv_port = LDAP_PORT;
			}
#ifdef LDAP_DNS
		} else {
			srv = dn2servers( ld, tmpref );
		}
#endif /* LDAP_DNS */

		if ( srv != NULL && send_server_request( ld, ber, ld->ld_msgid,
		    lr, srv, NULL, 1 ) >= 0 ) {
			++count;
		} else {
			Debug( LDAP_DEBUG_ANY,
			    "Unable to chase referral (%s)\n", 
			    ldap_err2string( ld->ld_errno ), 0, 0 );
			rc = append_referral( ld, &unfollowed, ref );
		}

		if ( !newdn && refdn != NULL ) {
			free( refdn );
		}
	}

	free( *errstrp );
	*errstrp = unfollowed;

	return(( rc == 0 ) ? count : rc );
}


int
append_referral( LDAP *ld, char **referralsp, char *s )
{
	int	first;

	if ( *referralsp == NULL ) {
		first = 1;
		*referralsp = (char *)malloc( strlen( s ) + LDAP_REF_STR_LEN
		    + 1 );
	} else {
		first = 0;
		*referralsp = (char *)realloc( *referralsp,
		    strlen( *referralsp ) + strlen( s ) + 2 );
	}

	if ( *referralsp == NULL ) {
		ld->ld_errno = LDAP_NO_MEMORY;
		return( -1 );
	}

	if ( first ) {
		strcpy( *referralsp, LDAP_REF_STR );
	} else {
		strcat( *referralsp, "\n" );
	}
	strcat( *referralsp, s );

	return( 0 );
}



static BerElement *
re_encode_request( LDAP *ld, BerElement *origber, int msgid, char **dnp )
{
/*
 * XXX this routine knows way too much about how the lber library works!
 */
	unsigned long	along, tag;
	long		ver;
	int		rc;
	BerElement	tmpber, *ber;
	char		*orig_dn;

	Debug( LDAP_DEBUG_TRACE,
	    "re_encode_request: new msgid %d, new dn <%s>\n",
	    msgid, ( *dnp == NULL ) ? "NONE" : *dnp, 0 );

	tmpber = *origber;

	/*
	 * all LDAP requests are sequences that start with a message id,
	 * followed by a sequence that is tagged with the operation code
	 */
	if ( ber_scanf( &tmpber, "{i", &along ) != LDAP_TAG_MSGID ||
	    ( tag = ber_skip_tag( &tmpber, &along )) == LBER_DEFAULT ) {
                ld->ld_errno = LDAP_DECODING_ERROR;
		return( NULL );
	}

        if (( ber = alloc_ber_with_options( ld )) == NULLBER ) {
                return( NULL );
        }

	/* bind requests have a version number before the DN & other stuff */
	if ( tag == LDAP_REQ_BIND && ber_get_int( &tmpber, (long *)&ver ) ==
	    LBER_DEFAULT ) {
                ld->ld_errno = LDAP_DECODING_ERROR;
		ber_free( ber, 1 );
		return( NULL );
	}

	/* the rest of the request is the DN followed by other stuff */
	if ( ber_get_stringa( &tmpber, &orig_dn ) == LBER_DEFAULT ) {
		ber_free( ber, 1 );
		return( NULL );
	}

	if ( *dnp == NULL ) {
		*dnp = orig_dn;
	} else {
		free( orig_dn );
	}

	if ( tag == LDAP_REQ_BIND ) {
		rc = ber_printf( ber, "{it{is", msgid, tag, ver, *dnp );
	} else {
		rc = ber_printf( ber, "{it{s", msgid, tag, *dnp );
	}

	if ( rc == -1 ) {
		ber_free( ber, 1 );
		return( NULL );
	}

	if ( ber_write( ber, tmpber.ber_ptr, ( tmpber.ber_end -
	    tmpber.ber_ptr ), 0 ) != ( tmpber.ber_end - tmpber.ber_ptr ) ||
	    ber_printf( ber, "}}" ) == -1 ) {
		ld->ld_errno = LDAP_ENCODING_ERROR;
		ber_free( ber, 1 );
		return( NULL );
	}

#ifdef LDAP_DEBUG
	if ( ldap_debug & LDAP_DEBUG_PACKETS ) {
		Debug( LDAP_DEBUG_ANY, "re_encode_request new request is:\n",
		    0, 0, 0 );
		ber_dump( ber, 0 );
	}
#endif /* LDAP_DEBUG */

	return( ber );
}


LDAPRequest *
find_request_by_msgid( LDAP *ld, int msgid )
{
    	LDAPRequest	*lr;

	for ( lr = ld->ld_requests; lr != NULL; lr = lr->lr_next ) {
		if ( msgid == lr->lr_msgid ) {
			break;
		}
	}

	return( lr );
}
#endif /* LDAP_REFERRALS */


#ifdef LDAP_DNS
static LDAPServer *
dn2servers( LDAP *ld, char *dn )	/* dn can also be a domain.... */
{
	char		*p, *domain, *host, *server_dn, **dxs;
	int		i, port;
	LDAPServer	*srvlist, *prevsrv, *srv;

	if (( domain = strrchr( dn, '@' )) != NULL ) {
		++domain;
	} else {
		domain = dn;
	}

	if (( dxs = getdxbyname( domain )) == NULL ) {
		ld->ld_errno = LDAP_NO_MEMORY;
		return( NULL );
	}

	srvlist = NULL;

	for ( i = 0; dxs[ i ] != NULL; ++i ) {
		port = LDAP_PORT;
		server_dn = NULL;
		if ( strchr( dxs[ i ], ':' ) == NULL ) {
			host = dxs[ i ];
		} else if ( strlen( dxs[ i ] ) >= 7 &&
		    strncmp( dxs[ i ], "ldap://", 7 ) == 0 ) {
			host = dxs[ i ] + 7;
			if (( p = strchr( host, ':' )) == NULL ) {
				p = host;
			} else {
				*p++ = '\0';
				port = atoi( p );
			}
			if (( p = strchr( p, '/' )) != NULL ) {
				server_dn = ++p;
				if ( *server_dn == '\0' ) {
					server_dn = NULL;
				}
			}
		} else {
			host = NULL;
		}

		if ( host != NULL ) {	/* found a server we can use */
			if (( srv = (LDAPServer *)calloc( 1,
			    sizeof( LDAPServer ))) == NULL ) {
				free_servers( srvlist );
				srvlist = NULL;
				break;		/* exit loop & return */
			}

			/* add to end of list of servers */
			if ( srvlist == NULL ) {
				srvlist = srv;
			} else {
				prevsrv->lsrv_next = srv;
			}
			prevsrv = srv;
			
			/* copy in info. */
			if (( srv->lsrv_host = strdup( host )) == NULL ||
			    ( server_dn != NULL && ( srv->lsrv_dn =
			    strdup( server_dn )) == NULL )) {
				free_servers( srvlist );
				srvlist = NULL;
				break;		/* exit loop & return */
			}
			srv->lsrv_port = port;
		}
	}

	ldap_value_free( dxs );

	if ( srvlist == NULL ) {
		ld->ld_errno = LDAP_SERVER_DOWN;
	}

	return( srvlist );
}
#endif /* LDAP_DNS */
