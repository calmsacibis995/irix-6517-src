
#include <sys/types.h>
#include <sys/file.h>
#include <sys/uuid.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include "types.h"
#include "mlog.h"
#include "getopt.h"
#include "inv_priv.h"



/*----------------------------------------------------------------------*/
/*  These are hacked up routines meant ONLY for low-level debugging     */
/*  purposes. These are mostly unused since most the debugging code     */
/*  was ported to xfsdump and xfsrestore as formal suboptions of -I.    */
/*----------------------------------------------------------------------*/


static char *mnt_str[] = { "/usr/lib", "/usr", "/u/sw/courses", "/pro/leda",
			   "/root", "/a/xfs/xlv/e", "/dana/hates/me", "/the/krays" };

static char *dev_str[] = { "/dev/usr/lib", "/dev/usr", "/dev/u/sw/courses", 
			   "/dev/pro/leda",
			   "/dev/root", "/dev/a/xfs/xlv/e", "/dev/dana/hates/me",
			   "/dev/the/krays" };

typedef enum { BULL = -1, WRI, REC, QUE, DEL, MP, QUE2 } hi;

void usage( void );
char *progname;
char *sesfile;


void
CREAT_mfiles( inv_stmtoken_t tok, uuid_t *moid, ino_t f, ino_t n )
{
	uuid_t labelid;
	char label[128], strbuf[20];
	char *str;
	uint_t stat;

	uuid_create( &labelid, &stat );
	uuid_to_string( &labelid, &str, &stat );
	strncpy( strbuf, str, 8 );
	free (str);
	strbuf[8] = '\0';
	sprintf(label,"%s_%s (%d-%d)\0","MEDIA_FILE", strbuf, (int)f, (int)n );
	
	inv_put_mediafile( tok, moid, label, 12, f, 0, n, 0, 0xffff, 
			  BOOL_TRUE, BOOL_FALSE );

}

typedef struct ses{
	uuid_t fsid;
	size_t sz;
	char   *buf;
} ses;

#define SESLIM  240

intgen_t
recons_test( int howmany )
{
	int fd, i, rval = 1;
	
	ses sarr[ SESLIM];
	
	fd = open( sesfile, O_RDONLY );
	
	for ( i=0; i<howmany && i < SESLIM; i++ ){
		rval = get_invtrecord( fd, &sarr[i], 
				       sizeof( uuid_t ) + sizeof( size_t ), 0,
				       SEEK_CUR, BOOL_FALSE );
		ASSERT( rval > 0 );
		ASSERT( sarr[i].sz > 0 );
		sarr[i].buf = calloc( 1,  sarr[i].sz );
		rval = get_invtrecord( fd, sarr[i].buf,  sarr[i].sz, 0, SEEK_CUR,
				       BOOL_FALSE );
		ASSERT( rval > 0 );
	}
	
	
	
	for ( i=0; i<howmany && i < SESLIM; i++ ){
		if ( inv_put_sessioninfo( sarr[i].buf, sarr[i].sz ) < 0)
			printf("$ insert failed.\n");
	}

	close(fd);


}




intgen_t
delete_test( int n )
{
	int fd, i;
	uuid_t moid;
	char *str = 0;
	uint_t stat;

	fd = open( "moids", O_RDONLY );
	if ( fd < 0 ) return -1;
	
	get_invtrecord( fd, &moid, sizeof(uuid_t), (n-1)* sizeof( uuid_t),
		        SEEK_SET, 0 );
	uuid_to_string( &moid, &str, &stat );
	printf("Searching for Moid = %s\n", str );
	free( str );
	if (! inv_delete_mediaobj( &moid ) ) return -1;
	    
	return 1;

}

int
sess_queries_byuuid(char *uu)
{
	u_int stat;
	uuid_t uuid;
	inv_session_t *ses;
	invt_pr_ctx_t prctx;

	uuid_from_string (uu, &uuid, &stat);
	printf("uuid = %s\n", uu);
	if (inv_get_session_byuuid(&uuid, &ses)) {
		if (!ses)
			return -1;
		prctx.index = 0;
		prctx.depth = PR_ALL;
		prctx.mobj.type = INVT_NULLTYPE;
		DEBUG_sessionprint(ses, 99, &prctx);
		inv_free_session(&ses);
		return 1;
	}

	return -1;
}


int
sess_queries_bylabel(char *lab)
{
	inv_session_t *ses;
	invt_pr_ctx_t prctx;

	printf("label = %s\n", lab);
	if (inv_get_session_bylabel(lab, &ses)) {
		if (!ses)
			return -1;
		prctx.index = 0;
		prctx.depth = PR_ALL;
		prctx.mobj.type = INVT_NULLTYPE;
		DEBUG_sessionprint(ses, 99, &prctx);
		inv_free_session(&ses);
		return 1;
	}

	return -1;
}


intgen_t
query_test( int level )
{
	int i;
	inv_idbtoken_t tok;
	time_t *tm;
	inv_session_t *ses;
	invt_pr_ctx_t prctx;
	
	if (level == -2) {
		printf("mount pt %s\n",sesfile);
		tok = inv_open( INV_BY_MOUNTPT, INV_SEARCH_ONLY, sesfile );
		if (! tok ) return -1;
		idx_DEBUG_print (tok->d_invindex_fd);
		return 1;
	}
		
	for (i = 7; i<8; i++) {
		printf("\n\n\n----------------------------------\n"
		       "$ Searching fs %s\n", mnt_str[7-i] );
		tok = inv_open( INV_BY_MOUNTPT, INV_SEARCH_ONLY, mnt_str[7-i] );
		if (! tok ) return -1;

		prctx.index = i;
		if (level == -1 )
			invmgr_inv_print( tok->d_invindex_fd, &prctx );
		else {
		if (inv_lasttime_level_lessthan( tok, level, &tm ) && tm) {
			printf("\n\nTIME %s %ld\n", ctime(tm), (long) *tm );
			free (tm);
		}
		if (inv_lastsession_level_lessthan( tok, level, &ses ) && ses) {
			DEBUG_sessionprint( ses, 99, &prctx);
			free ( ses->s_streams );
			free ( ses );
		}

		if (inv_lastsession_level_equalto( tok, level, &ses ) && ses) {
			printf("Gotcha\n");
			DEBUG_sessionprint( ses, 99, &prctx );
			free ( ses->s_streams );
			free ( ses );
		}
	        }
		inv_close( tok );
	}
	return 1;
}				
				   

/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

intgen_t
write_test( int nsess, int nstreams, int nmedia, int dumplevel )
{
	int i,j,k,m,fd;
	uint_t stat;
	uuid_t *fsidp;
	inv_idbtoken_t tok1;
	inv_sestoken_t tok2;
	inv_stmtoken_t tok3;	
	char *dev, *mnt;
	char label[120];
	uuid_t fsidarr[8], labelid;
	uuid_t sesidarr[8];
	char *str;
	char strbuf[128];
	void *bufp;
	size_t sz;
	int rfd;

#ifdef FIRSTTIME
	printf("first time!\n");
	for (i=0; i<8; i++) {
		uuid_create( &fsidarr[i], &stat );
		ASSERT ( stat == uuid_s_ok );
		uuid_create( &sesidarr[i], &stat );
		ASSERT ( stat == uuid_s_ok );
	}
	fd = open( "uuids", O_RDWR | O_CREAT );
	PUT_REC(fd, (void *)fsidarr, sizeof (uuid_t) * 8, 0L );
	PUT_REC(fd, (void *)sesidarr, sizeof (uuid_t) * 8, sizeof (uuid_t) * 8 );
	close(fd);
#endif
	fd = open("uuids", O_RDONLY );
	GET_REC( fd, fsidarr, sizeof (uuid_t) * 8, 0L );
	GET_REC( fd, sesidarr, sizeof (uuid_t) * 8, sizeof (uuid_t) * 8 );
	close(fd);
#ifdef RECONS
	rfd = open( sesfile, O_RDWR | O_CREAT );
	fchmod( rfd, INV_PERMS );
#endif

	for ( i = 0; i < nsess; i++ ) {
		j = i % 8;
		/*mnt = mnt_str[j];
		dev = dev_str[7-j];*/
		mnt = mnt_str[0];
		dev = dev_str[7];
		fsidp = &fsidarr[0]; /* j */
		tok1 = inv_open( INV_BY_UUID, INV_SEARCH_N_MOD, fsidp );
		ASSERT (tok1 != INV_TOKEN_NULL );

		uuid_create( &labelid, &stat );
		uuid_to_string( &labelid, &str, &stat );
		strncpy( strbuf, str, 8 );
		free (str);
		strbuf[8] = '\0';
		sprintf(label,"%s_%s (%d)\0","SESSION_LABEL", strbuf, i );
		
		tok2 = inv_writesession_open(tok1, fsidp,
					     &labelid,
					     label, 
					     (bool_t)i%2,
					     (bool_t)i%2,
					     dumplevel, nstreams, 
					     time(NULL),
					     mnt, dev );
		ASSERT (tok2 != INV_TOKEN_NULL );
		for (m = 0; m<nstreams; m++) {
			tok3 = inv_stream_open( tok2,"/dev/rmt");
			ASSERT (tok3 != INV_TOKEN_NULL );

			for (k = 0; k<nmedia; k++ )
				CREAT_mfiles( tok3, &labelid, k*100,
					      k*100 + 99 );
			inv_stream_close( tok3, BOOL_TRUE );
		}
	
#ifdef RECONS
		if (inv_get_sessioninfo( tok2, &bufp, &sz ) == BOOL_TRUE ) {
		      put_invtrecord( rfd, fsidp, sizeof( uuid_t ), 0, 
				        SEEK_END, BOOL_FALSE );
			
			put_invtrecord( rfd, &sz, sizeof( size_t ), 0,
				        SEEK_END, BOOL_FALSE); 
			put_invtrecord( rfd, bufp, sz, 0, SEEK_END, BOOL_FALSE );
		}
#endif
#ifdef NOTDEF
		if (inv_get_sessioninfo( tok2, &bufp, &sz ) == BOOL_TRUE )
			inv_put_sessioninfo(  fsidp, bufp, sz );
#endif
		inv_writesession_close( tok2 );
		inv_close( tok1 );
	}	
#ifdef RECONS	
	close( rfd );
#endif
	return 1;
}


void usage( void )
{
	printf("( %s ./inv w|r|q -v <verbosity> -s <nsess>"
	       "-t <strms> -m <nmediafiles> \n", optarg );
}


int
mp_test(int nstreams)
{
#if 0
	tok1 = inv_open( INV_BY_UUID, fsidp );
	ASSERT (tok1 != INV_TOKEN_NULL );

	tok2 = inv_writesession_open(tok1, fsidp,
				     &labelid,
				     label, 
				     (bool_t)i%2,
				     (bool_t)i%2,
				     dumplevel, nstreams, 
				     time(NULL),
				     mnt, dev );
	ASSERT (tok2 != INV_TOKEN_NULL );

	for (m = 0; m<nstreams; m++) {
			tok3 = inv_stream_open( tok2,"/dev/rmt");
			ASSERT (tok3 != INV_TOKEN_NULL );

			for (k = 0; k<nmedia; k++ )
				CREAT_mfiles( tok3, &labelid, k*100,
					      k*100 + 99 );
			inv_stream_close( tok3, BOOL_TRUE );
		}
#endif	
}




/*----------------------------------------------------------------------*/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/

	
main(int argc, char *argv[])
{
	int rval = -1, nsess = 8, nmedia = 2, nstreams = 3, level = 0;
	int cc = 0;
	extern char *optarg;
	extern int optind;
	char *uuid = NULL;
	char *label = NULL;
	hi op = BULL;

	progname = argv[0];
	sesfile = "sessions";
	assert ( argc > 1 );
	
	mlog_init( argc, argv );

	if (! inv_DEBUG_print(argc, argv))
		return 0;
	
	optind = 1;
	optarg = 0;

	while( ( cc = getopt( argc, argv, GETOPT_CMDSTRING)) != EOF ) {
		switch ( cc ) {
		      case 'w':
			op = WRI;
			break;
		      case 'r':
			op = REC;	
			break;
			
		      case 'q':
			op = QUE;			
			break;
			
		      case 'd':
			op = DEL;			
			break;

		      case 'z':
			op = MP;
			break;
			
		      case 'g':
			op = QUE2;
			break;

		      case 'u':
			uuid = optarg;
			break;

		      case 'L':
			label = optarg;
			break;

		      case 's':
			nsess = atoi(optarg);
			break;
			
		      case 'l':
			level = atoi(optarg);
			break;

		      case 't':
			nstreams = atoi(optarg);
			break;
			
		      case 'm':
			nmedia = atoi( optarg );
			break;
		      case 'v':
			break;
			
		      case 'f':
			sesfile = optarg;
			break;

		      default:
			usage(); 
			break;
		}
	}
	
	
	if ( op == WRI )
		rval = write_test( nsess, nstreams, nmedia, level );
	else if ( op == QUE )
		rval = query_test( level );
	else if ( op == REC )
		rval = recons_test( nsess );
	else if ( op == DEL )
		rval = delete_test( nsess );
	else if ( op == MP )
		rval = mp_test (nstreams);
	else if ( op == QUE2 ) {
		if (uuid)
			rval = sess_queries_byuuid(uuid);
		else if (label)
			rval = sess_queries_bylabel(label);
		}	
	else
		usage();
		
	if (rval < 0 )
		printf( "aborted\n");
	else
		printf( "done\n" );


}

